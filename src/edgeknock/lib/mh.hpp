/**
 * Mouse Hook
 *
 * @author Takuto Yanagida
 * @version 2026-05-30
 */

#pragma once

#include <windows.h>
#include "gsl/gsl"

#define WM_MOUSEMOVEHOOK (WM_USER + 1)

namespace {

	HHOOK hook{};
	HWND wnd{};

	LRESULT WINAPI callback(int code, WPARAM wp, LPARAM lp) noexcept {
		[[gsl::suppress("type.1")]]
		const MSLLHOOKSTRUCT* mhs = reinterpret_cast<const MSLLHOOKSTRUCT*>(lp);

		if (code == 0)	{
			if (wnd && (wp == WM_MOUSEMOVE || wp == WM_NCMOUSEMOVE)) {
				::PostMessage(wnd, WM_MOUSEMOVEHOOK, mhs->pt.x, mhs->pt.y);
			}
		}
		return ::CallNextHookEx(hook, code, wp, lp);
	}

}

namespace mh {

	static int set_hook(HWND w) noexcept {
		wnd  = w;
		hook = ::SetWindowsHookEx(WH_MOUSE_LL, callback, NULL, 0);
		if (hook == nullptr) {
			return -1;
		}
		return 0;
	}

	static int end_hook() noexcept {
		if (::UnhookWindowsHookEx(hook) != 0) {
			return 0;
		}
		return -1;
	}

}
