#pragma once

#include <restinio/all.hpp>

#include <string>

#include "pet_data_types.hpp"
#include "db_layer.hpp"

namespace crud_example
{

class request_processor_t
{
public:
	request_processor_t(db_layer_t & db);

	void
	on_create_new_pet(
		const restinio::request_handle_t & req);

	void
	on_get_all_pets(
		const restinio::request_handle_t & req);

	void
	on_get_specific_pet(
		const restinio::request_handle_t & req,
		pet_id_t pet_id);

	void
	on_patch_specific_pet(
		const restinio::request_handle_t & req,
		pet_id_t pet_id);

	void
	on_delete_specific_pet(
		const restinio::request_handle_t & req,
		pet_id_t pet_id);

	void
	on_make_batch_upload_form(
		const restinio::request_handle_t & req);

private:
	db_layer_t & m_db;

	model::pet_identity_t
	create_new_pet(const restinio::request_handle_t & req);

	model::bunch_of_pet_ids_t
	batch_create_new_pets(const restinio::request_handle_t & req);

	model::all_pets_t
	get_all_pets();

	model::pet_with_id_t
	get_specific_pet(pet_id_t pet_id);

	model::pet_identity_t
	patch_specific_pet(
		const restinio::request_handle_t & req,
		pet_id_t pet_id);

	model::pet_identity_t
	delete_specific_pet(pet_id_t pet_id);
};

} /* namespace crud_example */

