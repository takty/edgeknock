/**
 * Shell Executer
 *
 * @author Takuto Yanagida
 * @version 2024-04-30
 */

#pragma once
#include <windows.h>
#include "path.h"

class shell_executer {

	static HANDLE h_thread;

	struct file_param {
		wchar_t file[MAX_PATH], param[MAX_PATH];
	};

	static DWORD WINAPI thread_proc(LPVOID d) {
		file_param* fp = (file_param*)d;
		wchar_t old_cd[MAX_PATH];
		::GetCurrentDirectory(MAX_PATH, old_cd);

		wchar_t parent[MAX_PATH];
		::SetCurrentDirectory(path::parent(parent, fp->file));

		SHELLEXECUTEINFO sei{};
		sei.cbSize       = sizeof(SHELLEXECUTEINFO);
		sei.fMask        = SEE_MASK_FLAG_LOG_USAGE | SEE_MASK_FLAG_DDEWAIT | SEE_MASK_UNICODE;
		sei.hwnd         = nullptr;
		sei.lpVerb       = nullptr;
		sei.lpFile       = fp->file;
		sei.lpParameters = fp->param;
		sei.lpDirectory  = nullptr;
		sei.nShow        = SW_SHOW;
		sei.hInstApp     = nullptr;
		bool ret = ::ShellExecuteEx(&sei);

		_RPTFWN(_CRT_WARN, L"ShellExecute %d\n", GetLastError());

		::SetCurrentDirectory(old_cd);  // for making removable disk ejectable
		::HeapFree(::GetProcessHeap(), 0, fp);
		::ExitThread(0);
	}

public:

	static void execute(const wchar_t file[], const wchar_t param[]) {
		file_param* fp = (file_param*)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(file_param));
		if (fp == NULL) return;
		wcscpy_s(fp->file, MAX_PATH, file);
		wcscpy_s(fp->param, MAX_PATH, param);

		h_thread = ::CreateThread(nullptr, 0, shell_executer::thread_proc, fp, 0, nullptr);
		if (h_thread) {
			::CloseHandle(h_thread);  // Release handle
			h_thread = nullptr;
		}
		else {
			::HeapFree(::GetProcessHeap(), 0, fp);
		}
	}

	static bool is_executing() {
		return h_thread != nullptr;
	}

};

HANDLE shell_executer::h_thread;
