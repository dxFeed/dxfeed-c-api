/*
* The contents of this file are subject to the Mozilla Public License Version
* 2.0 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Initial Developer of the Original Code is Devexperts LLC.
* Portions created by the Initial Developer are Copyright (C) 2010
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*
*/

#pragma once

#include <utility>
#include <cstddef>
#include <functional>
#include <type_traits>

namespace dx {

/**
 * Combines the passed hash (first parameter) with the calculated hashes for the rest of the parameters and
 * substitutes it for the first parameter. Does nothing, since the passed hash has nothing to combine with.
 *
 * @param[in, out] seed The passed hash with which to combine the computed hashes.
 */
inline void hashCombineInPlace(std::size_t& seed) {}

/**
 * Combines the passed hash (first parameter) with the calculated hashes for the rest of the parameters and
 * substitutes it for the first parameter.
 *
 * @tparam T The type of the second parameter.
 * @param[in, out] seed The passed hash with which to combine the computed hashes.
 * @param v The parameter whose hash is to be computed and combined with the first hash.
 */
template <typename T>
inline void hashCombineInPlace(std::size_t& seed, T&& v) {
	using DecayedT = typename std::decay<T>::type;

	seed ^= (std::hash<DecayedT>{}(std::forward<T>(v)) + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

/**
 * Combines the passed hash (first parameter) with the calculated hashes for the rest of the parameters and
 * substitutes it for the first parameter.
 *
 * @tparam T The type of the second parameter.
 * @tparam Ts The types of the rest parameters.
 * @param seed[in, out] The passed hash with which to combine the computed hashes.
 * @param v The second parameter whose hash is to be computed and combined with the first hash.
 * @param vs The remaining parameters whose hashes are to be calculated and combined with the first hash.
 */
template <typename T, typename... Ts>
inline void hashCombineInPlace(std::size_t& seed, T&& v, Ts&&... vs) {
	hashCombineInPlace(seed, std::forward<T>(v));
	hashCombineInPlace(seed, std::forward<Ts>(vs)...);
}

/**
 * Calculates the parameter hashes, combines them, and returns them as a result.
 *
 * @tparam T The type of the first parameter.
 * @tparam Ts The types of the rest parameters.
 * @param v The value of the first parameter.
 * @param vs The values of the rest parameters.
 * @return The combined hash of all parameters.
 */
template <typename T, typename... Ts>
inline std::size_t hashCombine(T&& v, Ts&&... vs) {
	using DecayedT = typename std::decay<T>::type;
	auto seed = std::hash<DecayedT>{}(std::forward<T>(v));

	hashCombineInPlace(seed, std::forward<Ts>(vs)...);

	return seed;
}

}  // namespace dx