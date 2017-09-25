#pragma once
#include "Timer.h"
#include <string>

template <typename T> 
class basic_stopwatch : T {
	typedef typename T BaseTimer;

public:
	// create, optionally start timing an activity
	explicit basic_stopwatch(bool startSW)
		: basic_stopwatch(std::cout, "Stopwatch", startSW) {}

	explicit basic_stopwatch(char const* activity = "Stopwatch", bool startSW = true)
		: basic_stopwatch(std::cout, activity, startSW) {}

	basic_stopwatch(std::ostream& log,
		char const* activity = "Stopwatch",
		bool startSW = true) : mLog(log) {
		if (startSW) {
			start(activity);
		}
	}

	// stop and destroy a stopwatch
	~basic_stopwatch() {
		stop();
	}

	// predicate: return true if the stopwatch is running
	bool isStarted() const { return BaseTimer::isStarted(); }

	// show accumulated time, keep running
	void show(char const* event = "Accumulated time") {
		mLog << event << ": " << BaseTimer::getMs() << " ms\n";
	}

	// (re)start a stopwatch
	void start(char const* activity = "Stopwatch") {
		mActivity = activity;
		mLog << "Start timing " << mActivity << '\n';
		BaseTimer::start();
	}

	// stop a running stopwatch
	void stop() {
		if (isStarted()) {
			mLog << "Stop timing ";
			show(mActivity.c_str());
			BaseTimer::clear();
		}
	}

private: // members
	std::string mActivity; // "activity" string
	std::ostream& mLog; // stream on which to log events
};

typedef basic_stopwatch<TimerBaseChrono> StopwatchChrono;