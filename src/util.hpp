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

#include <fstream>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include <spdlog/spdlog.h>
#include <ctre.hpp>

#pragma once

#define u_char unsigned char

std::vector<u_char> read_file_bytes(std::string& fn) {
	std::ifstream fh{fn, std::ios::binary | std::ios::ate};

	if (!fh == true) {
//		spdlog::get("stderr")->error("could not open file: {}", fn);
		exit(0);
	}

	auto end = fh.tellg();
	fh.seekg(0, std::ios::beg);

	auto size = std::size_t(end - fh.tellg());

	if (size == 0) {
		return {};
	}

	std::vector<u_char> bytes(size);

	if (!fh.read(reinterpret_cast<char*>(bytes.data()), bytes.size()) == true) {
//		spdlog::get("stderr")->error("could not read file: {}", fn);
		exit(0);
	}

//	spdlog::get("console")->debug("process_file(): read file {} at {} bytes", fn, fn.size());

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

template <typename T, typename U>
U noexcept_map_at(std::map<T, U>& m, T k) noexcept {
	try {
		return m.at(k);
	} catch (const std::out_of_range& err) {
//		spdlog::get("console")->debug("noexcept_map_at(): {} not in {}", k, m);
		return {};
	}
}

static std::unordered_set<char> filter_chars = {',', '.', '\'', '|', '-'};

static std::unordered_map<char, char> replace_chars = {{' ', '_'}, {':', '-'}};

std::string clean_name(std::string const& name) {
	std::string cleaned = "";

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

bool is_valid_isbn(std::string isbn) {
	/* spdlog::get("console")->debug("is_valid_isbn(): raw ISBN: {}", _isbn); */

	if (ctre::match<all_one_char>(isbn)) {
		return false;
	}

	// TODO: make this line work instead of the below
	/* _isbn = std::erase_if(_isbn, is_not_isbn_char); */
	{
		std::string new_isbn = "";
		for (auto c : isbn) {
			if (isbn_chars.contains(c)) {
				new_isbn += c;
			}
		}
		isbn = new_isbn;
	}

	/* spdlog::get("console")->debug("is_valid_isbn(): cleaned ISBN: {}", _isbn); */

	if (isbn.length() == 10) {
		int multiplier = 10;
		int sum = 0;

		auto isbn_end = isbn.end();
		for (auto di = isbn.begin(); di != isbn_end; ++di) {
			if (multiplier < 1) {
//				spdlog::get("stderr")->error("is_valid_isbn(): tried to use ISBN 10 multiplier < 2! ISBN: {}", isbn);
			}

			auto d = *di;

			if (d == 'X') {
				if (di == isbn_end - 1) {
					sum += multiplier * 10;
				} else {
					/* spdlog::get("console")->debug("is_valid_isbn(): ISBN {} had X not at end", _isbn); */
					return false;
				}
			} else {
				sum += multiplier * ctoi(d);
			}

			multiplier--;
		}

		if (sum % 11 == 0) {
			return true;
		}

		/* spdlog::get("console")->debug("is_valid_isbn(): ISBN {} invalid ISBN 10 checksum", _isbn); */

		return false;
	} else if (isbn.length() == 13) {
		auto first_12 = isbn.substr(0, 12);
		unsigned long check_digit = 0;
		try {
			check_digit = std::stoul(isbn.substr(12));
		} catch (const std::invalid_argument err) {
			/* spdlog::get("console")->debug("ISBN {} cannot be converted to number", _isbn.substr(12)); */
			return false;
		}
		int one = 1;
		int three = 3;
		int multiplier = one;
		int sum = 0;

		for (auto c : first_12) {
			sum += multiplier * ctoi(c);
			// swap values for mulitiplier
			multiplier ^= one ^ three;
		}

		int remainder = sum % 10;

		if (remainder == 0 && check_digit == 0) {
			return true;
		}

		if (std::cmp_equal((10 - remainder), check_digit)) {
			return true;
		}

		/* spdlog::get("console")->debug("is_valid_isbn(): ISBN {} invalid ISBN 13 checksum", _isbn); */

		return false;
	}

	/* spdlog::get("console")->debug("is_valid_isbn(): ISBN {} is not a valid length", _isbn); */

	return false;
}
