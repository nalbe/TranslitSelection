// MainWindow.h - TranslitSelection tray-only layout fixer
#pragma once

// Implementation-specific headers
#include "cst/winapi/Window/Window.hpp"
#include "cst/winapi/WindowTheme/WindowTheme.hpp"
#include "cst/winapi/TrayIcon/TrayIcon.hpp"
#include "cst/FormattedText/FormattedText.hpp"
#include "cst/UniqID/UniqID.hpp"
#include "KeyboardHook.hpp"
#include "Transliterator.hpp"
#include "resource.h"

// Standard library headers
#include <string>
#include <fstream>
#include <utility>

// Windows system headers
#include <Windows.h>


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

	explicit MainWindow(bool enableLogging) :
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
			.withIcon              ( ::LoadIconW(m_hInstance, MAKEINTRESOURCE(IDI_ICON1)), false)
			.withHandle            ( handle()                )
			.withMenu              ( [this]() -> HMENU { return buildMenu(); } )
			.withMenuFlags         ( TPM_LEFTALIGN | TPM_RIGHTBUTTON )
			.withImmediateCreation ()
			(/* end of TrayIcon config */)
		),
		m_hook                 ( handle(), &Transliterator::isOwnKey /* end of KeyboardHook */ ),
		m_translit             ( [this](const char* line) { logLine(line); } /* end of Transliterator */ )
	{
		const std::wstring dir = moduleDir();

		openLog(dir, enableLogging);
		log("=== TranslitSelection starting ===");
		log("self HWND=%p", static_cast<void*>(handle()));

		cst::winapi::WindowThemeProxy{}->follow_system_theme(handle());

		m_hook.install();
		m_translit.load(dir);
	}


private:
	enum MenuCommand : UINT
	{
		cmdToggleLayout = 1,
		cmdKeepSelection,
		cmdExit
	};

	bool m_layoutToggle = true;    // from tray menu checkbox
	bool m_keepSelection = false;  // re-select pasted text after translate

	std::ofstream m_logFile;

	// log lines are stamped with ms since startup: the hook round-trip
	// delays are the interesting part of the timings
	const ULONGLONG m_startedAt = ::GetTickCount64();

	// swallows the owned keys and posts them back to us
	KeyboardHook m_hook;

	// the action behind every one of those keys
	Transliterator m_translit;


	// -- logging -------------------------------------------------------

	template <typename... Args>
	void log(const char* fmt, Args&&... args)
	{
		const cst::FormattedTextA<4096> body{ fmt, std::forward<Args>(args)... };
		logLine(body.c_str());
	}

	// only the window thread logs: the hook thread reports through messages
	void logLine(const char* line)
	{
		if (!m_logFile.is_open()) {
			return;
		}
		m_logFile << '[' << (::GetTickCount64() - m_startedAt) << "] "
			<< line << std::endl;
	}

	// the folder holding the exe: layout.txt and translit.log live there
	static std::wstring moduleDir()
	{
		wchar_t path[MAX_PATH] = {};
		::GetModuleFileNameW(nullptr, path, MAX_PATH);
		std::wstring dir(path);
		const size_t slash = dir.find_last_of(L"\\/");
		return (slash == std::wstring::npos) ? dir : dir.substr(0, slash + 1);
	}

	void openLog(const std::wstring& dir, bool enable)
	{
		if (!enable) {
			return;
		}
		m_logFile.open(dir + L"translit.log", std::ios::app);
		if (!m_logFile.is_open()) {
			::OutputDebugStringW(L"TranslitSelection: cannot open translit.log\n");
		}
	}

	HMENU buildMenu()
	{
		HMENU menu = ::CreatePopupMenu();
		::AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, Transliterator::gestureHint().c_str());
		::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
		::AppendMenuW(menu, MF_STRING | (m_layoutToggle ? MF_CHECKED : MF_UNCHECKED),
			cmdToggleLayout, L"Switch layout after translit");
		::AppendMenuW(menu, MF_STRING | (m_keepSelection ? MF_CHECKED : MF_UNCHECKED),
			cmdKeepSelection, L"Keep selection after translit");
		::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
		::AppendMenuW(menu, MF_STRING, cmdExit, L"Exit");
		return menu;
	}


	// -- window procedure ---------------------------------------------

	LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		// system tray messages (right-click menu only; action is on hotkey)
		if (TrayIcon::hasMessage(uMsg) and TrayIcon::hasID(wParam)) {
			if (lParam == WM_RBUTTONUP or lParam == WM_CONTEXTMENU) {
				TrayIcon::menu.show();
			}
			return 0;
		}

		switch (uMsg)
		{
		case WM_CREATE:
			return 0;

		case KeyboardHook::m_msgKey:
			m_translit.run(static_cast<BYTE>(wParam), m_layoutToggle, m_keepSelection);
			return 0;

		case KeyboardHook::m_msgInstallFailed:
			log("keyboard hook FAILED (err=%lu): the owned keys reach the focused app", wParam);
			return 0;

		case WM_DESTROY:
			log("WM_DESTROY received");
			m_hook.uninstall();
			cst::winapi::WindowThemeProxy{}->disable_theme_support();
			::PostQuitMessage(0);
			return 0;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case cmdToggleLayout:
				m_layoutToggle = !m_layoutToggle;
				log("Layout-toggle: %s", m_layoutToggle ? "ON" : "OFF");
				return 0;

			case cmdKeepSelection:
				m_keepSelection = !m_keepSelection;
				log("Keep-selection: %s", m_keepSelection ? "ON" : "OFF");
				return 0;

			case cmdExit:
				::DestroyWindow(hWnd);
				return 0;

			default: break;
			}
			break;

		default: break;
		}

		return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
	}

};  // class MainWindow