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

#include <cpr/cpr.h>
#include <docopt/docopt.h>

#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <unordered_set>
#include <mutex>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "fmt/ranges.h"
#include "ctre/ctre.hpp"
//#include "fmt/core.h"
#include "indicators/indicators.hpp"
#include "pugixml/pugixml.hpp"
#include "taskflow/taskflow.hpp"
#include "backward/backward.hpp"

static const char usage[] =
    R"(Usage: isbn_scanner [-hdmo OUTPUT_DIR] [--version] [--verbose | --debug] [--dry-run]

-h --help                                show this message
-d --debug                               show debug output
-m --move                                move files instead of copy
-o <OUTPUT_DIR>, --output <OUTPUT_DIR>   directory to put organized files in [default: /tmp/]
--verbose                                show more output
--dry-run                                don't move or copy files, just lookup metadata
)";

static const char version[] = "ISBN Scanner v0.1";

static constexpr auto isbn_pattern = ctll::fixed_string{"([0-9\\-]{9,15}[0-9X])"};

static std::string tika_url = "http://localhost:9998";
static std::string worldcat_url = "http://classify.oclc.org/classify2/Classify";

static std::unordered_set<u_char> author_filter_chars = {
  ',',
  '.',
  ' '
};

static size_t books_organized = 0;
static std::mutex books_organized_lock {};

#define u_char unsigned char

std::vector<u_char> read_file_bytes(std::string &fn) {
  std::ifstream fh{fn, std::ios::binary | std::ios::ate};

  if (!fh == true) {
    spdlog::get("stderr")->error("could not open file: {}", fn);
    exit(0);
  }

  auto end = fh.tellg();
  fh.seekg(0, std::ios::beg);

  auto size = std::size_t(end - fh.tellg());

  if (size == 0) {
    return {};
  }

  std::vector<u_char> bytes(size);

  if (!fh.read(reinterpret_cast<char *>(bytes.data()), bytes.size()) == true) {
    spdlog::get("stderr")->error("could not read file: {}", fn);
    exit(0);
  }

  spdlog::get("console")->debug("read file {} at {} bytes", fn, fn.size());

  return bytes;
}

constexpr int ctoi(char c) noexcept { return c - '0'; }

bool is_valid_isbn(std::string isbn) {
  isbn = std::erase_if(isbn, [](u_char c) {
    if (c > '9' || c < '0') {
      return true;
    }
    return false;
  });

  if (isbn.length() == 10) {
    int multiplier = 1;
    int sum = 0;

    for (auto d : isbn) {
      if (d == 'X') {
        sum += multiplier * 10;
        continue;
      }
      sum += multiplier * ctoi(d);
    }

    if (sum % 11 == 0) {
      return true;
    }

    spdlog::get("console")->debug("isbn {} invalid ISBN 10 checksum", isbn);

    return false;
  } else if (isbn.length() == 13) {
    auto first_12 = isbn.substr(0, 12);
    auto check_digit = std::stoi(isbn.substr(12));
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

    if ((10 - remainder) == check_digit) {
      return true;
    }

    spdlog::get("console")->debug("isbn {} invalid ISBN 13 checksum", isbn);

    return false;
  }

  return false;
}

std::vector<std::string> find_isbns(std::string &text) {
  auto matches = std::vector<std::string>{};
  for (auto match : ctre::range<isbn_pattern>(text)) {
    matches.emplace_back(match.get<0>());
  }
  return matches;
}

std::string get_file_text(std::string &fn) {
  auto url = fmt::format("{}/tika/form", tika_url);
  auto resp =
      cpr::Post(cpr::Url{url}, cpr::Multipart{{"upload", cpr::File{fn}}});

  if (resp.status_code != 200) {
    spdlog::get("console")->debug("could not get text for file: {}", fn);
    return "";
  }

  return resp.text;
}

std::map<std::string, std::string> get_isbn_info(std::string &isbn) {
  auto resp = cpr::Get(cpr::Url{worldcat_url},
                       cpr::Parameters{{"isbn", isbn}, {"summary", "true"}});

  if (resp.status_code != 200) {
    spdlog::get("console")->debug("could not request metadata for ISBN: {}", isbn);
    return {};
  }

  spdlog::get("console")->debug("got something for {}", isbn);

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(resp.text.c_str());

  if (!result) {
    spdlog::get("console")->debug("could not parse XML for ISBN: {}", isbn);
    return {};
  }

  auto book_info = std::map<std::string, std::string>{
    {"isbn", isbn}
  };

  for (auto node : doc.children()) {
    if (node.name() == std::string{"classify"}) {
      for (auto child : node.children()) {
        if (child.name() == std::string{"work"}) {
          for (auto attr : child.attributes()) {
            book_info.emplace(attr.name(), attr.value());
            spdlog::get("console")->debug("{} book info: {} => {}", isbn, attr.name(), attr.value());
          }
        }
      }
    }
  }

  return book_info;
}

void move_file(std::string &fn, std::map<std::string, std::string> &&fileinfo,
               std::map<std::string, docopt::value> &args) {
  auto author = fileinfo.at("author");
  spdlog::get("console")->debug("raw author {}", author);

  std::transform(author.begin(), author.end(), author.begin(), [](u_char c) {
    if (author_filter_chars.contains(c)) {
      return '_';
    }
    return static_cast<char>(std::tolower(c));
  });
  spdlog::get("console")->debug("reformatted author {}", author);

  auto new_fn = fmt::format("{}_{}_{}", fileinfo.at("isbn"), author, fileinfo.at("title"));

  std::string operation = "dry ran";
  std::filesystem::path target {};

  if (!args.at("--dry-run")) {
    std::string output_dir = args.at("OUTPUT_DIR").asString();
    std::filesystem::path target_dir {};
    std::filesystem::path target_file {new_fn};
    target = target_dir / target_file;
    std::filesystem::copy(fn, target);
    if (args.at("--move")) {
      std::filesystem::remove(fn);
      operation = "moved";
    } else {
      operation = "copied";
    }
  }

  spdlog::get("console")->info("{} file {} to {}", operation, fn, target.string());
 
  return;
}

void process_file(std::string &fn, std::map<std::string, docopt::value> &args) {
  spdlog::get("console")->debug("working on {}", fn);
  std::string filetext = get_file_text(fn);
  if (filetext == "") {
    spdlog::get("console")->debug("{} got no text", fn);
    return;
  }

  auto isbns = find_isbns(filetext);

  if (!isbns.empty()) {
    // TODO: this could probably be a transform()
    for (auto isbn : isbns) {
      if (!is_valid_isbn(isbn)) {
        continue;
      }
      spdlog::get("console")->info("file {} = ISBN {}", fn, isbn);

      auto fileinfo = get_isbn_info(isbn);

      if (fileinfo.empty()) {
        spdlog::get("console")->debug("couldn't find any info for file {}", fn);
        continue;
      }

      move_file(fn, std::move(fileinfo), args);

      spdlog::get("console")->debug("{} was organized", fn);

      books_organized_lock.lock();
      books_organized++;
      books_organized_lock.unlock();
    }
  } else {
    spdlog::get("console")->debug("{} had no ISBNs", fn);
  }
}

int main(int argc, char *argv[]) {
  auto console_log = spdlog::stdout_color_mt("console");
  auto error_log = spdlog::stdout_color_mt("stderr");
  spdlog::flush_every(std::chrono::seconds(5));

  auto args = docopt::docopt(usage, {argv + 1, argv + argc}, true, version);
  if (args.at("--debug").asBool()) {
    console_log->set_level(spdlog::level::debug);
  } else if (args.at("--verbose").asBool()) {
    console_log->set_level(spdlog::level::info);
  } else {
    console_log->set_level(spdlog::level::warn);
  }

  auto curr_dir = std::filesystem::current_path();

  console_log->info("gathering files...");

  auto files = std::vector<std::string>{};
  for (auto &filepath : std::filesystem::directory_iterator(curr_dir)) {
    files.push_back(filepath.path());
  }

  console_log->info("processing {} files...", files.size());

  tf::Executor executor;
  tf::Taskflow taskflow;

  indicators::BlockProgressBar bar{
    indicators::option::BarWidth{80},
    indicators::option::ForegroundColor{indicators::Color::white},
    indicators::option::MaxProgress{files.size()},
    indicators::option::ShowRemainingTime{true}};

  if (args.at("--debug").asBool()) {
    taskflow.for_each(files.begin(), files.end(), [&](auto &fn) {
      process_file(fn, args);
    });
  } else {
    taskflow.for_each(files.begin(), files.end(), [&](auto &fn) {
      process_file(fn, args);
      bar.tick();
    });
  }

  executor.run(taskflow).wait();

  // simple call to put spdlog below progres bar
  puts("");
  console_log->info("organized {}/{}", books_organized, files.size());
  console_log->info("done!");

  return 0;
}
