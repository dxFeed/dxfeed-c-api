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

}

#include "BinaryQTPParser.hpp"
#include "Connection.hpp"

namespace dx {

void BinaryQTPParser::setCurrentTimeMark(int currentTimeMark) { currentTimeMark_ = currentTimeMark; }

BinaryQTPParser::BinaryQTPParser(Connection *connection)
	: connection_{connection}, bufferedInputConnectionContext_{nullptr}, currentTimeMark_{0} {}

void BinaryQTPParser::setContext(void *bufferedInputConnectionContext) {
	bufferedInputConnectionContext_ = bufferedInputConnectionContext;
}

void BinaryQTPParser::parseHeartbeat() {
	auto length = dx_get_in_buffer_limit(bufferedInputConnectionContext_) -
		dx_get_in_buffer_position(bufferedInputConnectionContext_);

	//skip empty heartbeat
	if (length == 0) {
		return;
	}

	auto payload = HeartbeatPayload();

	if (payload.parseFrom(bufferedInputConnectionContext_)) {
		connection_->processIncomingHeartbeatPayload(payload);
	}
}

}  // namespace dx
