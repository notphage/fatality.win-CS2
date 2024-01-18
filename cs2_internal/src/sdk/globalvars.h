#pragma once

namespace sdk
{
	using register_tags_func = void(*)();
	using logging_channel_id_t = int;

	enum logging_response_t
	{
		LR_CONTINUE,
		LR_DEBUGGER,
		LR_ABORT,
	};


	enum logging_severity_t
	{
		//-----------------------------------------------------------------------------
		// An informative logging message.
		//-----------------------------------------------------------------------------
		LS_MESSAGE = 0,

		//-----------------------------------------------------------------------------
		// A warning, typically non-fatal
		//-----------------------------------------------------------------------------
		LS_WARNING = 1,

		//-----------------------------------------------------------------------------
		// A message caused by an Assert**() operation.
		//-----------------------------------------------------------------------------
		LS_ASSERT = 2,

		//-----------------------------------------------------------------------------
		// An error, typically fatal/unrecoverable.
		//-----------------------------------------------------------------------------
		LS_ERROR = 3,

		//-----------------------------------------------------------------------------
		// A placeholder level, higher than any legal value.
		// Not a real severity value!
		//-----------------------------------------------------------------------------
		LS_HIGHEST_SEVERITY = 4,
	};

	// note: this is incomplete.
	// offset to client time (realtime in s1)
	struct global_vars_t
	{
		float real_time; // 0x0000
		int32_t frame_count; // 0x0004
		float frame_time; // 0x0008
		float abs_frame_time; // 0x000C
		int32_t max_clients; // 0x0010
		float interval_per_tick; // 0x0014
		char pad_0018[20]; // 0x0018
		float cur_time; // 0x002C
		char pad_0030[24]; // 0x0030
		void *net_chan; // 0x0048
		char pad_0050[304]; // 0x0050
		char *map_path; // 0x0180
		char *map_name; // 0x0188
		char pad_0190[112]; // 0x0190
	}; // Size: 0x0200
}
