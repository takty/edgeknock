/**
 * Path Utilities
 *
 * @author Takuto Yanagida
 * @version 2025-11-04
 */

#pragma once
#include <windows.h>

class path {

	static int find_last_of(const wchar_t* str, wchar_t c) noexcept {
		for (int i = (int)wcslen(str) - 1; i >= 0; --i) {
			if (str[i] == c) return i;
		}
		return -1;
	}

public:

	static wchar_t* name(wchar_t* ret, const wchar_t* path) noexcept {
		const int i = find_last_of(path, L'\\');

		if (i == -1) {  // When path is only file name
			wcscpy_s(ret, MAX_PATH, path);
		}
		else if (i == 2 && path[3] == L'\0') {  // When path is root
			const LPWSTR r = lstrcpyn(ret, path, 3);  // return "C:"
		}
		else {
			wcscpy_s(ret, MAX_PATH, path + i + 1);
		}
		return ret;
	}

	static wchar_t* parent(wchar_t* ret, const wchar_t* path) noexcept {
		int i = find_last_of(path, L'\\');

		if (i == -1) {
			ret[0] = L'\0';
		}
		else if (i == 2 && path[3] == L'\0') {  // When path is root
			ret[0] = L'\0';
		}
		else {
			if (i == 2) ++i;
			const LPWSTR r = lstrcpyn(ret, path, i + 1);
		}
		return ret;
	}

	static wchar_t* parent_dest(wchar_t* path) noexcept {
		int i = find_last_of(path, L'\\');

		if (i == -1) {
			path[0] = L'\0';
		}
		else if (i == 2 && path[3] == L'\0') {  // When path is root
			path[0] = L'\0';
		}
		else {
			if (i == 2) ++i;
			path[i] = L'\0';
		}
		return path;
	}

	static wchar_t* absolute_path(wchar_t* path) noexcept {
		wchar_t tmp[MAX_PATH]{};

		if (path[0] == L'.' && path[1] == L'\\') {
			::GetModuleFileName(NULL, &tmp[0], MAX_PATH - 1);
			parent_dest(&tmp[0]);
			wcscat_s(&tmp[0], MAX_PATH, path + 1);
			wcscpy_s(path, MAX_PATH, &tmp[0]);
		}
		return path;
	}

};
