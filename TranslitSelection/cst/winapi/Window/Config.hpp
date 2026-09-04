// Config.hpp
#pragma once

// Implementation-specific headers
#include "cst/winapi/WinHandles/WinHandles.hpp"

// Standard library headers
#include <memory>
#include <utility>
#include <string>
#include <functional>

// Windows system headers
#include <Windows.h>



namespace cst::winapi::detail::Window
{

	// ====================================================================
	//  Config - Window configuration parameters
	// ====================================================================
	struct Config
	{
		// -- class parameters ------------------------------------------------
	public:
		std::wstring className;
		std::wstring classTitle;
		DWORD        classStyle = {};

		// -- window parameters -----------------------------------------------
	public:
		IconHandle icon       = {};
		DWORD      style      = {};
		DWORD      exStyle    = {};
		RECT       rect       = {};
		bool       show       = {};
		UINT       controlId  = {};
		bool       immediate  = {};

		using MessageHandler = std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)>;
		MessageHandler messageHandler =
			[](HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
				return DefWindowProc(hWnd, uMsg, wParam, lParam);
			};

		// -- gdi resources ---------------------------------------------------
	public:
		CursorHandle cursor     = {};
		BrushHandle  background = {};

		// -- chainable field setters -----------------------------------------
	public:
		Config& withClassName(std::wstring&&)       noexcept;
		Config& withClassTitle(std::wstring&&)      noexcept;
		Config& withClassStyle(DWORD)               noexcept;
	public:
		Config& withIcon(HICON, bool = true)        noexcept;
		Config& withStyle(DWORD)                    noexcept;
		Config& withExStyle(DWORD)                  noexcept;
		Config& withRect(RECT)                      noexcept;
		Config& withShow()                          noexcept;
		Config& withControlId(UINT)                 noexcept;
		Config& withMessageHandler(MessageHandler)  noexcept;
		Config& withImmediateCreation()             noexcept;
		Config& withDeferredCreation()              noexcept;
	public:
		Config& withCursor(CursorHandle)            noexcept;
		Config& withBackground(BrushHandle)         noexcept;
		Config&& operator()()                       noexcept;

	};  // struct Config

}  // namespace cst::winapi::detail::Window




namespace cst::winapi::detail::Window
{

	// add class name
	inline auto Config::withClassName(std::wstring&& v) noexcept -> Config&
	{
		className = std::move(v);
		return *this;
	}

	// add class title
	inline auto Config::withClassTitle(std::wstring&& v) noexcept -> Config&
	{
		classTitle = std::move(v);
		return *this;
	}

	// add class style
	inline auto Config::withClassStyle(DWORD v) noexcept -> Config&
	{
		classStyle = v;
		return *this;
	}


	// add window icon
	inline auto Config::withIcon(HICON hIcon, bool del) noexcept -> Config&
	{
		icon = { hIcon, del };
		return *this;
	}

	// add window style
	inline auto Config::withStyle(DWORD value) noexcept -> Config&
	{
		style = value;
		return *this;
	}

	// add extended window style
	inline auto Config::withExStyle(DWORD value) noexcept -> Config&
	{
		exStyle = value;
		return *this;
	}

	// add window rect
	inline auto Config::withRect(RECT rc) noexcept -> Config&
	{
		rect = rc;
		return *this;
	}

	// show window
	inline auto Config::withShow() noexcept -> Config&
	{
		show = true;
		return *this;
	}

	// add control id
	inline auto Config::withControlId(UINT id) noexcept -> Config&
	{
		controlId = id;
		return *this;
	}

	// add message handler
	inline auto Config::withMessageHandler(MessageHandler handler) noexcept -> Config&
	{
		messageHandler = std::move(handler);
		return *this;
	}

	// create window immediately
	inline auto Config::withImmediateCreation() noexcept -> Config&
	{
		immediate = true;
		return *this;
	}

	// create window after creation
	inline auto Config::withDeferredCreation() noexcept -> Config& {
		immediate = false;
		return *this;
	}

	// add cursor
	inline auto Config::withCursor(CursorHandle hCursor) noexcept -> Config&
	{
		cursor = std::move(hCursor);
		return *this;
	}

	// add background
	inline auto Config::withBackground(BrushHandle hBrush) noexcept -> Config&
	{
		background = std::move(hBrush);
		return *this;
	}

	// lval -> rval conversion
	inline auto Config::operator()() noexcept -> Config&&
	{
		return std::move(*this);
	}


}  // namespace cst::winapi::detail::Window



