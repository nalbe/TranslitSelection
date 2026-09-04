// UniqID.hpp
#pragma once

// Standard library headers
#include <cassert>
#include <utility>  // exchange



namespace cst
{

	// =====================================================================
	//  UniqID - unique identifier generator
	// =====================================================================
	class UniqID final
	{
	public:
		using self_type  = UniqID;
		using value_type = unsigned;
		using size_type  = value_type;

	private:
		static inline size_type s_counter = 1024;  // WM_USER
		value_type m_value;

	public:
		operator value_type() const noexcept
		{
			assert(m_value != 0 and
				"Use of moved or initialization order fiasco");
			return m_value;
		}

		UniqID() :
			m_value(s_counter++)
		{}

		UniqID(UniqID&& other) noexcept :
			m_value(std::exchange(other.m_value, 0))
		{}

		UniqID& operator=(UniqID&& other) noexcept
		{
			if (this != &other) {
				m_value = std::exchange(other.m_value, 0);
			}
			return *this;
		}

		UniqID(const UniqID&)            = delete;
		UniqID& operator=(const UniqID&) = delete;

	};  // class UniqID



	// =====================================================================
	//  UniqIDMixin - inheritable unique identifier
	// =====================================================================
	class UniqIDMixin
	{
	public:
		UniqID id;

	};  // class UniqIDMixin

}  // namespace cst



