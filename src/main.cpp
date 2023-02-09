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

struct Host {
	std::string host;
	int port;
};

struct Tika : public Host {};

struct WorldCat : public Host {
	std::string path;
};

std::unordered_set<Book> parse_worldcat_data(const std::string& worldcat_xml) {
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_string(worldcat_xml.c_str());

	if (!result) {
		spdlog::get("console")->warn("parse_worldcat_data(): could not parse XML:\n{}", worldcat_xml);
		return {};
	}

	const auto classify = doc.child("classify");

	if (!classify.child("work").empty()) {
		const auto work = classify.child("work");

		const auto author = work.attribute("author").value();
		const auto title = work.attribute("title").value();
		auto lowYearStr = std::string{work.attribute("lyr").value()};
		auto highYearStr = std::string{work.attribute("hyr").value()};

		if (lowYearStr.empty()) {
			lowYearStr = "0";
		}

		if (highYearStr.empty()) {
			highYearStr = "0";
		}

		long lowYear = 0;
		try {
			lowYear = std::stol(lowYearStr);
		} catch (const std::exception& err) {
		}

		long highYear = 0;
		try {
			highYear = std::stol(highYearStr);
		} catch (const std::exception& err) {
		}

		std::unordered_set<Book> book{};
		book.emplace(0ul, author, title, lowYear, highYear, "");
		return book;
	}

	if (classify.child("works").empty()) {
		spdlog::get("console")->debug("parse_worldcat_data(): worldcat had no result");
		return {};
	}

	std::unordered_set<Book> books{};

	for (auto work : classify.child("works").children("work")) {
		const auto author = work.attribute("author").value();
		const auto title = work.attribute("title").value();
		auto lowYearStr = std::string{work.attribute("lyr").value()};
		auto highYearStr = std::string{work.attribute("hyr").value()};

		if (lowYearStr.empty()) {
			lowYearStr = "0";
		}

		if (highYearStr.empty()) {
			highYearStr = "0";
		}

		long lowYear = 0;
		try {
			lowYear = std::stol(lowYearStr);
		} catch (const std::exception& err) {
		}

		long highYear = 0;
		try {
			highYear = std::stol(highYearStr);
		} catch (const std::exception& err) {
		}

		books.emplace(0ul, author, title, lowYear, highYear, "");
	}

	return books;
}

std::unordered_set<Book> get_by_isbn(RateLimited<WorldCat, std::string>& rateWorldCat, ISBN isbn) {
	auto requestWorldCat = [&isbn](WorldCat& worldCat) {
		auto client = httplib::Client(worldCat.host, worldCat.port);

		auto resp = client.Get(fmt::format("{}?isbn={}", worldCat.path, isbn));

		if (!resp) {
			spdlog::get("console")->warn("get_by_isbn(): could not reach worldcat, request failed: {}",
										 to_string(resp.error()));
			return std::string{""};
		}

		if (resp->status != 200) {
			spdlog::get("console")->warn("get_by_isbn(): could not request metadata for ISBN: {} status: {} path: {}",
										 isbn, resp->status, resp->location);
			return std::string{""};
		}

		return resp->body;
	};

	const auto body = rateWorldCat.use(requestWorldCat);

	return parse_worldcat_data(body);
}

std::string get_file_text(const Tika& tika, const std::string& fn, const json& filetypes) {
	auto ext = get_file_extension(fn);
	if (ext.empty()) {
		spdlog::get("console")->warn("skipping {} because it does not have a file extension", fn);
		return "";
	}

	if (!filetypes.contains(ext)) {
		spdlog::get("console")->warn("skipping {} because no mime type is known for the extension {}", fn, ext);
		return "";
	}

	const auto mime_type = filetypes[ext].get<std::string>();

	auto client = httplib::Client(tika.host, tika.port);

	const auto content = read_file_bytes_as_string(fn);

	auto form = httplib::MultipartFormDataItems{httplib::MultipartFormData{"upload", content, fn, mime_type}};

	auto resp = client.Post("/tika/form", form);

	if (!resp) {
		spdlog::get("console")->warn("get_file_text(): could not reach tika, request failed: {}",
									 httplib::to_string(resp.error()));
		return "";
	}

	if (resp->status != 200) {
		spdlog::get("console")->warn("get_file_text(): could not get text for file, tika failed to process it: {}", fn);
		return "";
	}

	return resp->body;
}

void process_file(const std::string& filepath,
				  size_t max_chars,
				  Lockable<json>& output,
				  const json& filetypes,
				  const Tika& tika,
				  RateLimited<WorldCat, std::string>& worldCat) {
	//	spdlog::get("console")->info("process_file(): working on {}", filepath);
	const auto filetext = get_file_text(tika, filepath, filetypes);
	if (filetext.empty()) {
		spdlog::get("console")->debug("process_file(): {} got no text", filepath);
		return;
	}
	spdlog::get("console")->debug("process_file(): {} got file text", filepath);

	const auto found_isbns = find_isbns(filetext.substr(0, max_chars));
	if (found_isbns.empty()) {
		spdlog::get("console")->debug("process_file(): {} no found_isbns", filepath);
		return;
	}

	std::unordered_set<ISBN> isbns{};
	for (const auto& isbn : found_isbns) {
		const auto result = is_valid_isbn(isbn);
		bool is_valid = get<0>(result);
		ISBN cleaned_isbn = get<1>(result);
		if (is_valid) {
			isbns.insert(cleaned_isbn);
		}
	}

	if (isbns.empty()) {
		spdlog::get("console")->debug("process_file(): {} no valid ISBNs", filepath);
		return;
	}

	spdlog::get("console")->debug("process_file(): found {} valid ISBNs", isbns.size());

	std::unordered_set<Book> books{};

	for (ISBN isbn : isbns) {
		auto newBooks = get_by_isbn(worldCat, isbn);

		if (newBooks.empty()) {
			spdlog::get("console")->debug("process_file(): WorldCat returned nothing for isbn: {}", isbn);
			continue;
		}

		spdlog::get("console")->debug("process_file(): WorldCat found {} works for {}", newBooks.size(), isbn);

		for (auto newBook : newBooks) {
			newBook.isbn = isbn;
			newBook.filepath = filepath;
			books.insert(newBook);
		}
	}

	if (books.empty()) {
		spdlog::get("console")->debug("process_file(): none of the ISBNs were found on WorldCat");
		return;
	}

	spdlog::get("console")->debug("process_file(): found {} total works", books.size());

	Book bestMatch;

	if (books.size() > 1) {
		const std::string filename = std::filesystem::path(filepath).filename().string();
		std::unordered_map<size_t, std::string> levDists{};
		size_t lowestDist = 999999999999999;

		for (const auto& book : books) {
			ASSERT(!book.title.empty());
			size_t dist = levenshtein_distance(book.title, filename);
			if (dist < lowestDist) {
				bestMatch = book;
			}
		}
	} else {
		ASSERT(books.size() == 1);
		bestMatch = *books.begin();
	}

	ASSERT(bestMatch.isbn != 0ul);

	auto book_json = bestMatch.to_json();

	spdlog::get("console")->debug("process_file(): adding {} to JSON output", filepath);

	output.use([&book_json](json& out) {
		out.push_back(book_json);
	});

	spdlog::get("console")->info("process_file(): successfully processed {}", filepath);
}

void print_usage(const clipp::group& cli, const std::string& programName) {
	auto manPage = clipp::make_man_page(cli, programName, clipp::doc_formatting());
	for (const auto& page : manPage) {
		fmt::print("=== {} ===\n{}\n", page.title(), page.content());
	}
}

std::string get_feature_string() {
#ifndef NDEBUG
	std::string build = "debug";
#else
	std::string build = "release";
#endif
	return fmt::format("Features: {} build", build);
}

int signalReceived = -233;

#ifdef ISBN_SCANNER_IMPLEMENT_MAIN
int main(int argc, char* argv[]) {
	auto console_log = spdlog::stdout_color_mt("console");
	auto error_log = spdlog::stdout_color_mt("stderr");
	spdlog::flush_every(std::chrono::seconds(5));

	bool debug = false;
	bool verbose = false;
	std::string inDirectory;
	std::string outputJsonFilepath;
	bool version = false;
	std::string filetypesJsonPath;
	std::string configFilepath;

	auto cli =
		clipp::group((clipp::required("-i", "--input") & clipp::value("input directory", inDirectory)),
					 (clipp::required("-o", "--output") & clipp::value("output JSON file", outputJsonFilepath)),
					 clipp::option("-d", "--debug").set(debug).doc("enable debug logging"),
					 clipp::option("-v", "--verbose").set(verbose).doc("enable verbose logging"),
					 clipp::option("--version").set(version).doc("print version and feature info"),
					 (clipp::required("-f", "--filetypes") &
					  clipp::value("file types (mime types) JSON database", filetypesJsonPath)),
					 (clipp::required("-c", "--config") & clipp::value("configuration TOML filepath", configFilepath)));

	auto res = clipp::parse(argc, argv, cli);

	if (res.any_error()) {
		print_usage(cli, argv[0]);
		return 0;
	}

	if (version) {
		fmt::print("ISBN Scanner v{}\n{}\n", VERSION, get_feature_string());
		return 0;
	}

	ASSERT(!(debug && verbose));

	console_log->set_level(spdlog::level::warn);
	if (verbose) {
		console_log->set_level(spdlog::level::info);
	} else if (debug) {
		console_log->set_level(spdlog::level::debug);
	}

	json filetypes;
	try {
		std::ifstream filetypesJson(filetypesJsonPath);
		filetypes = json::parse(filetypesJson);
		filetypesJson.close();
	} catch (const std::exception& err) {
		error_log->error("could not open {} for filetypes (MIME types)", filetypesJsonPath);
		return 0;
	}

	std::unordered_set<std::string> processed_files{};
	json previousBooks;
	try {
		std::ifstream previousOutput(outputJsonFilepath);
		previousBooks = json::parse(previousOutput);
		for (auto& previousBook : previousBooks) {
			ASSERT(previousBook.is_object());
			processed_files.insert(previousBook["filepath"]);
		}
		previousOutput.close();
	} catch (const std::exception& err) {
	}

	auto config = toml::parse_file(configFilepath);

	auto max_chars = config["option"]["max_characters_to_search"].value<long>().value();
	ASSERT(max_chars > 0);

	auto tika_host = config["tika"]["host"].value<std::string>();
	auto tika_port = config["tika"]["port"].value<int>();
	const Tika tika{tika_host.value(), tika_port.value()};

	auto worldcat_host = config["worldcat"]["host"].value<std::string>();
	auto worldcat_port = config["worldcat"]["port"].value<int>();
	auto worldcat_path = config["worldcat"]["path"].value<std::string>();
	auto worldcat_rate = config["worldcat"]["rate_milliseconds"].value<int>();
	WorldCat worldCatInfo{{worldcat_host.value(), worldcat_port.value()}, worldcat_path.value()};
	RateLimited<WorldCat, std::string> worldCat{std::move(worldCatInfo),
												std::chrono::milliseconds(worldcat_rate.value())};

	console_log->info("main(): gathering files...");

	auto files = std::vector<std::string>{};
	for (const auto& filepath : std::filesystem::recursive_directory_iterator(inDirectory)) {
		if (filepath.is_directory()) {
			continue;
		}

		const auto filepathString = filepath.path().string();
		if (processed_files.contains(filepathString)) {
			spdlog::get("console")->info("skipping {} because it was processed on a previous run", filepathString);
			continue;
		}

		auto ext = get_file_extension(filepath.path().string());
		if (!filetypes.contains(ext)) {
			spdlog::get("console")->info("skipping {} because it does not have a supported file extension", filepathString);
		}

		files.push_back(filepath.path());
	}

	console_log->info("main(): {} files found", files.size());

	auto writeOutputJson = [&outputJsonFilepath](auto& out) {
		if (out.empty()) {
			return;
		}

		std::ofstream fh(outputJsonFilepath);
		std::string str{out.dump(4)};
		fh.write(str.data(), static_cast<long>(str.size()));
		fh.close();
	};

	Lockable<json> output{std::move(previousBooks)};

	auto handler = [](int signalNum) {
		spdlog::get("console")->debug("signal {} received", signalNum);
		signalReceived = signalNum;
	};
	std::signal(SIGINT, handler);

	spdlog::get("console")->info("main(): beginning scanning");

	tf::Executor executor{};
	tf::Taskflow taskflow{};
	taskflow.for_each(files.begin(), files.end(), [&](const auto& filepath) {
		if (signalReceived != -233) {
			spdlog::get("console")->debug("signal acknowledged, writing output file");
			output.use(writeOutputJson);
			return;
		}
		process_file(filepath, static_cast<size_t>(max_chars), output, filetypes, tika, worldCat);
	});
	executor.run(taskflow).wait();

	output.use(writeOutputJson);

	return 0;
}
#endif