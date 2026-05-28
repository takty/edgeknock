/**
 * File Path Operations
 *
 * @author Takuto Yanagida
 * @version 2026-05-28
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

const std::wstring PATH_EXT_DIR(L"<folder>");

namespace path {

	constexpr wchar_t PATH_SEPARATOR   = L'\\';  // File path separator
	constexpr wchar_t DRIVE_IDENTIFIER = L':';   // Drive identifier
	constexpr wchar_t EXT_PREFIX       = L'.';   // Extension prefix

	constexpr wchar_t const * const UNC_PREFIX = L"\\\\?\\";
	constexpr size_t UNC_PREFIX_SIZE = 4;

	bool has_unc_prefix(std::wstring_view path) noexcept {
		return 0 == path.compare(0, UNC_PREFIX_SIZE, UNC_PREFIX);
	}

	void append_root_separator_if_drive(std::wstring& out, std::wstring_view path) noexcept {
		if (path.empty()) return;
		if (path.back() == DRIVE_IDENTIFIER) {
			out.append(1, PATH_SEPARATOR);
		}
	}

	// Extract file name (UNC ok)
	std::wstring name(std::wstring_view path) noexcept {
		if (path.empty()) return L"";
		const auto size = path.size();
		const auto lp   = (path.back() == PATH_SEPARATOR) ? size - 2 : size - 1;
		const auto pos  = path.find_last_of(PATH_SEPARATOR, lp);

		if (pos == std::wstring_view::npos) {  // File name only
			return std::wstring{ path.substr(0, lp + 1) };
		}
		return std::wstring{ path.substr(pos + 1, lp - pos) };
	}

	// Extract file name without extention
	std::wstring name_without_ext(std::wstring_view path) noexcept {
		auto ret = name(path);
		const auto pos = ret.find_last_of(EXT_PREFIX);
		if (pos != std::wstring::npos) ret.resize(pos);
		return ret;
	}

	// Extract file extention
	std::wstring ext(std::wstring_view path) noexcept {
		auto n = name(path);
		const auto pos = n.find_last_of(EXT_PREFIX);

		if (pos == std::wstring::npos) {  // No extention
			return L"";  // Return empty
		}
		auto ret = n.substr(pos + 1);
		std::transform(std::begin(ret), std::end(ret), std::begin(ret), ::towlower);  // To lower case
		return ret;
	}

	// Extract parent path
	std::wstring parent(std::wstring_view path) noexcept {
		if (path.empty()) return L"";
		const auto size = path.size();
		const auto pos = path.find_last_of(PATH_SEPARATOR);

		if (pos == std::wstring_view::npos) {  // Abnormal
			return L"";  // Return empty
		}
		if (1 < size && path.at(pos - 1) == DRIVE_IDENTIFIER) {  // When root 'C:\'
			if (pos == size - 1) {  // Pos is the tail end
				return L"";  // Return empty
			} else {  // Return containing '\'
				return std::wstring{ path.substr(0, pos + 1) };
			}
		}
		return std::wstring{ path.substr(0, pos) };
	}

	// Quote path
	std::wstring quote(std::wstring_view path) noexcept {
		std::wstring ret;
		ret.reserve(path.size() + 2);
		ret.push_back(L'\"');
		ret.append(path);
		ret.push_back(L'\"');
		return ret;
	}

	// Ensure the path begin with UNC prefix "\\?\"
	std::wstring ensure_unc_prefix(std::wstring_view path) noexcept {
		if (has_unc_prefix(path)) {
			return std::wstring{ path };
		}
		return std::wstring{ UNC_PREFIX }.append(path);
	}

	// Ensure the path does not begin with UNC prefix "\\?\"
	std::wstring ensure_no_unc_prefix(std::wstring_view path) noexcept {
		if (has_unc_prefix(path)) {
			return std::wstring{ path.substr(UNC_PREFIX_SIZE) };
		}
		return std::wstring{ path };
	}

	// Ensure the path begin with UNC prefix "\\?\" if the path is too long.
	std::wstring ensure_unc_prefix_if_needed(std::wstring_view path) noexcept {
		const auto p = ensure_no_unc_prefix(path);
		if (MAX_PATH - 1 < p.size()) {
			return ensure_unc_prefix(p);
		}
		return p;
	}

	// Translate relative path to absolute path
	std::wstring absolute_path(std::wstring_view path, std::wstring_view module_file_path) noexcept {
		if (path.size() < 2) {
			return std::wstring{ path };
		}
		if (path.at(0) == EXT_PREFIX && path.at(1) == PATH_SEPARATOR) {
			auto ret = parent(module_file_path);
			return ret.append(path, 1);
		}
		return std::wstring{ path };
	}

	// Make quoted and space-separated path string
	std::wstring space_separated_quoted_paths_string(const std::vector<std::wstring>& paths) noexcept {
		std::wstring ret;
		for (const auto& path : paths) {
			ret.append(1, L'\"').append(ensure_unc_prefix_if_needed(path));
			append_root_separator_if_drive(ret, path);
			ret.append(L"\" ");
		}
		if (!ret.empty()) ret.pop_back();  // Remove last space
		return ret;
	}

	// Make NULL-separated and double-NULL-terminated path string
	std::wstring null_separated_paths_string(const std::vector<std::wstring>& paths) noexcept {
		std::wstring ret;
		for (const auto& path : paths) {
			ret.append(path);
			append_root_separator_if_drive(ret, path);
			ret.append(1, L'\0');
		}
		ret.append(1, L'\0');
		return ret;
	}

	// Check whether the path is root
	constexpr bool is_root(std::wstring_view path) noexcept {
		const auto size = path.size();
		if (size == 0) return false;
		return
			(1 < size && path.at(size - 2) == DRIVE_IDENTIFIER && path.at(size - 1) == PATH_SEPARATOR) ||
			(0 < size && path.at(size - 1) == DRIVE_IDENTIFIER);
	}

};
