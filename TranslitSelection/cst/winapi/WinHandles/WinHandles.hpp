// WinHandles.hpp
#pragma once

// Standard library headers
#include <type_traits>
#include <memory>

// Windows system headers
#include <Windows.h>



namespace cst::winapi::detail
{

	//  _HandleDeleter - deleter for Win32 GDI / User objects
	template <typename Handle, auto DestroyFn>
	struct _HandleDeleter
	{
		bool own;
		_HandleDeleter(bool b = true) : own(b) {}
		void operator()(Handle h) const noexcept { if (own and h) DestroyFn(h); }

	};  // struct _HandleDeleter


	//  _HandleArrayDeleter - stateful deleter for an array of handles
	template <typename Handle, auto DestroyFn>
	struct _HandleArrayDeleter
	{
		size_t count;
		bool own;

		_HandleArrayDeleter(size_t n, bool b = true) noexcept : count(n), own(b) {}
		_HandleArrayDeleter() : count(0), own(false) {}

		void operator()(Handle* p) const noexcept {
			if (own and p) {
				for (size_t i{}; i < count; ++i) {
					if (p[i] != nullptr) {
						DestroyFn(p[i]);
					}
				}
				delete[] p;
			}
		}

	};  // struct _HandleArrayDeleter


	//  _DCReleaser - deleter for DCs
	struct _DCReleaser
	{
		HWND wnd;
		void operator()(HDC h) const noexcept { if (wnd and h) { ReleaseDC(wnd, h); } }
	};


	//  _ComDeleter - deleter for COM interfaces
	template <typename Interface>
	struct _ComDeleter
	{
		void operator()(Interface* p) const noexcept { if (p) { p->Release(); } }

	};  // struct _ComDeleter



	/// == aliases for convenience ==============================================
	template <typename H, auto Del
	> using ResourceHandle       = std::unique_ptr< std::remove_pointer_t<H>, _HandleDeleter<H, Del> >;
	template <typename H, auto Del
	> using ResourceArrayHandle  = std::unique_ptr< H[], _HandleArrayDeleter<H, Del>                 >;

}  // namespace cst::winapi::detail




namespace cst::winapi
{

	/// -- user resources ------------------------------------------------------

	using CursorHandle        = detail::ResourceHandle< HCURSOR,  DestroyCursor >;
	using MenuHandle          = detail::ResourceHandle< HMENU,    DestroyMenu   >;
	using IconHandle          = detail::ResourceHandle< HICON,    DestroyIcon   >;

	// -- user resource arrays -------------------------------------------------

	using IconArrayHandle     = detail::ResourceArrayHandle< HICON,   DestroyIcon   >;
	using CursorArrayHandle   = detail::ResourceArrayHandle< HCURSOR, DestroyCursor >;

	/// -- gdi resources -------------------------------------------------------

	using BrushHandle         = detail::ResourceHandle< HBRUSH,   DeleteObject >;
	using PenHandle           = detail::ResourceHandle< HPEN,     DeleteObject >;
	using FontHandle          = detail::ResourceHandle< HFONT,    DeleteObject >;
	using BitmapHandle        = detail::ResourceHandle< HBITMAP,  DeleteObject >;
	using RegionHandle        = detail::ResourceHandle< HRGN,     DeleteObject >;
	using PaletteHandle       = detail::ResourceHandle< HPALETTE, DeleteObject >;

	/// -- gdi resource arrays -------------------------------------------------

	using PenArrayHandle      = detail::ResourceArrayHandle< HPEN,     DeleteObject >;
	using BrushArrayHandle    = detail::ResourceArrayHandle< HBRUSH,   DeleteObject >;
	using FontArrayHandle     = detail::ResourceArrayHandle< HFONT,    DeleteObject >;
	using BitmapArrayHandle   = detail::ResourceArrayHandle< HBITMAP,  DeleteObject >;
	using RegionArrayHandle   = detail::ResourceArrayHandle< HRGN,     DeleteObject >;
	using PaletteArrayHandle  = detail::ResourceArrayHandle< HPALETTE, DeleteObject >;

	/// -- gdi resources (DC) --------------------------------------------------

	using DCHandle            = detail::ResourceHandle< HDC, DeleteDC >;
	using WindowDCHandle      = std::unique_ptr< std::remove_pointer_t<HDC>, detail::_DCReleaser >;
	template <typename Interface
	> using ComHandle         = std::unique_ptr< Interface, detail::_ComDeleter<Interface> >;
	template <typename T
	> using ObjectHandle      = std::unique_ptr<std::remove_pointer_t<T>>;

}  // namespace cst::winapi



