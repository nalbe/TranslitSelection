// IconProxy.hpp
#pragma once

// Implementation-specific headers
#include "cst/winapi/WinHandles/WinHandles.hpp"

// Standard library headers
#include <utility>

// Windows system headers
#include <Windows.h>
#include <shellapi.h>



namespace cst::winapi::detail::TrayIcon
{

	// ========================================================================
	//  IconProxy - manages a collection of tray icons
	// ========================================================================
	class IconProxy
	{
		using self_type = IconProxy;

		// -- members -------------------------------------------------------------
	private:
		static constexpr size_t npos = SIZE_MAX;
		NOTIFYICONDATA&  m_nid;
		IconArrayHandle& m_hIcons;
		size_t m_idx;

		// -- setters -------------------------------------------------------------
	public:
		self_type& add(IconArrayHandle&&)                 noexcept;
		self_type& set(HICON)                             noexcept;
		self_type& set()                                  noexcept;
		self_type& select(size_t)                         noexcept;
		self_type& next(size_t, size_t)                   noexcept;
		self_type& next()                                 noexcept;
		self_type& reset()                                noexcept;
		bool update()                                     noexcept;

		// -- queries -------------------------------------------------------------
	public:
		size_t count()                              const noexcept;
		size_t index()                              const noexcept;
		HICON handle(size_t)                        const noexcept;
		HICON handle()                              const noexcept;
		HICON operator[](size_t)                    const noexcept;

		// -- lifecycle -----------------------------------------------------------
	public:
		IconProxy(NOTIFYICONDATA&, IconArrayHandle&)      noexcept;
		IconProxy(IconProxy&&)                            = delete;
		IconProxy& operator=(IconProxy&&)                 = delete;
		IconProxy(const IconProxy&)                       = delete;
		IconProxy& operator=(const IconProxy&)            = delete;

	};  // class IconProxy

}  // namespace cst::winapi::detail::TrayIcon




namespace cst::winapi::detail::TrayIcon
{

	/// -- setters -------------------------------------------------------------

	// sets the provided icon collection
	inline auto IconProxy::add(IconArrayHandle&& icons) noexcept -> self_type&
	{
		m_hIcons = std::move(icons);
		reset();
		return *this;
	}

	// sets the provided icon
	inline auto IconProxy::set(HICON hIcon) noexcept -> self_type&
	{
		m_idx = npos;
		m_nid.hIcon = hIcon;
		return *this;
	}

	// add the current icon to a shared system icon
	inline auto IconProxy::set() noexcept -> self_type&
	{
		return set(::LoadIcon(nullptr, IDI_APPLICATION));
	}

	// add the current icon by index
	inline auto IconProxy::select(size_t index) noexcept -> self_type&
	{
		if (index >= count()) {
			return *this;
		}
		m_idx = index;
		m_nid.hIcon = m_hIcons[m_idx];
		return *this;
	}

	// advance to the next icon within the given range [begin, end)
	inline auto IconProxy::next(size_t begin, size_t end) noexcept -> self_type&
	{
		if (count() == 0) {
			return *this;
		}
		if (begin >= end or end > count()) {
			return *this;
		}

		if (m_idx == npos or m_idx < begin or m_idx >= end) {
			m_idx = begin;
		}
		else if (++m_idx >= end) {
			m_idx = begin;
		}

		return select(m_idx);
	}

	// advance to the next icon within the given range [begin, end)
	inline auto IconProxy::next() noexcept -> self_type&
	{
		return next(0, count());
	}

	// first icon from the managed array, or a shared system icon
	inline auto IconProxy::reset() noexcept -> self_type&
	{
		if (count() > 0) {
			return select(0);
		}
		return set();
	}

	// update the icon
	inline bool IconProxy::update() noexcept
	{
		return ::Shell_NotifyIcon(NIM_MODIFY, &m_nid);
	}


	/// -- queries -------------------------------------------------------------

	// get the number of icons
	inline size_t IconProxy::count() const noexcept
	{
		return m_hIcons.get_deleter().count;
	}

	// get the current icon index
	inline size_t IconProxy::index() const noexcept
	{
		return m_idx;
	}

	// get an icon by index
	inline HICON IconProxy::handle(size_t index) const noexcept
	{
		return index < count()
			? m_hIcons[index]
			: nullptr;
	}

	// get current icon
	inline HICON IconProxy::handle() const noexcept
	{
		return m_nid.hIcon;
	}

	// overload to get an icon by index
	inline HICON IconProxy::operator[](size_t index) const noexcept
	{
		return handle(index);
	}


	/// -- lifecycle -----------------------------------------------------------

	// constructor
	inline IconProxy::IconProxy(NOTIFYICONDATA& nid, IconArrayHandle& icons) noexcept :
		m_nid(nid),
		m_hIcons(icons),
		m_idx(npos)
	{}


}  //  namespace cst::winapi::detail::TrayIcon



