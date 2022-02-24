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

#include <cstdint>

namespace dx {

struct HeartbeatPayload {
	enum ContentType : unsigned {
		EMPTY = 0x0U,		 // no heartbeat payload
		TIME_MILLIS = 0x1U,	 // currentTimeMillis only
		TIME_MARK = 0x2U,
		DELTA_MARK = 0x4U,
		LAG_MARK = 0x8U,
	};

private:

	unsigned contentMask_; // content bits
	std::uint64_t timeMillis_;
	int timeMark_;
	int deltaMark_;
	int lagMark_;

public:

	HeartbeatPayload();

	bool isEmpty() const;

	void clear();

	bool hasTimeMillis() const;

	std::uint64_t getTimeMillis() const;

	void setTimeMillis(std::uint64_t timeMillis);

	bool hasTimeMark() const;

	int getTimeMark() const;

	void setTimeMark(int timeMark);

	bool hasDeltaMark() const;

	int getDeltaMark() const;

	void setDeltaMark(int deltaMark);

	bool hasLagMark() const;

	int getLagMark() const;

	void setLagMark(int lagMark);

	// bufferedOutputConnectionContext should be locked outside this function
	bool composeTo(void* bufferedOutputConnectionContext) const;

	// bufferedInputConnectionContext should be locked outside this function
	bool parseFrom(void* bufferedInputConnectionContext);
};

}  // namespace dx
