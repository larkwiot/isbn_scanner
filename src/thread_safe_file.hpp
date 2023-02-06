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

#include <string>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <utility>

#pragma once

class ThreadSafeFile {
	std::mutex _mutex {};
	std::ofstream output;

   public:
	std::filesystem::path _filepath;

	ThreadSafeFile() = default;
	explicit ThreadSafeFile(std::filesystem::path filepath) {
		_filepath = std::move(filepath);
		output = {_filepath};
		_mutex.unlock();
	};
	~ThreadSafeFile() noexcept(false) {
		output.close();
		_mutex.unlock();
	};

	void write(const std::string& text) {
		_mutex.lock();

		output.write(text.data(), static_cast<long>(text.size()));

		_mutex.unlock();
	}

	void step_back_one_char() {
		_mutex.lock();

		output.seekp(-1, std::ios_base::end);

		_mutex.unlock();
	}
};
