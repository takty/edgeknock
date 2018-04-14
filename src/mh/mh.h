#pragma once

#define WM_MOUSEMOVEHOOK (WM_USER + 1)

#ifdef __cplusplus
extern "C" {
#endif
	__declspec(dllexport) int SetOwnerWindow(HWND);
	__declspec(dllexport) int SetHook(void);
	__declspec(dllexport) int EndHook(void);
	__declspec(dllexport) LRESULT CALLBACK HookProc(int, WPARAM, LPARAM);
#ifdef __cplusplus
}
#endif
