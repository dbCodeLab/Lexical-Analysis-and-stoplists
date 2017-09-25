#pragma once

#include <iostream>
#include <chrono>

class TimerBaseChrono {
public:
	
	// clears the timer
	TimerBaseChrono() : m_start(std::chrono::high_resolution_clock::time_point::min()) { }

	// clears the timer
	void clear() {
		m_start = std::chrono::high_resolution_clock::time_point::min();
	}

	// returns true if the timer is running
	bool isStarted() const {
		return (m_start.time_since_epoch() != std::chrono::high_resolution_clock::time_point::min().time_since_epoch());
	}

	// start the timer
	void start() {
		m_start = std::chrono::high_resolution_clock::now();
	}

	// get the number of milliseconds since the timer was started
	unsigned long getMs() {
		if (isStarted()) {
			std::chrono::high_resolution_clock::duration diff;
			diff = std::chrono::high_resolution_clock::now() - m_start;
			return (unsigned)(std::chrono::duration_cast<std::chrono::milliseconds>(diff).count());
		}
		return 0;
	}

	private:
		std::chrono::high_resolution_clock::time_point m_start;
};