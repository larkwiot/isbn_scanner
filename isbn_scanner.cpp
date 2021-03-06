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

#include "util.hpp"
#include "isbn_scanner.hpp"

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

static size_t books_organized = 0;
static std::mutex books_organized_lock {};

void increment_books_organized() {
  books_organized_lock.lock();
  books_organized++;
  books_organized_lock.unlock();
  return;
}


auto find_isbns(std::string &text) {
  auto matches = std::set<std::string>{};
  for (auto match : ctre::range<isbn_pattern>(text)) {
    matches.emplace(match.get<0>());
  }
  return matches;
}

std::string get_file_text(std::string &fn) {
  auto url = fmt::format("{}/tika/form", tika_url);
  auto resp =
      cpr::Post(cpr::Url{url}, cpr::Multipart{{"upload", cpr::File{fn}}});

  if (resp.status_code != 200) {
    spdlog::get("console")->debug("get_file_text(): could not get text for file: {}", fn);
    return "";
  }

  return resp.text;
}

json get_file_metadata (std::string& fn) {
  auto url = fmt::format("{}/meta/form", tika_url);
  auto resp = cpr::Post(cpr::Url{url}, cpr::Multipart{{"upload", cpr::File{fn}}}, cpr::Header{{"Accept", "application/json"}});

  if (resp.status_code != 200) {
    spdlog::get("console")->debug("get_file_metadata(): could not get metadata for file: {}", fn);
    return {};
  }

  return json{resp.text};
}

json worldcat_to_json(std::string worldcat_xml) {
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(worldcat_xml.c_str());

  if (!result) {
    spdlog::get("console")->debug("worldcat_to_json(): could not parse XML:\n{}", worldcat_xml);
    return {};
  }

  json book_info;

  for (auto node : doc.children()) {
    if (node.name() == std::string{"classify"}) {
      for (auto child : node.children()) {
        if (child.name() == std::string{"work"}) {
          for (auto attr : child.attributes()) {
            book_info[attr.name()] = attr.value();
            /* spdlog::get("console")->debug("process_file(): {} book info: {} => {}", isbn, attr.name(), attr.value()); */
          }
        }
      }
    }
  }

  spdlog::get("console")->debug("worldcat_to_json(): book info {}", book_info);

  return book_info;

}

json get_isbn_info(std::string const &isbn) {
  auto resp = cpr::Get(cpr::Url{worldcat_url},
                       cpr::Parameters{{"isbn", isbn}, {"summary", "true"}});

  if (resp.status_code != 200) {
    spdlog::get("console")->debug("get_isbn_info(): could not request metadata for ISBN: {}", isbn);
    return {};
  }

  return worldcat_to_json(resp.text);
}

json get_title_info(std::string const &title) {
  auto resp = cpr::Get(cpr::Url{worldcat_url},
                       cpr::Parameters{{"title", title}, {"summary", "true"}});

  if (resp.status_code != 200) {
    spdlog::get("console")->debug("get_title_info(): could not request metadata for title: {}", title);
  }

  return worldcat_to_json(resp.text);
}

void move_file(std::string &fn, Book const& book,
               std::map<std::string, docopt::value> const &args) {

  auto new_fn = book.get_new_filename();

  std::string output_dir = "./";
  if (args.at("OUTPUT_DIR").asBool() == true) {
    output_dir = args.at("OUTPUT_DIR").asString();
  }

  std::string operation = "dry ran";
  std::filesystem::path target_dir {output_dir};
  std::filesystem::path target_file {new_fn};
  std::filesystem::path target = target_dir / target_file;
  
  if (!args.at("--dry-run")) {  
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

void process_file(std::string &fn, std::map<std::string, docopt::value> const& args) {
  /* spdlog::get("console")->debug("process_file(): working on {}", fn); */
  std::string filetext = get_file_text(fn);
  if (filetext == "") {
    spdlog::get("console")->debug("process_file(): {} got no text", fn);
    return;
  }

  Book book {get_file_metadata(fn)};
  if (book.get_isbn().empty() == false) {
    json isbn_info = get_isbn_info(book.get_isbn());
    book.merge_from_json(isbn_info);
    move_file(fn, book, args);
    increment_books_organized();
    return;
  }

  if (book.get_title().empty() == false) {
    json title_info = get_title_info(book.get_title());
    book.merge_from_json(title_info);
    move_file(fn, book, args);
    increment_books_organized();
    return;
  }

  auto isbns = find_isbns(filetext);

  /* spdlog::get("console")->debug("process_file(): {} found ISBNs: {}", fn, isbns); */

  std::set<std::string> valid_isbns {};
  std::copy_if(isbns.begin(), isbns.end(), std::inserter(valid_isbns, valid_isbns.begin()), is_valid_isbn);

  if (!valid_isbns.empty()) {
    // TODO: this could probably be a transform()
    for (auto isbn : valid_isbns) {
      Book book {get_isbn_info(isbn)};

      if (book.is_info_found() == false) {
        spdlog::get("console")->debug("could not find any info for {} with {}", fn, isbn);
        continue;
      }

      move_file(fn, book, args);
      return;
    }

    spdlog::get("console")->info("could not find valid ISBN for {}", fn);
  } else {
    spdlog::get("console")->info("{} had no ISBNs", fn);
  }

  return;
}

int main(int argc, char *argv[]) {
  auto console_log = spdlog::stdout_color_mt("console");
  auto error_log = spdlog::stdout_color_mt("stderr");
  spdlog::flush_every(std::chrono::seconds(5));

  auto args = docopt::docopt(usage, {argv + 1, argv + argc}, true, version);
  if (args.at("--debug").asBool()) {
    console_log->set_level(spdlog::level::debug);

    spdlog::get("console")->debug("main(): args:");
    for (auto const& arg : args) {
      std::cout << arg.first << " -> " << arg.second << "\n";
    }
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
  spdlog::set_level(spdlog::level::info);
  console_log->info("organized {}/{}", books_organized, files.size());
  console_log->info("done!");

  return 0;
}
