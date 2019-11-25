#include "request_processor.hpp"

#include <restinio/helpers/http_field_parsers/content-type.hpp>
#include <restinio/helpers/file_upload.hpp>

#include <fmt/format.h>

#include <stdexcept>

namespace crud_example
{

namespace errors
{

const int unknow_error = -1;
const int json_dto_error = 1;
const int sqlite_error = 2;
const int invalid_pet_id = 3;
const int invalid_request = 4;

} /* namespace errors */

// Type of object to be returned in a HTTP-response for a failed request.
struct failure_description_t
{
	int m_error_code;
	std::string m_description;

	template<typename Json_Io>
	void json_io(Json_Io & io)
	{
		io & json_dto::mandatory("code", m_error_code)
			& json_dto::mandatory("description", m_description);
	}
};

// A type of exception to be thrown in the case of some failure
// during processing of a request.
//
// NOTE: this error is related to some business-logic problem.
class request_processing_failure_t : public std::runtime_error
{
	const restinio::http_status_line_t m_response_status;
	const failure_description_t m_failure_description;

public:
	request_processing_failure_t(
		restinio::http_status_line_t response_status,
		failure_description_t failure_description)
		:	std::runtime_error("request processing failure")
		,	m_response_status{std::move(response_status)}
		,	m_failure_description{std::move(failure_description)}
	{}

	const auto &
	response_status() const noexcept
	{
		return m_response_status;
	}

	const auto &
	failure_description() const noexcept
	{
		return m_failure_description;
	}
};

namespace
{

// Helper function for wrapping request processing routine and making
// the response in dependency of processing result.
template<typename F>
void
wrap_request_processing(
	const restinio::request_handle_t & req,
	F && functor)
{
	std::string response_body;
	auto response_status = restinio::status_ok();
	try
	{
		response_body = json_dto::to_json(functor());
	}
	catch(const request_processing_failure_t & x)
	{
		response_status = x.response_status();
		response_body = json_dto::to_json(x.failure_description());
	}
	catch(...)
	{
		response_status = restinio::status_internal_server_error();
		response_body = json_dto::to_json(
				failure_description_t{
						errors::unknow_error,
						"unexpected application failure"});
	}

	req->create_response(response_status)
		.append_header_date_field()
		.append_header(restinio::http_field::content_type, "application/json")
		.set_body(response_body)
		.done();
}

// Helper function for wrapping actual business-logic code and intercept
// errors related to JSON-processing, interactions with DB and so on.
// All such errors are converted into request_processing_failure_t.
template<typename F>
auto
wrap_business_logic_action(F && functor)
{
	try
	{
		return functor();
	}
	catch(const json_dto::ex_t & x)
	{
		throw request_processing_failure_t(
				restinio::status_bad_request(),
				failure_description_t{
						errors::json_dto_error,
						fmt::format("json-related-error: {}", x.what())
				});
	}
	catch(const SQLite::Exception & x)
	{
		throw request_processing_failure_t(
				restinio::status_internal_server_error(),
				failure_description_t{
						errors::sqlite_error,
						fmt::format("sqlite-related-error: error_code={}, "
								"ext_error_code={}, desc='{}'",
								x.getErrorCode(),
								x.getExtendedErrorCode(),
								x.getErrorStr())
				});
	}
}

enum class create_new_mode_t
{
	single,
	batch
};

restinio::expected_t<create_new_mode_t, request_processing_failure_t>
detect_create_new_mode(
	const restinio::request_handle_t & req)
{
	const auto unexpected = [](const char * msg) {
		return restinio::make_unexpected(request_processing_failure_t{
				restinio::status_bad_request(),
				failure_description_t{errors::invalid_request, msg}
			});
	};

	// Content-Type HTTP-field should be present.
	const auto content_type_raw = req->header().opt_value_of(
			restinio::http_field::content_type);
	if(!content_type_raw)
		return unexpected("Content-Type HTTP-field is absent");

	// Content-Type should have right format.
	namespace hfp = restinio::http_field_parsers;
	const auto content_type = hfp::content_type_value_t::try_parse(*content_type_raw);
	if(!content_type)
		return unexpected("unable to parse Content-Type HTTP-field");

	// If Content-Type is "application/json" then we assume that it is
	// a request for addition of single pet.
	if("application" == content_type->media_type.type &&
			"json" == content_type->media_type.subtype)
		return create_new_mode_t::single;

	// If Content-Type if "multipart/form-data" then we assume that it is
	// a request for batch addition of new pets.
	if("multipart" == content_type->media_type.type &&
			"form-data" == content_type->media_type.subtype)
		return create_new_mode_t::batch;

	return unexpected("unsupported value of Content-Type");
}

} /* namespace anonymous */

request_processor_t::request_processor_t(db_layer_t & db)
	:	m_db{db}
{
}

void
request_processor_t::on_create_new_pet(
	const restinio::request_handle_t & req)
{
	auto mode = detect_create_new_mode(req);
	if(mode)
	{
		switch(*mode)
		{
		case create_new_mode_t::single:
			wrap_request_processing(req, [&] { return create_new_pet(req); });
		break;

		case create_new_mode_t::batch:
			wrap_request_processing(req, [&] { return batch_create_new_pets(req); });
		break;
		}
	}
	else
	{
		const auto & error = mode.error();
		req->create_response(error.response_status())
			.append_header_date_field()
			.append_header(restinio::http_field::content_type, "application/json")
			.set_body(json_dto::to_json(error.failure_description()))
			.done();
	}
}

void
request_processor_t::on_get_all_pets(
	const restinio::request_handle_t & req)
{
	wrap_request_processing(req, [&] { return get_all_pets(); });
}

void
request_processor_t::on_get_specific_pet(
	const restinio::request_handle_t & req,
	pet_id_t pet_id)
{
	wrap_request_processing(req, [&] { return get_specific_pet(pet_id); });
}

void
request_processor_t::on_patch_specific_pet(
	const restinio::request_handle_t & req,
	pet_id_t pet_id)
{
	wrap_request_processing(req, [&] { return patch_specific_pet(req, pet_id); });
}

void
request_processor_t::on_delete_specific_pet(
	const restinio::request_handle_t & req,
	pet_id_t pet_id)
{
	wrap_request_processing(req, [&] { return delete_specific_pet(pet_id); });
}

void
request_processor_t::on_make_batch_upload_form(
	const restinio::request_handle_t & req)
{
	req->create_response()
		.append_header_date_field()
		.append_header(restinio::http_field::content_type, "text/html; charset=utf-8")
		.set_body(
R"---(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Select file with batch info of new pets</title>
</head>
<body>
<p>Please select file to be uploaded to server.</p>
<form method="post" action="http://localhost:8080/all/v1/pets" enctype="multipart/form-data">
    <p><input type="file" name="file" id="file"></p>
    <p><button type="submit">Submit</button></p>
</form>
</body>
</html>
)---"
		)
		.done();
}

model::pet_identity_t
request_processor_t::create_new_pet(
	const restinio::request_handle_t & req)
{
	return wrap_business_logic_action([&] {
			return model::pet_identity_t{
				m_db.create_new_pet(
						json_dto::from_json<model::pet_without_id_t>(req->body()))
			};
		});
}

model::bunch_of_pet_ids_t
request_processor_t::batch_create_new_pets(
	const restinio::request_handle_t & req)
{
	using namespace restinio::file_upload;

	return wrap_business_logic_action([&]() {
			// Content of file with new pets should be found in
			// the request's body.
			//
			// NOTE: it's safe to store reference to uploaded file's content
			// as string_view because uploaded_content won't outlive the
			// request object with the actual uploaded data.
			//
			restinio::string_view_t uploaded_content;
			const auto result = enumerate_parts_with_files(*req,
				[this, &uploaded_content](part_description_t part) {
					if("file" == part.name)
					{
						uploaded_content = part.body;
						return handling_result_t::stop_enumeration;
					}
					else
						return handling_result_t::continue_enumeration;
				});

			if(!result || uploaded_content.empty())
				// There is no uploaded file or request's body has invalid format.
				throw request_processing_failure_t(
						restinio::status_bad_request(),
						failure_description_t{
								errors::invalid_request,
								"no file with new pets found"
						});

			// The content of uploaded file should be parsed.
			const auto pets = json_dto::from_json<model::bunch_of_pets_without_id_t>(
					json_dto::make_string_ref(
							uploaded_content.data(), uploaded_content.size()));

			return m_db.create_bunch_of_pets(pets);
		});
}

model::all_pets_t
request_processor_t::get_all_pets()
{
	return wrap_business_logic_action([&] {
			return m_db.get_all_pets();
		});
}

model::pet_with_id_t
request_processor_t::get_specific_pet(
	pet_id_t pet_id)
{
	return wrap_business_logic_action([&] {
			auto pet = m_db.get_pet(pet_id);
			if(!pet)
				throw request_processing_failure_t(
						restinio::status_not_found(),
						failure_description_t{
								errors::invalid_pet_id,
								fmt::format("pet with this ID not found, ID={}", pet_id)
						});
			return std::move(*pet);
		});
}

model::pet_identity_t
request_processor_t::patch_specific_pet(
	const restinio::request_handle_t & req,
	pet_id_t pet_id)
{
	return wrap_business_logic_action([&] {
			const auto update_result = m_db.update_pet(
					pet_id,
					json_dto::from_json<model::pet_without_id_t>(req->body()));
			if(db_layer_t::update_result_t::updated != update_result)
				throw request_processing_failure_t(
						restinio::status_not_found(),
						failure_description_t{
								errors::invalid_pet_id,
								fmt::format("pet with this ID not found, ID={}", pet_id)
						});
			return model::pet_identity_t{pet_id};
		});
}

model::pet_identity_t
request_processor_t::delete_specific_pet(
	pet_id_t pet_id)
{
	return wrap_business_logic_action([&] {
			const auto delete_result = m_db.delete_pet(pet_id);
			if(db_layer_t::delete_result_t::deleted != delete_result)
				throw request_processing_failure_t(
						restinio::status_not_found(),
						failure_description_t{
								errors::invalid_pet_id,
								fmt::format("pet with this ID not found, ID={}", pet_id)
						});
			return model::pet_identity_t{pet_id};
		});
}

} /* namespace crud_example */

