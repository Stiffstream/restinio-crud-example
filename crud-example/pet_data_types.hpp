#pragma once

#include <json_dto/pub.hpp>

#include <string>

namespace crud_example
{

using pet_id_t = std::int32_t;

namespace model
{

struct pet_data_t
{
	std::string m_name;
	std::string m_type;
	std::string m_owner;
	std::string m_picture;

	template<typename Json_Io>
	void json_io(Json_Io & io)
	{
		io & json_dto::mandatory("name", m_name)
			& json_dto::mandatory("type", m_type)
			& json_dto::mandatory("owner", m_owner)
			& json_dto::mandatory("picture", m_picture);
	}
};

struct pet_without_id_t
{
	pet_data_t m_data;

	template<typename Json_Io>
	void json_io(Json_Io & io)
	{
		m_data.json_io(io);
	}
};

struct pet_with_id_t
{
	pet_id_t m_id;
	pet_data_t m_data;

	template<typename Json_Io>
	void json_io(Json_Io & io)
	{
		io & json_dto::mandatory("id", m_id);
		m_data.json_io(io);
	}
};

struct pet_identity_t
{
	pet_id_t m_id;

	template<typename Json_Io>
	void json_io(Json_Io & io)
	{
		io & json_dto::mandatory("id", m_id);
	}
};

struct all_pets_t
{
	std::vector<pet_with_id_t> m_pets;

	template<typename Json_Io>
	void json_io(Json_Io & io)
	{
		io & json_dto::mandatory("pets", m_pets);
	}
};

struct bunch_of_pets_without_id_t
{
	std::vector<pet_without_id_t> m_pets;

	template<typename Json_Io>
	void json_io(Json_Io & io)
	{
		io & json_dto::mandatory("pets", m_pets);
	}
};

struct bunch_of_pet_ids_t
{
	std::vector<pet_id_t> m_ids;

	template<typename Json_Io>
	void json_io(Json_Io & io)
	{
		io & json_dto::mandatory("ids", m_ids);
	}
};

} /* namespace model */

} /* namespace crud_example */

