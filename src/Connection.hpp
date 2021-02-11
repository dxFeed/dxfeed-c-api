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

#pragma once

#include <limits>
#include <memory>
#include <chrono>

#include "BinaryQTPComposer.hpp"
#include "BinaryQTPParser.hpp"
#include "HeartbeatPayload.hpp"
#include "TimeMarkUtil.hpp"

namespace dx {

struct Connection {
	static const int DELTA_MARK_UNKNOWN = std::numeric_limits<int>::max();

	std::unique_ptr<BinaryQTPComposer> composer;
	std::unique_ptr<BinaryQTPParser> parser;

	// TODO: atomic
	int lastDeltaMark = DELTA_MARK_UNKNOWN;
	int connectionRttMark = 0;
	int incomingLagMark;

	void processIncomingHeartBeat(const HeartbeatPayload& heartbeatPayload);

	void createOutgoingHeartbeat(long currentTimeMillis, int currentTimeMark, int lagMark);

	int computeTimeMark(int currentTimeMark) const;
};

}  // namespace dx