cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(ISBNScanner VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(scanner src/main.cpp src/main.hpp)

include(cmake/Findluajit.cmake)
target_link_libraries(scanner PRIVATE ${LUAJIT_LIBRARY})

include(cmake/CPM.cmake)

CPMAddPackage(NAME libassert GITHUB_REPOSITORY jeremy-rifkin/libassert GIT_TAG v1.1)
target_link_libraries(scanner PRIVATE assert dl)

CPMAddPackage("gh:fmtlib/fmt#9.1.0")
CPMAddPackage(NAME spdlog GITHUB_REPOSITORY gabime/spdlog GIT_TAG v1.11.0 OPTIONS "SPDLOG_FMT_EXTERNAL 1")
target_link_libraries(scanner PRIVATE spdlog::spdlog_header_only)

CPMAddPackage(NAME sol2 GITHUB_REPOSITORY ThePhD/sol2 GIT_TAG v3.3.0 DOWNLOAD_ONLY YES)
add_library(sol2 INTERFACE IMPORTED)
target_include_directories(sol2 INTERFACE ${sol2_SOURCE_DIR}/include ${LUAJIT_INCLUDE_DIR})
target_link_libraries(sol2 INTERFACE ${LUAJIT_LIBRARY})
target_link_libraries(scanner PRIVATE sol2)

CPMAddPackage("gh:libcpr/cpr#1.10.0")
target_link_libraries(scanner PRIVATE cpr)

CPMAddPackage(NAME taocpptuple GITHUB_REPOSITORY taocpp/tuple GIT_TAG 1.0.0 DOWNLOAD_ONLY YES)
add_library(taocpptuple INTERFACE IMPORTED)
target_include_directories(taocpptuple INTERFACE ${taocpptuple_SOURCE_DIR}/include)
target_link_libraries(scanner PRIVATE taocpptuple)

CPMAddPackage(NAME magic_enum GITHUB_REPOSITORY Neargye/magic_enum GIT_TAG v0.8.2)
target_link_libraries(scanner PRIVATE magic_enum)

CPMAddPackage(NAME clipp GITHUB_REPOSITORY muellan/clipp GIT_TAG v1.2.3 DOWNLOAD_ONLY YES)
add_library(clipp INTERFACE IMPORTED)
target_include_directories(clipp INTERFACE ${clipp_SOURCE_DIR}/include)
target_link_libraries(scanner PRIVATE clipp)

CPMAddPackage(NAME toml GITHUB_REPOSITORY marzer/tomlplusplus GIT_TAG v3.2.0 DOWNLOAD_ONLY YES)
add_library(toml INTERFACE IMPORTED)
target_include_directories(toml INTERFACE ${toml_SOURCE_DIR}/include)
target_link_libraries(scanner PRIVATE toml)

CPMAddPackage(NAME doctest GITHUB_REPOSITORY doctest/doctest GIT_TAG v2.4.9)
target_link_libraries(scanner PRIVATE doctest)

CPMAddPackage(NAME ctre GITHUB_REPOSITORY "hanickadot/compile-time-regular-expressions" VERSION 3.7.1 DOWNLOAD_ONLY YES)
add_library(ctre INTERFACE IMPORTED)
target_include_directories(ctre INTERFACE ${ctre_SOURCE_DIR}/single-header)
target_link_libraries(scanner PRIVATE ctre)

CPMAddPackage("gh:zeux/pugixml#v1.13")
target_link_libraries(scanner PRIVATE pugixml)

CPMAddPackage("gh:p-ranav/indicators#v2.2")
target_link_libraries(scanner PRIVATE indicators)

CPMAddPackage(NAME nlohmann_json VERSION 3.11.2 URL https://github.com/nlohmann/json/releases/download/v3.11.2/include.zip URL_HASH SHA256=e5c7a9f49a16814be27e4ed0ee900ecd0092bfb7dbfca65b5a421b774dccaaed)
add_library(nlohmann_json INTERFACE IMPORTED)
target_include_directories(nlohmann_json INTERFACE ${nlohmann_json_SOURCE_DIR}/include)
target_link_libraries(scanner PRIVATE nlohmann_json)

CPMAddPackage(NAME taskflow GITHUB_REPOSITORY taskflow/taskflow GIT_TAG v3.5.0 DOWNLOAD_ONLY YES)
add_library(taskflow INTERFACE IMPORTED)
target_include_directories(taskflow INTERFACE ${taskflow_SOURCE_DIR}/taskflow)
target_link_libraries(scanner PRIVATE taskflow)
