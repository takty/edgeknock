/**
 * DC Utilities
 *
 * @author Takuto Yanagida
 * @version 2026-05-30
 */

#pragma once

#include <string_view>
#include <windows.h>

namespace dc {

	HFONT get_default_font(HWND wnd) noexcept {
		NONCLIENTMETRICS ncm{};
		ncm.cbSize = sizeof(ncm);
		const int dpi = ::GetDpiForWindow(wnd);

		::SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0, dpi);
		return ::CreateFontIndirect(&(ncm.lfMessageFont));
	}

	SIZE get_text_size(HWND wnd, std::wstring_view msg) noexcept {
		HDC hdc = ::GetDC(wnd);
		HFONT f = get_default_font(wnd);
		::SelectObject(hdc, f);
		SIZE s{};
		::GetTextExtentPoint32(hdc, msg.data(), gsl::narrow_cast<int>(msg.length()), &s);
		::DeleteObject(f);
		::ReleaseDC(wnd, hdc);
		return s;
	}

}
