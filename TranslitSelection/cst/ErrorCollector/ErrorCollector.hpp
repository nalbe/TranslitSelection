// ErrorCollector.hpp
#pragma once

// Implementation specific headers
#include "cst/ConcurrentVector/ConcurrentVector.hpp"
#include "cst/FormattedRecord/FormattedRecord.hpp"

// Standard library headers
#include <functional>
#include <mutex>
#include <utility>   // std::move



// ErrorCollector
namespace cst
{

	// ==============================================================
	//  ErrorCollector - thread-safe error collector
	// ==============================================================
	class ErrorCollector
	{
		// -- type aliases ----------------------------------------------
	public:
		using self_type               = ErrorCollector;
		using value_type              = ErrorRecord;
		using callback_function_type  = std::function<void(const value_type&)>;
		using size_type               = std::size_t;
		using container_type          = ConcurrentVector<value_type>;

		// -- members ---------------------------------------------------
	protected:
		mutable container_type         m_errors;

	private:
		mutable std::mutex             m_handler_mutex;
		mutable callback_function_type m_handler;

		// -- non-modifiers ---------------------------------------------
	public:
		auto snapshot()                                 const noexcept -> decltype(m_errors.snapshot());
		bool empty()                                    const noexcept;
		bool has_errors()                               const noexcept;

		// -- modifiers -------------------------------------------------
	public:
		void push_back(value_type&&)                    const noexcept;
		void push_back(const value_type&)               const noexcept;
		template <typename... Args
		> void emplace_back(Args&&...)                  const noexcept;
		void clear()                                    const noexcept;
		void set_handler(callback_function_type)        const noexcept;

		// -- internals -------------------------------------------------
	protected:
		virtual void _notify()                          const noexcept;

		// -- lifecycle -------------------------------------------------
	public:
		~ErrorCollector()                                    = default;
		ErrorCollector()                                     = default;
		ErrorCollector(ErrorCollector&&)                      noexcept;
		ErrorCollector& operator=(ErrorCollector&&)           noexcept;
		ErrorCollector(const ErrorCollector&)                 noexcept;
		ErrorCollector& operator=(const ErrorCollector&)      noexcept;

	};  // class ErrorCollector




	/// -- non-modifiers ---------------------------------------------

	// snapshot
	inline auto ErrorCollector::snapshot() const noexcept -> decltype(m_errors.snapshot())
	{
		return m_errors.snapshot();
	}

	// is empty
	inline bool ErrorCollector::empty() const noexcept
	{
		return m_errors.empty();
	}

	// check for errors
	inline bool ErrorCollector::has_errors() const noexcept
	{
		return !empty();
	}


	/// -- modifiers -------------------------------------------------

	// push_back moved
	inline void ErrorCollector::push_back(value_type&& error) const noexcept
	{
		m_errors.emplace_back(std::move(error));  // terminate on exception
		_notify();
	}

	// push_back copied
	inline void ErrorCollector::push_back(const value_type& error) const noexcept
	{
		m_errors.emplace_back(error);  // terminate on exception
		_notify();
	}

	// emplace
	template <typename... Args>
	void ErrorCollector::emplace_back(Args&&... args) const noexcept
	{
		m_errors.emplace_back(std::forward<Args>(args)...);
		_notify();
	}

	// clear
	inline void ErrorCollector::clear() const noexcept
	{
		m_errors.clear();
	}

	// set error handler
	inline void ErrorCollector::set_handler(callback_function_type error_handler) const noexcept
	{
		try {
			std::lock_guard<std::mutex> lock(m_handler_mutex);
			m_handler = std::move(error_handler);
		}
		catch (const std::exception& e)
		{
			m_errors.emplace_back(
				FormattedRecord{ ErrorLevel::Error, "Handler move assignment failed." },
				e
			);
		}
		catch (...) {
			m_errors.emplace_back(
				FormattedRecord{ ErrorLevel::Error, "Handler move assignment failed." }
			);
		}
	}


	/// -- internals -------------------------------------------------

	// invoke handler and store
	inline void ErrorCollector::_notify() const noexcept
	{
		try {
			callback_function_type handler_copy;  // terminate on exception
			{
				std::lock_guard<std::mutex> lock(m_handler_mutex);
				handler_copy = m_handler;
			}
			if (handler_copy) {
				handler_copy(m_errors.back());
			}
		}
		catch (const std::exception& e) {
			{
				std::lock_guard<std::mutex> lock(m_handler_mutex);
				m_handler = {};
			}
			m_errors.emplace_back(
				FormattedRecord{ ErrorLevel::Error, "Handler invocation failed." },
				e
			);
		}
		catch (...) {
			{
				std::lock_guard<std::mutex> lock(m_handler_mutex);
				m_handler = {};
			}
			m_errors.emplace_back(
				FormattedRecord{ ErrorLevel::Error, "Handler invocation failed." }
			);
		}
	}


	/// -- lifecycle -------------------------------------------------

	// move constructor
	inline ErrorCollector::ErrorCollector(ErrorCollector&& other) noexcept :
		m_errors(std::move(other.m_errors)),
		m_handler(std::move(other.m_handler))
	{}

	// move assignment
	inline ErrorCollector& ErrorCollector::operator=(ErrorCollector&& other) noexcept
	{
		m_errors = std::move(other.m_errors);
		m_handler = std::move(other.m_handler);
		return *this;
	}

	// copy constructor
	inline ErrorCollector::ErrorCollector(const ErrorCollector& other) noexcept :
		m_errors(other.m_errors),
		m_handler(other.m_handler)
	{}

	// copy assignment
	inline ErrorCollector& ErrorCollector::operator=(const ErrorCollector& other) noexcept
	{
		m_errors = other.m_errors;
		m_handler = other.m_handler;
		return *this;
	}


}  // namespace cst



// ErrorCollectorMixin
namespace cst
{

	// ==============================================================
	//  ErrorCollectorMixin - thread-safe error collector mixin
	// ==============================================================
	class ErrorCollectorMixin
	{
		// -- type aliases ----------------------------------------------
	public:
		using self_type                = ErrorCollectorMixin;
		using callback_function_type   = typename ErrorCollector::callback_function_type;
		using value_type               = typename ErrorCollector::value_type;

		// -- members ---------------------------------------------------
	protected:
		mutable ErrorCollector m_errors;

		// -- non-modifiers ---------------------------------------------
	public:
		auto get_errors()                               const noexcept;
		bool has_errors()                               const noexcept;

		// -- modifiers -------------------------------------------------
	public:
		void push_error(const value_type&)              const noexcept;
		void push_error(value_type&&)                   const noexcept;
		template <typename... Args
		> void emplace_error(Args&&...)                 const noexcept;
		void clear_errors()                             const noexcept;
		void set_error_handler(callback_function_type)  const noexcept;

		// -- lifecycle -------------------------------------------------
	protected:
		~ErrorCollectorMixin()                               = default;
		ErrorCollectorMixin()                                = default;
		ErrorCollectorMixin(const self_type&)                = default;
		ErrorCollectorMixin& operator=(const self_type&)     = default;
		ErrorCollectorMixin(self_type&&)                     = default;
		ErrorCollectorMixin& operator=(self_type&&)          = default;

	};  // class ErrorCollectorMixin




	/// -- non-modifiers ---------------------------------------------

	// get errors
	inline auto ErrorCollectorMixin::get_errors() const noexcept
	{
		return m_errors.snapshot();
	}

	// check for errors
	inline bool ErrorCollectorMixin::has_errors() const noexcept
	{
		return !m_errors.empty();
	}


	/// -- modifiers -------------------------------------------------

	// push copied error
	inline void ErrorCollectorMixin::push_error(const value_type& error) const noexcept
	{
		m_errors.push_back(error);
	}

	// push moved error
	inline void ErrorCollectorMixin::push_error(value_type&& error) const noexcept
	{
		m_errors.push_back(std::move(error));
	}

	// emplace error
	template <typename... Args>
	inline void ErrorCollectorMixin::emplace_error(Args&&... args) const noexcept
	{
		m_errors.emplace_back(std::forward<Args>(args)...);
	}

	// clear errors
	inline void ErrorCollectorMixin::clear_errors() const noexcept
	{
		m_errors.clear();
	}

	// set error handler
	inline void ErrorCollectorMixin::set_error_handler(callback_function_type handler) const noexcept
	{
		m_errors.set_handler(handler);
	}


}  // namespace cst



