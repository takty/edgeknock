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
#include <tlhelp32.h>
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

	bool get_monitor_rect(HMONITOR hmon, RECT& rect) noexcept {
		MONITORINFOEX mi{};
		mi.cbSize = sizeof(MONITORINFOEX);
		if (!::GetMonitorInfo(hmon, &mi)) {
			return false;
		}
		rect = mi.rcMonitor;
		return true;
	}

	struct find_window_params {
		std::vector<DWORD> pids;
		HWND hwnd;
	};

	BOOL CALLBACK enum_windows_for_pid(HWND hwnd, LPARAM lParam) noexcept {
		[[gsl::suppress("type.1")]]
		auto* params = reinterpret_cast<find_window_params*>(lParam);
		DWORD pid{};
		::GetWindowThreadProcessId(hwnd, &pid);

		for (const DWORD tar_pid : params->pids) {
			if (pid == tar_pid) {
				if (::IsWindowVisible(hwnd) && ::GetWindow(hwnd, GW_OWNER) == nullptr) {
					params->hwnd = hwnd;
					return FALSE;
				}
			}
		}
		return TRUE;
	}

	bool is_candidate_shell_top_level_window(HWND hwnd) noexcept {
		if (hwnd == nullptr) {
			return false;
		}
		if (!::IsWindowVisible(hwnd) || ::GetWindow(hwnd, GW_OWNER) != nullptr) {
			return false;
		}
		wchar_t class_name[256]{};
		::GetClassName(hwnd, &class_name[0], 256);
		const std::wstring_view cn{ &class_name[0] };

		if (cn == L"Edgeknock") {
			return false;
		}
		return (
			cn == L"CabinetWClass" ||
			cn == L"ExploreWClass" ||
			cn == L"ApplicationFrameWindow"
		);
	}

	BOOL CALLBACK enum_windows_for_fallback(HWND hwnd, LPARAM lParam) noexcept {
		[[gsl::suppress("type.1")]]
		auto* out = reinterpret_cast<HWND*>(lParam);
		if (out == nullptr || *out != nullptr) {
			return FALSE;
		}
		if (!is_candidate_shell_top_level_window(hwnd)) {
			return TRUE;
		}
		*out = hwnd;
		return FALSE;
	}

	std::vector<DWORD> get_child_pids(DWORD pid) noexcept {
		std::vector<DWORD> cps;
		HANDLE hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		if (hSnapshot != INVALID_HANDLE_VALUE) {
			PROCESSENTRY32 pe32 = { sizeof(pe32) };
			if (::Process32First(hSnapshot, &pe32)) {
				do {
					if (pe32.th32ParentProcessID == pid) {
						cps.push_back(pe32.th32ProcessID);
					}
				} while (::Process32Next(hSnapshot, &pe32));
			}
			::CloseHandle(hSnapshot);
		}
		return cps;
	}

	HWND find_main_window_by_pid(DWORD pid) noexcept {
		find_window_params params{ {}, nullptr };
		if (pid != 0) {
			params.pids.push_back(pid);
		}

		for (int i = 0; i < 40 && params.hwnd == nullptr; ++i) {
			if (pid != 0) {
				[[gsl::suppress("type.1")]]
				::EnumWindows(enum_windows_for_pid, reinterpret_cast<LPARAM>(&params));
				if (params.hwnd == nullptr) {
					for (const DWORD cpid : get_child_pids(pid)) {
						if (std::find(params.pids.begin(), params.pids.end(), cpid) == params.pids.end()) {
							params.pids.push_back(cpid);
							std::vector<DWORD> gs = get_child_pids(cpid);
							params.pids.insert(params.pids.end(), gs.begin(), gs.end());
						}
					}
				}
			}
			if (params.hwnd == nullptr) {
				const HWND fg = ::GetForegroundWindow();
				if (is_candidate_shell_top_level_window(fg)) {
					params.hwnd = fg;
					break;
				}
			}
			::Sleep(50);
		}
		if (params.hwnd == nullptr && pid == 0) {
			HWND found = nullptr;
			[[gsl::suppress("type.1")]]
			::EnumWindows(enum_windows_for_fallback, reinterpret_cast<LPARAM>(&found));
			params.hwnd = found;
		}
		return params.hwnd;
	}

	bool ensure_window_on_monitor(HWND wnd, HMONITOR target_hmon) noexcept {
		if (wnd == nullptr || target_hmon == nullptr) {
			return false;
		}
		RECT tar_mon_r{};
		if (!get_monitor_rect(target_hmon, tar_mon_r)) {
			return false;
		}
		RECT win_r{};
		if (!::GetWindowRect(wnd, &win_r)) {
			return false;
		}
		const HMONITOR cur_mon = ::MonitorFromRect(&win_r, MONITOR_DEFAULTTONEAREST);
		if (cur_mon == target_hmon) {
			return true;
		}
		RECT cur_mon_r{};
		if (!get_monitor_rect(cur_mon, cur_mon_r)) {
			return false;
		}

		auto [cur_dpi_x, cur_dpi_y] = get_monitor_dpi(cur_mon);
		auto [tar_dpi_x, tar_dpi_y] = get_monitor_dpi(target_hmon);
		if (cur_dpi_x == 0) cur_dpi_x = 96;
		if (cur_dpi_y == 0) cur_dpi_y = 96;
		if (tar_dpi_x == 0) tar_dpi_x = 96;
		if (tar_dpi_y == 0) tar_dpi_y = 96;

		int win_w = win_r.right  - win_r.left;
		int win_h = win_r.bottom - win_r.top;
		const int tar_mon_w = tar_mon_r.right   - tar_mon_r.left;
		const int tar_mon_h = tar_mon_r.bottom  - tar_mon_r.top;
		const int rel_x = win_r.left - cur_mon_r.left;
		const int rel_y = win_r.top  - cur_mon_r.top;

		int x = tar_mon_r.left + ::MulDiv(rel_x, gsl::narrow_cast<int>(tar_dpi_x), gsl::narrow_cast<int>(cur_dpi_x));
		int y = tar_mon_r.top  + ::MulDiv(rel_y, gsl::narrow_cast<int>(tar_dpi_y), gsl::narrow_cast<int>(cur_dpi_y));

		if (win_w > tar_mon_w) {
			win_w = tar_mon_w;
			x = tar_mon_r.left;
		} else {
			const int max_x = tar_mon_r.right - win_w;
			if (x < tar_mon_r.left) x = tar_mon_r.left;
			if (x > max_x) x = max_x;
		}
		if (win_h > tar_mon_h) {
			win_h = tar_mon_h;
			y = tar_mon_r.top;
		} else {
			const int max_y = tar_mon_r.bottom - win_h;
			if (y < tar_mon_r.top) y = tar_mon_r.top;
			if (y > max_y) y = max_y;
		}

		::MoveWindow(wnd, x, y, win_w, win_h, TRUE);
		return true;
	}

}
