// ErrorLevel.hpp
#pragma once

// Standard library headers
#include <string_view>
#include <cassert>
#include <type_traits>



namespace cst
{

	// ======================================================================
	//  ErrorLevel - lightweight severity level enumeration wrapper
	// ======================================================================
	class ErrorLevel
	{
		// -- underlying enum ---------------------------------------------------
	public:
		enum class Value : unsigned char
		{
			Debug = 0, Info = 1, Warning = 2, Error = 3, Critical = 4, Fatal = 5, _count
		};

		// -- convenience constants (mirror enum values) ------------------------
	public:
		inline static constexpr Value Debug    = Value::Debug;
		inline static constexpr Value Info     = Value::Info;
		inline static constexpr Value Warning  = Value::Warning;
		inline static constexpr Value Error    = Value::Error;
		inline static constexpr Value Critical = Value::Critical;
		inline static constexpr Value Fatal    = Value::Fatal;

		// -- members -----------------------------------------------------------
	private:
		Value m_value{ Debug };

		// -- non-modifiers -----------------------------------------------------
	public:
		constexpr Value value()                               const noexcept;
		constexpr bool valid()                                const noexcept;
		constexpr std::string_view to_string_view()           const noexcept;

		// -- conversion --------------------------------------------------------
	public:
		/* implicit */ constexpr operator unsigned()          const noexcept;
		explicit constexpr operator int()                     const noexcept;
		explicit constexpr operator std::size_t()             const noexcept;
		explicit constexpr operator unsigned char()           const noexcept;
		explicit constexpr operator Value()                   const noexcept;

		// -- free operators ----------------------------------------------------
	public:
		friend constexpr bool operator==(const ErrorLevel&, const ErrorLevel&);
		friend constexpr bool operator!=(const ErrorLevel&, const ErrorLevel&);
		friend constexpr bool operator <(const ErrorLevel&, const ErrorLevel&);
		friend constexpr bool operator >(const ErrorLevel&, const ErrorLevel&);

		// -- lifecycle ---------------------------------------------------------
	public:
		template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0
		> constexpr explicit ErrorLevel(T v)                                 noexcept;
		constexpr ErrorLevel()                                      noexcept;
		constexpr ErrorLevel(Value v)                               noexcept;
		~ErrorLevel()                                              = default;
		ErrorLevel(const ErrorLevel&)                              = default;
		ErrorLevel& operator=(const ErrorLevel&)                   = default;
		ErrorLevel(ErrorLevel&&)                                   = default;
		ErrorLevel& operator=(ErrorLevel&&)                        = default;

	};  // class ErrorLevel

}  // namespace cst



namespace cst
{

	/// -- non-modifiers -----------------------------------------------------

	// Returns the stored error level value
	constexpr ErrorLevel::Value ErrorLevel::value() const noexcept
	{
		return m_value;
	}

	// Checks whether the stored value is within the valid range
	constexpr bool ErrorLevel::valid() const noexcept
	{
		return  m_value < Value::_count;
	}

	// string view representation of the error level
	constexpr std::string_view ErrorLevel::to_string_view() const noexcept
	{
		switch (m_value)
		{
		case Value::Debug:    { return "Debug";    }
		case Value::Info:     { return "Info";     }
		case Value::Warning:  { return "Warning";  }
		case Value::Error:    { return "Error";    }
		case Value::Critical: { return "Critical"; }
		case Value::Fatal:    { return "Fatal";    }
		default:              { return "Unknown";  }
		}
	}


	/// -- conversion --------------------------------------------------------

	// Implicit converts the error level to an unsigned integer
	constexpr ErrorLevel::operator unsigned() const noexcept
	{
		return static_cast<unsigned>(m_value);
	}

	// Converts the error level to an integer
	constexpr ErrorLevel::operator int() const noexcept
	{
		return static_cast<int>(m_value);
	}

	// Converts the error level to a size_t
	constexpr ErrorLevel::operator std::size_t() const noexcept
	{
		return static_cast<std::size_t>(m_value);
	}

	// Converts the error level to an unsigned char
	constexpr ErrorLevel::operator unsigned char() const noexcept
	{
		return static_cast<unsigned char>(m_value);
	}

	// Converts the error level to a strongly typed enum
	constexpr ErrorLevel::operator Value() const noexcept
	{
		return m_value;
	}


	/// -- free operators ----------------------------------------------------

	// equality
	constexpr bool operator==(const ErrorLevel& lhs, const ErrorLevel& rhs)
	{
		return  lhs.value() == rhs.value();
	}

	// inequality
	constexpr bool operator!=(const ErrorLevel& lhs, const ErrorLevel& rhs)
	{
		return !(lhs == rhs);
	}

	// less than
	constexpr bool operator<(const ErrorLevel& lhs, const ErrorLevel& rhs)
	{
		return  static_cast<unsigned>(lhs) < static_cast<unsigned>(rhs);
	}

	// greater than
	constexpr bool operator>(const ErrorLevel& lhs, const ErrorLevel& rhs)
	{
		return  static_cast<unsigned>(lhs) > static_cast<unsigned>(rhs);
	}


	/// -- lifecycle ---------------------------------------------------------

	// constructs from an integral value with bounds checking
	template <typename T, std::enable_if_t<std::is_integral_v<T>, int>>
	constexpr ErrorLevel::ErrorLevel(T v) noexcept :
		m_value(v < static_cast<T>(Value::_count) ? static_cast<Value>(v) : Value::Debug)
	{}

	// constructs with default error level
	constexpr ErrorLevel::ErrorLevel() noexcept
	{}

	// constructs from a strongly typed enum value
	constexpr ErrorLevel::ErrorLevel(Value v) noexcept :
		m_value(v)
	{}


}  // namespace cst



