#ifndef UTILS_VALUE_OBFUSCATION_H
#define UTILS_VALUE_OBFUSCATION_H

#include <utils/random.h>

namespace utils
{
	template <typename T, T A, T B>
	class xor_value
	{
	public:
		__forceinline static T get() { return value ^ cipher; }

	private:
		const static volatile inline T value{A ^ B}, cipher{B};
	};

	template <typename T = uintptr_t>
	class encrypted_ptr
	{
	public:
		__forceinline encrypted_ptr() { this->value = this->cipher = RANDOM_SEED; }

		__forceinline explicit encrypted_ptr(T *const value) { *this = encrypted_ptr(uintptr_t(value)); }

		__forceinline explicit encrypted_ptr(uintptr_t value)
		{
			this->cipher = RANDOM_SEED;
			this->value = uintptr_t(value) ^ this->cipher;
		}

		__forceinline T *operator()() const { return operator->(); }

		__forceinline T &operator*() const { return *reinterpret_cast<T *>(this->value ^ this->cipher); }

		__forceinline T *operator->() const { return reinterpret_cast<T *>(this->value ^ this->cipher); }

		__forceinline bool operator==(const encrypted_ptr &other) const
		{
			return this->operator->() == other.operator->();
		}

		explicit __forceinline operator bool() const { return !this->operator!(); }

		__forceinline bool operator!() const { return this->operator->() == nullptr; }

		__forceinline uintptr_t at(ptrdiff_t rel) const { return uintptr_t(operator->()) + rel; }

		__forceinline encrypted_ptr<T> &deref(const size_t amnt)
		{
			for (auto i = 0u; i < amnt; i++)
				*this = encrypted_ptr<T>(*reinterpret_cast<T **>(this->value ^ this->cipher));
			return *this;
		}

	private:
		uintptr_t value{};
		uintptr_t cipher{};
	};

	template <typename T = uintptr_t>
	class encrypted_pptr
	{
	public:
		__forceinline encrypted_pptr() { this->value = this->cipher = RANDOM_SEED; }

		__forceinline explicit encrypted_pptr(T *const value) { *this = encrypted_pptr(uintptr_t(value)); }

		__forceinline explicit encrypted_pptr(uintptr_t value)
		{
			this->cipher = RANDOM_SEED;
			this->value = uintptr_t(value) ^ this->cipher;
		}

		__forceinline T *operator()() const { return operator->(); }

		__forceinline T &operator*() const { return **reinterpret_cast<T **>(this->value ^ this->cipher); }

		__forceinline T *operator->() const { return *reinterpret_cast<T **>(this->value ^ this->cipher); }

		__forceinline bool operator==(const encrypted_pptr &other) const { return this->operator->() == other.operator->(); }

		explicit __forceinline operator bool() const { return !this->operator!(); }

		__forceinline bool operator!() const { return this->operator->() == nullptr; }

		__forceinline uintptr_t at(ptrdiff_t rel) const { return uintptr_t(operator->()) + rel; }

		__forceinline encrypted_pptr<T> &deref(const size_t amnt)
		{
			for (auto i = 0u; i < amnt; i++)
				*this = encrypted_pptr<T>(*reinterpret_cast<T **>(this->value ^ this->cipher));
			return *this;
		}

	private:
		uintptr_t value{};
		uintptr_t cipher{};
	};
} // namespace utils

#define XOR_16(val)                                                                                                    \
	(decltype(val))(::utils::xor_value<uint16_t, (uint16_t)val,                                                        \
									   ::utils::random::_uint<__COUNTER__, 0xFFFF>::value>::get())
#define XOR_32(val)                                                                                                    \
	(decltype(val))(::utils::xor_value<uint32_t, (uint32_t)val,                                                        \
									   ::utils::random::_uint<__COUNTER__, 0xFFFFFFFF>::value>::get())
#define XOR_64(val)                                                                                                    \
	(decltype(val))(::utils::xor_value<uint64_t, (uint64_t)val,                                                        \
									   ::utils::random::_uint64<__COUNTER__, 0xFFFFFFFFFFFFFFFF>::value>::get())

#endif // UTILS_VALUE_OBFUSCATION_H
