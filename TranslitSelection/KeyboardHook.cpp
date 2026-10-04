// KeyboardHook.cpp - low-level keyboard hook on a thread of its own
#include "KeyboardHook.hpp"

namespace
{
	// what the hook callback works on, SetWindowsHookEx passes no user data
	struct HookContext
	{
		HWND            window;
		key_filter_type filter;
		BYTE            down;  // vk of an owned key held down right now
	};

	// touched by the hook thread only: the callback is called in the context
	// of the thread that installed the hook
	HookContext* g_context = nullptr;

	// how long uninstall() waits for the hook thread to notice WM_QUIT
	constexpr DWORD k_stopTimeoutMs = 2000;

}  // namespace


// the hook lives on this thread for as long as it is installed
DWORD WINAPI KeyboardHook::threadProc(LPVOID param)
{
	KeyboardHook* self = static_cast<KeyboardHook*>(param);

	HookContext context{ self->m_window, self->m_filter, 0 };
	g_context = &context;

	const HHOOK hook = ::SetWindowsHookExW(WH_KEYBOARD_LL, &KeyboardHook::lowLevelProc,
		::GetModuleHandleW(nullptr), 0);
	if (!hook) {
		::PostMessageW(self->m_window, m_msgInstallFailed, ::GetLastError(), 0);
		return 0;
	}

	MSG msg;
	while (::GetMessageW(&msg, nullptr, 0, 0) > 0) {
		::TranslateMessage(&msg);
		::DispatchMessageW(&msg);
	}

	::UnhookWindowsHookEx(hook);
	g_context = nullptr;
	return 0;
}

// swallows the keys the filter claims, modified ones and nothing else
LRESULT CALLBACK KeyboardHook::lowLevelProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode != HC_ACTION) {
		return ::CallNextHookEx(nullptr, nCode, wParam, lParam);
	}

	const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
	if (!info || !g_context || !g_context->filter(static_cast<BYTE>(info->vkCode))) {
		return ::CallNextHookEx(nullptr, nCode, wParam, lParam);
	}

	const BYTE vk = static_cast<BYTE>(info->vkCode);

	// a modified key belongs to the focused app
	if (::GetAsyncKeyState(VK_CONTROL) < 0
		|| ::GetAsyncKeyState(VK_SHIFT)   < 0
		|| ::GetAsyncKeyState(VK_MENU)    < 0)
	{
		g_context->down = 0;
		return ::CallNextHookEx(nullptr, nCode, wParam, lParam);
	}

	if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
		if (g_context->down == 0) {  // auto-repeat is swallowed, not fired
			g_context->down = vk;
			::PostMessageW(g_context->window, m_msgKey, vk, 0);
		}
		return 1;
	}

	if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
		g_context->down = 0;
		return 1;
	}

	return ::CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void KeyboardHook::install()
{
	m_thread = ::CreateThread(nullptr, 0, &KeyboardHook::threadProc, this, 0, &m_threadId);
	if (!m_thread) {
		::PostMessageW(m_window, m_msgInstallFailed, ::GetLastError(), 0);
	}
}

void KeyboardHook::uninstall()
{
	if (!m_thread) {
		return;
	}
	::PostThreadMessageW(m_threadId, WM_QUIT, 0, 0);
	::WaitForSingleObject(m_thread, k_stopTimeoutMs);
	::CloseHandle(m_thread);
	m_thread   = nullptr;
	m_threadId = 0;
}
