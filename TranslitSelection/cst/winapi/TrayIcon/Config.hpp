// Config.hpp
#pragma once

// Implementation-specific headers
#include "cst/winapi/WinHandles/WinHandles.hpp"

// Standard library headers
#include <string.h>
#include <functional>

// Windows system headers
#include <Windows.h>
#include <shellapi.h>



namespace cst::winapi::detail::TrayIcon
{

	// ============================================================================
	//  Config - TrayIcon class parameters
	// ============================================================================
	struct Config
	{
		// -- members -------------------------------------------------------------
	public:
		std::function<HMENU()> menuFn;
		NOTIFYICONDATA  nid{};
		IconArrayHandle icons{};
		UINT            menuFlags{};
		bool            immediate{};

		// -- chainable field setters ---------------------------------------------
	public:
		Config& withHandle(HWND)                        noexcept;
		Config& withID(UINT)                            noexcept;
		Config& withCallbackMessage(UINT)               noexcept;
		Config& withIconHandle(HICON)                   noexcept;
		Config& withFlags(UINT)                         noexcept;
		Config& withTooltip(const wchar_t*)             noexcept;
		Config& withState(DWORD)                        noexcept;
		Config& withStateMask(DWORD)                    noexcept;
		Config& withVersion(UINT)                       noexcept;
		Config& withNotifyIconData(NOTIFYICONDATA&&)    noexcept;
		Config& withIcons(IconArrayHandle&&)            noexcept;
		Config& withIcons(HICON*, size_t, bool = true)  noexcept;
		Config& withIcon(HICON, bool = true)            noexcept;
		Config& withMenu(std::function<HMENU()>)        noexcept;
		Config& withMenuFlags(UINT)                     noexcept;
		Config& withImmediateCreation()                 noexcept;
		Config& withDeferredCreation()                  noexcept;
		Config&& operator()()                           noexcept;

	};  // struct Config

}  // namespace cst::winapi::detail::TrayIcon



namespace cst::winapi::detail::TrayIcon
{

	/// -- chainable field setters ---------------------------------------------

	// add nid handle
	inline Config& Config::withHandle(HWND hWnd) noexcept
	{
		nid.hWnd = hWnd;
		return *this;
	}

	// add nid ID
	inline Config& Config::withID(UINT uID) noexcept
	{
		nid.uID = uID;
		return *this;
	}

	// add nid callback message
	inline Config& Config::withCallbackMessage(UINT uMsg) noexcept
	{
		nid.uCallbackMessage = uMsg;
		return *this;
	}

	// add raw nid icon
	inline Config& Config::withIconHandle(HICON hIcon) noexcept
	{
		nid.hIcon = hIcon;
		return *this;
	}

	// add nid flags
	inline Config& Config::withFlags(UINT uFlags) noexcept
	{
		nid.uFlags = uFlags;
		return *this;
	}

	// add nid tooltip
	inline Config& Config::withTooltip(const wchar_t* szTip) noexcept
	{
		wcsncpy_s(nid.szTip, szTip, _TRUNCATE);
		return *this;
	}

	// add nid state
	inline Config& Config::withState(DWORD dwState) noexcept
	{
		nid.dwState = dwState;
		return *this;
	}

	// add nid state mask
	inline Config& Config::withStateMask(DWORD dwStateMask) noexcept
	{
		nid.dwStateMask = dwStateMask;
		return *this;
	}

	// add nid version
	inline Config& Config::withVersion(UINT uVersion) noexcept
	{
		nid.uVersion = uVersion;
		return *this;
	}

	// add icon array wrapper
	inline Config& Config::withIcons(IconArrayHandle&& hIcons) noexcept
	{
		icons = std::move(hIcons);
		return *this;
	}

	// add icon array wrapper from handle array
	inline Config& Config::withIcons(HICON* pIcons, size_t nSize, bool takeOwnership) noexcept
	{
		icons = { pIcons, {nSize, takeOwnership} };
		return *this;
	}

	// add icon wrapper from handle
	inline Config& Config::withIcon(HICON hIcon, bool takeOwnership) noexcept
	{
		icons = { new HICON[1]{ hIcon }, {1, takeOwnership} };
		return *this;
	}

	// add menu function
	inline Config& Config::withMenu(std::function<HMENU()> fn) noexcept
	{
		menuFn = std::move(fn);
		return *this;
	}

	// add nid
	inline Config& Config::withNotifyIconData(NOTIFYICONDATA&& notifyIconData) noexcept
	{
		nid = std::move(notifyIconData);
		return *this;
	}

	// add menu flags
	inline Config& Config::withMenuFlags(UINT uFlags) noexcept
	{
		menuFlags = uFlags;
		return *this;
	}

	// create icon immediately
	inline Config& Config::withImmediateCreation() noexcept
	{
		immediate = true;
		return *this;
	}

	// create icon later
	inline Config& Config::withDeferredCreation() noexcept
	{
		immediate = false;
		return *this;
	}

	// lval -> rval conversion
	inline Config&& Config::operator()() noexcept
	{
		return std::move(*this);
	}


};  // namespace cst::winapi::detail::TrayIcon



