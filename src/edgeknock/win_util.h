/**
 *
 * Window Utilities
 *
 * @author Takuto Yanagida
 * @version 2019-05-02
 *
 */


#pragma once
#include <windows.h>
#include "path.h"


class win_util {

	static DWORD WINAPI check_module_directory_thread(LPVOID ps) {
		HWND hwnd = (HWND) ((long long int*) ps)[0];
		UINT msg = (UINT)((long long int*)ps)[1];
		wchar_t path[MAX_PATH];
		::GetModuleFileName(NULL, path, MAX_PATH - 1);
		path::parent_dest(path);

		// Observe ini file writing
		HANDLE handle = ::FindFirstChangeNotification(path, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
		if (handle == INVALID_HANDLE_VALUE) return (DWORD)-1;

		// Wait until changing
		while (true) {
			if (::WaitForSingleObject(handle, INFINITE) == WAIT_OBJECT_0) {
				::SendMessage(hwnd, msg, 0, 0);  // Notify changing
			} else {
				break;
			}
			// Wait again
			if (!::FindNextChangeNotification(handle)) break;
		}
		::FindCloseChangeNotification(handle);
		return 0;
	}

public:

	static void check_module_directory(HWND hwnd, UINT msg) {
		DWORD thread_id;
		static long long int ps[] = { (long long int) hwnd, msg };
		::CreateThread(NULL, 0, win_util::check_module_directory_thread, ps, 0, &thread_id);
	}

	static void set_foreground_window(HWND hWnd) {
		int win_id = ::GetWindowThreadProcessId(GetForegroundWindow(), NULL);
		int tar_id = ::GetWindowThreadProcessId(hWnd, NULL);
		DWORD t;

		::AttachThreadInput(tar_id, win_id, TRUE);
		::SystemParametersInfo(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &t, 0);
		::SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, NULL, 0);
		::SetForegroundWindow(hWnd);
		::SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, t, NULL, 0);
		::AttachThreadInput(tar_id, win_id, FALSE);
	}

	static bool is_foreground_window_fullscreen(void) {
		RECT s;
		HWND fw = ::GetForegroundWindow();
		::GetWindowRect(fw, &s);

		const int sw = ::GetSystemMetrics(SM_CXSCREEN);
		const int sh = ::GetSystemMetrics(SM_CYSCREEN);

		if (s.left <= 0 && s.top <= 0 && s.right >= sw && s.bottom >= sh) {
			wchar_t cls[256];
			::GetClassName(fw, cls, 256);
			if (wcscmp(cls, L"Progman") != 0 && wcscmp(cls, L"WorkerW") != 0) return true;
		}
		return false;
	}

	static HFONT get_default_font() {
		NONCLIENTMETRICS ncm;
		ncm.cbSize = sizeof(ncm);
		ncm.cbSize -= sizeof(ncm.iPaddedBorderWidth);
		::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
		return ::CreateFontIndirect(&(ncm.lfMessageFont));
	}

};
