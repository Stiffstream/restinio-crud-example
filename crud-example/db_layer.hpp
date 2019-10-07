#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <nonstd/optional.hpp>

#include "pet_data_types.hpp"

#include <mutex>

namespace crud_example
{

class db_layer_t
{
public:
	// The result of 'update pet' operation.
	enum class update_result_t
	{
		updated,
		not_found
	};

	// The result of 'delete pet' operation.
	enum class delete_result_t
	{
		deleted,
		not_found
	};

	db_layer_t(const char * database_name);

	pet_id_t
	create_new_pet(const model::pet_without_id_t & pet);

	model::all_pets_t
	get_all_pets();

	nonstd::optional<model::pet_with_id_t>
	get_pet(pet_id_t id);

	update_result_t
	update_pet(pet_id_t id, const model::pet_without_id_t & pet);

	delete_result_t
	delete_pet(pet_id_t id);

private:
	// This is a special class that open a DB instance and
	// creates necessary table(s) if needed.
	//
	// The presence of that class allows to initialize
	// prepared statements just in the constructor of db_layer_t
	// (note it is an error to create a prepared statement if
	// there is no table in the DB yet).
	class db_with_tables_t
	{
		SQLite::Database m_db;

	public:
		db_with_tables_t(const char * database_name);

		auto & db() noexcept { return m_db; }

		operator SQLite::Database&() noexcept { return m_db; }
	};

	db_with_tables_t m_db;

	std::mutex m_lock;

	SQLite::Statement m_create_new_stmt;
	SQLite::Statement m_last_insert_rowid_stmt;
	SQLite::Statement m_get_all_pets_stmt;
	SQLite::Statement m_get_pet_stmt;
	SQLite::Statement m_update_pet_stmt;
	SQLite::Statement m_delete_pet_stmt;
};

} /* namespace crud_example */

