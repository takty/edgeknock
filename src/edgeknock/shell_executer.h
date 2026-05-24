/**
 * Shell Executer
 *
 * @author Takuto Yanagida
 * @version 2026-05-24
 */

#pragma once

#include <windows.h>

#include "path.hpp"
#include "file_system.hpp"

class shell_executer {

	static HANDLE h_thread;

	struct file_param {
		wchar_t file[MAX_PATH], param[MAX_PATH];
	};

	static DWORD WINAPI thread_proc(LPVOID d) noexcept {
		[[gsl::suppress("type.1")]]
		file_param* fp = reinterpret_cast<file_param*>(d);

		const auto old_cd = file_system::current_directory_path();
		const auto parent = path::parent(&fp->file[0]);
		::SetCurrentDirectory(parent.c_str());

		[[gsl::suppress("type.7")]]
		SHELLEXECUTEINFO sei{};
		sei.cbSize       = sizeof(SHELLEXECUTEINFO);
		sei.fMask        = SEE_MASK_FLAG_LOG_USAGE | SEE_MASK_FLAG_DDEWAIT | SEE_MASK_UNICODE;
		sei.hwnd         = nullptr;
		sei.lpVerb       = nullptr;
		sei.lpFile       = &fp->file[0];
		sei.lpParameters = &fp->param[0];
		sei.lpDirectory  = nullptr;
		sei.nShow        = SW_SHOW;
		sei.hInstApp     = nullptr;
		::ShellExecuteEx(&sei);

		_RPTFWN(_CRT_WARN, L"ShellExecute %d\n", GetLastError());

		::SetCurrentDirectory(old_cd.c_str());  // for making removable disk ejectable
		::HeapFree(::GetProcessHeap(), 0, fp);
		::ExitThread(0);
	}

public:

	static void execute(const std::wstring& file, const std::wstring& param) noexcept {
		[[gsl::suppress("type.1")]]
		file_param* fp = reinterpret_cast<file_param*>(::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(file_param)));
		if (fp == NULL) return;

		wcscpy_s(&(fp->file[0]), MAX_PATH, file.c_str());
		wcscpy_s(&(fp->param[0]), MAX_PATH, param.c_str());

		h_thread = ::CreateThread(nullptr, 0, shell_executer::thread_proc, fp, 0, nullptr);
		if (h_thread) {
			::CloseHandle(h_thread);  // Release handle
			h_thread = nullptr;
		}
		else {
			::HeapFree(::GetProcessHeap(), 0, fp);
		}
	}

	static bool is_executing() noexcept {
		return h_thread != nullptr;
	}

};

HANDLE shell_executer::h_thread;
