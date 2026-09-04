// BalloonNotifier.hpp
#pragma once

// Implementation-specific headers
#include "cst/FormattedText/FormattedText.hpp"

// Standard library headers
#include <string>

// Windows system headers
#include <Windows.h>
#include <shellapi.h>



namespace cst::winapi::detail::TrayIcon
{

	// ==========================================================================
	//  BalloonNotifier - notify manager for a system tray icon
	// ==========================================================================
	class BalloonNotifier
	{
		// -- aliases -----------------------------------------------------------
	public:
		using Header = FormattedText<64>;
		using Body   = FormattedText<256>;

		// -- members -----------------------------------------------------------
	private:
		NOTIFYICONDATA& m_nid;

		// -- balloons ----------------------------------------------------------
	public:
		bool show        (const Header&, const Body&, DWORD = NIIF_NOSOUND, UINT = 2000) const noexcept;
		bool showError   (const Header&, const Body&, DWORD = NIIF_NOSOUND, UINT = 3000) const noexcept;
		bool showWarning (const Header&, const Body&, DWORD = NIIF_NOSOUND, UINT = 2000) const noexcept;
		bool showInfo    (const Header&, const Body&, DWORD = NIIF_NOSOUND, UINT = 1000) const noexcept;

		// -- internals ---------------------------------------------------------
	private:
		bool showImpl(const Header&, const Body&, DWORD, UINT) const noexcept;

		// -- lifecycle ---------------------------------------------------------
	public:
		BalloonNotifier(NOTIFYICONDATA&)                   noexcept;
		BalloonNotifier(const BalloonNotifier&)            = delete;
		BalloonNotifier(BalloonNotifier&&)                 = delete;
		BalloonNotifier& operator=(const BalloonNotifier&) = delete;
		BalloonNotifier& operator=(BalloonNotifier&&)      = delete;
	};  // class BalloonNotifier

}  // namespace cst::winapi::detail::TrayIcon




namespace cst::winapi::detail::TrayIcon
{

	/// -- balloons ----------------------------------------------------------

	// show with explicit flags / timeout
	inline bool BalloonNotifier::show(const Header& header, const Body& body, DWORD flags, UINT timeout) const noexcept
	{
		return showImpl(header, body, flags, timeout);
	}

	// adds NIIF_ERROR to base flags
	inline bool BalloonNotifier::showError(const Header& header, const Body& body, DWORD flags, UINT timeout) const noexcept
	{
		return showImpl(header, body, NIIF_ERROR | flags, timeout);
	}

	// adds NIIF_WARNING to base flags
	inline bool BalloonNotifier::showWarning(const Header& header, const Body& body, DWORD flags, UINT timeout) const noexcept
	{
		return showImpl(header, body, NIIF_WARNING | flags, timeout);
	}

	// adds NIIF_INFO to base flags
	inline bool BalloonNotifier::showInfo(const Header& header, const Body& body, DWORD flags, UINT timeout) const noexcept
	{
		return showImpl(header, body, NIIF_INFO | flags, timeout);
	}


	/// -- internals ---------------------------------------------------------

	// performs the actual Shell_NotifyIcon call
	inline bool BalloonNotifier::showImpl(const Header& header, const Body& body, DWORD infoFlags, UINT timeout) const noexcept
	{
		NOTIFYICONDATA nid{};
		nid.cbSize = sizeof(NOTIFYICONDATA);
		nid.hWnd = m_nid.hWnd;
		nid.uID = m_nid.uID;

		nid.uFlags = NIF_INFO;
		nid.dwInfoFlags = infoFlags;
		nid.uTimeout = timeout;  // ignored on Vista+, kept for compatibility

		if (!IsEqualGUID(m_nid.guidItem, GUID_NULL)) {
			nid.guidItem = m_nid.guidItem;
			nid.uFlags |= NIF_GUID;
		}

		wcscpy_s(nid.szInfoTitle, _countof(nid.szInfoTitle), header);
		wcscpy_s(nid.szInfo, _countof(nid.szInfo), body);
		return Shell_NotifyIcon(NIM_MODIFY, &nid);
	}


	/// -- lifecycle ---------------------------------------------------------

	// constructor
	inline BalloonNotifier::BalloonNotifier(NOTIFYICONDATA& nid) noexcept :
		m_nid(nid)
	{}


}  // namespace cst::winapi::detail::TrayIcon



