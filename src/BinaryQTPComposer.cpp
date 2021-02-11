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
#include "ConnectionContextData.h"

#ifdef __cplusplus
}
#endif

#include "BinaryQTPComposer.hpp"
#include <new>

namespace dx {

int BinaryQTPComposer::writeEmptyHeartbeatMessage() const {
	if (!dx_write_byte(bufferedOutputConnectionContext_, 0)) {
		return false;
	}

	return true;
}

int BinaryQTPComposer::writeHeartbeatMessage(const HeartbeatPayload &heartbeatPayload) const {



	return true;
}

BinaryQTPComposer::BinaryQTPComposer(): bufferedOutputConnectionContext_{nullptr} {}

BinaryQTPComposer* BinaryQTPComposer::create() {
	return new (std::nothrow) BinaryQTPComposer{};
}

void BinaryQTPComposer::destroy(BinaryQTPComposer* composer) {
	delete composer;
}

void BinaryQTPComposer::setContext(void* bufferedOutputConnectionContext) {
	bufferedOutputConnectionContext_ = bufferedOutputConnectionContext;
}

int BinaryQTPComposer::composeEmptyHeartbeatMessage() const {
	//TODO: stats

	return writeEmptyHeartbeatMessage();
}


int BinaryQTPComposer::composeHeartbeatMessage(const HeartbeatPayload& heartbeatPayload) const {
	//TODO: stats

	return writeHeartbeatMessage(heartbeatPayload);
}

}  // namespace dx

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_binary_qtp_composer) {
	auto composer = dx::BinaryQTPComposer::create();

	if (!dx_set_subsystem_data(connection, dx_ccs_binary_qtp_composer, static_cast<void*>(composer))) {
		dx::BinaryQTPComposer::destroy(composer);

		return false;
	}

	return true;
}

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_binary_qtp_composer) {
	int res = true;
	auto composer = static_cast<dx::BinaryQTPComposer*>(
		dx_get_subsystem_data(connection, dx_ccs_binary_qtp_composer, &res));

	if (composer == nullptr) {
		return res;
	}

	dx::BinaryQTPComposer::destroy(composer);

	return res;
}

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_binary_qtp_composer) {
	return true;
}


void* dx_create_binary_qtp_composer() {
	return static_cast<void*>(dx::BinaryQTPComposer::create());
}

int dx_destroy_binary_qtp_composer(void* binary_qtp_composer) {
	dx::BinaryQTPComposer::destroy(static_cast<dx::BinaryQTPComposer*>(binary_qtp_composer));

	return true;
}

int dx_set_composer_context(void* binary_qtp_composer, void* buffered_output_connection_context) {
	if (binary_qtp_composer == nullptr) {
		return false;
	}

	auto composer = static_cast<dx::BinaryQTPComposer*>(binary_qtp_composer);

	composer->setContext(buffered_output_connection_context);

	return true;
}

int dx_compose_empty_heartbeat(void* binary_qtp_composer) {
	if (binary_qtp_composer == nullptr) {
		return false;
	}

	auto composer = static_cast<dx::BinaryQTPComposer*>(binary_qtp_composer);

	return composer->composeEmptyHeartbeatMessage();
}

int dx_compose_heartbeat(void* binary_qtp_composer, const void* heartbeat_payload) {
	if (binary_qtp_composer == nullptr) {
		return false;
	}

	if (heartbeat_payload == nullptr) {
		return dx_compose_empty_heartbeat(binary_qtp_composer);
	}

	auto composer = static_cast<dx::BinaryQTPComposer*>(binary_qtp_composer);
	auto heartbeatPayload = static_cast<const dx::HeartbeatPayload*>(heartbeat_payload);

	return composer->composeHeartbeatMessage(*heartbeatPayload);
}

void* dx_get_binary_qtp_composer(dxf_connection_t connection) {
	return dx_get_subsystem_data(connection, dx_ccs_binary_qtp_composer, nullptr);
}