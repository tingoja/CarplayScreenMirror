#pragma once
#include <scs_sdk/scssdk_telemetry.h>
#include <string>
#include <cstdarg>
#include <cstdio>

namespace scs_logging {
	inline scs_log_t g_scs_log{};
	inline std::string g_prefix{};

	inline void init(const scs_telemetry_init_params_t* const params, const std::string& prefix) {
		const scs_telemetry_init_params_v101_t* version_params = reinterpret_cast<const scs_telemetry_init_params_v101_t*>(params);
		g_scs_log = version_params->common.log;
		g_prefix = "[" + prefix + "] ";
	}

	inline void scs_log(const scs_log_type_t type, const char* format, ...) {
		if (!g_scs_log) return;

		char buffer[1024];
		va_list args;
		va_start(args, format);
		vsnprintf(buffer, sizeof(buffer), format, args);
		va_end(args);

		std::string final_message = g_prefix + buffer;
		g_scs_log(type, final_message.c_str());
	}
}
