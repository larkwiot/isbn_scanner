#include <chrono>
#include <functional>
#include <mutex>
#include <thread>

#pragma once

template <typename T, typename U>
class RateLimited {
	std::chrono::high_resolution_clock _clock{};
	std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>> _last_use{};
	std::mutex _mutex{};
	T _item;

   public:
	std::chrono::duration<double> _interval{};

	RateLimited() = default;
	explicit RateLimited(T&& item, std::chrono::duration<double>&& interval) : _item(item), _interval(interval) {
		_mutex.unlock();
	};
	~RateLimited() noexcept(false) {
		_mutex.unlock();
	};

	U use(const std::function<U(T&)>& function) {
		_mutex.lock();

		auto now = std::chrono::high_resolution_clock::now();
		auto difference = _interval - (now - _last_use);
		std::this_thread::sleep_for(difference);

		U result = function(_item);

		_mutex.unlock();

		return result;
	}
};
