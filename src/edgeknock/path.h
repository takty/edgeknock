/**
 *
 * Path Utilities
 *
 * @author Takuto Yanagida
 * @version 2019-05-01
 *
 */


#pragma once
#include <windows.h>

class path {

	static int find_last_of(const wchar_t *str, wchar_t c) {
		int ret = -1;
		for (int i = 0; str[i] != L'\0'; ++i) {
			if (str[i] == c) ret = i;
		}
		return ret;
	}

public:

	static wchar_t* name(wchar_t *ret, const wchar_t *path) {
		int pos = find_last_of(path, L'\\');

		if (pos == -1) {  // When path is only file name
			wcscpy_s(ret, MAX_PATH, path);
		}
		else if (pos == 2 && path[3] == L'\0') {  // When path is root
			LPWSTR r = lstrcpyn(ret, path, 3);  // return "C:"
		}
		else {
			wcscpy_s(ret, MAX_PATH, path + pos + 1);
		}
		return ret;
	}

	static wchar_t* parent(wchar_t *ret, const wchar_t *path) {
		int pos = find_last_of(path, L'\\');

		if (pos == -1) {
			ret[0] = L'\0';
		}
		else if (pos == 2 && path[3] == L'\0') {  // When path is root
			ret[0] = L'\0';
		}
		else {
			if (pos == 2) ++pos;
			LPWSTR r = lstrcpyn(ret, path, pos + 1);
		}
		return ret;
	}

	static wchar_t* parent_dest(wchar_t *path) {
		int pos = find_last_of(path, L'\\');

		if (pos == -1) {
			path[0] = L'\0';
		}
		else if (pos == 2 && path[3] == L'\0') {  // When path is root
			path[0] = L'\0';
		}
		else {
			if (pos == 2) ++pos;
			path[pos] = L'\0';
		}
		return path;
	}

	static wchar_t* absolute_path(wchar_t *path) {
		wchar_t tmp[MAX_PATH];

		if (path[0] == L'.' && path[1] == L'\\') {
			::GetModuleFileName(NULL, tmp, MAX_PATH - 1);
			parent_dest(tmp);
			wcscat_s(tmp, MAX_PATH, path + 1);
			wcscpy_s(path, MAX_PATH, tmp);
		}
		return path;
	}

};
