#include "request_processor.hpp"

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

} /* namespace anonymous */

request_processor_t::request_processor_t(db_layer_t & db)
	:	m_db{db}
{
}

void
request_processor_t::on_create_new_pet(
	const restinio::request_handle_t & req)
{
	wrap_request_processing(req, [&] { return create_new_pet(req); });
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

