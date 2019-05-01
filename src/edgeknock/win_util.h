/**
 *
 * Window Utilities
 *
 * @author Takuto Yanagida
 * @version 2019-05-01
 *
 */


#pragma once
#include <windows.h>


class win_util {

public:

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
