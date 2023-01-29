/*
   Copyright 2021 larkwiot

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
#include <map>
#include <string>
#include <unordered_set>
#include <mutex>

#include <assert.hpp>
#include <cpr/cpr.h>
#include <clipp.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/ranges.h>
#include <ctre.hpp>
#include <pugixml.hpp>
#include <indicators/block_progress_bar.hpp>
#include <nlohmann/json.hpp>
#include <taskflow.hpp>

#include "util.hpp"

using json = nlohmann::json;

enum class TransferMode {
    MOVE, COPY, DRY_RUN
};

class MetadataField {
  std::string value;

 public:
  MetadataField() = default;
  ~MetadataField() = default;

  void set(std::string const& v) noexcept {
    if (value.empty()) {
      value = v;
    }
    return;
  }

  std::string get() const noexcept {
    return value;
  }

  void set_from_json(json const& metadata, std::string&& key) noexcept {
    if (metadata.contains(key)) {
      set(metadata[key].get<std::string>());
    }
    return;
  }
};



class Book {
  std::filesystem::path full_path;
  MetadataField isbn;
  MetadataField author;
  MetadataField title;
  MetadataField year;

 public:
  Book() = default;
  Book(json metadata) {
    merge_from_json(metadata);
  }

  void merge_book(Book&& book) noexcept {
    this->isbn.set(book.isbn.get());
    this->author.set(book.author.get());
    this->title.set(book.title.get());
    this->year.set(book.year.get());
    return;
  }

  void merge_from_json(json metadata) noexcept {
    this->isbn.set_from_json(metadata, "isbn");
    this->author.set_from_json(metadata, "author");
    this->title.set_from_json(metadata, "title");
    this->year.set_from_json(metadata, "year");
  }

  std::string get_new_filename() const {
    return fmt::format("{}_{}_{}",
                    this->isbn.get(),
                    clean_name(this->title.get()),
                    clean_name(this->author.get()));
  }

  bool is_info_found() const noexcept {
    return !this->author.get().empty() && !this->title.get().empty() && !this->year.get().empty();
  }

  std::string get_isbn() const noexcept {
    return this->isbn.get();
  }

  std::string get_title() const noexcept {
    return this->title.get();
  }
};
