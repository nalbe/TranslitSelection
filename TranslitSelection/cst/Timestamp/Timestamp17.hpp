// Timestamp17.hpp
#pragma once

// Standard library headers
#include <chrono>
#include <string>
#include <string_view>
#include <cstdio>
#include <ctime>



namespace cst
{

	// ======================================================================
	//  Timestamp17 - chrono wrapper with component decomposition
	// ======================================================================
	class Timestamp17
	{
		// -- type aliases ------------------------------------------------------
	public:
		using time_point = std::chrono::system_clock::time_point;

		// -- date/time components ----------------------------------------------
	public:
		struct Components
		{
			int       year;
			unsigned  month;
			unsigned  day;
			int       hour;
			int       minute;
			long long second;
			long long millisecond;

		};  // struct Components

		// -- members -----------------------------------------------------------
	private:
		time_point m_value;

		// -- epoch / millisecond utilities -------------------------------------
	public:
		[[nodiscard]] long long millis()                        const noexcept;
		[[nodiscard]] long long epoch_seconds()                 const noexcept;
		[[nodiscard]] auto truncate_to_seconds()                const noexcept;

		// -- date/time field accessors -----------------------------------------
	public:
		[[nodiscard]] int        year()                         const noexcept;
		[[nodiscard]] unsigned   month()                        const noexcept;
		[[nodiscard]] unsigned   day()                          const noexcept;
		[[nodiscard]] int        hour()                         const noexcept;
		[[nodiscard]] int        minute()                       const noexcept;
		[[nodiscard]] long long  second()                       const noexcept;
		[[nodiscard]] long long  millisecond()                  const noexcept;
		[[nodiscard]] time_point timepoint()                    const noexcept;

		// -- decomposition -----------------------------------------------------
	public:
		[[nodiscard]] Components components()                   const noexcept;
		[[nodiscard]] Components components_local()             const noexcept;
		[[nodiscard]] Components operator()(bool)               const noexcept;

		// -- formatted string --------------------------------------------------
	public:
		[[nodiscard]] std::string format_utc()                  const;
		[[nodiscard]] std::string format_local()                const;
		[[nodiscard]] std::string format_milliseconds(std::string_view)    const;
		[[nodiscard]] std::string format(std::string_view, bool = true) const;

		// -- comparison --------------------------------------------------------
	public:
		[[nodiscard]] bool operator==(const Timestamp17&)       const noexcept;
		[[nodiscard]] bool operator!=(const Timestamp17&)       const noexcept;
		[[nodiscard]] bool operator <(const Timestamp17&)       const noexcept;
		[[nodiscard]] bool operator<=(const Timestamp17&)       const noexcept;
		[[nodiscard]] bool operator >(const Timestamp17&)       const noexcept;
		[[nodiscard]] bool operator>=(const Timestamp17&)       const noexcept;

		[[nodiscard]] long long diff_millis(const Timestamp17&) const noexcept;

		// -- internals ---------------------------------------------------------
	private:
		Components from_tm(const std::tm&, long long)           const noexcept;
		Components decompose()                                  const noexcept;
		Components decompose_local()                            const noexcept;

		// -- lifecycle ---------------------------------------------------------
	public:
		explicit Timestamp17(time_point);
		explicit Timestamp17(long long);
		Timestamp17();
		Timestamp17(const Timestamp17&)                              = default;
		Timestamp17& operator=(const Timestamp17&)                   = default;
		Timestamp17(Timestamp17&&)                                   = default;
		Timestamp17& operator=(Timestamp17&&)                        = default;

	};  // class Timestamp17



	// -- alias -------------------------------------------------------------
	using Timestamp = Timestamp17;

}  // namespace cst




namespace cst
{

	/// -- epoch / millisecond utilities -------------------------------------

	// Returns milliseconds since epoch
	inline long long Timestamp17::millis() const noexcept
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(m_value.time_since_epoch()).count();
	}

	// Returns seconds since epoch
	inline long long Timestamp17::epoch_seconds() const noexcept
	{
		return std::chrono::duration_cast<std::chrono::seconds>(m_value.time_since_epoch()).count();
	}

	// Returns time_point truncated to seconds precision
	inline auto Timestamp17::truncate_to_seconds() const noexcept
	{
		return std::chrono::time_point_cast<std::chrono::seconds>(m_value);
	}


	/// -- date/time field accessors -----------------------------------------

	// extracts year component
	inline int Timestamp17::year() const noexcept
	{
		return decompose().year;
	}

	// extracts month component
	inline unsigned Timestamp17::month() const noexcept
	{
		return decompose().month;
	}

	// extracts day component
	inline unsigned Timestamp17::day() const noexcept
	{
		return decompose().day;
	}

	// extracts hour component
	inline int Timestamp17::hour() const noexcept
	{
		return static_cast<int>((epoch_seconds() / 3600) % 24);
	}

	// extracts minute component
	inline int Timestamp17::minute() const noexcept
	{
		return static_cast<int>((epoch_seconds() / 60) % 60);
	}

	// extracts second component
	inline long long Timestamp17::second() const noexcept
	{
		return epoch_seconds() % 60;
	}

	// extracts millisecond component
	inline long long Timestamp17::millisecond() const noexcept
	{
		return millis() % 1000;
	}

	// underlying time point
	inline std::chrono::system_clock::time_point Timestamp17::timepoint() const noexcept
	{
		return m_value;
	}


	/// -- decomposition -----------------------------------------------------

	// decompose components as UTC time
	inline Timestamp17::Components Timestamp17::components() const noexcept
	{
		return decompose();
	}

	// decompose components as local time
	inline Timestamp17::Components Timestamp17::components_local() const noexcept
	{
		return decompose_local();
	}

	// decompose components
	inline Timestamp17::Components Timestamp17::operator()(bool local) const noexcept
	{
		return local ? decompose_local() : decompose();
	}


	/// -- formatted string --------------------------------------------------

	// format timestamp as UTC time
	inline std::string Timestamp17::format_utc() const
	{
		const auto c = decompose();
		char buf[20];
		std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u %02d:%02d:%02lld",
			c.year, c.month, c.day, c.hour, c.minute, c.second
		);
		return buf;
	}

	// format timestamp as local time
	inline std::string Timestamp17::format_local() const
	{
		const auto c = decompose_local();
		char buf[20];
		std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u %02d:%02d:%02lld",
			c.year, c.month, c.day, c.hour, c.minute, c.second
		);
		return buf;
	}

	// format milliseconds
	std::string Timestamp17::format_milliseconds(std::string_view fmt) const
	{
		std::string fmt_str(fmt);
		long long ms = millisecond();

		size_t buf_size = std::max<size_t>(fmt_str.size() * 2, 32);
		std::string result;

		while (true) {
			result.resize(buf_size);
			int written = std::snprintf(
				result.data(), buf_size, fmt_str.c_str(), ms
			);
			if (written >= 0 and static_cast<size_t>(written) < buf_size) {
				result.resize(written);
				break;
			}
			buf_size *= 2;
			if (buf_size > 1024) {  // 1KB cap
				return {};
			}
		}
		return result;
	}

	// formats timestamp using custom format string
	inline std::string Timestamp17::format(std::string_view fmt, bool local) const
	{
		auto get_tm = [&]() {
			std::time_t t = static_cast<std::time_t>(epoch_seconds());
			std::tm tm{};

			if (local) {
#ifdef _WIN32
				_localtime64_s(&tm, &t);  // MSVC/MinGW thread-safe variant
#else
				localtime_r(&t, &tm);  // POSIX thread-safe variant
#endif
			}
			else {
#ifdef _WIN32
				_gmtime64_s(&tm, &t);
#else
				gmtime_r(&t, &tm);
#endif
			}
			return tm;
		};

		const auto tm = get_tm();
		std::string fmt_str(fmt);  // copy to make sure it's null-terminated
		size_t buf_size = std::max<size_t>(fmt_str.size() * 4, 64);
		std::string result;

		while (true) {
			result.resize(buf_size);
			size_t written = std::strftime(
				result.data(), buf_size, fmt_str.c_str(), &tm
			);
			if (written != 0) {
				result.resize(written);
				break;
			}
			buf_size *= 2;
			if (buf_size > 1024 * 1024) {  // 1MB cap
				return {};
			}
		}
		return result;
	}


	/// -- comparison --------------------------------------------------------

	// equality
	inline bool Timestamp17::operator==(const Timestamp17& other) const noexcept
	{
		return m_value == other.m_value;
	}

	// inequality
	inline bool Timestamp17::operator!=(const Timestamp17& other) const noexcept
	{
		return m_value != other.m_value;
	}

	// less than
	inline bool Timestamp17::operator<(const Timestamp17& other) const noexcept
	{
		return m_value < other.m_value;
	}

	// less than or equal
	inline bool Timestamp17::operator<=(const Timestamp17& other) const noexcept
	{
		return m_value <= other.m_value;
	}

	// greater than
	inline bool Timestamp17::operator>(const Timestamp17& other) const noexcept
	{
		return m_value > other.m_value;
	}

	// greater than or equal
	inline bool Timestamp17::operator>=(const Timestamp17& other) const noexcept
	{
		return m_value >= other.m_value;
	}


	// Returns difference between timestamps in milliseconds
	inline long long Timestamp17::diff_millis(const Timestamp17& other) const noexcept
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(m_value - other.m_value).count();
	}


	/// -- internals ---------------------------------------------------------

	// Helper function to decomposes internal time_point into components using local time zone
	inline Timestamp17::Components Timestamp17::from_tm(const std::tm& tm, long long fractional_ms) const noexcept
	{
		return {
				tm.tm_year + 1900,
				static_cast<unsigned>(tm.tm_mon + 1),
				static_cast<unsigned>(tm.tm_mday),
				tm.tm_hour,
				tm.tm_min,
				static_cast<long long>(tm.tm_sec),
				fractional_ms
		};
	}

	// Decomposes internal time_point into components as UTC
	inline Timestamp17::Components Timestamp17::decompose() const noexcept
	{
		using namespace std::chrono;
		const auto ms_tp = floor<milliseconds>(m_value);
		const auto secs = duration_cast<seconds>(ms_tp.time_since_epoch());
		const auto fractional_ms = (ms_tp.time_since_epoch() - secs).count();

		std::time_t t = secs.count();
		std::tm tm{};
#ifdef _WIN32
		_gmtime64_s(&tm, &t);
#else
		gmtime_r(&t, &tm);
#endif
		return from_tm(tm, fractional_ms);
	}

	// Decomposes internal time_point into components as local
	inline Timestamp17::Components Timestamp17::decompose_local() const noexcept
	{
		using namespace std::chrono;
		const auto ms_tp = floor<milliseconds>(m_value);
		const auto secs = duration_cast<seconds>(ms_tp.time_since_epoch());
		const auto fractional_ms = (ms_tp.time_since_epoch() - secs).count();

		std::time_t t = secs.count();
		std::tm tm{};
#ifdef _WIN32
		_localtime64_s(&tm, &t);
#else
		localtime_r(&t, &tm);
#endif
		return from_tm(tm, fractional_ms);
	}


	/// -- lifecycle ---------------------------------------------------------

	// Constructs from time_point truncated to milliseconds
	inline Timestamp17::Timestamp17(std::chrono::system_clock::time_point tp) :
		m_value{ std::chrono::floor<std::chrono::milliseconds>(tp) }
	{}

	// Constructs from milliseconds since epoch
	inline Timestamp17::Timestamp17(long long millis) :
		Timestamp17{ std::chrono::system_clock::time_point{ std::chrono::milliseconds{ millis } } }
	{}

	// Constructs timestamp with current system time
	inline Timestamp17::Timestamp17() :
		Timestamp17{ std::chrono::system_clock::now() }
	{}


}  // namespace cst




/*
	
	// Formats the timestamp as '2026-04-10 03:27:09.925' format
	std::string formatWithMillis() const noexcept
	{
		return std::format("{:%F %T}.{:03d}",
			m_timestamp.truncate_to_seconds(),
			m_timestamp.millisecond()
		);
	}

*/



