// MainWindow.h - TranslitSelection tray-only layout fixer
#pragma once

// Implementation-specific headers
#include "cst/winapi/Window/Window.hpp"
#include "cst/winapi/WindowTheme/WindowTheme.hpp"
#include "cst/winapi/TrayIcon/TrayIcon.hpp"
#include "cst/winapi/WinHandles/WinHandles.hpp"
#include "cst/UniqID/UniqID.hpp"
#include "LayoutMap.hpp"
#include "resource.h"

// Standard library headers
#include <string>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <functional>
#include <vector>

// Logging
#include "cst/LogManager/LogManager.hpp"
#include "cst/winapi/ClipboardManager/ClipboardManager.hpp"

// Windows system headers
#include <Windows.h>
#include <commctrl.h>

// Library links
#pragma comment(lib, "comctl32.lib")



// ====================================================================
//  MainWindow - hidden window (message pump only) + tray icon
//  Left-click tray  = no action
//  Right-click tray = popup menu (variants)
// ====================================================================
class MainWindow :
	public cst::winapi::WindowThemeMixin<MainWindow>,
	public cst::winapi::Window,
	public cst::winapi::TrayIcon
{
public:
	static constexpr LPCWSTR ClassName  = L"TranslitSelectionClass";
	static constexpr LPCWSTR ClassTitle = L"TranslitSelection";

private:
	const cst::UniqID m_idTrayExit;
	const cst::UniqID m_idToggleLayout;
	const cst::UniqID m_idKeepSelection;

	bool m_layoutToggle = false;   // from tray menu checkbox
	bool m_keepSelection = false;  // re-select pasted text after translate

	std::wstring m_savedClipboard;  // clipboard contents before this operation

	HBRUSH m_bgBrush {};

	// logger
	std::ofstream    m_logFile;
	cst::LogManager  m_log;

	void log(const char* msg)
	{
		if (m_logFile.is_open()) {
			m_logFile << msg << std::endl;
		}
	}

	void logHwnd(const char* prefix, HWND h)
	{
		char buf[128];
		sprintf_s(buf, "%s HWND=%p", prefix, static_cast<void*>(h));
		log(buf);
	}

public:
	MainWindow() :
		Window( Window::Config{}
			.withClassName         ( ClassName  )
			.withClassTitle        ( ClassTitle )
			.withMessageHandler    ( [this](auto... args) { return WndProc(args...); })
			.withIcon              ( ::LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCE(IDI_ICON1)), false)
			.withStyle             ( WS_OVERLAPPED )
			.withRect              ( RECT{ 0, 0, 0, 0 } )
			.withImmediateCreation ()
			(/* end of Window config */)
		),
		TrayIcon( TrayIcon::Config{}
			.withID                ( cst::UniqID{}           )
			.withCallbackMessage   ( cst::UniqID{}           )
			.withTooltip           ( ClassTitle              )
			.withVersion           ( NOTIFYICON_VERSION      )
			.withFlags             ( NIF_MESSAGE | NIF_TIP | NIF_ICON )
			.withIcons             ( new HICON[2] {
						::LoadIconW(m_hInstance, MAKEINTRESOURCE(IDI_ICON1)),
						::LoadIconW(m_hInstance, MAKEINTRESOURCE(IDI_ICON1))
					}, 2, false )
			.withHandle            ( handle()                )
			.withMenu              (
				[this]() -> HMENU {
					HMENU menu = ::CreatePopupMenu();
					::AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"Select text, press F8");
					::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
					::AppendMenuW(menu, MF_STRING | (m_layoutToggle ? MF_CHECKED : MF_UNCHECKED),
						m_idToggleLayout, L"Switch layout after translit");
					::AppendMenuW(menu, MF_STRING | (m_keepSelection ? MF_CHECKED : MF_UNCHECKED),
						m_idKeepSelection, L"Keep selection after translit");
					::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
					::AppendMenuW(menu, MF_STRING, m_idTrayExit, L"Exit");
					return menu;
				})
			.withMenuFlags         ( TPM_LEFTALIGN | TPM_RIGHTBUTTON )
			.withImmediateCreation ()
			(/* end of TrayIcon config */)
		)
	{
		m_logFile.open("C:\\Users\\qqq\\Desktop\\translit.log", std::ios::app);
		log("=== TranslitSelection starting ===");

		cst::winapi::WindowThemeProxy{}->follow_system_theme(handle());
		RegisterHotKey(handle(), 1, 0, VK_F8);

		logHwnd("MainWindow created, self=", handle());
		log("Global hotkey F8 registered");

		// load the transliteration table from layout.txt next to the exe
		// (falls back to the built-in table if absent or broken)
		wchar_t exePath[MAX_PATH] = {};
		::GetModuleFileNameW(nullptr, exePath, MAX_PATH);
		std::wstring tablePath = exePath;
		size_t slash = tablePath.find_last_of(L"\\/");
		if (slash != std::wstring::npos) {
			tablePath = tablePath.substr(0, slash + 1);
			tablePath += L"layout.txt";
		}
		bool custom = kf::LayoutMap::warmup(tablePath);
		if (!custom) {
			log("LayoutMap: FAILED to load layout.txt - translit disabled");
		} else {
			char b[256];
			std::string langs;
			for (auto const& l : kf::LayoutMap::languages()) {
				char cbuf[32];
				::WideCharToMultiByte(CP_UTF8, 0, l.c_str(), -1, cbuf, 32, nullptr, nullptr);
				langs += std::string(cbuf) + " ";
			}
			sprintf_s(b, "LayoutMap: loaded %d columns: %s",
				(int)kf::LayoutMap::languages().size(), langs.c_str());
			log(b);
		}
	}

	private:
	// -- clipboard helpers (thin wrappers over the shared library) -----
	static std::wstring readClipboard()
	{
		return cst::winapi::ClipboardManager::GetText();
	}

	static void writeClipboard(const std::wstring& text)
	{
		cst::winapi::ClipboardManager::SetText(text);
	}

	// -- send a Ctrl+<key> via SendInput using scancode+extended so the
	//    event looks like a real physical keystroke. bare-vk injections are
	//    dropped by modern toolkits (Firefox/Gecko), but scancode ones pass.
	static void sendCtrlKey(BYTE vk)
	{
		static const BYTE ctrlScan = 0x1D;   // left ctrl
		BYTE keyScan = ::MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);

		INPUT down[2] = {};
		down[0].type = INPUT_KEYBOARD; down[0].ki.wVk = VK_CONTROL; down[0].ki.wScan = ctrlScan; down[0].ki.dwFlags = KEYEVENTF_SCANCODE;
		down[1].type = INPUT_KEYBOARD; down[1].ki.wVk = vk;         down[1].ki.wScan = keyScan;  down[1].ki.dwFlags = KEYEVENTF_SCANCODE;
		::SendInput(2, down, sizeof(INPUT));

		INPUT up[2] = {};
		up[0].type = INPUT_KEYBOARD; up[0].ki.wVk = vk;         up[0].ki.wScan = keyScan;  up[0].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
		up[1].type = INPUT_KEYBOARD; up[1].ki.wVk = VK_CONTROL; up[1].ki.wScan = ctrlScan; up[1].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
		::SendInput(2, up, sizeof(INPUT));
	}

	// empties the Windows clipboard so any later copy leaves an unambiguous
	// fingerprint (we can tell a real selection-copy from stale contents).
	static void clearClipboard()
	{
		cst::winapi::ClipboardManager::ClearClipboardData();
	}

	// -- resolve an HKL's language to a dictionary column index, or -1 -----
	//    matches by ISO 639 language code (En/Ru/Fr...), falling back to the
	//    raw LANGID hex, so a column header can be "en", "en-US", "En" etc.
	static int languageOfLayout(HKL hkl)
	{
		if (!hkl) return -1;
		WORD langid = LOWORD(hkl);

		wchar_t buf[16] = {};
		int n = ::GetLocaleInfoW(MAKELCID(langid, SORT_DEFAULT), LOCALE_SISO639LANGNAME, buf, 16);
		if (n > 0) {
			int idx = kf::LayoutMap::languageIndex(buf);
			if (idx >= 0) return idx;
		}

		wchar_t hex[8];
		swprintf_s(hex, L"%04X", static_cast<unsigned>(langid));
		return kf::LayoutMap::languageIndex(hex);
	}

	// -- the HKL that follows <current> in the system layout cycle, or 0 ---
	static HKL nextLayout(HKL current)
	{
		UINT count = ::GetKeyboardLayoutList(0, nullptr);
		if (count < 2) return 0;
		std::vector<HKL> layouts(count);
		::GetKeyboardLayoutList(count, layouts.data());
		for (UINT i = 0; i < count; ++i) {
			if (layouts[i] == current) {
				return layouts[(i + 1) % count];
			}
		}
		return 0;
	}

	// -- toggle the foreground window's keyboard layout to the next one
	//    (RU <-> EN). used when the F8 menu option is enabled. note: like any
	//    WM_INPUTLANGCHANGEREQUEST approach it works in GUI apps but not in
	//    the classic console.
	void toggleKeyboardLayout()
	{
		HWND hwnd = ::GetForegroundWindow();
		if (!hwnd) {
			log("toggleKeyboardLayout: no foreground window");
			return;
		}

		DWORD threadId = ::GetWindowThreadProcessId(hwnd, nullptr);
		HKL current    = ::GetKeyboardLayout(threadId);
		HKL next       = nextLayout(current);
		if (!next) {
			log("toggleKeyboardLayout: <2 layouts installed");
			return;
		}

		::PostMessageW(hwnd, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(next));

		char buf[128];
		sprintf_s(buf, "toggleKeyboardLayout: current=0x%04X next=0x%04X",
			static_cast<unsigned>(LOWORD(current)), static_cast<unsigned>(LOWORD(next)));
		log(buf);
	}

	// send a single keystroke via scancode SendInput
	static void sendKey(WORD vk)
	{
		BYTE scan = ::MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
		INPUT down = {};
		down.type = INPUT_KEYBOARD; down.ki.wVk = vk; down.ki.wScan = scan; down.ki.dwFlags = KEYEVENTF_SCANCODE;
		::SendInput(1, &down, sizeof(INPUT));
		INPUT up = {};
		up.type = INPUT_KEYBOARD; up.ki.wVk = vk; up.ki.wScan = scan; up.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
		::SendInput(1, &up, sizeof(INPUT));
	}

	// send Shift+Left N times to re-select N characters after a paste.
	// cursor sits at the end of the pasted text, so we select backwards.
	// works in any GUI app since it goes through the physical keyboard.
	static void selectLeft(int n)
	{
		INPUT down = {};
		down.type = INPUT_KEYBOARD; down.ki.wVk = VK_SHIFT; down.ki.dwFlags = KEYEVENTF_SCANCODE;
		down.ki.wScan = ::MapVirtualKeyA(VK_SHIFT, MAPVK_VK_TO_VSC);
		::SendInput(1, &down, sizeof(INPUT));

		for (int i = 0; i < n; ++i) {
			sendKey(VK_LEFT);
		}

		INPUT up = {};
		up.type = INPUT_KEYBOARD; up.ki.wVk = VK_SHIFT; up.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
		up.ki.wScan = ::MapVirtualKeyA(VK_SHIFT, MAPVK_VK_TO_VSC);
		::SendInput(1, &up, sizeof(INPUT));
	}

	// -- the main action: fix selected text ---------------------------
	//    Invoked by the global hotkey. The user never leaves the target app,
	//    so keyboard focus stays inside it; a real Ctrl+C / Ctrl+V therefore
	//    lands in the right window with no focus-juggling at all.
	void fixSelectedText()
	{
		HWND target = ::GetForegroundWindow();
		logHwnd("fixSelectedText: target=", target);
		wchar_t cls[128] = {};
		if (target and ::GetClassNameW(target, cls, 128)) {
			char cbuf[256];
			::WideCharToMultiByte(CP_UTF8, 0, cls, -1, cbuf, 256, nullptr, nullptr);
			char msg[320];
			sprintf_s(msg, "fixSelectedText: target class='%s'", cbuf);
			log(msg);
		}

		char buf[128];
		DWORD targetTid = target ? ::GetWindowThreadProcessId(target, nullptr) : 0;
		sprintf_s(buf, "fixSelectedText: target thread id=%lu", targetTid);
		log(buf);

		// COPY phase: clear the clipboard, then send a scancode Ctrl+C. If the
		// clipboard ends up non-empty, a real selection was copied; if it stays
		// empty, focus/selection is not on an editable control -> abort without
		// pasting.

		// save whatever was on the clipboard before we stomp it, so we can
		// put it back after the paste. dropped on the early-return paths
		// (empty selection / no change) -- nothing to restore there.
		m_savedClipboard = cst::winapi::ClipboardManager::GetText();

		clearClipboard();
		sendCtrlKey('C');
		::Sleep(180);

		std::wstring text = readClipboard();
		sprintf_s(buf, "fixSelectedText: clipboard after Ctrl+C len=%zu", text.size());
		log(buf);

		if (text.empty()) {
			log("fixSelectedText: clipboard empty after Ctrl+C, nothing to fix");
			return;
		}

		// determine the direction from the ACTIVE keyboard layout of the
		// target window: the text was typed in the active layout (from), and
		// it should be transliterated into the NEXT layout in the switching
		// cycle (to). this scales to any number of installed layouts.
		HKL targetLayout = targetTid ? ::GetKeyboardLayout(targetTid) : 0;
		int from = languageOfLayout(targetLayout);
		int to   = languageOfLayout(nextLayout(targetLayout));

		sprintf_s(buf, "fixSelectedText: from=%d to=%d", from, to);
		log(buf);

		if (from < 0 || to < 0) {
			log("fixSelectedText: layout(s) not present in layout.txt, aborting");
			return;
		}

		std::wstring fixed = kf::LayoutMap::transliterate(text, from, to);

		// log actual contents for diagnosis (UTF-8 so cyrillic survives)
		char inbuf[2048] = {};
		::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, inbuf, 2048, nullptr, nullptr);
		char inmsg[2200];
		sprintf_s(inmsg, "fixSelectedText: INPUT_TEXT='%s'", inbuf);
		log(inmsg);

		char outbuf[2048] = {};
		::WideCharToMultiByte(CP_UTF8, 0, fixed.c_str(), -1, outbuf, 2048, nullptr, nullptr);
		char outmsg[2200];
		sprintf_s(outmsg, "fixSelectedText: FIXED_TEXT='%s'", outbuf);
		log(outmsg);

		if (fixed == text) {
			log("fixSelectedText: no change needed");
			return;
		}

		// put transliterated text on clipboard
		writeClipboard(fixed);
		log("fixSelectedText: transliterated text written to clipboard");

		// PASTE phase: real Ctrl+V. focus is still in the target app, so the
		// keystroke replaces the selection with the fixed text.
		sendCtrlKey('V');
		::Sleep(120);   // let the target app finish inserting before re-selecting
		log("fixSelectedText: SendInput Ctrl+V sent");

		// re-select the pasted text via keyboard: Shift+Left N times from
		// cursor (which sits at the end of the pasted text). works everywhere
		// since it goes through the physical keyboard, just like Ctrl+C/V.
		if (m_keepSelection) {
			selectLeft(static_cast<int>(fixed.size()));
			log("fixSelectedText: selection re-highlighted via keyboard");
		}

		// put the user's original clipboard contents back
		cst::winapi::ClipboardManager::SetText(m_savedClipboard);
		log("fixSelectedText: original clipboard restored");

		log("fixSelectedText: DONE");
	}

	// -- window procedure ---------------------------------------------
	LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		// system tray messages (right-click menu only; action is on hotkey)
		if (TrayIcon::hasMessage(uMsg)) {
			if (TrayIcon::hasID(wParam)) {
				if (lParam == WM_RBUTTONUP or lParam == WM_CONTEXTMENU) {
					if (!TrayIcon::menu.show()) { return 0; }
				}
				return 0;
			}
		}

		switch (uMsg)
		{
		case WM_CREATE:
			return 0;

		case WM_HOTKEY:
			log("--- HOTKEY received ---");
			fixSelectedText();
			if (m_layoutToggle) {
				toggleKeyboardLayout();
			}
			return 0;

		case WM_DESTROY:
			log("WM_DESTROY received");
			::UnregisterHotKey(hWnd, 1);
			cst::winapi::WindowThemeProxy{}->disable_theme_support();
			if (m_bgBrush) {
				::DeleteObject(m_bgBrush);
				m_bgBrush = nullptr;
			}
			::PostQuitMessage(0);
			return 0;

		case WM_COMMAND:
		{
			WORD id = LOWORD(wParam);

			if (id == m_idToggleLayout) {
				m_layoutToggle = !m_layoutToggle;
				log(m_layoutToggle ? "Layout-toggle: ON" : "Layout-toggle: OFF");
				return 0;
			}
			if (id == m_idKeepSelection) {
				m_keepSelection = !m_keepSelection;
				log(m_keepSelection ? "Keep-selection: ON" : "Keep-selection: OFF");
				return 0;
			}
			if (id == m_idTrayExit) {
				::DestroyWindow(hWnd);
				return 0;
			}
			break;
		}

		default: break;
		}

		return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
	}

};  // class MainWindow
