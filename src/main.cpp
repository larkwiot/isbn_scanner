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

#include "main.hpp"

static constexpr auto isbn_pattern = ctll::fixed_string{"([0-9\\-]{9,15}[0-9X])"};

static std::string tika_url = "http://localhost:9998";
static std::string worldcat_url = "http://classify.oclc.org/classify2/Classify";

static size_t books_organized = 0;
static std::mutex books_organized_lock{};

void increment_books_organized() {
	books_organized_lock.lock();
	books_organized++;
	books_organized_lock.unlock();
}

auto find_isbns(std::string& text) {
	auto matches = std::set<std::string>{};
	for (auto match : ctre::range<isbn_pattern>(text)) {
		matches.emplace(match.get<0>());
	}
	return matches;
}

std::string get_file_text(std::string& fn) {
	auto client = httplib::Client(tika_url);

	auto filebytes = read_file_bytes(fn);

	auto form = httplib::MultipartFormDataItems {
		{"upload", "", fn, "application/octet-stream"}
	};

	auto headers = httplib::Headers {
		{"Accept", "*/*"}
	};

	auto resp = client.Post("/tika/form", headers, form);

	if (resp->status != 200) {
		spdlog::get("console")->debug("get_file_text(): could not get text for file: {}", fn);
		return "";
	}

	return resp->body;
}

json get_file_metadata(std::string& fn, const std::string& filetext) {
	auto client = httplib::Client(tika_url);

	auto form = httplib::MultipartFormDataItems {
		{"upload", filetext, "", ""}
	};

	auto headers = httplib::Headers {
		{"Accept", "application/json"}
	};

	auto resp = client.Post("/meta/form", headers, form);

	if (resp->status != 200) {
		spdlog::get("console")->debug("get_file_metadata(): could not get metadata for file: {}", fn);
		return {};
	}

	return json{resp->body};
}

std::vector<WorldcatBook> parse_worldcat_data(std::string worldcat_xml) {
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_string(worldcat_xml.c_str());

	if (!result) {
		spdlog::get("console")->debug("parse_worldcat_data(): could not parse XML:\n{}", worldcat_xml);
		return {};
	}

	std::vector<WorldcatBook> books {};

	for (auto work : doc.child("classify").child("works").children("work")) {
		const auto author = work.attribute("author").value();
		const auto title = work.attribute("title").value();
		const auto lowYear = work.attribute("lyr").value();
		const auto highYear = work.attribute("hyr").value();
		books.emplace_back(author, title, lowYear, highYear);
	}

	spdlog::get("console")->debug("parse_worldcat_data(): book info {}", books.rbegin()->_title);

	return books;
}

std::vector<WorldcatBook> get_by_isbn(std::string const& isbn) {
	auto client = httplib::Client(worldcat_url);

	auto resp = client.Get(fmt::format("_isbn={}&summary=true", isbn));

	if (resp->status != 200) {
		spdlog::get("console")->debug("get_by_isbn(): could not request metadata for ISBN: {}", isbn);
		return {};
	}
	return parse_worldcat_data(resp->body);
}

//std::vector<WorldcatBook> get_by_title(std::string const& title) {
//	auto resp = cpr::Get(cpr::Url{worldcat_url}, cpr::Parameters{{"title", title}, {"summary", "true"}});
//
//	if (resp.status_code != 200) {
//		spdlog::get("console")->debug("get_by_title(): could not request metadata for title: {}", title);
//	}
//
//	return parse_worldcat_data(resp.text);
//}

void move_file(std::string& fn, Book const& book, const std::string& outputDirectory, TransferMode mode) {
	auto new_fn = book.get_new_filename();

	std::string output_dir = outputDirectory;

	std::string operation = "dry ran";
	std::filesystem::path target_dir{output_dir};
	std::filesystem::path target_file{new_fn};
	std::filesystem::path target = target_dir / target_file;

	switch (mode) {
		case TransferMode::MOVE:
			std::filesystem::copy(fn, target);
			std::filesystem::remove(fn);
			break;
		case TransferMode::COPY:
			std::filesystem::copy(fn, target);
			break;
		case TransferMode::DRY_RUN:
			break;
	}

	spdlog::get("console")->info("{} file {} to {}", operation, fn, target.string());
}

void process_file(std::string& fn, const std::string& outputDirectory, TransferMode mode) {
	/* spdlog::get("console")->debug("process_file(): working on {}", fn); */
	std::string filetext = get_file_text(fn);
	if (filetext.empty()) {
		spdlog::get("console")->debug("process_file(): {} got no text", fn);
		return;
	}

	const auto found_isbns = find_isbns(filetext);
	if (found_isbns.empty()) {
		spdlog::get("console")->debug("process_file(): {} no found_isbns", fn);
		return;
	}

	std::vector<std::string> isbns {};
	for (auto isbn : found_isbns) {
		if (is_valid_isbn(isbn)) {
			isbns.push_back(isbn);
		}
	}

	if (isbns.empty()) {
		spdlog::get("console")->debug("process_file(): {} no valid isbns", fn);
		return;
	}

	std::string isbn = "";
	if (isbns.size() > 1) {
		// TODO: Add CLI option for selection
		isbn = isbns.at(0);
	} else {
		isbn = isbns.at(0);
	}

	auto worldcatBooks = get_by_isbn(isbn);
	if (worldcatBooks.empty()) {
		spdlog::get("console")->debug("process_file(): {} worldcat returned nothing", fn);
		return;
	}

	WorldcatBook worldcatBook {};
	if (worldcatBooks.size() > 1) {
		// TODO: Add CLI option for selection
		worldcatBook = worldcatBooks.at(0);
	} else {
		worldcatBook = worldcatBooks.at(0);
	}

	Book book {fn, std::move(isbn), std::move(worldcatBook)};

	move_file(fn, book, outputDirectory, mode);
	increment_books_organized();
}

void print_usage(const clipp::group& cli, const std::string& programName) {
	auto manPage = clipp::make_man_page(cli, programName, clipp::doc_formatting());
	for (const auto& page : manPage) {
		fmt::print("=== {} ===\n{}\n", page.title(), page.content());
	}
}

int main(int argc, char* argv[]) {
	auto console_log = spdlog::stdout_color_mt("console");
	auto error_log = spdlog::stdout_color_mt("stderr");
	spdlog::flush_every(std::chrono::seconds(5));

	bool debug = false;
	bool verbose = false;
	std::string inDirectory = "";
	std::string outDirectory = "";
	bool move = false;
	bool dryRun = false;

	auto cli = clipp::group(clipp::option("-o", "--output").set(outDirectory).doc("output directory"),
							clipp::option("-i", "--input").set(inDirectory).doc("input directory"),
							clipp::option("-d", "--debug").set(debug).doc("debug logging"),
							clipp::option("-v", "--verbose").set(verbose).doc("verbose logging"),
							clipp::option("-m", "--move").set(move).doc("move instead of copy"),
							clipp::option("--dry-run").set(dryRun).doc("simulate process without actually copying or moving any files"));

	if (!clipp::parse(argc, argv, cli)) {
		print_usage(cli, argv[0]);
		return 0;
	}

	ASSERT(!(debug && verbose));

	console_log->set_level(spdlog::level::warn);
	if (verbose) {
		console_log->set_level(spdlog::level::info);
	} else if (debug) {
		console_log->set_level(spdlog::level::debug);
	}

	TransferMode mode = TransferMode::COPY;
	if (move) {
		mode = TransferMode::MOVE;
	}
	if (dryRun) {
		mode = TransferMode::DRY_RUN;
	}

//	auto curr_dir = std::filesystem::current_path();
//
//	console_log->info("gathering files...");
//
	auto files = std::vector<std::string>{};
	for (auto& filepath : std::filesystem::directory_iterator(inDirectory)) {
		files.push_back(filepath.path());
	}

//	console_log->info("processing {} files...", files.size());

	tf::Taskflow taskflow;
	taskflow.for_each(files.begin(), files.end(), [&](auto& fn) { process_file(fn, outDirectory, mode); });

//	indicators::BlockProgressBar bar{
//		indicators::option::BarWidth{80}, indicators::option::ForegroundColor{indicators::Color::white},
//		indicators::option::MaxProgress{files.size()}, indicators::option::ShowRemainingTime{true}};
//
//	if (debug) {
//	} else {
//		taskflow.for_each(files.begin(), files.end(), [&](auto& fn) {
//			process_file(fn, outDirectory, mode);
//			bar.tick();
//		});
//	}
//
	tf::Executor executor;
	executor.run(taskflow).wait();

//	// simple call to put spdlog below progres bar
//	puts("");
//	spdlog::set_level(spdlog::level::info);
//	console_log->info("organized {}/{}", books_organized, files.size());
//	console_log->info("done!");

	return 0;
}
