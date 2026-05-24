/**
 * File System Operations
 *
 * @author Takuto Yanagida
 * @version 2026-05-24
 */

#pragma once

#include <string>
#include <vector>

#include <FileAPI.h>
#include <Shlobj.h>

#include "path.hpp"

namespace file_system {

	// Template version of find first file
	template<typename F> bool find_first_file(const std::wstring& path, F fn) {
		auto parent{ path };
		parent += (parent.back() == path::PATH_SEPARATOR) ? L"*" : L"\\*";

		WIN32_FIND_DATA wfd{};
		auto sh = ::FindFirstFileEx(parent.c_str(), FindExInfoBasic, &wfd, FindExSearchNameMatch, nullptr, 0);
		if (sh == INVALID_HANDLE_VALUE) return false;
		parent.resize(parent.size() - 1);
		do {
			if (!::lstrcmp(&wfd.cFileName[0], L".") || !::lstrcmp(&wfd.cFileName[0], L"..")) continue;
			if (!fn(parent, wfd)) break;
		} while (::FindNextFile(sh, &wfd));
		::FindClose(sh);
		return true;
	}

	// Calculate file/directory size internally
	bool calc_file_size_internally(const std::wstring& path, uint64_t& size, const uint64_t& limitTime) {
		if (limitTime > 0 && ::GetTickCount64() > limitTime) return false;
		bool success = true;

		find_first_file(path, [&](const std::wstring& parent, const WIN32_FIND_DATA& wfd) {
			if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				return success = calc_file_size_internally(parent + std::wstring{ &wfd.cFileName[0] }, size, limitTime);
			}
			size += (static_cast<uint64_t>(wfd.nFileSizeHigh) << 32) + wfd.nFileSizeLow;
			return true;
		});
		return success;
	}

	// Check whether there is an execution file that has the same name as the name of path
	bool is_existing_same_name_execution_file(const std::wstring& path) {
		bool ret = false;
		auto temp = path::parent(path);
		if (temp.empty()) return false;  // path is root

		find_first_file(temp, [&](const std::wstring&, const WIN32_FIND_DATA& wfd) {
			auto e = path::ext(std::wstring{ &wfd.cFileName[0] });
			if (e == L"exe" || e == L"bat") {
				ret = true;
				return false;  // break;
			}
			return true;  // continue
		});
		return ret;
	}

	// Check whether the path is a removable disk
	bool is_removable(const std::wstring& path) noexcept {
		return ::GetDriveType(path.c_str()) == DRIVE_REMOVABLE;
	}

	// Check whether the path is a directory
	bool is_directory(const std::wstring& path) noexcept {
		const auto attr = ::GetFileAttributes(path::ensure_unc_prefix(path).c_str());
		return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

	// Check whether the file of the path is existing
	bool is_existing(const std::wstring& path) noexcept {
		const auto attr = ::GetFileAttributes(path::ensure_unc_prefix(path).c_str());
		return attr != INVALID_FILE_ATTRIBUTES;
	}

	// Get desktop directory path
	std::wstring desktop_path() {
		std::wstring ret;
		PWSTR buf = nullptr;
		if (::SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &buf) == S_OK) {
			ret.assign(buf);
			::CoTaskMemFree(buf);
		}
		return ret;
	}

	// Get the exe file path
	std::wstring module_file_path() noexcept {
		std::vector<wchar_t> buf(MAX_PATH);
		for (DWORD nSize = MAX_PATH;; nSize *= 2) {
			buf.resize(nSize);
			const auto len = ::GetModuleFileName(nullptr, buf.data(), nSize);
			if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
				return std::wstring(buf.data(), len);
			}
		}
	}

	// Get the current directory path
	std::wstring current_directory_path() noexcept {
		std::vector<wchar_t> buf(MAX_PATH);
		for (DWORD nSize = MAX_PATH;;) {
			buf.resize(nSize);
			nSize = ::GetCurrentDirectory(nSize, buf.data());
			if (nSize < buf.size()) {
				return std::wstring(buf.data());
			}
		}
	}

	// Make unique new name
	std::wstring unique_name(const std::wstring& obj, const std::wstring& post = std::wstring()) {
		if (path::is_root(obj)) return L"";  // Failure
		const auto parent = path::parent(obj);
		auto name = path::name(obj);

		std::wstring ext;
		if (!is_directory(obj) && name.front() != path::EXT_PREFIX) {  // When the file is not dot file
			name = path::name_without_ext(obj);
			ext = path::ext(obj);
			if (!ext.empty()) ext.insert(std::begin(ext), path::EXT_PREFIX);
		}

		auto build_path = [&](const std::wstring& suffix) {
			return parent + std::wstring(1, path::PATH_SEPARATOR) + name + post + suffix + ext;
		};

		auto ret = build_path(L"");
		for (int i = 1; is_existing(ret); ++i) {
			ret = build_path(L"(" + std::to_wstring(i) + L")");
		}
		return ret;
	}

	// Get drive size
	void drive_size(const std::wstring& path, uint64_t& size, uint64_t& free) noexcept {
		ULARGE_INTEGER f{}, s{};
		::GetDiskFreeSpaceEx(path.c_str(), &f, &s, nullptr);
		free = f.QuadPart;
		size = s.QuadPart;
	}

	// Calculate file/directory size
	bool calc_file_size(const std::wstring& path, uint64_t& size, const uint64_t& limitTime) {
		if (!is_directory(path)) {
			auto hf = ::CreateFile(path.c_str(), 0, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
			if (hf == INVALID_HANDLE_VALUE) {
				return false;
			}
			LARGE_INTEGER s{};
			::GetFileSizeEx(hf, &s);
			size = s.QuadPart;
			::CloseHandle(hf);
			return true;
		}
		size = 0;
		return calc_file_size_internally(path, size, limitTime);
	}

	bool calc_file_size(const std::vector<std::wstring>& paths, uint64_t& size, const uint64_t& limitTime) {
		size = 0;
		for (const auto& p : paths) {
			uint64_t s{};
			if (!calc_file_size(p, s, limitTime)) return false;
			size += s;
		}
		return true;
	}

	// Extract command line string (path|opt)
	std::pair<std::wstring, std::wstring> extract_command_line_string(const std::wstring& line, const std::vector<std::wstring>& objs) {
		const auto sep = line.find_first_of(L'|');
		if (sep == std::wstring::npos) {
			return { path::absolute_path(line, file_system::module_file_path()), path::space_separated_quoted_paths_string(objs) };
		}
		auto opt = line.substr(sep + 1);

		auto replace_placeholder = [&](const std::wstring& placeholder, const std::wstring& replacement) {
			const auto pos = opt.find(placeholder);
			if (pos != std::wstring::npos) opt.replace(pos, placeholder.size(), replacement);
		};

		// %path%  -> "path\file"
		replace_placeholder(L"%path%", path::quote(path::ensure_unc_prefix_if_needed(objs.front())));
		// %paths% -> "path\file1" "path\file2"...
		replace_placeholder(L"%paths%", path::space_separated_quoted_paths_string(objs));
		// %file% -> "file"
		replace_placeholder(L"%file%", path::quote(path::name(objs.front())));
		return { path::absolute_path(line.substr(0, sep), file_system::module_file_path()), opt };
	}

};
