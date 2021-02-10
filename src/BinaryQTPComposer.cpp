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

#include "BinaryQTPComposer.h"

#include "BufferedInput.h"
#include "BufferedOutput.h"

#ifdef __cplusplus
}
#endif

#include "BinaryQTPComposer.hpp"

namespace dx {

int BinaryQTPComposer::writeEmptyHeartbeatMessage(void *bufferedOutputConnectionContext) const {
	if (!dx_write_byte(bufferedOutputConnectionContext, 0)) {
		return false;
	}

	return true;
}

int BinaryQTPComposer::writeHeartbeatMessage(void *bufferedOutputConnectionContext,
											 const HeartbeatPayload &heartbeatPayload) const {


	return true;
}

}  // namespace dx

int dx_compose_empty_heartbeat(void* qtp_composer, void* buffered_output_connection_context) {
	if (qtp_composer == nullptr || buffered_output_connection_context == nullptr) {
		return false;
	}

	auto composer = reinterpret_cast<dx::BinaryQTPComposer*>(qtp_composer);

	return composer->writeEmptyHeartbeatMessage(buffered_output_connection_context);
}

int dx_compose_heartbeat(void* qtp_composer, void* buffered_output_connection_context, const void* heartbeat_payload) {
	if (qtp_composer == nullptr || buffered_output_connection_context == nullptr) {
		return false;
	}

	if (heartbeat_payload == nullptr) {
		return dx_compose_empty_heartbeat(qtp_composer, buffered_output_connection_context);
	}

	auto composer = reinterpret_cast<dx::BinaryQTPComposer*>(qtp_composer);
	auto heartbeatPayload = reinterpret_cast<const dx::HeartbeatPayload*>(heartbeat_payload);

	return composer->writeHeartbeatMessage(buffered_output_connection_context, *heartbeatPayload);
}