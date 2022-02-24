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

extern "C" {

#include "DXPMessageData.h"

}

#include "HeartbeatPayload.hpp"

namespace dx {

class Connection;

// bufferedOutputConnectionContext_ should be set and locked before any usage of compose* or write* functions
class BinaryQTPComposer {
	Connection* connection_;
	void* bufferedOutputConnectionContext_;
	int totalLag_;

protected:

	int writeMessageHeader(dx_message_type_t messageType) const;

	int finishComposingMessage() const;

	int writeEmptyHeartbeatMessage() const;

	int writeHeartbeatMessage(const HeartbeatPayload& heartbeatPayload) const;

public:

	explicit BinaryQTPComposer(Connection* connection);

	void setContext(void* bufferedOutputConnectionContext);

	int composeEmptyHeartbeatMessage() const;

	int composeHeartbeatMessage(const HeartbeatPayload& heartbeatPayload) const;

	void addComposingLag(int composingLagMark);

	int getTotalLagAndClear();
};

}
