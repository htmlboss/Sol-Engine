#pragma once

#include <spdlog/spdlog.h>

// Helper macros to pass messages to spdlog (or maybe another logger in the future)

#define LOG_CRITICAL(msg) \
	spdlog::get("console")->critical(msg); \
	std::abort();

#define LOG_ERROR(msg) \
	spdlog::get("console")->error(msg);

#define LOG_INFO(msg) \
	spdlog::get("console")->info(msg);
