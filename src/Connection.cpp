/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
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

#ifdef __cplusplus
extern "C" {
#endif

#include "Connection.h"

#ifdef __cplusplus
}
#endif

#include "Connection.hpp"

namespace dx {

void Connection::processIncomingHeartBeat(const HeartbeatPayload& heartbeatPayload) {
	int currentTimeMark = TimeMarkUtil::currentTimeMark();

	if (heartbeatPayload.hasTimeMark()) {
		lastDeltaMark = TimeMarkUtil::signedDeltaMark(currentTimeMark - heartbeatPayload.getTimeMark());

		if (heartbeatPayload.hasDeltaMark()) {
			connectionRttMark = TimeMarkUtil::signedDeltaMark(lastDeltaMark + heartbeatPayload.getDeltaMark());
		}
	}

	incomingLagMark = heartbeatPayload.getLagMark() + connectionRttMark;
	parser->setCurrentTimeMark(computeTimeMark(currentTimeMark));
}

int Connection::computeTimeMark(int currentTimeMark) const {
	int mark =
		static_cast<int>(static_cast<unsigned>(currentTimeMark - incomingLagMark) & TimeMarkUtil::TIME_MARK_MASK);

	if (mark == 0) {
		mark = 1;  // make sure it is non-zero to distinguish it from N/A time mark
	}

	return mark;
}

void Connection::createOutgoingHeartbeat(long currentTimeMillis, int currentTimeMark, int lagMark) {

}

}  // namespace dx