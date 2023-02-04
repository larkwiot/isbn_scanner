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

#include <fmt/core.h>
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

#include "util.hpp"

#pragma once

using json = nlohmann::json;

struct WorldcatBook {
	std::string _author;
	std::string _title;
	std::string _lowYear;
	std::string _highYear;

	WorldcatBook() = default;
	explicit WorldcatBook(std::string&& author, std::string&& title, std::string&& lowYear, std::string&& highYear)
		: _author(author), _title(title), _lowYear(lowYear), _highYear(highYear) {};
	~WorldcatBook() noexcept(false) = default;
};

class Book {
	std::filesystem::path _filepath;
	std::string _isbn;
	std::string _title;
	std::string _author;
	std::string _lowYear;
	std::string _highYear;

   public:
	Book() = default;
	Book(std::filesystem::path&& filepath, std::string&& isbn, WorldcatBook&& worldcatBook) {
		_filepath = filepath;
		_isbn = isbn;
		_author = worldcatBook._author;
		_title = worldcatBook._title;
		_lowYear = worldcatBook._lowYear;
		_highYear = worldcatBook._highYear;
	}
	Book(Book&& book) noexcept(false) {
		_isbn = book._isbn;
		_author = book._author;
		_title = book._title;
		_lowYear = book._lowYear;
		_highYear = book._highYear;
	}
	~Book() noexcept(false) = default;

	std::string get_new_filename() const {
		return fmt::format("{}_{}_{}", _isbn, get_title(), get_author());
	}

	bool is_info_complete() const {
		return !_author.empty() && !_title.empty() && !_lowYear.empty() && !_highYear.empty();
	}

	std::string get_isbn() const {
		return _isbn;
	}

	std::string get_title() const {
		return clean_name(_title);
	}

	std::string get_author() const {
		return clean_name(_author);
	}
};
