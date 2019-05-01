/**
 *
 * Corner Detector
 *
 * @author Takuto Yanagida
 * @version 2019-05-01
 *
 */


#pragma once


class corner_detector {

	int _width = 32;
	int _last_corner = -1;

	const int get_area(const int x, const int y, const int cx_screen, const int cy_screen) {
		const int mx = cx_screen - 1;
		const int my = cy_screen - 1;

		if (x == 0 && (0 <= y && y < 0 + _width)) return 0;
		if ((0 <= x && x < 0 + _width) && y == 0) return 0;
		if (x == mx && (0 <= y && y < 0 + _width))  return 1;
		if ((mx - _width < x && x <= mx) && y == 0) return 1;
		if (x == mx && (my - _width < y && y <= my)) return 2;
		if ((mx - _width < x && x <= mx) && y == my) return 2;
		if (x == 0 && (my - _width < y && y <= my)) return 3;
		if ((0 <= x && x < 0 + _width) && y == my)  return 3;
		return -1;
	}

public:

	corner_detector() {}

	~corner_detector() {}

	void set_corner_size(int s) {
		_width = s;
	}

	int detect(const int x, const int y, const int cx_screen, const int cy_screen) {
		int corner = get_area(x, y, cx_screen, cy_screen);
		if (corner == _last_corner) return -1;
		_last_corner = corner;
		return corner;
	}

};
