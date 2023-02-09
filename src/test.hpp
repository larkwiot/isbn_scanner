#pragma once

#ifdef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#undef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#define DOCTEST_CONFIG_IMPLEMENT

#include <doctest/doctest.h>

int main(int argc, char** argv) {
	auto console_log = spdlog::stdout_color_mt("console");
	auto error_log = spdlog::stdout_color_mt("stderr");
	spdlog::flush_every(std::chrono::seconds(5));
	spdlog::set_level(spdlog::level::debug);

	doctest::Context context;

	context.applyCommandLine(argc, argv);

	int res = context.run();

	if (context.shouldExit()) {
		return res;
	}

	return res;
}
#else
#define ISBN_SCANNER_IMPLEMENT_MAIN
#include <doctest/doctest.h>
#endif

