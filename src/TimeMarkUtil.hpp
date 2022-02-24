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

#include <chrono>

namespace dx {
namespace TimeMarkUtil {
static const unsigned TIME_MARK_MASK = 0x3fffffffU;

inline static int currentTimeMark() {
	return static_cast<int>(
		static_cast<unsigned>(std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now())
								  .time_since_epoch()
								  .count()) &
		TIME_MARK_MASK);
}

inline static int signedDeltaMark(int mark) {
	return static_cast<int>(static_cast<unsigned>(mark) << 2U) >> 2U;
}

}  // namespace TimeMarkUtil
}  // namespace dx
