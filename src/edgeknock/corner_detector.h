/**
 * Corner Detector
 *
 * @author Takuto Yanagida
 * @version 2024-07-01
 */

#pragma once
#include <chrono>
using namespace std::chrono;

class corner_detector {

	int _s = 32;
	int _d = 200;

	system_clock::time_point _last_t;
	int _last_c = -1;
	int _state  = 0;

	const int get_area(const int x, const int y, const int s_cx, const int s_cy) const {
		const int mx = s_cx - 1;
		const int my = s_cy - 1;

		if (
			(x == 0 && in_range(y, 0, _s - 1)) ||
			(in_range(x, 0, _s - 1) && y == 0)
			) {
			return 0;
		}
		if (
			(x == mx && in_range(y, 0, _s - 1)) ||
			(in_range(x, mx - _s, mx) && y == 0)
			) {
			return 1;
		}
		if (
			(x == mx && in_range(y, my - _s, my)) ||
			(in_range(x, mx - _s, mx) && y == my)
			) {
			return 2;
		}
		if (
			(x == 0 && in_range(y, my - _s, my)) ||
			(in_range(x, 0, _s - 1) && y == my)
			) {
			return 3;
		}
		return -1;
	}

	const bool in_range(const int v, const int min, const int max) const {
		return (min <= v && v <= max);
	}

public:

	corner_detector() {}

	~corner_detector() {}

	void set_corner_size(const int s) {
		_s = s;
	}

	void set_delay_time(const int ms) {
		_d = ms;
	}

	int detect(const system_clock::time_point t, const int x, const int y, const int s_cx, const int s_cy) {
		int c = get_area(x, y, s_cx, s_cy);

		if (_last_c != c) {
			_last_c = c;
			_last_t = t;
		}
		else {
			auto dt = duration_cast<milliseconds>(t - _last_t).count();
			if (_d <= dt) {
				if (_state == 0 && c != -1) {
					_state = 1;
					return c;
				}
				if (_state == 1 && c == -1) {
					_state = 0;
				}
			}
		}
		return -1;
	}

};
