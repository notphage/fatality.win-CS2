#pragma once

namespace lua
{
	struct engine_net_data_t
	{
		std::atomic_bool has_data, has_key;
		//TODO: std::vector<networking::msg::script> delta;
		std::string enc_key;
	} extern net_data;
}
