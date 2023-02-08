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

#include <nlohmann/json.hpp>
#include <string>

#pragma once

using json = nlohmann::json;

struct Book {
	std::string isbn;
	std::string author;
	std::string title;
	long lowYear{};
	long highYear{};
	std::string filepath;

	Book() = default;
	explicit Book(std::string&& _isbn,
				  std::string&& _author,
				  std::string&& _title,
				  long _lowYear,
				  long _highYear,
				  std::string&& _filepath)
		: isbn(_isbn), author(_author), title(_title), lowYear(_lowYear), highYear(_highYear), filepath(_filepath){};
	~Book() noexcept(false) = default;

	json to_json() const {
		return {{"filepath", filepath}, {"isbn", isbn},		   {"author", author},
				{"title", title},		{"low_year", lowYear}, {"high_year", highYear}};
	}

	bool operator==(const Book& other) const {
		return isbn == other.isbn && author == other.author && title == other.title && lowYear == other.lowYear &&
			   highYear == other.highYear && filepath == other.filepath;
	}
};

// define std::hash<Book>()(book) so it can go in unordered_set;
namespace std {
template <>
struct hash<Book> {
	size_t operator()(const Book& book) const {
		return std::hash<std::string>()(book.to_json().dump());
	}
};
}  // namespace std
