// FormattedRecord.hpp
#pragma once

// Implementation-specific headers
#include "cst/ErrorLevel/ErrorLevel.hpp"
#include "cst/Timestamp/Timestamp.hpp"

// Standard library headers
#include <string>
#include <format>
#include <type_traits>
#include <source_location>



/// FormattedRecord
namespace cst
{

	// ==================================================================
	//  FormattedRecord - storage for log records
	// ==================================================================
	struct FormattedRecord
	{
	public:
		Timestamp   timestamp;  // timestamp
		ErrorLevel  level;      // severity level: info, warning, error, fatal, etc.
		std::string message;    // custom message

		// -- format --------------------------------------------------------
	public:
		[[nodiscard]] std::string format_prefix()      const;  // "YYYY-MM-DD HH:MM:SS.mmm  [level]  "
		[[nodiscard]] std::string format()             const;  // "YYYY-MM-DD HH:MM:SS.mmm  [level]  message "

		// -- modifiers -----------------------------------------------------
	public:
		template <typename... Args
		> void assign(std::string_view, Args&&...);
		template <typename... Args
		> void assign(std::string_view, std::tuple<Args...>&&);

		// -- operators -----------------------------------------------------
	public:
		bool operator==(const FormattedRecord&)        const;

		// -- lifecycle -----------------------------------------------------
	public:
		FormattedRecord();
		FormattedRecord(ErrorLevel);
		template <typename ...Args
		> FormattedRecord(ErrorLevel, std::string, Args&&...);
		template <typename ...Args
		> FormattedRecord(std::string, Args&&...);
		FormattedRecord(FormattedRecord&&)                   = default;
		FormattedRecord& operator=(FormattedRecord&&)        = default;
		FormattedRecord(const FormattedRecord&)              = default;
		FormattedRecord& operator=(const FormattedRecord&)   = default;

	};  // struct FormattedRecord




	/// -- format --------------------------------------------------------

	// format to: "YYYY-MM-DD HH:MM:SS.mmm  [level]  "
	inline std::string FormattedRecord::format_prefix() const
	{
		return std::format("{:%F %T}.{:03d}  [{}]  ",
			timestamp.truncate_to_seconds(),
			timestamp.millisecond(),
			level.to_string_view());
	}

	// format to: "YYYY-MM-DD HH:MM:SS.mmm  [level]  message "
	inline std::string FormattedRecord::format() const
	{
		return format_prefix() + message;
	}


	/// -- modifiers -----------------------------------------------------

	// assign formatted message
	template <typename... Args>
	void FormattedRecord::assign(std::string_view fmt, Args&&... args)
	{
		message.clear();
		if constexpr (sizeof...(Args) == 0) {
			message = fmt;
		}
		else {
			try {
				std::vformat_to(
					std::back_inserter(message),
					fmt,
					std::make_format_args(args...));
			}
			catch (const std::format_error& e) {
				message = std::string("<format error: ") + e.what() + ">";
			}
			catch (...) {
				message = "<unknown error in format>";
			}
		}
	}

	// assign formatted message
	template <typename... Args>
	void FormattedRecord::assign(std::string_view fmt, std::tuple<Args...>&& args)
	{
		message.clear();
		if constexpr (sizeof...(Args) == 0) {
			message = fmt;
		}
		else {
			try {
				std::apply(
					[&](auto&&... unpacked) {
						std::vformat_to(
							std::back_inserter(message),
							fmt,
							std::make_format_args(unpacked...));
					},
					std::move(args)
				);
			}
			catch (const std::format_error& e) {
				message = std::string("<format error: ") + e.what() + ">";
			}
			catch (...) {
				message = "<unknown error in format>";
			}
		}
	}


	/// -- operators -----------------------------------------------------

	// equality (exclude timestamp)
	inline bool FormattedRecord::operator==(const FormattedRecord& other) const
	{
		return
			level   == other.level    and
			message == other.message;
	}


	/// -- lifecycle -----------------------------------------------------

	// default constructor
	inline FormattedRecord::FormattedRecord() :
		timestamp(),
		level(),
		message()
	{}

	// construct with level
	inline FormattedRecord::FormattedRecord(ErrorLevel lvl) :
		timestamp(),
		level(lvl),
		message()
	{}

	// construct with level and formatted message
	template <typename ...Args>
	FormattedRecord::FormattedRecord(ErrorLevel lvl, std::string fmt, Args&&... args) :
		timestamp(),
		level(lvl),
		message()
	{
		assign(std::move(fmt), std::forward<Args>(args)...);
	}

	// construct with formatted message
	template <typename ...Args>
	FormattedRecord::FormattedRecord(std::string fmt, Args&&... args) :
		FormattedRecord(ErrorLevel::Debug, std::move(fmt), std::forward<Args>(args)...)
	{}


}  // namespace cst



/// ErrorMetadata
namespace cst 
{

	// ==================================================================
	//  ErrorMetadata - error metadata wrapper
	// ==================================================================
	struct ErrorMetadata
	{
		std::string source;  // origin: subsystem, module, adapter, or API name
		std::string code;    // identifier: system error code, string code, or application-specific code
		std::string what;    // human-readable decription

		// -- format --------------------------------------------------------
	public:
		std::string format_metadata()          const;  // "[source: {}] [code: {}] [what: {}] "

		// -- operators -----------------------------------------------------
	public:
		bool operator==(const ErrorMetadata&)  const = default;

	};  // struct ErrorMetadata




	/// -- format --------------------------------------------------------

	// format to: "[source: {}] [code: {}] [what: {}] "
	inline std::string ErrorMetadata::format_metadata() const
	{
		std::string result;

		if (!source.empty()) {
			result += "[source: " + source + "] ";
		}
		if (!code.empty()) {
			result += "[code: " + code + "] ";
		}
		if (!what.empty()) {
			result += "[what: " + what + "] ";
		}
		return result;
	}


}  // namespace cst



/// MetadataDescriptor
namespace cst
{

	// ==================================================================
	//  MetadataDescriptor - error metadata descriptor
	// ==================================================================
	template<class E>
	struct MetadataDescriptor
	{
		// -- conversion ----------------------------------------------------
	public:
		ErrorMetadata operator ()(const E&);

	};  // struct MetadataDescriptor




	/// -- conversion ----------------------------------------------------

	// convert error to ErrorMetadata
	template<class E>
	ErrorMetadata MetadataDescriptor<E>::operator ()(const E& e)
	{
		ErrorMetadata meta{};

		if constexpr (std::is_convertible_v<E, std::string>) {
			meta.code = std::string(e);
		}
		else if constexpr (std::is_base_of_v<std::exception, E>) {
			meta.source = "Exception";
			meta.code = typeid(e).name();  // RTTI-name ("std::runtime_error")
			meta.what = e.what();
		}
		else if constexpr (std::is_arithmetic_v<E>) {
			meta.code = std::to_string(e);
		}
		else {
			meta.source = "Common";
			static_assert(!sizeof(E), "Unsupported type. Use custom specialization.");
		}

		return meta;
	}


}  // namespace cst



/// ErrorRecord
namespace cst
{

	// ==================================================================
	//  ErrorRecord - error record wrapper
	// ==================================================================
	struct ErrorRecord :
		public FormattedRecord,
		public ErrorMetadata
	{
	public:
		std::source_location where;  // location: function name, file, line

		// -- format --------------------------------------------------------
	public:
		std::string format_location()         const;  // "[where: {} ({}:{})] "
		std::string format_message()          const;  // "[msg: {}] "
		std::string format()                  const;  // "[msg: {}] [source: {}] [code: {}] [what: {}] [where: {} ({}:{})] "

		// -- operators -----------------------------------------------------
	public:
		explicit operator FormattedRecord();
		bool operator==(const ErrorRecord&)   const;

		// -- lifecycle -----------------------------------------------------
	public:
		template <typename E
		> ErrorRecord(FormattedRecord, E, std::source_location           = std::source_location::current());
		ErrorRecord(std::source_location                                 = std::source_location::current());
		ErrorRecord(FormattedRecord, std::source_location                = std::source_location::current());
		ErrorRecord(FormattedRecord, ErrorMetadata, std::source_location = std::source_location::current());
		~ErrorRecord()                                                   = default;
		ErrorRecord(const ErrorRecord&)                                  = default;
		ErrorRecord(ErrorRecord&&)                                       = default;
		ErrorRecord& operator=(const ErrorRecord&)                       = default;
		ErrorRecord& operator=(ErrorRecord&&)                            = default;

	};  // struct ErrorRecord




	/// -- format --------------------------------------------------------

	// format to: "[where: {} ({}:{})] "
	inline std::string ErrorRecord::format_location() const
	{
		return std::format("[where: {} ({}:{})] ",
			where.function_name(),
			where.file_name(),
			where.line());
	}

	// format to: "[msg: {}] "
	inline std::string ErrorRecord::format_message() const
	{
		return std::format("[msg: {}] ",
			FormattedRecord::message);
	}

	// format to: "[msg: {}] [source: {}] [code: {}] [what: {}] [where: {} ({}:{})] "
	inline std::string ErrorRecord::format() const
	{
		return
			FormattedRecord::format_prefix() +
			ErrorRecord::format_message()    +
			ErrorMetadata::format_metadata() +
			ErrorRecord::format_location();
	}


	/// -- operators -----------------------------------------------------

	// convert to FormattedRecord (keep timestamp and append metadata)
	inline ErrorRecord::operator FormattedRecord()
	{
		FormattedRecord rec{ this->level, format() };
		rec.timestamp = this->timestamp;
		return rec;
	}

	// compare
	inline bool ErrorRecord::operator==(const ErrorRecord& other) const
	{
		return
			static_cast<const FormattedRecord&>(*this) == static_cast<const FormattedRecord&>(other) and
			static_cast<const ErrorMetadata&>(*this)   == static_cast<const ErrorMetadata&>(other);
	}


	/// -- lifecycle -----------------------------------------------------

	// construct empty
	inline ErrorRecord::ErrorRecord(std::source_location loc) :
		FormattedRecord(),
		ErrorMetadata(),
		where(std::move(loc))
	{}

	// construct with FormattedRecord
	inline ErrorRecord::ErrorRecord(FormattedRecord rec, std::source_location loc) :
		FormattedRecord(std::move(rec)),
		ErrorMetadata(),
		where(std::move(loc))
	{}

	// construct with FormattedRecord and ErrorMetadata
	inline ErrorRecord::ErrorRecord(FormattedRecord rec, ErrorMetadata meta, std::source_location loc) :
		FormattedRecord(std::move(rec)),
		ErrorMetadata(std::move(meta)),
		where(std::move(loc))
	{}

	// construct with FormattedRecord and error
	template <typename E>
	ErrorRecord::ErrorRecord(FormattedRecord rec, E e, std::source_location loc) :
		FormattedRecord(std::move(rec)),
		ErrorMetadata(MetadataDescriptor<E>{}(e)),
		where(std::move(loc))
	{}


}  // namespace cst



