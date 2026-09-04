// Timestamp20.hpp
#pragma once

// Standard library headers
#include <chrono>
#include <string>
#include <string_view>
#include <format>



namespace cst
{

	// ======================================================================
	//  Timestamp20 - chrono wrapper with component decomposition
	// ======================================================================
	class Timestamp20
	{
		// -- type aliases ------------------------------------------------------
	public:
		using time_point = std::chrono::system_clock::time_point;

		// -- decomposed date/time components -----------------------------------
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
		[[nodiscard]] constexpr long long millis()           const noexcept;
		[[nodiscard]] constexpr long long epoch_seconds()    const noexcept;
		[[nodiscard]] constexpr auto truncate_to_seconds()   const noexcept;

		// -- date/time field accessors -----------------------------------------
	public:
		[[nodiscard]] constexpr int       year()             const noexcept;
		[[nodiscard]] constexpr unsigned  month()            const noexcept;
		[[nodiscard]] constexpr unsigned  day()              const noexcept;
		[[nodiscard]] constexpr int       hour()             const noexcept;
		[[nodiscard]] constexpr int       minute()           const noexcept;
		[[nodiscard]] constexpr long long second()           const noexcept;
		[[nodiscard]] constexpr long long millisecond()      const noexcept;
		[[nodiscard]] constexpr time_point timepoint()       const noexcept;

		// -- decomposition -----------------------------------------------------
	public:
		[[nodiscard]] constexpr Components components()      const noexcept;
		[[nodiscard]] Components components_local()          const noexcept;
		[[nodiscard]] constexpr Components operator()(bool)  const noexcept;

		// -- formatted string --------------------------------------------------
	public:
		[[nodiscard]] std::string format_utc()               const;
		[[nodiscard]] std::string format_local()             const;
		[[nodiscard]] std::string format(std::string_view, bool = true) const;

		// -- non-modifiers -----------------------------------------------------
	public:
		[[nodiscard]] constexpr auto operator<=>(const Timestamp20&)       const noexcept = default;
		[[nodiscard]] constexpr long long diff_millis(const Timestamp20&)  const noexcept;

		// -- internals ---------------------------------------------------------
	private:
		constexpr Components from_ms_timepoint(const auto&)  const noexcept;
		constexpr Components decompose()                     const noexcept;
		Components decompose_local()                         const noexcept;

		// -- lifecycle ---------------------------------------------------------
	public:
		explicit constexpr Timestamp20(time_point)                 noexcept;
		explicit constexpr Timestamp20(long long)                  noexcept;
		Timestamp20();
		Timestamp20(const Timestamp20&)                           = default;
		Timestamp20& operator=(const Timestamp20&)                = default;
		Timestamp20(Timestamp20&&)                                = default;
		Timestamp20& operator=(Timestamp20&&)                     = default;

	};  // class Timestamp20



	// -- alias -------------------------------------------------------------
	using Timestamp = Timestamp20;

}  // namespace cst



// std::format support
template<>
struct std::formatter<cst::Timestamp> :
	std::formatter<std::chrono::system_clock::time_point>
{
	auto format(const cst::Timestamp& ts, std::format_context& ctx) const
	{
		return std::formatter<std::chrono::system_clock::time_point>::format(ts.timepoint(), ctx);
	}
};




namespace cst
{

	/// -- epoch / millisecond utilities -------------------------------------

	// milliseconds since epoch
	constexpr long long Timestamp20::millis() const noexcept
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(m_value.time_since_epoch()).count();
	}

	// seconds since epoch
	constexpr long long Timestamp20::epoch_seconds() const noexcept
	{
		return std::chrono::duration_cast<std::chrono::seconds>(m_value.time_since_epoch()).count();
	}

	// time point truncated to seconds precision
	constexpr auto Timestamp20::truncate_to_seconds() const noexcept
	{
		return std::chrono::time_point_cast<std::chrono::seconds>(m_value);
	}


	/// -- date/time field accessors -----------------------------------------

	// year component
	constexpr int Timestamp20::year() const noexcept
	{
		return int(std::chrono::year_month_day{ std::chrono::floor<std::chrono::days>(m_value) }.year());
	}

	// month component
	constexpr unsigned Timestamp20::month() const noexcept
	{
		return unsigned(std::chrono::year_month_day{ std::chrono::floor<std::chrono::days>(m_value) }.month());
	}

	// day component
	constexpr unsigned Timestamp20::day() const noexcept
	{
		return unsigned(std::chrono::year_month_day{ std::chrono::floor<std::chrono::days>(m_value) }.day());
	}

	// hour component
	constexpr int Timestamp20::hour() const noexcept
	{
		using namespace std::chrono;
		const auto tp = floor<milliseconds>(m_value);
		const auto d = floor<days>(tp);
		return int(duration_cast<hours>(tp - d).count());
	}

	// minute component
	constexpr int Timestamp20::minute() const noexcept
	{
		using namespace std::chrono;
		const auto tp = floor<milliseconds>(m_value);
		const auto d = floor<days>(tp);
		const auto h = duration_cast<hours>(tp - d);
		return int(duration_cast<minutes>(tp - d - h).count());
	}

	// second component
	constexpr long long Timestamp20::second() const noexcept
	{
		using namespace std::chrono;
		const auto tp = floor<milliseconds>(m_value);
		const auto d = floor<days>(tp);
		const auto h = duration_cast<hours>(tp - d);
		const auto m = duration_cast<minutes>(tp - d - h);
		return long long(duration_cast<seconds>(tp - d - h - m).count());
	}

	// millisecond component
	constexpr long long Timestamp20::millisecond() const noexcept
	{
		using namespace std::chrono;
		const auto tp = floor<milliseconds>(m_value);
		const auto d = floor<days>(tp);
		const auto h = duration_cast<hours>(tp - d);
		const auto m = duration_cast<minutes>(tp - d - h);
		const auto s = duration_cast<seconds>(tp - d - h - m);
		return long long(duration_cast<milliseconds>(tp - d - h - m - s).count());
	}

	// underlying time point
	constexpr std::chrono::system_clock::time_point Timestamp20::timepoint() const noexcept
	{
		return m_value;
	}


	/// -- decomposition -----------------------------------------------------

	// decompose components as UTC time
	constexpr Timestamp20::Components Timestamp20::components() const noexcept
	{
		return decompose();
	}

	// decompose components as local time
	inline Timestamp20::Components Timestamp20::components_local() const noexcept
	{
		return decompose_local();
	}

	// decompose components
	constexpr Timestamp20::Components Timestamp20::operator()(bool local) const noexcept
	{
		return local ? decompose_local() : decompose();
	}


	/// -- formatted string --------------------------------------------------

	// formats timestamp as UTC time
	inline std::string Timestamp20::format_utc() const
	{
		return std::format("{:%Y-%m-%d %H:%M:%S}", truncate_to_seconds());
	}

	// formats timestamp as local time
	inline std::string Timestamp20::format_local() const
	{
		const std::chrono::zoned_time zt{ std::chrono::current_zone(), m_value };
		return std::format("{:%Y-%m-%d %H:%M:%S}", zt);
	}

	// formats timestamp using custom format string (local or UTC)
	inline std::string Timestamp20::format(std::string_view fmt, bool local) const
	{
		if (local) {
			const std::chrono::zoned_time zt{ std::chrono::current_zone(), m_value };
			return std::vformat(fmt, std::make_format_args(zt));
		}
		const auto tp = std::chrono::floor<std::chrono::milliseconds>(m_value);
		return std::vformat(fmt, std::make_format_args(tp));
	}


	/// -- non-modifiers -----------------------------------------------------

	// difference between timestamps in milliseconds
	constexpr long long Timestamp20::diff_millis(const Timestamp20& other) const noexcept
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(m_value - other.m_value).count();
	}


	/// -- internals ---------------------------------------------------------

	// decompose internal time_point into date-time components
	constexpr Timestamp20::Components Timestamp20::from_ms_timepoint(const auto& ms_tp) const noexcept
	{
		auto days = std::chrono::floor<std::chrono::days>(ms_tp);
		std::chrono::year_month_day ymd{ days };
		std::chrono::hh_mm_ss hms{ ms_tp - days };

		return {
			.year = static_cast<int>(ymd.year()),
			.month = static_cast<unsigned>(ymd.month()),
			.day = static_cast<unsigned>(ymd.day()),
			.hour = hms.hours().count(),
			.minute = hms.minutes().count(),
			.second = hms.seconds().count(),
			.millisecond = hms.subseconds().count()
		};
	}

	// decompose internal time_point into date-time components
	constexpr Timestamp20::Components Timestamp20::decompose() const noexcept
	{
		const auto ms_tp = std::chrono::floor<std::chrono::milliseconds>(m_value);
		return from_ms_timepoint(ms_tp);
	}

	// decompose internal time_point into date-time components as local
	inline Timestamp20::Components Timestamp20::decompose_local() const noexcept
	{
		const std::chrono::zoned_time zt{ std::chrono::current_zone(), m_value };
		auto local_ms = std::chrono::floor<std::chrono::milliseconds>(zt.get_local_time());
		return from_ms_timepoint(local_ms);
	}


	/// -- lifecycle ---------------------------------------------------------

	// constructs from time point truncated to milliseconds
	constexpr Timestamp20::Timestamp20(std::chrono::system_clock::time_point tp) noexcept :
		m_value{ std::chrono::floor<std::chrono::milliseconds>(tp) }
	{}

	// constructs from milliseconds since epoch
	constexpr Timestamp20::Timestamp20(long long millis) noexcept :
		Timestamp20{ std::chrono::system_clock::time_point{ std::chrono::milliseconds{ millis } } }
	{}

	// constructs timestamp with current system time
	inline Timestamp20::Timestamp20() :
		Timestamp{ std::chrono::system_clock::now() }
	{}


}  // namespace cst



