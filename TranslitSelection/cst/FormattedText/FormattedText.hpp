// FormattedText.hpp
#pragma once

// Standard library headers
#include <utility>      // std::forward
#include <array>        // std::array
#include <type_traits>  // std::is_same_v
#include <cstdio>       // _snprintf_s / _snwprintf_s
#include <memory>       // std::hash



namespace cst
{

	// =====================================================================
	//  BasicFormattedText - stack-allocated fixed-size string with printf-style formatting
	// =====================================================================
	template <typename CharT, size_t N>
	class BasicFormattedText
	{
		// -- compile-time assertions ------------------------------------------
		static_assert(N > 0, "Buffer size must be positive");

		// -- members ----------------------------------------------------------
	private:
		std::array<CharT, N> m_buffer{};
		bool m_truncated{};

		// -- query ------------------------------------------------------------
	public:
		operator const CharT* ()            const noexcept;
		const CharT* c_str()                const noexcept;
		size_t length()                     const noexcept;
		bool truncated()                    const noexcept;
		static constexpr size_t capacity()  noexcept;

		// -- modifiers --------------------------------------------------------
	public:
		void clear()                        noexcept;
		void assign(const CharT*)           noexcept;

		// -- lifecycle --------------------------------------------------------
	public:
		BasicFormattedText()                                      = default;
		~BasicFormattedText()                                     = default;
		BasicFormattedText(const BasicFormattedText&)             = default;
		BasicFormattedText(BasicFormattedText&&)                  = default;
		BasicFormattedText& operator=(const BasicFormattedText&)  = default;
		BasicFormattedText& operator=(BasicFormattedText&&)       = default;
		template <typename... Args>
		BasicFormattedText(const CharT*, Args&&...)                noexcept;

	};  // class BasicFormattedText

}  // namespace cst




namespace cst
{
	/// -- query ------------------------------------------------------------

	// Implicit conversion for WinAPI compatibility
	template <typename CharT, size_t N>
	BasicFormattedText<CharT, N>::operator const CharT*() const noexcept
	{
		return c_str();
	}

	// Raw pointer access
	template <typename CharT, size_t N>
	const CharT* BasicFormattedText<CharT, N>::c_str() const noexcept
	{
		return m_buffer.data();
	}

	// Current string length
	template <typename CharT, size_t N>
	size_t BasicFormattedText<CharT, N>::length() const noexcept
	{
		const CharT* p = m_buffer.data();
		size_t len{};
		while (*p++) { ++len; }
		return len;
	}

	// Whether the last formatting operation resulted in truncation
	template <typename CharT, size_t N>
	bool BasicFormattedText<CharT, N>::truncated() const noexcept
	{
		return m_truncated;
	}

	// Total compile-time capacity
	template <typename CharT, size_t N>
	constexpr size_t BasicFormattedText<CharT, N>::capacity() noexcept
	{
		return N;
	}


	/// -- modifiers --------------------------------------------------------

	// Clear contents
	template <typename CharT, size_t N>
	void BasicFormattedText<CharT, N>::clear() noexcept
	{
		m_buffer[0] = static_cast<CharT>(0);
		m_truncated = false;
	}

	// Assign a plain string (may truncate silently)
	template <typename CharT, size_t N>
	void BasicFormattedText<CharT, N>::assign(const CharT* str) noexcept
	{
		if (!str) {
			clear();
			return;
		}

		size_t len{};
		const CharT* p = str;
		while (*p++) { ++len; }

		size_t copyLen = (len < N - 1) ? len : (N - 1);

		for (size_t i{}; i < copyLen; ++i) {
			m_buffer[i] = str[i];
		}
		m_buffer[copyLen] = static_cast<CharT>(0);
		// truncation flag is not updated by assign (keeps previous state)
	}


	// -- lifecycle --------------------------------------------------------

	// Variadic formatting constructor
	template <typename CharT, size_t N>
	template <typename... Args>
	BasicFormattedText<CharT, N>::BasicFormattedText(const CharT* format, Args&&... args) noexcept
	{
		if (!format) {
			if constexpr (std::is_same_v<CharT, wchar_t>) { format = L""; }
			else { format = ""; }
		}

		int result = -1;
		if constexpr (std::is_same_v<CharT, char>) {
			result = _snprintf_s(m_buffer.data(), N, _TRUNCATE, format, std::forward<Args>(args)...);
		}
		else if constexpr (std::is_same_v<CharT, wchar_t>) {
			result = _snwprintf_s(m_buffer.data(), N, _TRUNCATE, format, std::forward<Args>(args)...);
		}
		else {
			static_assert(sizeof(CharT) == 0, "Unsupported character type");
		}

		m_truncated = (result == -1);  // -1 indicates truncation (or error)
	}


}  // namespace cst



// ------------------------------------------------------------------------
//  Comparison operators (allow different sizes, same character type)
// ------------------------------------------------------------------------

template <typename CharT, size_t N, size_t M>
bool operator==(const cst::BasicFormattedText<CharT, N>& lhs,
	const cst::BasicFormattedText<CharT, M>& rhs)
{
	const CharT* l = lhs.c_str();
	const CharT* r = rhs.c_str();
	while (*l && (*l == *r)) { ++l; ++r; }
	return (*l == *r);
}

template <typename CharT, size_t N, size_t M>
bool operator<(const cst::BasicFormattedText<CharT, N>& lhs,
	const cst::BasicFormattedText<CharT, M>& rhs)
{
	const CharT* l = lhs.c_str();
	const CharT* r = rhs.c_str();
	while (*l && (*l == *r)) { ++l; ++r; }
	return (*l < *r);
}

template <typename CharT, size_t N, size_t M>
bool operator!=(const cst::BasicFormattedText<CharT, N>& lhs,
	const cst::BasicFormattedText<CharT, M>& rhs)
{
	return !(lhs == rhs);
}

template <typename CharT, size_t N, size_t M>
bool operator>(const cst::BasicFormattedText<CharT, N>& lhs,
	const cst::BasicFormattedText<CharT, M>& rhs)
{
	return rhs < lhs;
}

template <typename CharT, size_t N, size_t M>
bool operator<=(const cst::BasicFormattedText<CharT, N>& lhs,
	const cst::BasicFormattedText<CharT, M>& rhs)
{
	return !(rhs < lhs);
}

template <typename CharT, size_t N, size_t M>
bool operator>=(const cst::BasicFormattedText<CharT, N>& lhs,
	const cst::BasicFormattedText<CharT, M>& rhs)
{
	return !(lhs < rhs);
}



// ------------------------------------------------------------------------
//  Hash support for unordered containers
// ------------------------------------------------------------------------
namespace std
{
	template <typename CharT, size_t N>
	struct hash<cst::BasicFormattedText<CharT, N>>
	{
		size_t operator()(const cst::BasicFormattedText<CharT, N>& s) const noexcept
		{
			size_t h{};
			for (const CharT* p = s.c_str(); *p; ++p) {
				h = h * 31 + static_cast<size_t>(*p);
			}
			return h;
		}
	};

}  // namespace std



// ------------------------------------------------------------------------
//  Convenience alias for TCHAR-based usage (backward compatibility)
// ------------------------------------------------------------------------
namespace cst
{

#ifdef UNICODE
	template <size_t N>
	using FormattedText = BasicFormattedText<wchar_t, N>;
#else
	template <size_t N>
	using FormattedText = BasicFormattedText<char, N>;
#endif

template <size_t N> using FormattedTextA = BasicFormattedText<char, N>;
template <size_t N> using FormattedTextW = BasicFormattedText<wchar_t, N>;

}  // namespace cst




/*
Usage example:

	using Header = FormattedText<64>;
	using Body = FormattedText<256>;

	auto foo = [](Header header, Body body){};
	foo(
		{ _T("...") },
		{ _T("...") }
	);

*/



