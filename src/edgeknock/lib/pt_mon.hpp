/**
 * Point in Monitor
 *
 * @author Takuto Yanagida
 * @version 2026-05-31
 */

#pragma once

#include <windows.h>
#include <shellscalingapi.h>

#include "gsl/gsl"

namespace point_in_monitor {

	bool get_monitor_rect(HMONITOR hmon, RECT& rect) noexcept {
		MONITORINFOEX mi{};
		mi.cbSize = sizeof(MONITORINFOEX);
		if (!::GetMonitorInfo(hmon, &mi)) {
			return false;
		}
		rect = mi.rcMonitor;
		return true;
	}

}

namespace {

	bool is_point_in_rect(const POINT& pt, const RECT& r, int margin = 0) noexcept {
		const LONG m = (margin > 0) ? gsl::narrow_cast<LONG>(margin) : 0;
		return (
			pt.x >= r.left   - m &&
			pt.x <  r.right  + m &&
			pt.y >= r.top    - m &&
			pt.y <  r.bottom + m
		);
	}

	LONG distance(const POINT& pt, const RECT& r) noexcept {
		LONG dx = 0;
		LONG dy = 0;

		if (pt.x < r.left) {
			dx = r.left - pt.x;
		} else if (pt.x >= r.right) {
			dx = pt.x - (r.right - 1);
		}
		if (pt.y < r.top) {
			dy = r.top - pt.y;
		} else if (pt.y >= r.bottom) {
			dy = pt.y - (r.bottom - 1);
		}
		return dx + dy;
	}

	bool to_local_clamped(const int x, const int y, const RECT& rect, POINT& lpt, SIZE& src) noexcept {
		src.cx = rect.right  - rect.left;
		src.cy = rect.bottom - rect.top;

		if (src.cx <= 0 || src.cy <= 0) {
			return false;
		}

		lpt.x = x - rect.left;
		lpt.y = y - rect.top;

		if (lpt.x < 0) {
			lpt.x = 0;
		} else if (lpt.x >= src.cx) {
			lpt.x = src.cx - 1;
		}
		if (lpt.y < 0) {
			lpt.y = 0;
		} else if (lpt.y >= src.cy) {
			lpt.y = src.cy - 1;
		}
		return true;
	}

	struct monitor_pick_params {
		POINT    pt;
		HMONITOR hmon;
		HMONITOR near_hmon;
		RECT     near_rect;
		LONG     near_dist;
	};

	struct mon_sel {
		HMONITOR hmon;
		RECT     rect;
	};

	mon_sel select_monitor_for_point(const int x, const int y, HMONITOR prev_hmon) noexcept {
		monitor_pick_params p{};
		p.pt        = POINT{ x, y };
		p.hmon      = nullptr;
		p.near_hmon = nullptr;
		p.near_rect = RECT{};
		p.near_dist = 0x7fffffff;

		[[gsl::suppress("type.1")]]
		::EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hmon, HDC, LPRECT, LPARAM lParam) -> BOOL {
			[[gsl::suppress("type.1")]]
			auto* pp = reinterpret_cast<monitor_pick_params*>(lParam);

			RECT r{};
			if (!point_in_monitor::get_monitor_rect(hmon, r)) {
				return TRUE;
			}
			if (is_point_in_rect(pp->pt, r)) {
				pp->hmon = hmon;
				return FALSE;  // Strict match has highest priority.
			}
			const LONG d = distance(pp->pt, r);
			if (d < pp->near_dist) {
				pp->near_dist = d;
				pp->near_hmon = hmon;
				pp->near_rect = r;
			}
			return TRUE;
		}, reinterpret_cast<LPARAM>(&p));

		mon_sel ret{};
		if (p.hmon && point_in_monitor::get_monitor_rect(p.hmon, ret.rect)) {
			ret.hmon = p.hmon;
		}
		if (ret.hmon == nullptr) {
			ret.hmon = p.near_hmon;
			ret.rect = p.near_rect;
		}
		if (prev_hmon != nullptr) {
			RECT prev_rect;
			const bool res = point_in_monitor::get_monitor_rect(prev_hmon, prev_rect);
			if (res && is_point_in_rect(p.pt, prev_rect, 3)) {
				ret.hmon = prev_hmon;
				ret.rect = prev_rect;
			}
		}
		return ret;
	}

}

namespace point_in_monitor {

	bool point_in_monitor(const int x, const int y, HMONITOR& hmon, POINT& lpt, SIZE& scr) noexcept {
		static HMONITOR prev_hmon{};

		const mon_sel sel = select_monitor_for_point(x, y, prev_hmon);

		if (sel.hmon == nullptr) {
			return false;
		}
		if (!to_local_clamped(x, y, sel.rect, lpt, scr)) {
			return false;
		}
		hmon      = sel.hmon;
		prev_hmon = sel.hmon;
		return true;
	}

}
