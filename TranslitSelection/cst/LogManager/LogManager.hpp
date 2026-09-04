// LogManager.hpp
#pragma once

// Implementation-specific headers
#include "cst/ErrorLevel/ErrorLevel.hpp"
#include "cst/Timestamp/Timestamp.hpp"
#include "cst/AsyncWorker/AsyncWorker.hpp"
#include "cst/Callback/Callback.hpp"

// Standard library headers
#include <mutex>
#include <string>
#include <string_view>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <thread>
#include <exception>



namespace cst
{

	// =============================================================================
	//  LogManager - thread-safe asynchronous file logger
	// =============================================================================
	class LogManager final :
		public ErrorCollectorMixin,
		public CallbackMixin<void(FormattedRecord)>
	{
		// -- nested classes ----------------------------------------------------
	private:
		template<typename... Args>
		class Task :
			FormattedRecord
		{
			std::string m_fmt;
			std::tuple<std::decay_t<Args>...> m_args;
		public:
			FormattedRecord& operator()();
			Task(ErrorLevel, std::string_view, Args&&...);

		};  // class Task

		// -- type aliases ------------------------------------------------------
	private:
		using callback_type           = CallbackMixin<void(FormattedRecord)>;
	public:
		using self_type               = LogManager;
		using callback_function_type  = typename callback_type::function_type;
		using callback_id_type        = typename callback_type::id_type;
		using record_type             = FormattedRecord;

		// -- members -----------------------------------------------------------
	private:
		std::mutex              m_mutex;
		std::atomic<ErrorLevel> m_minLevel;
		std::atomic<ErrorLevel> m_defaultLevel;
		AsyncWorker             m_workerThread;

		// -- asynchronous ------------------------------------------------------
	public:
		template <typename... Args
		> self_type& write(ErrorLevel, std::string_view, Args&&...);
		template <typename... Args
		> self_type& write(std::string_view, Args&&...);

		// -- non-modifiers -----------------------------------------------------
	public:
		ErrorLevel min_level()                                 const noexcept;
		ErrorLevel default_level()                             const noexcept;
		bool active()                                          const noexcept;
		bool has_errors()                                      const noexcept;

		// -- modifiers ---------------------------------------------------------
	public:
		bool start()                                                 noexcept;
		self_type& sync()                                            noexcept;
		self_type& min_level(ErrorLevel)                             noexcept;
		self_type& default_level(ErrorLevel)                         noexcept;
		self_type& stop()                                            noexcept;
		self_type& clear_errors()                                    noexcept;

		// -- lifecycle ---------------------------------------------------------
	public:
		~LogManager()                                                noexcept;
		LogManager()                                                 noexcept;
		LogManager(const self_type&)                                 = delete;
		self_type& operator=(const self_type&)                       = delete;
		LogManager(self_type&&)                                      = delete;
		self_type& operator=(self_type&&)                            = delete;

	};  // class LogManager

}  // namespace cst




namespace cst
{

	// -- LogManager::Task ---------------------------------------------------

	// construct formatting task
	template <typename... Args>
	LogManager::Task<Args...>::Task(ErrorLevel lvl, std::string_view fmt, Args&&... args) :
		FormattedRecord(lvl),
		m_fmt(fmt),
		m_args(std::forward<Args>(args)...)
	{}

	// format message
	template <typename... Args>
	auto LogManager::Task<Args...>::operator()() -> FormattedRecord&
	{
		this->assign(std::move(m_fmt), std::move(m_args));
		return static_cast<FormattedRecord&>(*this);
	}


}  // namespace cst



namespace cst
{

	/// -- asynchronous ------------------------------------------------------

	// enqueue formatted message (with prefix) if level passes filter
	template <typename... Args>
	auto LogManager::write(ErrorLevel level, std::string_view fmt_str, Args&&... args) -> self_type&
	{
		if (!active()) { return *this; }
		if (level < m_minLevel.load(std::memory_order_relaxed)) {
			return *this;
		}

		try {
			auto work = [
				task = Task<Args...>{ level, fmt_str, std::forward<Args>(args)... },
				&cb = m_callbacks
			]() mutable {
				cb.invoke(task());
				};
			m_workerThread.enqueue(std::move(work));
		}
		catch (const std::exception& e) {
			emplace_error(
				FormattedRecord{ ErrorLevel::Error, "Failed to enqueue log record." }, e
			);
			return *this;
		}
		catch (...) {
			emplace_error(
				FormattedRecord{ ErrorLevel::Error, "Failed to enqueue log record." }
			);
			return *this;
		}

		return *this;
	}

	// enqueue formatted default-level message (with prefix)
	template <typename... Args>
	auto LogManager::write(std::string_view fmt_str, Args&&... args) -> self_type&
	{
		return write(
			m_defaultLevel.load(std::memory_order_relaxed), fmt_str, std::forward<Args>(args)...
		);
	}


	/// -- non-modifiers -----------------------------------------------------

	// current minimum log level	
	ErrorLevel LogManager::min_level() const noexcept
	{
		return ErrorLevel(m_minLevel.load(std::memory_order_relaxed));
	}

	// current minimum log level	
	ErrorLevel LogManager::default_level() const noexcept
	{
		return ErrorLevel(m_defaultLevel.load(std::memory_order_relaxed));
	}

	// if logger is running and not in error state	
	bool LogManager::active() const noexcept
	{
		return m_workerThread.active();
	}

	// if logger is in error state
	bool LogManager::has_errors() const noexcept
	{
		return
			m_workerThread.has_errors() or
			m_callbacks.has_errors()    or
			m_errors.has_errors();
	}


	/// -- modifiers ---------------------------------------------------------

	// open log file and start worker thread	
	bool LogManager::start() noexcept
	{
		return active()
			? true
			: m_workerThread.start();
	}

	// blocks while all pending messages until this point are handled
	auto LogManager::sync() noexcept -> self_type&
	{
		if (!active()) { return *this; }
		if (!m_workerThread.sync()) {
			stop();
		}
		return *this;
	}

	// set minimum message level filter
	auto LogManager::min_level(ErrorLevel value) noexcept -> self_type&
	{
		m_minLevel.store(value, std::memory_order_relaxed);
		return *this;
	}

	// set minimum message level filter
	auto LogManager::default_level(ErrorLevel value) noexcept -> self_type&
	{
		m_defaultLevel.store(value, std::memory_order_relaxed);
		return *this;
	}

	// stop worker thread and close log file	
	auto LogManager::stop() noexcept -> self_type&
	{
		m_workerThread.stop();
		return *this;
	}

	// clear error state	
	auto LogManager::clear_errors() noexcept -> self_type&
	{
		m_workerThread.clear_errors();
		m_callbacks.clear_errors();
		m_errors.clear();
		return *this;
	}


	/// -- lifecycle ---------------------------------------------------------

	// destructor calls stop
	LogManager::~LogManager() noexcept
	{
		stop();
	}

	// constructor with default values
	LogManager::LogManager() noexcept :
		m_mutex(),
		m_minLevel(ErrorLevel::Debug),
		m_defaultLevel(ErrorLevel::Info),
		m_workerThread()
	{}


}  // namespace cst



