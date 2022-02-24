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

#include "BufferedOutput.h"

}

#include <cstring>
#include <new>

#include "BinaryQTPComposer.hpp"
#include "Connection.hpp"

namespace dx {

int BinaryQTPComposer::writeEmptyHeartbeatMessage() const {
	return dx_write_byte(bufferedOutputConnectionContext_, MESSAGE_HEARTBEAT);
}

int BinaryQTPComposer::writeHeartbeatMessage(const HeartbeatPayload& heartbeatPayload) const {
	if (!writeMessageHeader(MESSAGE_HEARTBEAT)) {
		return false;
	}

	if (!heartbeatPayload.composeTo(bufferedOutputConnectionContext_)) {
		return false;
	}

	return finishComposingMessage();
}

BinaryQTPComposer::BinaryQTPComposer(Connection* connection)
	: connection_{connection}, bufferedOutputConnectionContext_{nullptr}, totalLag_{0} {}

void BinaryQTPComposer::setContext(void* bufferedOutputConnectionContext) {
	bufferedOutputConnectionContext_ = bufferedOutputConnectionContext;
}

int BinaryQTPComposer::composeEmptyHeartbeatMessage() const {
	// TODO: stats

	return writeEmptyHeartbeatMessage();
}

int BinaryQTPComposer::composeHeartbeatMessage(const HeartbeatPayload& heartbeatPayload) const {
	// TODO: stats

	return writeHeartbeatMessage(heartbeatPayload);
}

void BinaryQTPComposer::addComposingLag(int composingLagMark) { totalLag_ += composingLagMark; }

int BinaryQTPComposer::getTotalLagAndClear() {
	auto result = totalLag_;

	totalLag_ = 0;

	return result;
}

int BinaryQTPComposer::writeMessageHeader(dx_message_type_t messageType) const {
	// reserve one byte for message length
	if (!dx_write_byte(bufferedOutputConnectionContext_, 0)) {
		return false;
	}

	if (!dx_write_compact_int(bufferedOutputConnectionContext_, messageType)) {
		return false;
	}

	return true;
}

int moveMessageData(void* bufferedOutputConnectionContext, int oldOffset, int newOffset, int dataLength) {
	if (!dx_ensure_capacity(bufferedOutputConnectionContext, newOffset + dataLength)) {
		return false;
	}

	auto* outBuffer = dx_get_out_buffer(bufferedOutputConnectionContext);

	(void)memmove(outBuffer + newOffset, outBuffer + oldOffset, dataLength);

	return true;
}

int BinaryQTPComposer::finishComposingMessage() const {
	auto messageLength =
		dx_get_out_buffer_position(bufferedOutputConnectionContext_) - 1;  // 1 is for the one byte reserved for size
	auto lengthSize = dx_get_compact_size(messageLength);

	if (lengthSize > 1) {
		/* Only one byte was initially reserved. Moving the message buffer */

		if (!moveMessageData(bufferedOutputConnectionContext_, 1, lengthSize, messageLength)) {
			return false;
		}
	}

	dx_set_out_buffer_position(bufferedOutputConnectionContext_, 0);

	if (!dx_write_compact_int(bufferedOutputConnectionContext_, messageLength)) {
		return false;
	}

	dx_set_out_buffer_position(bufferedOutputConnectionContext_, lengthSize + messageLength);

	return true;
}

}  // namespace dx
