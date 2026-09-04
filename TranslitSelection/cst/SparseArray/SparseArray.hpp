// SparseArray.hpp
#pragma once

// Standard library headers
#include <list>
#include <vector>
#include <stdexcept>
#include <iterator>   // std::prev
#include <utility>    // swap
#include <optional>
#include <cassert>



namespace cst
{

	// =====================================================================
	//  SparseVector - mixed container with stable indices
	// =====================================================================
	template <typename T>
	class SparseArray
	{
		// -- type aliases -----------------------------------------------------
	public:
		using self_type          = SparseArray;
		using value_type         = T;
		using reference          = T&;
		using const_reference    = const T&;
		using pointer            = T*;
		using const_pointer      = const T*;
		using size_type          = size_t;
		using difference_type    = std::ptrdiff_t;

		// -- container types --------------------------------------------------
	public:
		struct Data { value_type value; size_type index; };
		using container_type     = std::list<Data>;

		// -- constants --------------------------------------------------------
	public:
		static inline const size_type s_npos {};

		// -- iterator aliases -------------------------------------------------
	public:
		using iterator          = typename container_type::iterator;
		using const_iterator    = typename container_type::const_iterator;

		// -- members ----------------------------------------------------------
	private:
		container_type                       m_data;
		std::vector<std::optional<iterator>> m_ids{ std::nullopt };
		std::vector<size_type>               m_free;

		// -- non-modifiers ----------------------------------------------------
	public:
		[[nodiscard]] reference at(size_type);
		[[nodiscard]] const_reference at(size_type)          const;

		[[nodiscard]] reference operator[](size_type);
		[[nodiscard]] const_reference operator[](size_type)  const;

		[[nodiscard]] bool contains(size_type)               const;

		[[nodiscard]] size_type size()                       const noexcept;
		[[nodiscard]] size_type capacity()                   const noexcept;
		[[nodiscard]] bool empty()                           const noexcept;

		// -- modifiers --------------------------------------------------------
	public:
		size_type insert(const_reference);
		size_type insert(T&&);
		size_type erase(size_type);
		iterator erase(const_iterator);
		void clear();
		void swap(SparseArray&)                                    noexcept;

		// -- iterators --------------------------------------------------------
	public:
		[[nodiscard]] const_iterator cbegin()                const noexcept;
		[[nodiscard]] const_iterator cend()                  const noexcept;
		[[nodiscard]] const_iterator begin()                 const noexcept;
		[[nodiscard]] const_iterator end()                   const noexcept;
		[[nodiscard]] iterator       begin()                       noexcept;
		[[nodiscard]] iterator       end()                         noexcept;

		// -- lifecycle --------------------------------------------------------
	public:
		~SparseArray()                                            = default;
		SparseArray()                                             = default;
		SparseArray(SparseArray&&)                                 noexcept;
		SparseArray& operator=(SparseArray&&)                      noexcept;
		SparseArray(const SparseArray&);
		SparseArray& operator=(const SparseArray&);

	};  // class SparseArray

}  // namespace cst




namespace cst
{

	/// -- modifiers --------------------------------------------------------

	template <typename T>
	auto SparseArray<T>::insert(const_reference value) -> size_type
	{
		return insert(std::move( value_type{ value } ));
	}

	template <typename T>
	auto SparseArray<T>::insert(T&& value) -> size_type
	{
		size_type index;
		if (!m_free.empty()) {
			index = m_free.back();
			m_data.emplace_back(std::move(value), index);
			m_ids[index] = std::prev(m_data.end());
			m_free.pop_back();
		}
		else {
			index = m_ids.size();
			m_data.emplace_back(std::move(value), index);
			m_ids.emplace_back( std::prev(m_data.end()) );
		}
		return index;
	}

	template <typename T>
	auto SparseArray<T>::erase(size_type index) -> size_type
	{
		assert(index > 0 && index < m_ids.size() && m_ids[index].has_value());
		const_iterator it = m_ids[index].value();
		m_ids[index] = std::nullopt;
		m_free.push_back(index);
		auto next = m_data.erase(it);
		return next == m_data.end() ? s_npos : next->index;
	}

	template <typename T>
	auto SparseArray<T>::erase(const_iterator it) -> iterator
	{
		assert(it != m_data.end());
		m_ids[it->index] = std::nullopt;
		m_free.push_back(it->index);
		return m_data.erase(it);
	}

	template <typename T>
	void SparseArray<T>::clear()
	{
		m_data.clear();
		m_ids .clear();
		m_free.clear();
		m_ids.emplace_back(std::nullopt);
	}

	template <typename T>
	void SparseArray<T>::swap(SparseArray& other) noexcept
	{
		std::swap(m_data, other.m_data);
		std::swap(m_ids , other.m_ids );
		std::swap(m_free, other.m_free);
	}


	/// -- non-modifiers ----------------------------------------------------

	template <typename T>
	auto SparseArray<T>::at(size_type index) -> reference
	{
		if (index >= m_ids.size() or !m_ids[index].has_value()) {
			throw std::out_of_range("SparseArray::at: invalid index");
		}
		return (*m_ids[index])->value;
	}

	template <typename T>
	auto SparseArray<T>::at(size_type index) const -> const_reference
	{
		if (index >= m_ids.size() or !m_ids[index].has_value()) {
			throw std::out_of_range("SparseArray::at: invalid index");
		}
		return (*m_ids[index])->value;
	}

	template <typename T>
	auto SparseArray<T>::operator[](size_type index) -> reference
	{
		assert(index < m_ids.size() && m_ids[index].has_value());
		return m_ids[index].value()->value;
	}

	template <typename T>
	auto SparseArray<T>::operator[](size_type index) const -> const_reference
	{
		assert(index < m_ids.size() && m_ids[index].has_value());
		return m_ids[index].value()->value;
	}

	template <typename T>
	bool SparseArray<T>::contains(size_type index) const
	{
		return index < m_ids.size() and m_ids[index].has_value();
	}

	template <typename T>
	auto SparseArray<T>::size() const noexcept -> size_type
	{
		return m_data.size();
	}

	template <typename T>
	auto SparseArray<T>::capacity() const noexcept -> size_type
	{
		return m_ids.size();
	}

	template <typename T>
	bool SparseArray<T>::empty() const noexcept
	{
		return m_data.empty();
	}


	/// -- iterators --------------------------------------------------------

	template <typename T>
	typename SparseArray<T>::iterator SparseArray<T>::begin() noexcept
	{
		return m_data.begin();
	}

	template <typename T>
	typename SparseArray<T>::const_iterator SparseArray<T>::begin() const noexcept
	{
		return m_data.begin();
	}

	template <typename T>
	typename SparseArray<T>::const_iterator SparseArray<T>::cbegin() const noexcept
	{
		return m_data.cbegin();
	}

	template <typename T>
	typename SparseArray<T>::iterator SparseArray<T>::end() noexcept
	{
		return m_data.end();
	}

	template <typename T>
	typename SparseArray<T>::const_iterator SparseArray<T>::end() const noexcept
	{
		return m_data.end();
	}

	template <typename T>
	typename SparseArray<T>::const_iterator SparseArray<T>::cend() const noexcept
	{
		return m_data.cend();
	}


	/// -- lifecycle --------------------------------------------------------

	template <typename T>
	SparseArray<T>::SparseArray(SparseArray&& other) noexcept :
		m_data(std::exchange(other.m_data, {})),
		m_ids (std::exchange(other.m_ids, { std::nullopt })),
		m_free(std::exchange(other.m_free, {}))
	{}

	template <typename T>
	SparseArray<T>& SparseArray<T>::operator=(SparseArray&& other) noexcept
	{
		if (this == &other) { return *this; }
		m_data = std::exchange(other.m_data, {});
		m_ids  = std::exchange(other.m_ids, { std::nullopt });
		m_free = std::exchange(other.m_free, {});
		return *this;
	}

	template <typename T>
	SparseArray<T>::SparseArray(const SparseArray& other)
	{
		m_ids.resize(other.m_ids.size());

		for (size_type i{ 1 }; i < other.m_ids.size(); ++i) {
			if (other.m_ids[i].has_value()) {
				m_data.push_back(*( other.m_ids[i].value() ));
				m_ids[i] = std::prev(m_data.end());
			}
			else {
				m_free.push_back(i);
			}
		}
	}

	template <typename T>
	SparseArray<T>& SparseArray<T>::operator=(const SparseArray& other)
	{
		if (this == &other) { return *this; }
		SparseArray temp(other);
		swap(temp);
		return *this;
	}


} // namespace cst



