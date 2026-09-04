// Callback.hpp
#pragma once

// Implementation-specific headers
#include "cst/SparseArray/SparseArray.hpp"
#include "cst/ErrorCollector/ErrorCollector.hpp"

// Standard library headers
#include <vector>
#include <mutex>
#include <optional>
#include <functional>
#include <cstddef>  // size_t
#include <utility>  // forward



/// Callback
namespace cst
{

	template <typename Signature> class Callback;

	// ========================================================================
	//  Callback - thread-safe callback list with ID-based unsubscription
	// ========================================================================
	template <typename... Args>
	class Callback<void(Args...)> final :
		public ErrorCollectorMixin
	{
		// -- type aliases --------------------------------------------------------
	public:
		using self_type        = Callback<void(Args...)>;
		using function_type    = std::function<void(Args...)>;
		using container_type   = cst::SparseArray<function_type>;
		using size_type        = typename cst::SparseArray<function_type>::size_type;
		using id_type          = size_type;

		// -- members -------------------------------------------------------------
	private:
		mutable std::mutex     m_mutex;
		mutable container_type m_callbacks;

		// -- subscription --------------------------------------------------------
	public:
		id_type subscribe(function_type)           noexcept;
		bool unsubscribe(id_type)                  noexcept;

		// -- invocation ----------------------------------------------------------
	public:
		void invoke(Args...)                 const noexcept;
		void operator()(Args...)             const noexcept;

		// -- non-modifiers -------------------------------------------------------
	public:
		[[nodiscard]] bool empty()           const noexcept;
		[[nodiscard]] size_type size()       const noexcept;

		// -- modifiers -----------------------------------------------------------
	public:
		void compact()                             noexcept;
		void clear()                               noexcept;

		// -- lifecycle -----------------------------------------------------------
	public:
		Callback()                                 noexcept;
		Callback(const Callback&)                  = delete;
		Callback& operator=(const Callback&)       = delete;
		Callback(Callback&&)                       noexcept;
		Callback& operator=(Callback&&)            noexcept;

	};  // class Callback




	/// -- subscription --------------------------------------------------------

	// subscribe a callback and return a unique identifier (index in the vector)
	template <typename... Args>
	auto Callback<void(Args...)>::subscribe(function_type cb) noexcept -> id_type
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_callbacks.insert( std::move(cb) );
	}

	// unsubscribe a callback
	template <typename... Args>
	bool Callback<void(Args...)>::unsubscribe(id_type id) noexcept
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_callbacks.contains(id)) { return false; }
		m_callbacks.erase(id);
		return true;
	}


	/// -- invocation ----------------------------------------------------------

	// invoke all currently active callbacks
	template <typename... Args>
	void Callback<void(Args...)>::invoke(Args... args) const noexcept
	{
		// snapshot only the stable subscription IDs. This allocates a single
		// vector instead of copying the whole SparseArray (a std::list) and
		// allocating a node per callback on every message.
		std::vector<id_type> ids;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			ids.reserve(m_callbacks.size());
			for (auto it = m_callbacks.cbegin(); it != m_callbacks.cend(); ++it) {
				ids.push_back(it->index);
			}
		}

		for (id_type id : ids) {
			// grab a copy of the callback under the lock: a callback may
			// (un)subscribe itself reentrantly, so we must not keep the lock
			// across the invocation and must not touch SparseArray outside it.
			function_type fn;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				if (!m_callbacks.contains(id)) { continue; }  // unsubscribed meanwhile
				fn = m_callbacks.at(id);
			}
			try {
				fn(args...);
				continue;
			}
			catch (const std::exception& e) {
				emplace_error(
					FormattedRecord{ ErrorLevel::Error, "Callback [{}] invocation failed.", id },
					e
				);
			}
			catch (...) {
				emplace_error(
					FormattedRecord{ ErrorLevel::Error, "Callback [{}] invocation failed.", id }
				);
			}
			const_cast<Callback*>(this)->unsubscribe(id);  // remove bad subscription
		}
	}

	// convenience operator() to invoke the callbacks
	template <typename... Args>
	void Callback<void(Args...)>::operator()(Args... args) const noexcept
	{
		invoke(std::forward<Args>(args)...);
	}


	/// -- non-modifiers -------------------------------------------------------

	// true if there are no active subscriptions
	template <typename... Args>
	bool Callback<void(Args...)>::empty() const noexcept
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_callbacks.empty();
	}

	// the number of active subscriptions
	template <typename... Args>
	auto Callback<void(Args...)>::size() const noexcept -> size_type
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_callbacks.size();
	}


	/// -- modifiers -----------------------------------------------------------

	// compact the vector
	template <typename... Args>
	void Callback<void(Args...)>::compact() noexcept
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_callbacks.compact();
	}

	// remove all subscriptions and reset freelist
	template <typename... Args>
	void Callback<void(Args...)>::clear() noexcept
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_callbacks.clear();
	}


	/// -- lifecycle -----------------------------------------------------------

	// default constructor
	template <typename... Args>
	Callback<void(Args...)>::Callback() noexcept :
		ErrorCollectorMixin(),
		m_mutex(),
		m_callbacks()
	{}

	// move constructor
	template <typename... Args>
	Callback<void(Args...)>::Callback(Callback&& other) noexcept :
		ErrorCollectorMixin(std::move(other)),
		m_mutex(),
		m_callbacks(std::move(other.m_callbacks))
	{}

	// move assignment
	template <typename... Args>
	auto Callback<void(Args...)>::operator=(Callback&& other) noexcept -> self_type&
	{
		if (this == &other) {
			return *this;
		}
		ErrorCollectorMixin::operator=(std::move(other));
		m_callbacks  = std::move(other.m_callbacks);
		return *this;
	}


}  // namespace cst



// CallbackMixin
namespace cst
{

	template <typename Signature> class CallbackMixin;

	// ========================================================================
	//  CallbackMixin - convenience class for subscribing and unsubscribing
	// ========================================================================
	template <typename... Args>
	class CallbackMixin<void(Args...)>
	{
		// -- type aliases --------------------------------------------------------
	public:
		using function_type   = std::function<void(Args...)>;
		using container_type  = cst::SparseArray<function_type>;
		using size_type       = typename cst::SparseArray<function_type>::size_type;
		using id_type         = size_type;

		// -- protected members ---------------------------------------------------
	protected:
		Callback<void(Args...)> m_callbacks;

		// -- modifiers -----------------------------------------------------------
	public:
		id_type subscribe(function_type)                noexcept;
		bool unsubscribe(id_type)                       noexcept;

		// -- protected modifiers -------------------------------------------------
	protected:
		void invoke(Args...)                      const noexcept;

		// -- lifecycle -----------------------------------------------------------
	protected:
		CallbackMixin()                                = default;
		CallbackMixin(const CallbackMixin&)             = delete;
		CallbackMixin& operator=(const CallbackMixin&)  = delete;
		CallbackMixin(CallbackMixin&&)                 = default;
		CallbackMixin& operator=(CallbackMixin&&)      = default;

	};  // class CallbackMixin

	
	
	
	/// -- modifiers -----------------------------------------------------------

	// subscribe a callback
	template <typename... Args>
	auto CallbackMixin<void(Args...)>::subscribe(function_type fn) noexcept -> id_type
	{
		return m_callbacks.subscribe(std::move(fn));
	}

	// unsubscribe a callback
	template <typename... Args>
	bool CallbackMixin<void(Args...)>::unsubscribe(id_type id) noexcept
	{
		return m_callbacks.unsubscribe(id);
	}


	/// -- protected modifiers -------------------------------------------------

	// invoke all currently active callbacks
	template <typename... Args>
	void CallbackMixin<void(Args...)>::invoke(Args... args) const noexcept
	{
		m_callbacks.invoke(std::forward<Args>(args)...);
	}


}  // namespace cst



