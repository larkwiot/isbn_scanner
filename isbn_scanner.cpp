#include <cpr/cpr.h>
#include <docopt/docopt.h>

#include <filesystem>
#include <iostream>
#include <map>
#include <string>

#include "ctre.hpp"
//#include "fmt/core.h"
#include "indicators.hpp"
#include "pugixml/pugixml.hpp"
#include "spdlog/spdlog.h"
#include "taskflow/taskflow.hpp"

static const char usage[] =
    R"(Usage: isbn_scanner [-hdo OUTPUT_DIR] [--version] [--verbose]

-h --help       show this message
-d --debug      show debug output
-o OUTPUT_DIR   directory to put organized files in
--verbose       show more output
)";

static const char version[] = "ISBN Scanner v0.1";

static constexpr auto isbn_pattern = ctll::fixed_string{"([0-9\\-]{10,13})"};

static std::string tika_url = "http://localhost:9998";
static std::string worldcat_url = "http://classify.oclc.org/classify2/Classify";

#define u_char unsigned char

std::vector<u_char> read_file_bytes(std::string &fn) {
  std::ifstream fh{fn, std::ios::binary | std::ios::ate};

  if (!fh == true) {
    spdlog::error("could not open file: {}", fn);
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
    spdlog::error("could not read file: {}", fn);
    exit(0);
  }

  return bytes;
}

constexpr int ctoi(char c) noexcept { return c - '0'; }

bool is_valid_isbn(std::string &isbn) {
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
    spdlog::debug("could not get text for file: {}", fn);
    return "";
  }

  return resp.text;
}

std::map<std::string, std::string> get_isbn_info(std::string &isbn) {
  auto resp = cpr::Get(cpr::Url{worldcat_url},
                       cpr::Parameters{{"isbn", isbn}, {"summary", "true"}});

  if (resp.status_code != 200) {
    spdlog::debug("could not request metadata for ISBN: {}", isbn);
    return {};
  }

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(resp.text.c_str());

  if (!result) {
    spdlog::debug("could not parse XML for ISBN: {}", isbn);
    return {};
  }

  auto book_info = std::map<std::string, std::string>{};

  for (auto node : doc.children()) {
    if (node.name() == std::string{"classify"}) {
      for (auto child : node.children()) {
        if (child.name() == std::string{"work"}) {
          for (auto attr : child.attributes()) {
            book_info.emplace(attr.name(), attr.value());
          }
        }
      }
    }
  }

  return book_info;
}

void move_file(std::string &fn, std::map<std::string, std::string> &&fileinfo,
               std::map<std::string, docopt::value> &args) {
  std::cout << fn << fileinfo.at("author") << args.at("verbose");
}

void process_file(std::string &fn, std::map<std::string, docopt::value> &args) {
  std::string filetext = get_file_text(fn);
  if (filetext == "") {
    return;
  }

  auto isbns = find_isbns(filetext);

  if (!isbns.empty()) {
    // TODO: this could probably be a map()
    for (auto isbn : isbns) {
      if (!is_valid_isbn(isbn)) {
        continue;
      }

      auto fileinfo = get_isbn_info(isbn);

      if (fileinfo.empty()) {
        continue;
      }

      move_file(fn, std::move(fileinfo), args);
    }
  }
}

int main(int argc, char *argv[]) {
  auto args = docopt::docopt(usage, {argv + 1, argv + argc}, true, version);
  auto curr_dir = std::filesystem::current_path();

  spdlog::info("gathering files...");

  auto files = std::vector<std::string>{};
  for (auto &filepath : std::filesystem::directory_iterator(curr_dir)) {
    files.push_back(filepath.path());
  }

  spdlog::info("initializing before processing files...");

  indicators::BlockProgressBar bar{
      indicators::option::BarWidth{80},
      indicators::option::ForegroundColor{indicators::Color::white},
      indicators::option::MaxProgress{files.size()},
      indicators::option::PrefixText{"processing files..."},
      indicators::option::ShowRemainingTime{true}};

  tf::Executor executor;
  tf::Taskflow taskflow;

  taskflow.for_each(files.begin(), files.end(), [&](auto &fn) {
    process_file(fn, args);
    bar.tick();
  });

  executor.run(taskflow).wait();

  // simple call to put spdlog below progres bar
  puts("");

  spdlog::info("done!");

  return 0;
}
