/**
 * Module Directory Watcher
 *
 * @author Takuto Yanagida
 * @version 2026-05-30
 */

#pragma once

#include <windows.h>

#include "path.hpp"
#include "file_system.hpp"

namespace {

	struct mdw_params {
		HWND wnd;
		UINT msg;
	};

	DWORD WINAPI thread(LPVOID ps) noexcept {
		if (ps == nullptr) {
			return gsl::narrow_cast<DWORD>(-1);
		}
		const auto* const params = static_cast<const mdw_params*>(ps);
		const HWND wnd = params->wnd;
		const UINT msg = params->msg;

		auto temp = file_system::module_file_path();
		auto path = path::parent(temp);

		// Observe ini file writing
		HANDLE h = ::FindFirstChangeNotification(path.c_str(), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
		if (h == INVALID_HANDLE_VALUE) {
			return gsl::narrow_cast<DWORD>(-1);
		}
		// Wait until changing
		while (true) {
			if (::WaitForSingleObject(h, INFINITE) == WAIT_OBJECT_0) {
				::SendMessage(wnd, msg, 0, 0);  // Notify changing
			} else {
				break;
			}
			// Wait again
			if (!::FindNextChangeNotification(h)) break;
		}
		::FindCloseChangeNotification(h);
		return 0;
	}

}

namespace module_directory_watcher {

	void watch(HWND wnd, UINT msg) noexcept {
		DWORD t_id{};
		static mdw_params ps{};
		ps = { wnd, msg };
		::CreateThread(nullptr, 0, thread, &ps, 0, &t_id);
	}

}
