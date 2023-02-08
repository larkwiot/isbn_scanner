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

struct API {
	std::string tika_host;
	int tika_port;
	std::string worldcat_host;
	std::string worldcat_path;
	int worldcat_port;
};

std::unordered_set<Book> parse_worldcat_data(const std::string& worldcat_xml) {
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_string(worldcat_xml.c_str());

	if (!result) {
		spdlog::get("console")->debug("parse_worldcat_data(): could not parse XML:\n{}", worldcat_xml);
		return {};
	}

	const auto classify = doc.child("classify");

	if (!classify.child("work").empty()) {
		const auto work = classify.child("work");

		const auto author = work.attribute("author").value();
		const auto title = work.attribute("title").value();
		const auto lowYear = work.attribute("lyr").value();
		const auto highYear = work.attribute("hyr").value();

		std::unordered_set<Book> book {};
		book.emplace("", author, title, std::stol(lowYear), std::stol(highYear), "");
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
		const auto lowYear = work.attribute("lyr").value();
		const auto highYear = work.attribute("hyr").value();
		books.emplace("", author, title, std::stol(lowYear), std::stol(highYear), "");
	}

	return books;
}

std::unordered_set<Book> get_by_isbn(const std::string& worldcat_host,
									  const std::string& worldcat_path,
									  int worldcat_port,
									  const std::string& isbn) {
	auto client = httplib::Client(worldcat_host, worldcat_port);

	auto resp = client.Get(fmt::format("{}?isbn={}", worldcat_path, isbn));

	if (!resp) {
		spdlog::get("console")->debug("get_by_isbn(): could not reach worldcat, request failed: {}", to_string(resp.error()));
		return {};
	}

	if (resp->status != 200) {
		spdlog::get("console")->debug("get_by_isbn(): could not request metadata for ISBN: {} status: {} path: {}",
									  isbn, resp->status, resp->location);
		return {};
	}
	return parse_worldcat_data(resp->body);
}

std::string get_file_text(const std::string& tika_host, int tika_port, const std::string& fn, const json& filetypes) {
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

	auto client = httplib::Client(tika_host, tika_port);

	const auto content = read_file_bytes_as_string(fn);

	auto form =
		httplib::MultipartFormDataItems{httplib::MultipartFormData{"upload", content, fn, mime_type}};

	auto resp = client.Post("/tika/form", form);

	if (!resp) {
		spdlog::get("console")->debug("get_file_text(): could not reach tika, request failed: {}", httplib::to_string(resp.error()));
		return "";
	}

	if (resp->status != 200) {
		spdlog::get("console")->debug("get_file_text(): could not get text for file: {}", fn);
		return "";
	}

	return resp->body;
}

void process_file(
	const std::string& filepath, size_t max_chars, Lockable<json>& output, const json& filetypes, const API& api) {
	//	spdlog::get("console")->info("process_file(): working on {}", filepath);
	const auto filetext = get_file_text(api.tika_host, api.tika_port, filepath, filetypes);
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

	std::unordered_set<std::string> isbns{};
	for (const auto& isbn : found_isbns) {
		const auto result = is_valid_isbn(isbn);
		bool is_valid = get<0>(result);
		std::string cleaned_isbn = get<1>(result);
		if (is_valid) {
			isbns.insert(cleaned_isbn);
		}
	}

	if (isbns.empty()) {
		spdlog::get("console")->info("process_file(): {} no valid ISBNs", filepath);
		return;
	}

	spdlog::get("console")->debug("process_file(): found {} valid ISBNs", isbns.size());

	std::unordered_set<Book> books {};

	for (const auto& isbn : isbns) {
		auto newBooks = get_by_isbn(api.worldcat_host, api.worldcat_path, api.worldcat_port, isbn);

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

	Book bestMatch;

	if (books.size() > 1) {
		bestMatch.lowYear = 0;
		bestMatch.highYear = 0;
		for (const auto& book : books) {
			if (book.highYear > bestMatch.highYear) {
				bestMatch = book;
			} else if (book.highYear == bestMatch.highYear && book.lowYear > bestMatch.lowYear) {
				bestMatch = book;
			}
		}
		if (bestMatch.lowYear == 0 && bestMatch.highYear == 0) {
			spdlog::get("console")->warn("process_file(): multiple books found on WorldCat for {} but could not find an obvious best choice, choosing random one", filepath);
			bestMatch = *books.begin();
		}
	} else {
		ASSERT(books.size() == 1);
		bestMatch = *books.begin();
	}

	ASSERT(!bestMatch.isbn.empty());

	auto book_json = bestMatch.to_json();

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

	Lockable<json> output{std::move(previousBooks)};

	auto config = toml::parse_file(configFilepath);

	auto max_chars = config["option"]["max_characters_to_search"].value<long>().value();
	ASSERT(max_chars > 0);

	auto tika_host = config["tika"]["host"].value<std::string>();
	auto tika_port = config["tika"]["port"].value<int>();
	auto worldcat_host = config["worldcat"]["host"].value<std::string>();
	auto worldcat_path = config["worldcat"]["path"].value<std::string>();
	auto worldcat_port = config["worldcat"]["port"].value<int>();

	API api{tika_host.value(), tika_port.value(), worldcat_host.value(), worldcat_path.value(), worldcat_port.value()};

	auto files = std::vector<std::string>{};
	for (auto& filepath : std::filesystem::directory_iterator(inDirectory)) {
		files.push_back(filepath.path());
	}

	tf::Executor executor{};
	tf::Taskflow taskflow{};
	taskflow.for_each(files.begin(), files.end(), [&](const auto& filepath) {
		if (processed_files.contains(filepath)) {
			spdlog::get("console")->info("skipping {} because it was processed on a previous run", filepath);
			return;
		}
		process_file(filepath, static_cast<size_t>(max_chars), output, filetypes, api);
	});
	executor.run(taskflow).wait();

	output.use([&outputJsonFilepath](auto& out) {
		if (out.empty()) {
			return;
		}

		std::ofstream fh(outputJsonFilepath);
		std::string str{out.dump(4)};
		fh.write(str.data(), static_cast<long>(str.size()));
		fh.close();
	});

	return 0;
}
