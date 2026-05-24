/**
 * Preference (Reading and writing INI file)
 *
 * @author Takuto Yanagida
 * @version 2025-11-19
 */

#pragma once

#include <vector>
#include <string>
#include <sstream>

#include <windows.h>

#include "path.hpp"
#include "file_system.hpp"

class Pref {

	std::wstring path_;
	std::wstring cur_sec_;

	static std::wstring get_user_name() {
		DWORD bufLen = MAX_PATH;
		std::vector<wchar_t> buf(bufLen);

		while (true) {
			::GetUserName(buf.data(), &bufLen);
			if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) break;
			bufLen *= 2;
			buf.resize(bufLen);
		}
		return std::wstring{ buf.data() };
	}

public:

	Pref() noexcept {
		path_ = file_system::module_file_path();
		path_.resize(path_.size() - 3);
		path_.append(L"ini");
	}

	// Make INI file path for multi user
	void set_multi_user_mode() {
		// Save the path of the normal INI file
		auto normalPath{ path_ };

		// Create INI file path for current user
		auto name = path::name(normalPath);
		auto path = path::parent(normalPath);
		auto user = get_user_name();
		path.append(L"\\").append(user);
		path_.assign(path).append(L"\\").append(name);

		// When a normal INI file exists and there is no INI file for the current user
		if (file_system::is_existing(normalPath) && !file_system::is_existing(path_)) {
			::CreateDirectory(path.c_str(), nullptr);  // Make a directory
			::CopyFile(normalPath.c_str(), path_.c_str(), TRUE);  // Copy
		}
	}

	// Get INI file path
	const std::wstring& path() const noexcept { return path_; }

	// Set the current section
	void set_current_section(const std::wstring& sec) noexcept { cur_sec_.assign(sec); }

	// Get string item
	std::wstring item(const wchar_t* sec, const wchar_t* key, const wchar_t* def) const {
		std::vector<wchar_t> buf(MAX_PATH);
		DWORD nSize{ MAX_PATH };

		while (true) {
			const auto outLen = ::GetPrivateProfileString(sec, key, def, buf.data(), nSize, path_.c_str());
			if (outLen != buf.size() - 1) break;
			nSize *= 2;
			buf.resize(nSize);
		}
		return { buf.data() };
	}

	// Get string item
	std::wstring item(const std::wstring& sec, const std::wstring& key, const std::wstring& def) const {
		return item(sec.c_str(), key.c_str(), def.c_str());
	}

	// Get string item
	std::wstring item(const wchar_t* key, const wchar_t* def) const {
		return item(cur_sec_.c_str(), key, def);
	}

	// Get string item
	std::wstring item(const std::wstring& key, const std::wstring& def) const {
		return item(cur_sec_.c_str(), key.c_str(), def.c_str());
	}

	// Write a string item
	void set_item(const std::wstring& str, const std::wstring& sec, const std::wstring& key) noexcept {
		::WritePrivateProfileString(sec.c_str(), key.c_str(), str.c_str(), path_.c_str());
	}

	// Write a string item
	void set_item(const std::wstring& str, const std::wstring& key) noexcept {
		set_item(str, cur_sec_, key);
	}

	// Get integer item
	int item_int(const wchar_t* sec, const wchar_t* key, int def) noexcept {
		if (sec == nullptr || key == nullptr) return def;
		return ::GetPrivateProfileInt(sec, key, def, path_.c_str());
	}

	// Get integer item
	int item_int(const std::wstring& sec, const std::wstring& key, int def) noexcept {
		return item_int(sec.c_str(), key.c_str(), def);
	}

	// Get integer item
	int item_int(const wchar_t* key, int def) noexcept {
		return item_int(cur_sec_.c_str(), key, def);
	}

	// Get integer item
	int item_int(const std::wstring& key, int def) noexcept {
		return item_int(cur_sec_.c_str(), key.c_str(), def);
	}

	// Write an integer item
	void set_item_int(const std::wstring& sec, const std::wstring& key, int val) noexcept {
		try {
			::WritePrivateProfileString(sec.c_str(), key.c_str(), std::to_wstring(val).c_str(), path_.c_str());
		} catch (...) {
		}
	}

	// Write an integer item
	void set_item_int(const std::wstring& key, int val) noexcept {
		set_item_int(cur_sec_, key, val);
	}

	// Get section content
	template <typename Container> auto items(const std::wstring& sec, const std::wstring& key, int max) -> Container {
		Container c{};
		std::wstring def;
		for (int i = 0; i < max; ++i) {
			auto temp = item(sec, key + std::to_wstring(i + 1), def);
			if (temp.empty()) continue;
			c.resize(c.size() + 1);
			c.back() = temp;
		}
		return c;
	}

	// Write the content of the section
	template <typename Container> void set_items(const Container& c, const std::wstring& sec, const std::wstring& key) {
		::WritePrivateProfileString(sec.c_str(), nullptr, nullptr, path_.c_str());
		int i = 0;
		for (const auto& it : c) {
			std::wostringstream vss;
			vss << it;
			set_item(vss.str(), sec, key + std::to_wstring(i + 1));
			i += 1;
		}
	}

};
