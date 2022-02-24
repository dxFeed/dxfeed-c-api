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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>

#include "BinaryQTPComposer.hpp"
#include "BinaryQTPParser.hpp"
#include "DXTypes.h"
#include "HeartbeatPayload.hpp"
#include "TimeMarkUtil.hpp"

namespace dx {

class Connection {
	static const int DELTA_MARK_UNKNOWN = (std::numeric_limits<int>::max)();

	dxf_connection_t connectionHandle_;
	std::atomic<int> lastDeltaMark_;
	int connectionRttMark_;	 // assume rtt = 0 until we know it
	int incomingLagMark_;	 // includes connection rtt

	std::unique_ptr<BinaryQTPComposer> composer_;
	std::unique_ptr<BinaryQTPParser> parser_;

	int computeTimeMark(int currentTimeMark) const;

public:
	Connection(dxf_connection_t connectionHandle, void* bufferedOutputConnectionContext,
			   void* bufferedInputConnectionContext);

	void processIncomingHeartbeatPayload(const HeartbeatPayload& heartbeatPayload);

	int createOutgoingHeartbeat();

	void processIncomingHeartbeat();
};

}  // namespace dx
