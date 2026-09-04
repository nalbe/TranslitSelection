// Window.hpp
#pragma once

// Implementation-specific headers
#include "Config.hpp"

// Standard library headers
#include <stdexcept>



namespace cst::winapi
{

	// ====================================================================
	//  Window - represents a window
	// ====================================================================
	class Window
	{
		// -- type aliases ----------------------------------------------------
	public:
		using Config = detail::Window::Config;

		// -- protected members -----------------------------------------------
	protected:
		Config    m_config;
		HWND      m_hWnd;
		HINSTANCE m_hInstance;

		// -- members ---------------------------------------------------------
	private:
		static inline thread_local int s_activeWindows{};

		// -- component management --------------------------------------------
	public:
		bool create(HINSTANCE = GetModuleHandle(nullptr), HWND = nullptr);
		void destroy() noexcept;
		int loop();

		// -- accessors -------------------------------------------------------
	public:
		HWND handle()     const noexcept;

		// -- internals -------------------------------------------------------
	private:
		static LRESULT CALLBACK s_WndProc(HWND, UINT, WPARAM, LPARAM);
		bool registerClass(HINSTANCE);
		bool unregisterClass()            noexcept;
		bool registered()                 const;

		// -- lifecycle -------------------------------------------------------
	public:
		explicit Window(Config&&);
		Window(const Window&)             = delete;
		Window& operator=(const Window&)  = delete;
		Window(Window&&)                  noexcept;
		Window& operator=(Window&&)       noexcept;

	};  // class Window

}  // namespace cst::winapi




namespace cst::winapi
{

	/// -- component management --------------------------------------------

	// creates the window
	inline bool Window::create(HINSTANCE hInst, HWND hParent)
	{
		// register the window class if not already registered
		if (!registerClass(hInst)) {
			return false;
		};
		// prevent multiple instances
		if (m_hWnd) {
			return false;
		}
		// check class parameters
		if (m_config.className.empty() or m_config.classTitle.empty()) {
			return false;
		}

		HMENU hMenu = (m_config.style & WS_CHILD)
			? reinterpret_cast<HMENU>(static_cast<UINT_PTR>( m_config.controlId ))
			: nullptr;

		m_hWnd = CreateWindowEx(
			m_config.exStyle,
			m_config.className.c_str(),
			m_config.classTitle.c_str(),
			m_config.style,
			m_config.rect.left, m_config.rect.top,
			m_config.rect.right - m_config.rect.left, m_config.rect.bottom - m_config.rect.top,
			hParent, hMenu, hInst, this
		);
		if (!m_hWnd) {
			return false;
		}

		// increment windows count on successful creation
		++s_activeWindows;

		return true;
	}

	// destroys the window
	inline void Window::destroy() noexcept
	{
		if (m_hWnd) {
			::DestroyWindow(m_hWnd);
		}
	}

	// runs the message loop
	inline int Window::loop()
	{
		if (!m_hWnd) {
			return -1;
		}

		MSG msg = {};
		while (GetMessage(&msg, nullptr, 0, 0))
		{
			::TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		return static_cast<int>(msg.wParam);
	}


	/// -- accessors -------------------------------------------------------

	// returns the window handle
	inline HWND Window::handle() const noexcept
	{
		return m_hWnd;
	}


	/// -- internals -------------------------------------------------------

	// static window procedure
	inline LRESULT CALLBACK Window::s_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (uMsg == WM_NCCREATE) {
			auto* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
			auto* pThis = static_cast<Window*>(pCreate->lpCreateParams);
			SetWindowLongPtr(hWnd, 0, reinterpret_cast<LONG_PTR>(pThis));
			pThis->m_hWnd = hWnd;
		}

		auto* pThis = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, 0));
		if (!pThis) {
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
		}

		switch (uMsg)
		{
		case WM_DESTROY: {
			pThis->m_config.messageHandler(hWnd, uMsg, wParam, lParam);
			--s_activeWindows;
			if (s_activeWindows == 0) {
				::PostQuitMessage(0);  // no more windows
			}
			return 0;
		}
		default: {
			return pThis->m_config.messageHandler(hWnd, uMsg, wParam, lParam);
		}}
	}

	// register window class
	inline bool Window::registerClass(HINSTANCE hInst)
	{
		if (registered()) {
			return true;
		}

		m_hInstance      = hInst;
		WNDCLASSEX wc    = {};
		wc.cbSize        = sizeof(wc);
		wc.lpfnWndProc   = s_WndProc;
		wc.hInstance     = hInst;
		wc.hIcon         = m_config.icon.get();
		wc.hCursor       = m_config.cursor.get();
		wc.hbrBackground = m_config.background.get();
		wc.lpszClassName = m_config.className.c_str();
		wc.style         = m_config.classStyle;
		wc.cbWndExtra    = sizeof(Window*);

		return RegisterClassEx(&wc);
	}

	// unregister window class
	inline bool Window::unregisterClass() noexcept
	{
		if (!m_hInstance) {
			return false;
		}
		return UnregisterClass(m_config.className.c_str(), m_hInstance);
	}

	// if the window class is already registered
	inline bool Window::registered() const
	{
		if (!m_hInstance) {
			return false;
		}
		WNDCLASSEX existing = { sizeof(WNDCLASSEX) };
		return GetClassInfoEx(
			m_hInstance, m_config.className.c_str(), &existing
		);
	}


	// -- lifecycle -------------------------------------------------------

	// constructor
	inline Window::Window(Config&& cfg) :
		m_config( std::move(cfg) ),
		m_hInstance(),
		m_hWnd()
	{
		if (m_config.immediate) {
			if (!create()) {
				throw std::runtime_error("Failed to create window");
			}
		}
	}

	// move constructor
	inline Window::Window(Window&& other) noexcept :
		m_hWnd(std::exchange(other.m_hWnd, nullptr)),
		m_hInstance(std::exchange(other.m_hInstance, nullptr)),
		m_config(std::move(other.m_config))
	{
		if (m_hWnd) {
			SetWindowLongPtr(m_hWnd, 0, reinterpret_cast<LONG_PTR>(this));
		}
	}

	// move assignment operator
	inline Window& Window::operator=(Window&& other) noexcept
	{
		if (this == &other) {
			return *this;
		}
		destroy();
		m_hWnd = std::exchange(other.m_hWnd, nullptr);
		m_hInstance = std::exchange(other.m_hInstance, nullptr);
		m_config = std::move(other.m_config);
		if (m_hWnd) {
			SetWindowLongPtr(m_hWnd, 0, reinterpret_cast<LONG_PTR>(this));
		}
		return *this;
	}


}  // namespace cst::winapi



/*
NOTES:

	// -- no need to delete ---------------------------------------------

	HICON hIcon = LoadIcon(
		hInstance, MAKEINTRESOURCE(IDI_ICON1)
		);

	HICON hIcon = (HICON)LoadImage(
		hInstance, MAKEINTRESOURCE(IDI_ICON1),
		IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED
		);


	// -- should delete --------------------------------------------------

	HICON hIcon = (HICON)LoadImage(
		hInstance, MAKEINTRESOURCE(IDI_ICON1),
		IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR
		);

	LoadIconWithScaleDown(
		hInstance, MAKEINTRESOURCE(IDI_ICON1),
		GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), &hIcon
		);

	LoadIconMetric(
		hInstance, MAKEINTRESOURCE(IDI_ICON1),
		LIM_LARGE, &hIcon
		);

	ExtractIconEx(
		L"shell32.dll", 0, hLarge, hSmall, 1
		);

	HICON hIcon = ExtractAssociatedIcon(hInstance, L"file.txt", NULL);

	SHGetFileInfo(
		L"C:\\", FILE_ATTRIBUTE_DIRECTORY,
		&sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON
		);

	HICON hIcon = CreateIconFromResource(pIconData, dataSize, TRUE, 0x00030000);

	HICON hCopy = CopyIcon(hExistingIcon);

*/



