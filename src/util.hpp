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

#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include <spdlog/spdlog.h>
#include <ctre.hpp>

#include "test.hpp"

#pragma once

using ISBN = unsigned long;

std::vector<char> read_file_bytes(const std::string& fn) {
	std::ifstream fh{fn, std::ios::binary | std::ios::ate};

	if (!fh) {
		//		spdlog::get("stderr")->error("could not open file: {}", fn);
		exit(0);
	}

	auto end = fh.tellg();
	fh.seekg(0, std::ios::beg);

	auto size = std::size_t(end - fh.tellg());

	if (size == 0) {
		return {};
	}

	std::vector<char> bytes(size);

	if (!fh.read(reinterpret_cast<char*>(bytes.data()), static_cast<long>(bytes.size()))) {
		//		spdlog::get("stderr")->error("could not read file: {}", fn);
		exit(0);
	}

	//	spdlog::get("console")->debug("process_file(): read file {} at {} bytes", fn, fn.size());

	return bytes;
}

std::string read_file_bytes_as_string(const std::string& fn) {
	std::ifstream fh{fn, std::ios::binary | std::ios::ate};

	if (!fh) {
		exit(0);
	}

	auto end = fh.tellg();
	fh.seekg(0, std::ios::beg);

	auto size = std::size_t(end - fh.tellg());

	if (size == 0) {
		return {};
	}

	std::string bytes;
	bytes.resize(size);

	if (!fh.read(reinterpret_cast<char*>(bytes.data()), static_cast<long>(bytes.size()))) {
		exit(0);
	}

	return bytes;
}

constexpr int ctoi(char c) {
	int res = c - '0';
	if (res < 0 || res > 9) {
		//		spdlog::get("stderr")->error("ctoi(): resulting number out of range! got {} -> {}", c, res);
		return res;
	}
	return res;
}

TEST_CASE("ctoi()") {
	CHECK(ctoi('0') == 0);
	CHECK(ctoi('9') == 9);
}

static std::unordered_set<char> filter_chars = {',', '.', '\'', '|', '-'};

static std::unordered_map<char, char> replace_chars = {{' ', '_'}, {':', '-'}};

std::string clean_name(std::string const& name) {
	std::string cleaned;

	for (auto c : name) {
		if (filter_chars.contains(c)) {
			continue;
		} else if (replace_chars.contains(c)) {
			cleaned += replace_chars.at(c);
		} else {
			cleaned += c;
		}
	}

	return cleaned;
}

const std::unordered_set<char> isbn_chars = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'X'};

static constexpr auto all_one_char = ctll::fixed_string{"(.)\\1+"};

tao::tuple<bool, ISBN> is_valid_isbn(std::string isbn) {
//	spdlog::get("console")->debug("is_valid_isbn(): raw ISBN: {}", isbn);

	std::erase_if(isbn, [](char c) { return !isbn_chars.contains(c); });

	if (ctre::match<all_one_char>(isbn)) {
		return tao::make_tuple(false, 0ul);
	}

	if (isbn == "0123456789") {
		return tao::make_tuple(false, 0ul);
	}

	if (isbn.length() == 10) {
		spdlog::get("console")->debug("is_valid_isbn(): cleaned ISBN: {}", isbn);

		int multiplier = 10;
		int sum = 0;

		auto isbn_end = isbn.end();
		for (auto di = isbn.begin(); di != isbn_end; ++di) {
			if (multiplier < 1) {
				spdlog::get("stderr")->error("is_valid_isbn(): tried to use ISBN 10 multiplier < 2! ISBN: {}", isbn);
			}

			auto d = *di;

			if (d == 'X') {
				if (di == isbn_end - 1) {
					sum += multiplier * 10;
				} else {
					spdlog::get("console")->debug("is_valid_isbn(): ISBN {} had X not at end", isbn);
					return tao::make_tuple(false, 0ul);
				}
			} else {
				sum += multiplier * ctoi(d);
			}

			multiplier--;
		}

		if (sum % 11 == 0) {
			return tao::make_tuple(true, std::stoul(isbn));
			return tao::make_tuple(true, std::stoul(isbn));
		}

		spdlog::get("console")->debug("is_valid_isbn(): ISBN {} invalid ISBN 10 checksum", isbn);

		return tao::make_tuple(false, 0ul);

	} else if (isbn.length() == 13) {
		spdlog::get("console")->debug("is_valid_isbn(): cleaned ISBN: {}", isbn);

		auto first_12 = isbn.substr(0, 12);
		unsigned long check_digit = 0;
		try {
			check_digit = std::stoul(isbn.substr(12));
		} catch (const std::invalid_argument& err) {
			spdlog::get("console")->debug("ISBN {} cannot be converted to number", isbn.substr(12));
			return tao::make_tuple(false, 0ul);
		}
		int multiplier = 1;
		int sum = 0;

		for (auto c : first_12) {
			sum += multiplier * ctoi(c);
			// swap values for multiplier
			multiplier ^= 1 ^ 3;
		}

		int remainder = sum % 10;

		if (remainder == 0 && check_digit == 0) {
			return tao::make_tuple(true, std::stoul(isbn));
		}

		if (std::cmp_equal((10 - remainder), check_digit)) {
			return tao::make_tuple(true, std::stoul(isbn));
		}

		spdlog::get("console")->debug("is_valid_isbn(): ISBN {} invalid ISBN 13 checksum", isbn);

		return tao::make_tuple(false, 0ul);
	}

//	spdlog::get("console")->debug("is_valid_isbn(): ISBN {} is not a valid length", isbn);

	return tao::make_tuple(false, 0ul);
}

TEST_CASE("is_valid_isbn()") {
	// valid ISBN10s
	CHECK(get<0>(is_valid_isbn("0071466932")) == true);
	CHECK(get<0>(is_valid_isbn("193176932X")) == true);
	CHECK(get<0>(is_valid_isbn("052159104X")) == true);
	CHECK(get<0>(is_valid_isbn("158113052X")) == true);
	CHECK(get<0>(is_valid_isbn("8425507006")) == true);
	CHECK(get<0>(is_valid_isbn("0534393217")) == true);

	// invalid ISBN10s
	CHECK(get<0>(is_valid_isbn("1931769329")) == false);
	CHECK(get<0>(is_valid_isbn("1581130522")) == false);
	CHECK(get<0>(is_valid_isbn("8425507005")) == false);
	CHECK(get<0>(is_valid_isbn("053439XXXX")) == false);
	CHECK(get<0>(is_valid_isbn("12389X9814")) == false);
	CHECK(get<0>(is_valid_isbn("0000000000")) == false);
	CHECK(get<0>(is_valid_isbn("1111111111")) == false);

	// valid ISBN13s
	CHECK(get<0>(is_valid_isbn("9780735682931")) == true);
	CHECK(get<0>(is_valid_isbn("9780672328978")) == true);
	CHECK(get<0>(is_valid_isbn("9781447123309")) == true);
	CHECK(get<0>(is_valid_isbn("9780735682931")) == true);
	CHECK(get<0>(is_valid_isbn("9780735682931")) == true);
	CHECK(get<0>(is_valid_isbn("9781447123309")) == true);

	// invalid ISBN13s
	CHECK(get<0>(is_valid_isbn("978073568293X")) == false);
	CHECK(get<0>(is_valid_isbn("9780672328928")) == false);
	CHECK(get<0>(is_valid_isbn("9780735682932")) == false);
	CHECK(get<0>(is_valid_isbn("9780735482931")) == false);
	CHECK(get<0>(is_valid_isbn("9781447123308")) == false);
}

static constexpr auto isbn_pattern = ctll::fixed_string{"([0-9\\-\\s]+[0-9X])"};
static constexpr auto file_extension_pattern = ctll::fixed_string{"\\.([^\\.]+)$"};

auto find_isbns(const std::string& text) {
	auto matches = std::set<std::string>{};
	for (auto match : ctre::range<isbn_pattern>(text)) {
		matches.emplace(match.get<0>());
	}
	return matches;
}

TEST_CASE("find_isbns()") {
	auto result = find_isbns("007 14-66693       \t2");
	CHECK(result.size() == 1);
}

std::string get_file_extension(const std::string& fn) {
	auto match = ctre::search<file_extension_pattern>(fn);
	if (match) {
		return match.get<1>().to_string();
	}
	return "";
}

TEST_CASE("get_file_extension()") {
	CHECK(get_file_extension("blah.pdf") == "pdf");
	CHECK(get_file_extension("blah.......pdf") == "pdf");
	CHECK(get_file_extension("blah    .pdf") == "pdf");
}

// https://rosettacode.org/wiki/Levenshtein_distance#C++
size_t levenshtein_distance(const std::string& a, const std::string& b) {
	const size_t aLen = a.size();
	const size_t bLen = b.size();

	if (aLen == 0) {
		return bLen;
	}

	if (bLen == 0) {
		return aLen;
	}

	std::vector<size_t> costs(bLen + 1);

	std::iota(costs.begin(), costs.end(), 0);
	size_t i = 0;
	for (auto aChar : a) {
		costs[0] = i + 1;
		size_t corner = i;
		size_t j = 0;
		for (auto bChar : b) {
			size_t upper = costs[j + 1];

			if (aChar == bChar) {
				costs[j + 1] = corner;
			} else {
				costs[j + 1] = 1 + std::min(std::min(upper, corner), costs[j]);
			}

			corner = upper;

			++j;
		}

		++i;
	}

	return costs[bLen];
}

TEST_CASE("levenshtein_distance()") {
	CHECK(levenshtein_distance("rosettacode", "raisethysword") == 8);
}
