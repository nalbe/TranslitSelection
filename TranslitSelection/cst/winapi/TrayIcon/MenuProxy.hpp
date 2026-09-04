// MenuProxy.hpp
#pragma once

// Implementation-specific headers
#include "cst/winapi/WinHandles/WinHandles.hpp"

// Standard library headers
#include <functional>

// Windows system headers
#include <Windows.h>
#include <shellapi.h>



namespace cst::winapi::detail::TrayIcon
{

	// ========================================================================
	//  MenuProxy - manages the tray menu
	// ========================================================================
	class MenuProxy
	{
		// -- members -------------------------------------------------------------
	private:
		HWND& m_hWnd;
		UINT& m_uFlags;
		std::function<HMENU()>& m_menuProvider;

		// -- modifiers -----------------------------------------------------------
	public:
		bool add(std::function<HMENU()>)                  noexcept;
		void flags(UINT)                                  noexcept;

		// -- non-modifiers -------------------------------------------------------
	public:
		bool show()                                 const noexcept;
		bool hasMenu()                              const noexcept;
		MenuHandle handle()                         const noexcept;

		// -- lifecycle -----------------------------------------------------------
	public:
		MenuProxy(HWND&, std::function<HMENU()>&, UINT&)  noexcept;
		MenuProxy(MenuProxy&&)                            = delete;
		MenuProxy& operator=(MenuProxy&&)                 = delete;
		MenuProxy(const MenuProxy&)                       = delete;
		MenuProxy& operator=(const MenuProxy&)            = delete;

	};  // class MenuProxy

}  // namespace cst::winapi::detail::TrayIcon




namespace cst::winapi::detail::TrayIcon
{

	// -- modifiers -----------------------------------------------------------

	// add menu
	inline bool MenuProxy::add(std::function<HMENU()> fn) noexcept
	{
		m_menuProvider = std::move(fn);
		return true;
	}

	// add menu flags
	inline void MenuProxy::flags(UINT uFlags) noexcept
	{
		m_uFlags = uFlags;
	}


	// -- non-modifiers -------------------------------------------------------

	// show menu
	inline bool MenuProxy::show() const noexcept
	{
		if (!hasMenu()) {
			return false;
		}
		POINT pt;
		if (!::GetCursorPos(&pt)) {
			return false;
		}
		::SetForegroundWindow(m_hWnd);
		::TrackPopupMenu(handle().get(), m_uFlags, pt.x, pt.y, 0, m_hWnd, nullptr);
		::PostMessageW(m_hWnd, WM_NULL, 0, 0);
		return true;
	}

	// check if menu handle is present
	inline bool MenuProxy::hasMenu() const noexcept
	{
		return m_menuProvider.operator bool();
	}

	// retrieve menu handle
	inline MenuHandle MenuProxy::handle() const noexcept
	{
		return { m_menuProvider(), true };
	}


	/// -- lifecycle -----------------------------------------------------------

	// constructor
	inline MenuProxy::MenuProxy(HWND& hWnd, std::function<HMENU()>& fn, UINT& uFlags) noexcept :
		m_hWnd(hWnd),
		m_menuProvider(fn),
		m_uFlags(uFlags)
	{}


}  // namespace cst::winapi::detail::TrayIcon



