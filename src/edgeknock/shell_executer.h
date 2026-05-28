/**
 * Shell Executer
 *
 * @author Takuto Yanagida
 * @version 2026-05-28
 */

#pragma once

#include <memory>
#include <new>
#include <process.h>
#include <string_view>

#include <windows.h>

#include "path.hpp"
#include "file_system.hpp"

class shell_executer {

	static HANDLE h_thread;

	struct file_param {
		std::wstring file, param;
	};

	static unsigned __stdcall thread_proc(void* d) noexcept {
		auto* fp = static_cast<file_param*>(d);

		const auto old_cd = file_system::current_directory_path();
		const auto parent = path::parent(fp->file);
		::SetCurrentDirectory(parent.c_str());

		[[gsl::suppress("type.7")]]
		SHELLEXECUTEINFO sei{};
		sei.cbSize       = sizeof(SHELLEXECUTEINFO);
		sei.fMask        = SEE_MASK_FLAG_LOG_USAGE | SEE_MASK_FLAG_DDEWAIT | SEE_MASK_UNICODE;
		sei.hwnd         = nullptr;
		sei.lpVerb       = nullptr;
		sei.lpFile       = fp->file.c_str();
		sei.lpParameters = fp->param.c_str();
		sei.lpDirectory  = nullptr;
		sei.nShow        = SW_SHOW;
		sei.hInstApp     = nullptr;
		::ShellExecuteEx(&sei);

		_RPTFWN(_CRT_WARN, L"ShellExecute %d\n", GetLastError());

		::SetCurrentDirectory(old_cd.c_str());  // for making removable disk ejectable
		delete fp;
		return 0;
	}

public:

	static void execute(std::wstring_view file, std::wstring_view param) noexcept {
		auto fp = std::unique_ptr<file_param>{};
		try {
			fp = std::make_unique<file_param>();
		}
		catch (const std::bad_alloc&) {
			return;
		}

		fp->file  = file;
		fp->param = param;

		auto* raw_fp = fp.release();
		const auto thread_handle = ::_beginthreadex(nullptr, 0, shell_executer::thread_proc, raw_fp, 0, nullptr);
		h_thread = (thread_handle != 0) ? HANDLE(thread_handle) : nullptr;
		if (h_thread) {
			::CloseHandle(h_thread);  // Release handle
			h_thread = nullptr;
		}
		else {
			delete raw_fp;
		}
	}

	static bool is_executing() noexcept {
		return h_thread != nullptr;
	}

};

HANDLE shell_executer::h_thread;
