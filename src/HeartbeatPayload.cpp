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

extern "C" {

#include "BufferedInput.h"
#include "BufferedOutput.h"

}

#include "HeartbeatPayload.hpp"

namespace dx {

HeartbeatPayload::HeartbeatPayload()
	: contentMask_{ContentType::EMPTY}, timeMillis_{}, timeMark_{}, deltaMark_{}, lagMark_{} {}

bool HeartbeatPayload::isEmpty() const { return contentMask_ == ContentType::EMPTY; }

void HeartbeatPayload::clear() {
	contentMask_ = ContentType::EMPTY;
	timeMillis_ = 0ULL;
	timeMark_ = 0;
	deltaMark_ = 0;
	lagMark_ = 0;
}

bool HeartbeatPayload::hasTimeMillis() const { return (contentMask_ & ContentType::TIME_MILLIS) != 0U; }

std::uint64_t HeartbeatPayload::getTimeMillis() const { return timeMillis_; }

void HeartbeatPayload::setTimeMillis(std::uint64_t timeMillis) {
	contentMask_ |= ContentType::TIME_MILLIS;
	timeMillis_ = timeMillis;
}

bool HeartbeatPayload::hasTimeMark() const { return (contentMask_ & ContentType::TIME_MARK) != 0U; }

int HeartbeatPayload::getTimeMark() const { return timeMark_; }

void HeartbeatPayload::setTimeMark(int timeMark) {
	contentMask_ |= ContentType::TIME_MARK;
	timeMark_ = timeMark;
}

bool HeartbeatPayload::hasDeltaMark() const { return (contentMask_ & ContentType::DELTA_MARK) != 0U; }

int HeartbeatPayload::getDeltaMark() const { return deltaMark_; }

void HeartbeatPayload::setDeltaMark(int deltaMark) {
	contentMask_ |= ContentType::DELTA_MARK;
	deltaMark_ = deltaMark;
}

bool HeartbeatPayload::hasLagMark() const { return (contentMask_ & ContentType::LAG_MARK) != 0U; }

int HeartbeatPayload::getLagMark() const { return lagMark_; }

void HeartbeatPayload::setLagMark(int lagMark) {
	contentMask_ |= ContentType::LAG_MARK;
	lagMark_ = lagMark;
}

bool HeartbeatPayload::parseFrom(void* bufferedInputConnectionContext) {
	dxf_int_t contentMask{};

	if (!dx_read_compact_int(bufferedInputConnectionContext, &contentMask)) {
		return false;
	}

	contentMask_ = static_cast<unsigned>(contentMask);

	if (hasTimeMillis()) {
		dxf_long_t timeMillis{};

		if (!dx_read_compact_long(bufferedInputConnectionContext, &timeMillis)) {
			return false;
		}

		timeMillis_ = static_cast<std::uint64_t>(timeMillis);
	}

	if (hasTimeMark()) {
		dxf_int_t timeMark{};

		if (!dx_read_compact_int(bufferedInputConnectionContext, &timeMark)) {
			return false;
		}

		timeMark_ = timeMark;
	}

	if (hasDeltaMark()) {
		dxf_int_t deltaMark{};

		if (!dx_read_compact_int(bufferedInputConnectionContext, &deltaMark)) {
			return false;
		}

		deltaMark_ = deltaMark;
	}

	if (hasLagMark()) {
		dxf_int_t lagMark{};

		if (!dx_read_compact_int(bufferedInputConnectionContext, &lagMark)) {
			return false;
		}

		lagMark_ = lagMark;
	}

	return true;
}

bool HeartbeatPayload::composeTo(void* bufferedOutputConnectionContext) const {
	if (bufferedOutputConnectionContext == nullptr) return false;

	if (!dx_write_compact_int(bufferedOutputConnectionContext, static_cast<const dxf_int_t>(contentMask_))) {
		return false;
	}

	if (hasTimeMillis()) {
		if (!dx_write_compact_long(bufferedOutputConnectionContext, static_cast<const dxf_long_t>(timeMillis_))) {
			return false;
		}
	}

	if (hasTimeMark()) {
		if (!dx_write_compact_int(bufferedOutputConnectionContext, timeMark_)) {
			return false;
		}
	}

	if (hasDeltaMark()) {
		if (!dx_write_compact_int(bufferedOutputConnectionContext, deltaMark_)) {
			return false;
		}
	}

	if (hasLagMark()) {
		if (!dx_write_compact_int(bufferedOutputConnectionContext, lagMark_)) {
			return false;
		}
	}

	return true;
}

}  // namespace dx
