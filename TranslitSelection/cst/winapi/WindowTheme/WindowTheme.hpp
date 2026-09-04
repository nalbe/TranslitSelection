// WindowTheme.hpp
#pragma once

// Standard library headers
#include <type_traits>

// Windows system headers
#include <Windows.h>
#include <tchar.h>
#include <versionhelpers.h>  // windows version checking helpers
#include <uxtheme.h>         // visual styles and theme support
#include <dwmapi.h>          // window composition
#include <commctrl.h>        // modern controls

// Library links
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "Comctl32.lib")



namespace cst::winapi
{

	// =====================================================================
	//  WindowTheme - window dark/light theme support (singleton)
	// =====================================================================
	class WindowTheme final
	{
		// -- internal types ---------------------------------------------------
	private:
		struct cbData { bool is_dark_theme, isError; };

		// -- type aliases -----------------------------------------------------
	private:
		using SetPreferredAppModeFn              = INT(WINAPI*)(INT);
		using RefreshImmersiveColorPolicyStateFn = VOID(WINAPI*)();
		using ShouldAppsUseDarkModeFn            = BOOL(WINAPI*)();

		// -- members ----------------------------------------------------------
	private:
		HMODULE                            hUxtheme                            {};
		SetPreferredAppModeFn              pfnSetPreferredAppMode              {};
		RefreshImmersiveColorPolicyStateFn pfnRefreshImmersiveColorPolicyState {};
		ShouldAppsUseDarkModeFn            pfnShouldAppsUseDarkMode            {};

		// -- internals --------------------------------------------------------
	private:
		static BOOL CALLBACK ChildrenThemeCallback(HWND, LPARAM);
		BOOL setMain(HWND, BOOL)           const;
		BOOL setChildren(HWND, BOOL)       const;
		BOOL setControl(HWND, BOOL)        const;

		// -- theme management -------------------------------------------------
	public:
		BOOL enable_theme_support();
		BOOL follow_system_theme(HWND);
		BOOL force(HWND, BOOL);
		BOOL force_dark_theme(HWND);
		BOOL force_light_theme(HWND);
		BOOL is_dark_theme()               const;
		BOOL is_light_theme()              const;
		void disable_theme_support();

		// -- lifecycle --------------------------------------------------------
	private:
		~WindowTheme();
		WindowTheme()                               = default;
		WindowTheme(const WindowTheme&)             = delete;
		WindowTheme& operator=(const WindowTheme&)  = delete;
		WindowTheme(WindowTheme&&)                  = delete;
		WindowTheme& operator=(WindowTheme&&)       = delete;

		// -- singleton --------------------------------------------------------
	public:
		static WindowTheme& Instance();

	};  // class WindowTheme



	// =====================================================================
	//  WindowThemeProxy - provides access to WindowTheme singleton
	// =====================================================================
	struct WindowThemeProxy final
	{
		WindowTheme* operator->() { return &WindowTheme::Instance(); }

	};  // struct WindowThemeProxy



	// =====================================================================
	//  WindowThemeMixin - provides theme support to derived classes
	// =====================================================================
	template <typename Derived>
	struct WindowThemeMixin
	{
		explicit WindowThemeMixin()
		{
			static_assert(
				std::is_same_v<decltype(std::declval<Derived&>().handle()), HWND>,
				"Derived must provide: HWND handle();"
			);
			WindowThemeProxy{}->enable_theme_support();
		}

		void force_dark_theme()
		{
			WindowThemeProxy{}->force(
				static_cast<const Derived*>(this)->handle(), true
			);
		}

		void force_light_theme()
		{
			WindowThemeProxy{}->force(
				static_cast<const Derived*>(this)->handle(), false
			);
		}

		void follow_system_theme()
		{
			WindowThemeProxy{}->follow_system_theme(
				static_cast<const Derived*>(this)->handle()
			);
		}

	};  // struct WindowThemeMixin

}  // namespace cst::winapi




// class WindowTheme
namespace cst::winapi
{

	/// -- internals --------------------------------------------------------

	// callback for EnumChildWindows to apply theme to each child control
	inline BOOL CALLBACK WindowTheme::ChildrenThemeCallback(HWND hWndChild, LPARAM lParam)
	{
		cbData& data = *reinterpret_cast<cbData*>(lParam);
		WindowTheme& instance = Instance();
		if (!instance.setControl(hWndChild, data.is_dark_theme)) {
			data.isError = true;
		}
		return TRUE;
	}

	// applies theme to the window frame using DWM
	inline BOOL WindowTheme::setMain(HWND hWnd, BOOL isDarkTheme) const
	{
		BOOL success = TRUE;
		if (IsWindows10OrGreater()) {
			HRESULT hr = DwmSetWindowAttribute(
				hWnd,
				DWMWA_USE_IMMERSIVE_DARK_MODE,
				&isDarkTheme,
				sizeof(isDarkTheme)
			);
			if (FAILED(hr)) { success = FALSE; }
		}
		else {
			// DWM attribute not applicable
		}
		return success;
	}

	// applies theme to all child windows of a parent
	inline BOOL WindowTheme::setChildren(HWND hParent, BOOL isDarkTheme) const
	{
		cbData data{ !!isDarkTheme, false };
		EnumChildWindows(hParent, ChildrenThemeCallback, reinterpret_cast<LPARAM>(&data));
		return !data.isError;  // true if no child theming failed
	}

	// applies Explorer-based theme to a single control and triggers refresh
	inline BOOL WindowTheme::setControl(HWND hControl, BOOL isDarkTheme) const
	{
		if (!hControl) {
			return FALSE;
		}

		HRESULT hr = SetWindowTheme(
			hControl,
			isDarkTheme ? L"DarkMode_Explorer" : L"Explorer",
			NULL  // apply to the whole control
		);

		if (SUCCEEDED(hr)) {
			SendMessage(hControl, WM_THEMECHANGED, 0, 0);
		}

		return SUCCEEDED(hr);
	}


	/// -- theme management -------------------------------------------------

	// initializes theming support
	inline BOOL WindowTheme::enable_theme_support()
	{
		if (hUxtheme) {
			return TRUE;
		}  // multiple run guard

		INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES };
		if (!InitCommonControlsEx(&icex)) {
			return FALSE;
		}

		hUxtheme = LoadLibraryEx(_T("uxtheme.dll"), NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
		if (!hUxtheme) {
			return FALSE;
		}

		pfnRefreshImmersiveColorPolicyState = reinterpret_cast<RefreshImmersiveColorPolicyStateFn
		> (GetProcAddress(hUxtheme, MAKEINTRESOURCEA(104)));
		pfnShouldAppsUseDarkMode = reinterpret_cast<ShouldAppsUseDarkModeFn
		> (GetProcAddress(hUxtheme, MAKEINTRESOURCEA(132)));
		pfnSetPreferredAppMode = reinterpret_cast<SetPreferredAppModeFn
		> (GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135)));

		if (pfnSetPreferredAppMode) {
			pfnSetPreferredAppMode(1);
		}

		if (pfnRefreshImmersiveColorPolicyState) {
			pfnRefreshImmersiveColorPolicyState();
		}

		return TRUE;
	}

	// applies system theme
	inline BOOL WindowTheme::follow_system_theme(HWND hWnd)
	{
		return force(hWnd, is_dark_theme());
	}

	// applies preferred theme
	inline BOOL WindowTheme::force(HWND hWnd, BOOL is_dark_theme)
	{
		if (!hWnd || !hUxtheme) {
			return FALSE;
		}

		// get the class name of the window to determine its type
		TCHAR szClassName[256];
		if (GetClassName(hWnd, szClassName, ARRAYSIZE(szClassName)) == 0) {
			return FALSE;
		}

		BOOL result;

		if (CompareStringOrdinal(szClassName, -1, TOOLTIPS_CLASS, -1, TRUE) == CSTR_EQUAL) {
			result = setControl(hWnd, is_dark_theme);
			// tooltips often need a repaint message after theming
			SendMessage(hWnd, TTM_UPDATETIPTEXT, 0, 0);
		}  // dispatch based on window type
		else {
			// theme the parent window's non-client area (frame)
			result = setMain(hWnd, is_dark_theme);

			// theme the parent window's child controls within the client area
			result &= setChildren(hWnd, is_dark_theme);

			// force a repaint for parent windows (or dialogs)
			InvalidateRect(hWnd, NULL, TRUE);  // invalidate the whole client area
			UpdateWindow(hWnd);                // ensure it repaints immediately
		}  // assume it's a main application window, dialog, or other parent/non-tooltip window

		return result;
	}

	// applies dark theme
	inline BOOL WindowTheme::force_dark_theme(HWND hWnd)
	{
		return force(hWnd, TRUE);
	}

	// applies light theme
	inline BOOL WindowTheme::force_light_theme(HWND hWnd)
	{
		return force(hWnd, FALSE);
	}

	// queries system dark mode state via uxtheme api
	inline BOOL WindowTheme::is_dark_theme() const
	{
		return pfnShouldAppsUseDarkMode
			? (pfnShouldAppsUseDarkMode() & 0xff)
			: FALSE;
	}

	// queries system dark mode state via uxtheme api
	inline BOOL WindowTheme::is_light_theme() const
	{
		return !is_dark_theme();
	}

	// releases theming resources
	inline void WindowTheme::disable_theme_support()
	{
		// free uxtheme.dll if loaded
		if (hUxtheme) {
			FreeLibrary(hUxtheme);
			hUxtheme = NULL;  // prevent double free
		}

		// reset function pointers
		pfnRefreshImmersiveColorPolicyState  = NULL;
		pfnShouldAppsUseDarkMode             = NULL;
		pfnSetPreferredAppMode               = NULL;
	}


	/// -- lifecycle --------------------------------------------------------

	// ensures theme support is properly disabled on destruction
	inline WindowTheme::~WindowTheme()
	{
		disable_theme_support();
	}


	/// -- singleton --------------------------------------------------------

	// returns singleton instance of ThemeManager
	inline WindowTheme& WindowTheme::Instance()
	{
		static WindowTheme instance{};
		return instance;
	}


}  // namespace cst::winapi




/*
HINT:

If the manifest is in a '.manifest' file (not as a resource), link it via project settings.
In Visual Studio:
	Project > Properties > Manifest Tool > Input and Output.
		Set Additional Manifest Files to include '.manifest' file.
		-> $(ProjectDir)TabTap.exe.manifest

*/
	
/*
Usage example:

	// call this sequence during initialization:
	WindowThemeProxy{}->enable_theme_support();     // add global dark mode policy
	CreateWindow(...);                              // create window AFTER setting policy
	WindowThemeProxy{}->follow_system_theme(hWnd);  // force initial theme apply
	WindowThemeProxy{}->disable_theme_support();    // optionally, disable theme support
	
*/



