/**
 * Knock Detector
 *
 * @author Takuto Yanagida
 * @version 2024-07-04
 */

#pragma once
#include <chrono>
using namespace std::chrono;

class knock_detector {

	enum {
		L, R, T, B,
		EA = 0  /* edge area */,
		BA = -1 /* border area */,
		NA = -2 /* normal area*/
	};

	int _limit_t      = 300;
	int _max_sep      = 5;
	int _edge_seps[4] = { 3, 3, 3, 3 };
	int _edge_ws[4]   = { 4, 4, 4, 4 };
	int _ne_ws[4]     = { 18, 18, 18, 18 };

	system_clock::time_point _last_t;
	system_clock::time_point _out_t;
	system_clock::time_point _last_knock_t;
	bool _just_out = false;
	int _count     = 0;

public:

	enum { KNOCK = 0, READY = -1, NONE = -2 };

	struct area_index {
		int area;
		int index;

		area_index(int a, int i) : area(a), index(i) {}
		area_index(int a) : area(a), index(-1) {}
	};

private:

	area_index _last_ai = area_index(NA);
	area_index _last_knock_ai = area_index(NA);

	const area_index get_area(const int x, const int y, const int s_cx, const int s_cy) const {
		const int mx = s_cx - 1;
		const int my = s_cy - 1;

		// Corner
		if ((x == 0 || x == mx) && (y == 0 || y == my)) return -1;

		// Edge Areas
		if (0 <= x && x <= _edge_ws[L])       return area_index(L, y * _edge_seps[L] / my);
		if (x <= mx && mx - _edge_ws[R] <= x) return area_index(R, y * _edge_seps[R] / my);
		if (0 <= y && y <= _edge_ws[T])       return area_index(T, x * _edge_seps[T] / mx);
		if (y <= my && my - _edge_ws[B] <= y) return area_index(B, x * _edge_seps[B] / mx);

		// Border Areas
		if (x <= _edge_ws[L] + _ne_ws[L])        return area_index(BA);
		if (mx - (_edge_ws[R] + _ne_ws[R]) <= x) return area_index(BA);
		if (y <= _edge_ws[T] + _ne_ws[T])        return area_index(BA);
		if (my - (_edge_ws[B] + _ne_ws[B]) <= y) return area_index(BA);

		return area_index(NA);  // Normal Area
	}

public:

	knock_detector() {
	}

	~knock_detector() {
	}

	void set_time_limit(const int ms) {
		_limit_t = ms;
	}

	void set_max_separation(const int s) {
		_max_sep = s;
	}

	void set_edge_separations(const int left, const int right, const int top, const int bottom) {
		_edge_seps[0] = left;
		_edge_seps[1] = right;
		_edge_seps[2] = top;
		_edge_seps[3] = bottom;
	}

	void set_edge_widths(const int left, const int right, const int top, const int bottom) {
		_edge_ws[0] = left;
		_edge_ws[1] = right;
		_edge_ws[2] = top;
		_edge_ws[3] = bottom;
	}

	void set_no_effect_edge_widths(const int left, const int right, const int top, const int bottom) {
		_ne_ws[0] = left;
		_ne_ws[1] = right;
		_ne_ws[2] = top;
		_ne_ws[3] = bottom;
	}

	const area_index detect(const system_clock::time_point t, const int x, const int y, const int s_cx, const int s_cy) {
		const area_index ai = get_area(x, y, s_cx, s_cy);
		if (_last_ai.area == -2 && -2 < ai.area) {  // Get out of Normal Area
			_out_t = _last_t;
			_just_out = true;
		}
		bool knock = false;
		if (_just_out && _last_ai.area < 0 && EA <= ai.area) {  // Get out of Normal Area and enter Edge Area
			auto dt = duration_cast<milliseconds>(t - _out_t).count();
			if (dt <= _limit_t / 2) {
				knock = true;
			}
			_just_out = false;
		}
		_last_ai = ai;
		_last_t = t;

		if (knock) {
			auto dt = duration_cast<milliseconds>(t - _last_knock_t).count();
			if (0 < _count && _limit_t < dt) {
				_count = 0;
			}
			if (_count == 0 || (_last_knock_ai.area == ai.area && _last_knock_ai.index == ai.index)) {
				_count++;
			}
			_last_knock_t = t;
			_last_knock_ai = ai;

			if (_count == 1) {
				return area_index(READY);
			}
			if (_count == 2) {
				_count = 0;
				return ai;
			}
		}
		return area_index(NONE);
	}

};
