/**
 * Window Utilities
 *
 * @author Takuto Yanagida
 * @version 2026-05-28
 */

#pragma once

#include <utility>
#include <string>
#include <windows.h>

#include "gsl/gsl"
#include "path.hpp"
#include "file_system.hpp"

class win_util {
	struct watch_module_directory_params {
		HWND hwnd;
		UINT msg;
	};

	static DWORD WINAPI watch_module_directory_thread(LPVOID ps) noexcept {
		if (ps == nullptr) {
			return gsl::narrow_cast<DWORD>(-1);
		}
		const auto* const params = static_cast<const watch_module_directory_params*>(ps);
		const HWND hwnd = params->hwnd;
		const UINT msg  = params->msg;

		auto temp = file_system::module_file_path();
		auto path = path::parent(temp);

		// Observe ini file writing
		HANDLE h = ::FindFirstChangeNotification(path.c_str(), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
		if (h == INVALID_HANDLE_VALUE) return gsl::narrow_cast<DWORD>(-1);

		// Wait until changing
		while (true) {
			if (::WaitForSingleObject(h, INFINITE) == WAIT_OBJECT_0) {
				::SendMessage(hwnd, msg, 0, 0);  // Notify changing
			}
			else {
				break;
			}
			// Wait again
			if (!::FindNextChangeNotification(h)) break;
		}
		::FindCloseChangeNotification(h);
		return 0;
	}

public:

	static void watch_module_directory(HWND hwnd, UINT msg) noexcept {
		DWORD t_id{};
		static watch_module_directory_params ps{};
		ps.hwnd = hwnd;
		ps.msg = msg;
		::CreateThread(nullptr, 0, win_util::watch_module_directory_thread, &ps, 0, &t_id);
	}

	static void set_foreground_window(HWND hWnd) noexcept {
		const int win_id = ::GetWindowThreadProcessId(GetForegroundWindow(), NULL);
		const int tar_id = ::GetWindowThreadProcessId(hWnd, NULL);
		DWORD t = 0;

		::AttachThreadInput(tar_id, win_id, TRUE);
		::SystemParametersInfo(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &t, 0);
		::SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, NULL, 0);
		::SetForegroundWindow(hWnd);
		::SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, t, NULL, 0);
		::AttachThreadInput(tar_id, win_id, FALSE);
	}

	static bool is_foreground_window_fullscreen(void) noexcept {
		RECT wr{};
		HWND fw = ::GetForegroundWindow();
		::GetWindowRect(fw, &wr);

		const int dpi = ::GetDpiForWindow(fw);
		const int sw  = ::GetSystemMetricsForDpi(SM_CXSCREEN, dpi);
		const int sh  = ::GetSystemMetricsForDpi(SM_CYSCREEN, dpi);

		if (wr.left <= 0 && wr.top <= 0 && sw <= wr.right && sh <= wr.bottom) {
			wchar_t cn[256]{};
			::GetClassName(fw, &cn[0], 256);
			if (wcscmp(&cn[0], L"Progman") != 0 && wcscmp(&cn[0], L"WorkerW") != 0) {
				return true;
			}
		}
		return false;
	}

	static HFONT get_default_font(HWND hwnd) noexcept {
		NONCLIENTMETRICS ncm{};
		ncm.cbSize = sizeof(ncm);
		const int dpi = ::GetDpiForWindow(hwnd);
		::SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0, dpi);
		return ::CreateFontIndirect(&(ncm.lfMessageFont));
	}

	static SIZE get_text_size(HWND hwnd, const std::wstring& msg) noexcept {
		HDC hdc = ::GetDC(hwnd);
		HFONT dlgFont = win_util::get_default_font(hwnd);
		::SelectObject(hdc, dlgFont);
		SIZE font;
		::GetTextExtentPoint32(hdc, msg.c_str(), gsl::narrow_cast<int>(msg.length()), &font);
		::DeleteObject(dlgFont);
		::ReleaseDC(hwnd, hdc);
		return font;
	}

	static bool is_point_in_monitor(HMONITOR hmon, POINT pt) noexcept {
		MONITORINFOEX mi{};
		mi.cbSize = sizeof(MONITORINFOEX);
		if (GetMonitorInfo(hmon, &mi)) {
			return (pt.x >= mi.rcMonitor.left && pt.x < mi.rcMonitor.right &&
				pt.y >= mi.rcMonitor.top && pt.y < mi.rcMonitor.bottom);
		}
		return false;
	}

	static POINT get_physical_point_from_logical(HMONITOR hmon, POINT logical) noexcept {
		POINT physical = logical;
		UINT dpi_x, dpi_y;
		if (GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y) == S_OK) {
			physical.x = MulDiv(logical.x, dpi_x, 96);
			physical.y = MulDiv(logical.y, dpi_y, 96);
		}
		return physical;
	}

	static RECT get_monitor_rect(HMONITOR hmon) noexcept {
		MONITORINFOEX mi{};
		mi.cbSize = sizeof(MONITORINFOEX);
		if (!::GetMonitorInfo(hmon, &mi)) {
			return RECT{};
		}
		return mi.rcMonitor;
	}

	static std::pair<UINT, UINT> get_monitor_dpi(HMONITOR hmon) noexcept {
		UINT dpi_x, dpi_y;
		if (GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y) == S_OK) {
			return { dpi_x, dpi_y };
		}
		return { 96, 96 };
	}

};
