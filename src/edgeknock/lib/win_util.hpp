/**
 * Window Utilities
 *
 * @author Takuto Yanagida
 * @version 2026-05-31
 */

#pragma once

#include <string>
#include <string_view>
#include <windows.h>
#include <shellscalingapi.h>

#include "gsl/gsl"

namespace window_utilities {

	void set_foreground_window(HWND wnd) noexcept {
		DWORD t{};
		const int for_id = ::GetWindowThreadProcessId(::GetForegroundWindow(), nullptr);
		const int tar_id = ::GetWindowThreadProcessId(wnd, nullptr);

		::AttachThreadInput(tar_id, for_id, TRUE);
		::SystemParametersInfo(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &t, 0);
		::SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, nullptr, 0);
		::SetForegroundWindow(wnd);
		::SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, t, nullptr, 0);
		::AttachThreadInput(tar_id, for_id, FALSE);
	}

	bool is_foreground_window_fullscreen(void) noexcept {
		RECT r{};
		HWND w = ::GetForegroundWindow();
		::GetWindowRect(w, &r);

		const int dpi = ::GetDpiForWindow(w);
		const int sw  = ::GetSystemMetricsForDpi(SM_CXSCREEN, dpi);
		const int sh  = ::GetSystemMetricsForDpi(SM_CYSCREEN, dpi);

		if (r.left <= 0 && r.top <= 0 && sw <= r.right && sh <= r.bottom) {
			wchar_t cn[256]{};
			::GetClassName(w, &cn[0], 256);
			const std::wstring_view class_name{ &cn[0] };
			if (class_name != L"Progman" && class_name != L"WorkerW") {
				return true;
			}
		}
		return false;
	}

	std::pair<UINT, UINT> get_monitor_dpi(HMONITOR hmon) noexcept {
		UINT dpi_x, dpi_y;
		if (GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y) == S_OK) {
			return { dpi_x, dpi_y };
		}
		return { 96, 96 };
	}

}
