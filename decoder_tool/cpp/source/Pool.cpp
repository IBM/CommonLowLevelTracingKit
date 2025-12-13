// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "CommonLowLevelTracingKit/decoder/Pool.hpp"

namespace CommonLowLevelTracingKit::decoder::source {

	// ============================================================================
	// PooledString Implementation
	// ============================================================================

	PooledString::PooledString(StringPool *pool, std::string_view str)
		: m_pool(pool) {
		assign(str);
	}

	PooledString::PooledString(StringPool *pool, std::string &&str)
		: m_pool(pool) {
		assign(std::string_view{str});
	}

	PooledString::~PooledString() {
		clear();
	}

	PooledString::PooledString(PooledString &&other) noexcept
		: m_pool(other.m_pool)
		, m_data(other.m_data)
		, m_size(other.m_size)
		, m_uses_pool(other.m_uses_pool) {
		other.m_data = nullptr;
		other.m_size = 0;
		other.m_uses_pool = false;
	}

	PooledString &PooledString::operator=(PooledString &&other) noexcept {
		if (this != &other) {
			clear();
			m_pool = other.m_pool;
			m_data = other.m_data;
			m_size = other.m_size;
			m_uses_pool = other.m_uses_pool;
			other.m_data = nullptr;
			other.m_size = 0;
			other.m_uses_pool = false;
		}
		return *this;
	}

	void PooledString::assign(std::string_view str) {
		clear();
		if (str.empty()) return;

		m_size = str.size();
		if (m_pool != nullptr && str.size() < STRING_SLOT_SIZE) {
			// Use pool
			m_data = static_cast<char *>(m_pool->allocate());
			if (m_data != nullptr) {
				std::memcpy(m_data, str.data(), str.size());
				m_data[str.size()] = '\0';
				m_uses_pool = true;
				return;
			}
		}
		// Fall back to heap
		m_data = new char[str.size() + 1];
		std::memcpy(m_data, str.data(), str.size());
		m_data[str.size()] = '\0';
		m_uses_pool = false;
	}

	void PooledString::clear() noexcept {
		if (m_data != nullptr) {
			if (m_uses_pool && m_pool != nullptr) {
				m_pool->deallocate(m_data);
			} else {
				delete[] m_data;
			}
			m_data = nullptr;
			m_size = 0;
			m_uses_pool = false;
		}
	}

	std::string_view PooledString::view() const noexcept {
		if (m_data == nullptr) return {};
		return {m_data, m_size};
	}

	const char *PooledString::c_str() const noexcept {
		if (m_data == nullptr) return "";
		return m_data;
	}

	// ============================================================================
	// GlobalStringPool Implementation
	// ============================================================================

	StringPool &GlobalStringPool::instance() {
		static StringPool pool{4}; // Start with 4 blocks = 8192 string slots
		return pool;
	}

	PooledString GlobalStringPool::make(std::string_view str) {
		return PooledString{&instance(), str};
	}

} // namespace CommonLowLevelTracingKit::decoder::source
