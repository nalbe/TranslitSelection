// KeyboardHook.hpp - swallows keys and hands them to one window instead
#pragma once

// Windows system headers
#include <Windows.h>


// ====================================================================
//  KeyboardHook - low-level keyboard hook on a thread of its own
// ====================================================================

// asked about every key stroke the system delivers, on the hook thread
using key_filter_type = bool (*)(BYTE vk);


class KeyboardHook
{
public:
	// posted to the target window per swallowed key press, wParam is the vk
	static constexpr UINT m_msgKey = WM_APP + 1;

	// posted once if the hook did not come up, wParam is the error code
	static constexpr UINT m_msgInstallFailed = WM_APP + 2;

	KeyboardHook(HWND window, key_filter_type filter) :
		m_window(window),
		m_filter(filter)
	{}

	// the hook has to answer within LowLevelHooksTimeout (300ms), so it gets a
	// thread of its own rather than the one running the message pump
	void install();
	void uninstall();

private:
	static DWORD  WINAPI threadProc(LPVOID param);
	static LRESULT CALLBACK lowLevelProc(int nCode, WPARAM wParam, LPARAM lParam);

	HWND            m_window   = nullptr;
	key_filter_type m_filter   = nullptr;
	HANDLE          m_thread   = nullptr;
	DWORD           m_threadId = 0;

};  // class KeyboardHook
