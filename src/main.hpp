/*
   Copyright 2023 larkwiot

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <filesystem>
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <unordered_set>

#include <assert.hpp>

//#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

#include <clipp.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

//#include <fmt/ranges.h>
#include <ctre.hpp>
#include <pugixml.hpp>
#include <indicators/block_progress_bar.hpp>
#include <nlohmann/json.hpp>
#include <taskflow.hpp>
#include <unordered_set>
#include <tao/tuple/tuple.hpp>

#define TOML_HEADER_ONLY 0
#define TOML_IMPLEMENTATION
#include <toml++/toml.h>

#include "util.hpp"
#include "thread_safe_file.hpp"
#include "book.hpp"
#include "version.hpp"

#pragma once

using json = nlohmann::json;

enum class TransferMode {
    MOVE, COPY, DRY_RUN
};
