#include "db_layer.hpp"

namespace crud_example
{

db_layer_t::db_with_tables_t::db_with_tables_t(
	const char * database_name)
	:	m_db{database_name, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE}
{
	// Create the main table if it is not here yet.
	m_db.exec(R"sql(
			create table if not exists pets(
				id integer primary key autoincrement,
				name text,
				type text,
				owner text,
				picture text);
		)sql");
}

db_layer_t::db_layer_t(const char * database_name)
	:	m_db{database_name}
	,	m_create_new_stmt{m_db,
			R"sql(insert into pets(name, type, owner, picture)
					values(:name, :type, :owner, :picture);)sql"}
	,	m_last_insert_rowid_stmt{m_db,
			R"sql(select last_insert_rowid();)sql"}
	,	m_get_all_pets_stmt{m_db,
			R"sql(select id, name, type, owner, picture from pets;)sql"}
	,	m_get_pet_stmt{m_db,
			R"sql(select id, name, type, owner, picture from pets
					where id = :id;)sql"}
	,	m_update_pet_stmt{m_db,
			R"sql(update pets set
						name = :name,
						type = :type,
						owner = :owner,
						picture = :picture
					where id = :id)sql"}
	,	m_delete_pet_stmt{m_db,
			R"sql(delete from pets where id = :id)sql"}
{
}

pet_id_t
db_layer_t::create_new_pet(const model::pet_without_id_t & pet)
{
	std::lock_guard<std::mutex> lock{m_lock};

	m_create_new_stmt.reset();
	m_create_new_stmt.clearBindings();

	m_create_new_stmt.bindNoCopy(":name", pet.m_data.m_name);
	m_create_new_stmt.bindNoCopy(":type", pet.m_data.m_type);
	m_create_new_stmt.bindNoCopy(":owner", pet.m_data.m_owner);
	m_create_new_stmt.bindNoCopy(":picture", pet.m_data.m_picture);

	m_create_new_stmt.exec();

	m_last_insert_rowid_stmt.reset();
	m_last_insert_rowid_stmt.executeStep();

	return m_last_insert_rowid_stmt.getColumn(0);
}

model::bunch_of_pet_ids_t
db_layer_t::create_bunch_of_pets(
	const model::bunch_of_pets_without_id_t & pets)
{
	model::bunch_of_pet_ids_t result;

	std::lock_guard<std::mutex> lock{m_lock};

	SQLite::Transaction trx{m_db};

	for(const auto & current : pets.m_pets)
	{
		m_create_new_stmt.reset();
		m_create_new_stmt.clearBindings();

		m_create_new_stmt.bindNoCopy(":name", current.m_data.m_name);
		m_create_new_stmt.bindNoCopy(":type", current.m_data.m_type);
		m_create_new_stmt.bindNoCopy(":owner", current.m_data.m_owner);
		m_create_new_stmt.bindNoCopy(":picture", current.m_data.m_picture);

		m_create_new_stmt.exec();

		m_last_insert_rowid_stmt.reset();
		m_last_insert_rowid_stmt.executeStep();

		result.m_ids.push_back(m_last_insert_rowid_stmt.getColumn(0));
	}

	trx.commit();

	return result;
}

model::all_pets_t
db_layer_t::get_all_pets()
{
	model::all_pets_t result;

	std::lock_guard<std::mutex> lock{m_lock};

	m_get_all_pets_stmt.reset();
	while(m_get_all_pets_stmt.executeStep())
	{
		model::pet_with_id_t pet;
		pet.m_id = m_get_all_pets_stmt.getColumn(0);
		pet.m_data.m_name = m_get_all_pets_stmt.getColumn(1).getString();
		pet.m_data.m_type = m_get_all_pets_stmt.getColumn(2).getString();
		pet.m_data.m_owner = m_get_all_pets_stmt.getColumn(3).getString();
		pet.m_data.m_picture = m_get_all_pets_stmt.getColumn(4).getString();

		result.m_pets.push_back(std::move(pet));
	}

	return result;
}

nonstd::optional<model::pet_with_id_t>
db_layer_t::get_pet(pet_id_t id)
{
	nonstd::optional<model::pet_with_id_t> result;

	std::lock_guard<std::mutex> lock{m_lock};

	m_get_pet_stmt.reset();
	m_get_pet_stmt.clearBindings();

	m_get_pet_stmt.bind(":id", id);

	if(m_get_pet_stmt.executeStep())
	{
		model::pet_with_id_t pet;

		pet.m_id = m_get_pet_stmt.getColumn(0);
		pet.m_data.m_name = m_get_pet_stmt.getColumn(1).getString();
		pet.m_data.m_type = m_get_pet_stmt.getColumn(2).getString();
		pet.m_data.m_owner = m_get_pet_stmt.getColumn(3).getString();
		pet.m_data.m_picture = m_get_pet_stmt.getColumn(4).getString();

		result = std::move(pet);
	}

	return result;
}

db_layer_t::update_result_t
db_layer_t::update_pet(pet_id_t id, const model::pet_without_id_t & pet)
{
	std::lock_guard<std::mutex> lock{m_lock};

	m_update_pet_stmt.reset();
	m_update_pet_stmt.clearBindings();

	m_update_pet_stmt.bindNoCopy(":name", pet.m_data.m_name);
	m_update_pet_stmt.bindNoCopy(":type", pet.m_data.m_type);
	m_update_pet_stmt.bindNoCopy(":owner", pet.m_data.m_owner);
	m_update_pet_stmt.bindNoCopy(":picture", pet.m_data.m_picture);
	m_update_pet_stmt.bind(":id", id);

	return 1 == m_update_pet_stmt.exec() ?
			update_result_t::updated : update_result_t::not_found;
}

db_layer_t::delete_result_t
db_layer_t::delete_pet(pet_id_t id)
{
	std::lock_guard<std::mutex> lock{m_lock};

	m_delete_pet_stmt.reset();
	m_delete_pet_stmt.clearBindings();

	m_delete_pet_stmt.bind(":id", id);

	return 1 == m_delete_pet_stmt.exec() ?
			delete_result_t::deleted : delete_result_t::not_found;
}

} /* namespace crud_example */

