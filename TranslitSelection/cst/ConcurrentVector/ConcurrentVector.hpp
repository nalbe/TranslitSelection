// ConcurrentVector.hpp
#pragma once

// Standard library headers
#include <algorithm>   // std::max
#include <atomic>
#include <limits>      // std::numeric_limits
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>



namespace cst
{

	// =================================================================
	//   ConcurrentVector - thread-safe vector with concurrent access
	// =================================================================
	template <typename T>
	class ConcurrentVector
	{
		// -- nested types -------------------------------------------------
	private:
		struct RawBuffer
		{
			T* data;
			size_t capacity;

			explicit RawBuffer(size_t);
			~RawBuffer();

			RawBuffer(const RawBuffer&)             = delete;
			RawBuffer& operator=(const RawBuffer&)  = delete;

		};  // struct RawBuffer

		// -- type aliases -------------------------------------------------
	public:
		using self_type        = ConcurrentVector;
		using value_type       = T;
		using reference        = T&;
		using const_reference  = const T&;
		using pointer          = T*;
		using const_pointer    = const T*;

		// -- members ------------------------------------------------------
	private:
		std::atomic<size_t>        m_size        {};
		std::atomic<size_t>        m_capacity    {};
		std::atomic<unsigned>      m_version     {};
		std::shared_ptr<RawBuffer> m_buffer;
		mutable std::mutex         m_writeMutex;

		// -- internals ----------------------------------------------------
	private:
		void reallocate_(size_t);
		void ensure_capacity_(size_t);
		void check_index_(size_t, size_t, bool = false)          const;
		std::shared_ptr<RawBuffer> acquire_read_state_(size_t&)  const;

		// -- insertion -----------------------------------------------------
	public:
		void push_back(const T&);
		void push_back(T&&);
		void push(const T&);
		void push(T&&);
		template <typename... Args
		> void emplace_back(Args&&...);
		void insert(size_t, const T&);
		void insert(size_t, T&&);

		// -- deletion ------------------------------------------------------
	public:
		void pop_back();
		void erase(size_t);
		void erase(size_t, size_t);
		void clear();

		// -- modifiers -----------------------------------------------------
	public:
		void resize(size_t);
		void resize(size_t, const T&);
		void reserve(size_t);
		void shrink_to_fit();
		void swap(self_type&)                                 noexcept;

		// -- non-modifiers ------------------------------------------------
	public:
		T front()                                       const;
		T back()                                        const;
		T at(size_t)                                    const;
		T get(size_t)                                   const;
		bool empty()                                    const;
		size_t size()                                   const;
		size_t capacity()                               const;
		size_t max_size()                               const;
		std::vector<T> snapshot()                       const;

		// -- lifecycle ----------------------------------------------------
	public:
		~ConcurrentVector();
		ConcurrentVector()                                   = default;
		ConcurrentVector(self_type&&)                         noexcept;
		ConcurrentVector& operator=(self_type&&)              noexcept;
		ConcurrentVector(const self_type&);
		ConcurrentVector& operator=(const self_type&);

		/// -- free operators -----------------------------------------------
	private:
		template <typename U
		> friend bool operator==(const ConcurrentVector<U>&, const ConcurrentVector<U>&);
		template <typename U
		> friend bool operator!=(const ConcurrentVector<U>&, const ConcurrentVector<U>&);
		template <typename U
		> friend bool operator <(const ConcurrentVector<U>&, const ConcurrentVector<U>&);
		template <typename U
		> friend bool operator >(const ConcurrentVector<U>&, const ConcurrentVector<U>&);
		template <typename U
		> friend bool operator<=(const ConcurrentVector<U>&, const ConcurrentVector<U>&);
		template <typename U
		> friend bool operator>=(const ConcurrentVector<U>&, const ConcurrentVector<U>&);

	};  // class ConcurrentVector

}  // namespace cst




namespace cst
{

	/// -- nested types -------------------------------------------------

	// constructor
	template<typename T>
	ConcurrentVector<T>::RawBuffer::RawBuffer(size_t cap) :
		data(static_cast<T*>(::operator new(sizeof(T)* cap))),
		capacity(cap)
	{}
	
	// destructor
	template<typename T>
	ConcurrentVector<T>::RawBuffer::~RawBuffer()
	{
		::operator delete(data);
	}


	/// -- internals ----------------------------------------------------

	// reallocate
	template<typename T>
	void ConcurrentVector<T>::reallocate_(size_t new_cap)
	{
		auto new_buf = std::make_shared<RawBuffer>(new_cap);
		size_t sz = size();
		T* new_data = new_buf->data;

		if (sz > 0) {
			T* old_data = m_buffer->data;
			for (size_t i = 0; i < sz; ++i) {
				new (new_data + i) T(std::move(old_data[i]));
				old_data[i].~T();
			}
		}

		m_buffer = std::move(new_buf);
		m_capacity.store(new_cap, std::memory_order_release);
	}

	// ensure capacity
	template<typename T>
	void ConcurrentVector<T>::ensure_capacity_(size_t n)
	{
		if (n <= capacity()) { return; }
		size_t new_cap = std::max(capacity() * 2, size_t(4));
		while (new_cap < n) { new_cap *= 2; }
		reallocate_(new_cap);
	}

	// check index
	template<typename T>
	void ConcurrentVector<T>::check_index_(size_t idx, size_t sz, bool allow_equal) const 
	{
		if ((allow_equal and idx > sz) or (!allow_equal and idx >= sz)) {
			throw std::out_of_range("ConcurrentVector index out of range");
		}
	}

	// acquire read state
	template<typename T>
	std::shared_ptr<typename ConcurrentVector<T>::RawBuffer>
		ConcurrentVector<T>::acquire_read_state_(size_t& out_size) const
	{
		for (; ; ) {
			unsigned v = m_version.load(std::memory_order_acquire);
			if (v & 1) { continue; }
			out_size = m_size.load(std::memory_order_acquire);
			auto buf = m_buffer;
			if (m_version.load(std::memory_order_acquire) != v) { continue; }
			return buf;
		}
	}


	/// -- insertion -----------------------------------------------------

	// push back
	template<typename T>
	void ConcurrentVector<T>::push_back(const T& value)
	{
		std::lock_guard<std::mutex> lk(m_writeMutex);
		m_version.fetch_add(1, std::memory_order_release);
		size_t sz = size();
		ensure_capacity_(sz + 1);
		T* data = m_buffer->data;
		new (data + sz) T(value);
		m_size.store(sz + 1, std::memory_order_release);
		m_version.fetch_add(1, std::memory_order_release);
	}

	// push back
	template<typename T>
	void ConcurrentVector<T>::push_back(T&& value)
	{
		std::lock_guard<std::mutex> lk(m_writeMutex);
		m_version.fetch_add(1, std::memory_order_release);
		size_t sz = size();
		ensure_capacity_(sz + 1);
		new (m_buffer->data + sz) T(std::move(value));
		m_size.store(sz + 1, std::memory_order_release);
		m_version.fetch_add(1, std::memory_order_release);
	}

	// push
	template<typename T>
	void ConcurrentVector<T>::push(const T& value)
	{
		push_back(value);
	}

	// push
	template<typename T>
	void ConcurrentVector<T>::push(T&& value)
	{
		push_back(std::move(value));
	}

	// emplace back
	template<typename T>
	template<typename... Args>
	void ConcurrentVector<T>::emplace_back(Args&&... args)
	{
		std::lock_guard<std::mutex> lk(m_writeMutex);
		m_version.fetch_add(1, std::memory_order_release);
		size_t sz = size();
		ensure_capacity_(sz + 1);
		new (m_buffer->data + sz) T(std::forward<Args>(args)...);
		m_size.store(sz + 1, std::memory_order_release);
		m_version.fetch_add(1, std::memory_order_release);
	}

	// insert
	template<typename T>
	void ConcurrentVector<T>::insert(size_t pos, const T& value)
	{
		std::lock_guard<std::mutex> lk(m_writeMutex);
		size_t sz = size();
		if (pos > sz) { throw std::out_of_range("insert: pos > size()"); }
		m_version.fetch_add(1, std::memory_order_release);
		ensure_capacity_(sz + 1);
		T* data = m_buffer->data;
		for (size_t i = sz; i > pos; --i) {
			new (data + i) T(std::move(data[i - 1]));
			data[i - 1].~T();
		}
		new (data + pos) T(value);
		m_size.store(sz + 1, std::memory_order_release);
		m_version.fetch_add(1, std::memory_order_release);
	}

	// insert
	template<typename T>
	void ConcurrentVector<T>::insert(size_t pos, T&& value)
	{
		std::lock_guard<std::mutex> lk(m_writeMutex);
		size_t sz = size();
		if (pos > sz) { throw std::out_of_range("insert: pos > size()"); }
		m_version.fetch_add(1, std::memory_order_release);
		ensure_capacity_(sz + 1);
		T* data = m_buffer->data;
		for (size_t i = sz; i > pos; --i) {
			new (data + i) T(std::move(data[i - 1]));
			data[i - 1].~T();
		}
		new (data + pos) T(std::move(value));
		m_size.store(sz + 1, std::memory_order_release);
		m_version.fetch_add(1, std::memory_order_release);
	}


	/// -- deletion ------------------------------------------------------

	// pop back
	template<typename T>
	void ConcurrentVector<T>::pop_back()
	{
		std::lock_guard<std::mutex> lk(m_writeMutex);
		size_t sz = size();
		if (sz == 0) { throw std::out_of_range("pop_back on empty ConcurrentVector"); }
		m_version.fetch_add(1, std::memory_order_release);
		m_buffer->data[sz - 1].~T();
		m_size.store(sz - 1, std::memory_order_release);
		m_version.fetch_add(1, std::memory_order_release);
	}

	// erase
	template<typename T>
	void ConcurrentVector<T>::erase(size_t pos)
	{
		std::lock_guard<std::mutex> lk(m_writeMutex);
		size_t sz = size();
		if (pos >= sz) { throw std::out_of_range("erase: pos >= size()"); }
		m_version.fetch_add(1, std::memory_order_release);
		T* data = m_buffer->data;
		data[pos].~T();
		for (size_t i = pos + 1; i < sz; ++i) {
			new (data + i - 1) T(std::move(data[i]));
			data[i].~T();
		}
		m_size.store(sz - 1, std::memory_order_release);
		m_version.fetch_add(1, std::memory_order_release);
	}

	// erase
	template<typename T>
	void ConcurrentVector<T>::erase(size_t first, size_t last)
	{
		std::lock_guard<std::mutex> lk(m_writeMutex);
		size_t sz = size();
		if (first > last) { throw std::out_of_range("erase: first > last"); }
		if (last > sz) { throw std::out_of_range("erase: last > size()"); }
		if (first == last) { return; }
		m_version.fetch_add(1, std::memory_order_release);
		T* data = m_buffer->data;
		for (size_t i = first; i < last; ++i) {
			data[i].~T();
		}
		size_t shift = last - first;
		for (size_t i = last; i < sz; ++i) {
			new (data + i - shift) T(std::move(data[i]));
			data[i].~T();
		}
		m_size.store(sz - shift, std::memory_order_release);
		m_version.fetch_add(1, std::memory_order_release);
	}

	// clear
	template<typename T>
	void ConcurrentVector<T>::clear()
	{
		std::lock_guard<std::mutex> lk(m_writeMutex);
		size_t sz = size();
		if (sz == 0) { return; }
		m_version.fetch_add(1, std::memory_order_release);
		T* data = m_buffer->data;
		for (size_t i{}; i < sz; ++i) {
			data[i].~T();
		}
		m_size.store(0, std::memory_order_release);
		m_version.fetch_add(1, std::memory_order_release);
	}


	/// -- modifiers -----------------------------------------------------

	// resize
	template<typename T>
	void ConcurrentVector<T>::resize(size_t count)
	{
		std::lock_guard<std::mutex> lk(m_writeMutex);
		m_version.fetch_add(1, std::memory_order_release);
		size_t sz = size();
		if (count < sz) {
			T* data = m_buffer->data;
			for (size_t i = count; i < sz; ++i) {
				data[i].~T();
			}
		}
		else if (count > sz) {
			ensure_capacity_(count);
			T* data = m_buffer->data;
			for (size_t i = sz; i < count; ++i) {
				new (data + i) T();
			}  //  default-constructible
		}
		m_size.store(count, std::memory_order_release);
		m_version.fetch_add(1, std::memory_order_release);
	}

	// resize
	template<typename T>
	void ConcurrentVector<T>::resize(size_t count, const T& value)
	{
		std::lock_guard<std::mutex> lk(m_writeMutex);
		m_version.fetch_add(1, std::memory_order_release);
		size_t sz = size();
		if (count < sz) {
			T* data = m_buffer->data;
			for (size_t i = count; i < sz; ++i) {
				data[i].~T();
			}
		}
		else if (count > sz) {
			ensure_capacity_(count);
			T* data = m_buffer->data;
			for (size_t i = sz; i < count; ++i) {
				new (data + i) T(value);
			}
		}
		m_size.store(count, std::memory_order_release);
		m_version.fetch_add(1, std::memory_order_release);
	}

	// reserve
	template<typename T>
	void ConcurrentVector<T>::reserve(size_t new_cap)
	{
		std::lock_guard<std::mutex> lk(m_writeMutex);
		if (new_cap > capacity()) {
			m_version.fetch_add(1, std::memory_order_release);
			reallocate_(new_cap);
			m_version.fetch_add(1, std::memory_order_release);
		}
	}

	// shrink to fit
	template<typename T>
	void ConcurrentVector<T>::shrink_to_fit()
	{
		std::lock_guard<std::mutex> lk(m_writeMutex);
		size_t sz = size();
		if (sz == capacity()) { return; }
		m_version.fetch_add(1, std::memory_order_release);
		if (sz == 0) {
			m_buffer.reset();
			m_capacity.store(0, std::memory_order_release);
		}
		else {
			reallocate_(sz);
		}
		m_version.fetch_add(1, std::memory_order_release);
	}

	// swap
	template<typename T>
	void ConcurrentVector<T>::swap(self_type& other) noexcept
	{
		if (this == &other) { return; }
		std::lock(m_writeMutex, other.m_writeMutex);
		std::lock_guard<std::mutex> lk1(m_writeMutex, std::adopt_lock);
		std::lock_guard<std::mutex> lk2(other.m_writeMutex, std::adopt_lock);

		size_t sz1 = size();
		size_t cap1 = capacity();
		auto buf1 = std::move(m_buffer);

		m_size.store(other.size(), std::memory_order_relaxed);
		m_capacity.store(other.capacity(), std::memory_order_relaxed);
		m_buffer = std::move(other.m_buffer);

		other.m_size.store(sz1, std::memory_order_relaxed);
		other.m_capacity.store(cap1, std::memory_order_relaxed);
		other.m_buffer = std::move(buf1);
	}


	/// -- non-modifiers ------------------------------------------------

	// front
	template<typename T>
	T ConcurrentVector<T>::front() const
	{
		return at(0);
	}

	// back
	template<typename T>
	T ConcurrentVector<T>::back() const
	{
		size_t sz;
		std::shared_ptr<RawBuffer> buf = acquire_read_state_(sz);
		if (sz == 0) { throw std::out_of_range("back on empty ConcurrentVector"); }
		return buf->data[sz - 1];
	}

	// at
	template<typename T>
	T ConcurrentVector<T>::at(size_t index) const
	{
		size_t sz;
		std::shared_ptr<RawBuffer> buf = acquire_read_state_(sz);
		check_index_(index, sz);
		return buf->data[index];
	}

	// get
	template<typename T>
	T ConcurrentVector<T>::get(size_t index) const
	{
		size_t sz;
		std::shared_ptr<RawBuffer> buf = acquire_read_state_(sz);
		if (index >= sz) { throw std::out_of_range("get: index out of range"); }
		return buf->data[index];
	}

	// empty (atomic)
	template<typename T>
	bool ConcurrentVector<T>::empty() const
	{
		return m_size.load(std::memory_order_acquire) == 0;
	}

	// size (atomic)
	template<typename T>
	size_t ConcurrentVector<T>::size() const
	{
		return m_size.load(std::memory_order_acquire);
	}

	// capacity (atomic)
	template<typename T>
	size_t ConcurrentVector<T>::capacity() const
	{
		return m_capacity.load(std::memory_order_acquire);
	}

	// max_size (const)
	template<typename T>
	size_t ConcurrentVector<T>::max_size() const
	{
		return std::numeric_limits<size_t>::max() / sizeof(T);
	}

	// snapshot
	template<typename T>
	std::vector<T> ConcurrentVector<T>::snapshot() const
	{
		std::vector<T> result;
		for (;;) {
			unsigned v = m_version.load(std::memory_order_acquire);
			if (v & 1) { continue; }
			size_t sz = m_size.load(std::memory_order_acquire);
			auto buf = m_buffer;
			if (m_version.load(std::memory_order_acquire) != v) {
				continue;
			}
			if (buf == nullptr) {
				return result;   // уже содержит пустой вектор
			}
			result.assign(buf->data, buf->data + sz);
			return result;
		}
	}


	/// -- lifecycle ----------------------------------------------------

	// destructor
	template<typename T>
	ConcurrentVector<T>::~ConcurrentVector()
	{
		if (m_buffer) {
			T* d = m_buffer->data;
			size_t sz = size();
			for (size_t i{}; i < sz; ++i) {
				d[i].~T();
			}
		}
	}

	// move constructor
	template<typename T>
	ConcurrentVector<T>::ConcurrentVector(self_type&& other) noexcept
	{
		std::lock_guard<std::mutex> lk(other.m_writeMutex);
		m_size.store(other.m_size.load(std::memory_order_relaxed));
		m_capacity.store(other.m_capacity.load(std::memory_order_relaxed));
		m_buffer = std::move(other.m_buffer);
		other.m_size = 0;
		other.m_capacity = 0;
	}

	// move assignment
	template<typename T>
	ConcurrentVector<T>& ConcurrentVector<T>::operator=(self_type&& other) noexcept
	{
		if (this != &other) {
			std::lock(m_writeMutex, other.m_writeMutex);
			std::lock_guard<std::mutex> lk1(m_writeMutex, std::adopt_lock);
			std::lock_guard<std::mutex> lk2(other.m_writeMutex, std::adopt_lock);

			// destroy current live objects BEFORE releasing our buffer:
			// RawBuffer only releases raw memory, it does not run ~T().
			if (m_buffer) {
				size_t sz = size();
				T* d = m_buffer->data;
				for (size_t i{}; i < sz; ++i) {
					d[i].~T();
				}
			}

			m_size.store(other.m_size.load(std::memory_order_relaxed));
			m_capacity.store(other.m_capacity.load(std::memory_order_relaxed));
			m_buffer = std::move(other.m_buffer);
			other.m_size = 0;
			other.m_capacity = 0;
		}
		return *this;
	}

	// copy constructor
	template<typename T>
	ConcurrentVector<T>::ConcurrentVector(const self_type& other)
	{
		std::lock_guard<std::mutex> lk(other.m_writeMutex);
		size_t sz = other.size();
		size_t cap = other.capacity();
		if (cap > 0) {
			m_buffer = std::make_shared<RawBuffer>(cap);
			T* src = other.m_buffer->data;
			T* dst = m_buffer->data;
			for (size_t i{}; i < sz; ++i) {
				new (dst + i) T(src[i]);
			}
		}
		m_size.store(sz, std::memory_order_relaxed);
		m_capacity.store(cap, std::memory_order_relaxed);
	}

	// copy assignment
	template<typename T>
	ConcurrentVector<T>& ConcurrentVector<T>::operator=(const self_type& other)
	{
		if (this != &other) {
			std::lock(m_writeMutex, other.m_writeMutex);
			std::lock_guard<std::mutex> lk1(m_writeMutex, std::adopt_lock);
			std::lock_guard<std::mutex> lk2(other.m_writeMutex, std::adopt_lock);

			size_t sz = other.size();
			size_t cap = other.capacity();
			std::shared_ptr<RawBuffer> new_buf;
			if (cap > 0) {
				new_buf = std::make_shared<RawBuffer>(cap);
				T* src = other.m_buffer->data;
				T* dst = new_buf->data;
				for (size_t i{}; i < sz; ++i) {
					new (dst + i) T(src[i]);
				}
			}
			if (m_buffer) {
				T* old = m_buffer->data;
				size_t old_sz = size();
				for (size_t i{}; i < old_sz; ++i) {
					old[i].~T();
				}
			}
			m_buffer = std::move(new_buf);
			m_size.store(sz, std::memory_order_relaxed);
			m_capacity.store(cap, std::memory_order_relaxed);
		}
		return *this;
	}


	/// -- free operators -----------------------------------------------

	// equality
	template <typename U>
	bool operator==(const ConcurrentVector<U>& lhs, const ConcurrentVector<U>& rhs)
	{
		return lhs.snapshot() == rhs.snapshot();
	}

	// inequality
	template <typename U>
	bool operator!=(const ConcurrentVector<U>& lhs, const ConcurrentVector<U>& rhs)
	{
		return !(lhs == rhs);
	}

	// less than
	template <typename U>
	bool operator<(const ConcurrentVector<U>& lhs, const ConcurrentVector<U>& rhs)
	{
		return lhs.snapshot() < rhs.snapshot();
	}

	// less than or equal
	template <typename U>
	bool operator<=(const ConcurrentVector<U>& lhs, const ConcurrentVector<U>& rhs)
	{
		return !(rhs < lhs);
	}

	// greater than
	template <typename U>
	bool operator>(const ConcurrentVector<U>& lhs, const ConcurrentVector<U>& rhs)
	{
		return rhs < lhs;
	}

	// greater than or equal
	template <typename U>
	bool operator>=(const ConcurrentVector<U>& lhs, const ConcurrentVector<U>& rhs)
	{
		return !(lhs < rhs);
	}


} // namespace cst



