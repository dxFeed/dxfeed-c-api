#pragma once

#include <utility>
#include <cstddef>

namespace dx {

inline void hashCombineInPlace(std::size_t& seed) {}

template <typename T>
inline void hashCombineInPlace(std::size_t& seed, T&& v) {
	using U = typename std::decay<T>::type;

	seed ^= (std::hash<U>{}(std::forward<T>(v)) + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

template <typename T, typename... Ts>
inline void hashCombineInPlace(std::size_t& seed, T&& v, Ts&&... vs) {
	hashCombineInPlace(seed, std::forward<T>(v));
	hashCombineInPlace(seed, std::forward<Ts>(vs)...);
}

template <typename... Ts>
inline std::size_t hashCombine(std::size_t seed, Ts&&... vs) {
	hashCombineInPlace(seed, std::forward<Ts>(vs)...);

	return seed;
}

}  // namespace dx