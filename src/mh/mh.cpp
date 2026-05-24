// mh.cpp : Defines the exported functions for the DLL application.

#include "stdafx.h"

#include "gsl/gsl"

HINSTANCE HInst;

#pragma comment(linker, "/section:.shared,rws")
#pragma data_seg(".shared")
HHOOK HHook = NULL;
HWND HWnd = NULL;
#pragma data_seg()

VOID SetModuleHandle(HMODULE hModule) noexcept {
	HInst = hModule;
}

__declspec(dllexport) int SetOwnerWindow(HWND hWnd) {
	HWnd = hWnd;
	return 0;
}

__declspec(dllexport) int SetHook() {
	HHook = SetWindowsHookEx(WH_MOUSE, (HOOKPROC)HookProc, HInst, (DWORD)NULL);
	if (HHook == NULL) {
		return -1;
	}
	return 0;
}

__declspec(dllexport) int EndHook() {
	if (UnhookWindowsHookEx(HHook) != 0) {
		return 0;
	}
	return -1;
}

__declspec(dllexport) LRESULT CALLBACK HookProc(int code, WPARAM wp, LPARAM lp) {
	[[gsl::suppress("type.1")]]
	const MOUSEHOOKSTRUCT* mhs = reinterpret_cast<const MOUSEHOOKSTRUCT*>(lp);

	if (code < 0) {
		return CallNextHookEx(HHook, code, wp, lp);
	}
	if (HWnd && (wp == WM_MOUSEMOVE || wp == WM_NCMOUSEMOVE)) {
		PostMessage(HWnd, WM_MOUSEMOVEHOOK, mhs->pt.x, mhs->pt.y);
	}
	return CallNextHookEx(HHook, code, wp, lp);
}
