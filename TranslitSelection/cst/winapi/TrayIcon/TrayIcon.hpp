// TrayIcon.hpp
#pragma once

// Implementation-specific headers
#include "Config.hpp"
#include "BalloonNotifier.hpp"
#include "IconProxy.hpp"
#include "MenuProxy.hpp"

// Standard library headers
#include <type_traits>
#include <utility>
#include <stdexcept>



namespace cst::winapi
{

	// =============================================================================
	//  TrayIcon - wrapper for Shell_NotifyIcon
	// =============================================================================
	class TrayIcon
	{
		// -- type aliases ------------------------------------------------------------
	public:
		using self_type = TrayIcon;
		using Config    = detail::TrayIcon::Config;

		// -- protected members ---------------------------------------------------
	protected:
		Config m_config;

		// -- nested controls -----------------------------------------------------
	public:
		detail::TrayIcon::IconProxy       icon;
		detail::TrayIcon::MenuProxy       menu;
		detail::TrayIcon::BalloonNotifier balloon;

		// -- shell operations ----------------------------------------------------
	public:
		bool create(HWND = nullptr)           noexcept;
		bool update()                         noexcept;
		void remove()                         noexcept;
		bool setVersion()                     noexcept;

		// -- queries -------------------------------------------------------------
	public:
		bool hasID(WPARAM)              const noexcept;
		bool hasMessage(UINT)           const noexcept;

		// -- internals -----------------------------------------------------------
	private:
		bool call(DWORD)                      noexcept;

		// -- lifecycle -----------------------------------------------------------
	public:
		TrayIcon(Config&&);
		~TrayIcon()                           noexcept;
		TrayIcon(TrayIcon&&)                  noexcept;
		TrayIcon& operator=(TrayIcon&&)       noexcept;
		TrayIcon(const TrayIcon&)             = delete;
		TrayIcon& operator=(const TrayIcon&)  = delete;

	};  // class TrayIcon

} // namespace cst::winapi




namespace cst::winapi
{

	/// -- shell operations ----------------------------------------------------

	// create icon
	inline bool TrayIcon::create(HWND hWnd) noexcept
	{
		icon.reset();
		if (hWnd) {
			m_config.nid.hWnd = hWnd;
		}
		if (!call(NIM_ADD)) {
			return false;
		}
		return m_config.nid.uVersion
			? setVersion()
			: true;
	}

	// update icon
	inline bool TrayIcon::update() noexcept
	{
		return call(NIM_MODIFY);
	}

	// destroy icon
	inline void TrayIcon::remove() noexcept
	{
		call(NIM_DELETE);
	}

	// add version
	inline bool TrayIcon::setVersion() noexcept
	{
		return call(NIM_SETVERSION);
	}


	/// -- queries -------------------------------------------------------------

	// verify message id
	inline bool TrayIcon::hasID(WPARAM wParam) const noexcept
	{
		return wParam == m_config.nid.uID;
	}

	// verify message callback
	inline bool TrayIcon::hasMessage(UINT uMsg) const noexcept
	{
		return uMsg == m_config.nid.uCallbackMessage;
	}


	/// -- internals -----------------------------------------------------------

	// generic caller
	inline bool TrayIcon::call(DWORD msg) noexcept
	{
		return ::Shell_NotifyIcon(msg, &m_config.nid);
	}


	/// -- lifecycle -----------------------------------------------------------

	// constructor
	inline TrayIcon::TrayIcon(Config&& config) :
		m_config(std::move(config)()),
		icon(m_config.nid, m_config.icons),
		menu(m_config.nid.hWnd, m_config.menuFn, m_config.menuFlags),
		balloon(m_config.nid)
	{
		m_config.nid.cbSize = sizeof(NOTIFYICONDATA);

		if (m_config.immediate) {
			if (!create()) {
				throw std::runtime_error("Failed to create tray icon");
			}
		}
	}

	// destructor
	inline TrayIcon::~TrayIcon() noexcept
	{
		remove();
	}

	// move constructor
	inline TrayIcon::TrayIcon(TrayIcon&& other) noexcept :
		m_config(std::exchange(other.m_config, {})),
		icon(m_config.nid, m_config.icons),
		menu(m_config.nid.hWnd, m_config.menuFn, m_config.menuFlags),
		balloon(m_config.nid)
	{
		icon.select(other.icon.index());
	}

	// move assignment
	inline TrayIcon& TrayIcon::operator=(TrayIcon&& other) noexcept
	{
		if (this != &other) {
			m_config = std::exchange(other.m_config, {});
			icon.select(other.icon.index());
		}
		return *this;
	}


} // namespace cst::winapi



