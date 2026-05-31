/**
 * Shell Executer
 *
 * @author Takuto Yanagida
 * @version 2026-05-31
 */

#pragma once

#include <memory>
#include <new>
#include <string_view>
#include <atomic>
#include <process.h>
#include <windows.h>

#include "path.hpp"
#include "file_system.hpp"
#include "win_util.hpp"

namespace {

	std::atomic<bool> executing = false;

	struct file_param {
		std::wstring file, param;
		HMONITOR mon{};
	};

	unsigned __stdcall thread_proc(void* d) noexcept {
		auto* fp = static_cast<file_param*>(d);

		const auto old_cd = file_system::current_directory_path();
		const auto parent = path::parent(fp->file);
		::SetCurrentDirectory(parent.c_str());

		[[gsl::suppress("type.7")]]
		SHELLEXECUTEINFO sei{};
		sei.cbSize       = sizeof(SHELLEXECUTEINFO);
		sei.fMask        = SEE_MASK_FLAG_LOG_USAGE | SEE_MASK_FLAG_DDEWAIT | SEE_MASK_UNICODE | SEE_MASK_NOCLOSEPROCESS;
		sei.hwnd         = nullptr;
		sei.lpVerb       = nullptr;
		sei.lpFile       = fp->file.c_str();
		sei.lpParameters = fp->param.c_str();
		sei.lpDirectory  = nullptr;
		sei.nShow        = SW_SHOW;
		sei.hInstApp     = nullptr;

		if (::ShellExecuteEx(&sei)) {
			if (fp->mon) {
				DWORD pid{};
				if (sei.hProcess) {
					pid = ::GetProcessId(sei.hProcess);
					::WaitForInputIdle(sei.hProcess, 3000);
				}
				HWND w = window_utilities::find_main_window_by_pid(pid);
				if (w) window_utilities::ensure_window_on_monitor(w, fp->mon);
			}
			if (sei.hProcess) {
				::CloseHandle(sei.hProcess);
			}
		}

		::SetCurrentDirectory(old_cd.c_str());  // for making removable disk ejectable
		delete fp;
		executing.store(false, std::memory_order_release);
		return 0;
	}

}

namespace shell_executer {

	void execute(std::wstring_view file, std::wstring_view param, HMONITOR tar_mon) noexcept {
		bool expected = false;
		if (!executing.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
			return;
		}

		auto fp = std::unique_ptr<file_param>{};
		try {
			fp = std::make_unique<file_param>();
		} catch (const std::bad_alloc&) {
			executing.store(false, std::memory_order_release);
			return;
		}

		fp->file  = file;
		fp->param = param;
		fp->mon   = tar_mon;

		auto* raw_fp = fp.release();
		const auto thread_handle = ::_beginthreadex(nullptr, 0, thread_proc, raw_fp, 0, nullptr);
		const HANDLE h_thread = (thread_handle != 0) ? HANDLE(thread_handle) : nullptr;

		if (h_thread) {
			::CloseHandle(h_thread);  // Release handle
		} else {
			delete raw_fp;
			executing.store(false, std::memory_order_release);
		}
	}

	bool is_executing() noexcept {
		return executing.load(std::memory_order_acquire);
	}

}
