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

#include "Connection.h"

#include "BufferedInput.h"
#include "BufferedOutput.h"
#include "ConnectionContextData.h"
#include "DXErrorCodes.h"
#include "DXErrorHandling.h"
#include "DXNetwork.h"

}

#include "Connection.hpp"

namespace dx {

void Connection::processIncomingHeartbeatPayload(const HeartbeatPayload& heartbeatPayload) {
	int currentTimeMark = TimeMarkUtil::currentTimeMark();

	if (heartbeatPayload.hasTimeMark()) {
		lastDeltaMark_ = TimeMarkUtil::signedDeltaMark(currentTimeMark - heartbeatPayload.getTimeMark());

		if (heartbeatPayload.hasDeltaMark()) {
			connectionRttMark_ = TimeMarkUtil::signedDeltaMark(lastDeltaMark_ + heartbeatPayload.getDeltaMark());
		}
	}

	incomingLagMark_ = heartbeatPayload.getLagMark() + connectionRttMark_;
	parser_->setCurrentTimeMark(computeTimeMark(currentTimeMark));

	if (connectionHandle_ != nullptr) {
		auto connectionContextData = dx_get_connection_context_data(connectionHandle_);

		if (connectionContextData->on_server_heartbeat_notifier != nullptr) {
			connectionContextData->on_server_heartbeat_notifier(
				connectionHandle_, static_cast<dxf_long_t>(heartbeatPayload.getTimeMillis()),
				heartbeatPayload.getLagMark(), connectionRttMark_,
				connectionContextData->on_server_heartbeat_notifier_user_data);
		}
	}
}

int Connection::computeTimeMark(int currentTimeMark) const {
	int mark =
		static_cast<int>(static_cast<unsigned>(currentTimeMark - incomingLagMark_) & TimeMarkUtil::TIME_MARK_MASK);

	if (mark == 0) {
		mark = 1;  // make sure it is non-zero to distinguish it from N/A time mark
	}

	return mark;
}

int Connection::createOutgoingHeartbeat() {
	auto payload = HeartbeatPayload();

	payload.setTimeMillis(
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
			.count());
	payload.setTimeMark(TimeMarkUtil::currentTimeMark());
	payload.setLagMark(composer_->getTotalLagAndClear());

	int deltaMark = lastDeltaMark_;

	if (deltaMark != DELTA_MARK_UNKNOWN) {
		payload.setDeltaMark(deltaMark);
	}

	return composer_->composeHeartbeatMessage(payload);
}

Connection::Connection(dxf_connection_t connectionHandle, void* bufferedOutputConnectionContext,
					   void* bufferedInputConnectionContext)
	: connectionHandle_{connectionHandle},
	  lastDeltaMark_{DELTA_MARK_UNKNOWN},
	  connectionRttMark_{0},
	  incomingLagMark_{0} {
	composer_ = std::unique_ptr<BinaryQTPComposer>(new BinaryQTPComposer(this));
	composer_->setContext(bufferedOutputConnectionContext);
	parser_ = std::unique_ptr<BinaryQTPParser>(new BinaryQTPParser(this));
	parser_->setContext(bufferedInputConnectionContext);
}

void Connection::processIncomingHeartbeat() { parser_->parseHeartbeat(); }

}  // namespace dx

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_css_connection_impl) {
	auto bufferedInputConnectionContext = dx_get_buffered_input_connection_context(connection);
	auto bufferedOutputConnectionContext = dx_get_buffered_output_connection_context(connection);

	if (bufferedInputConnectionContext == nullptr || bufferedOutputConnectionContext == nullptr) {
		return dx_set_error_code(dx_cec_connection_context_not_initialized);
	}

	auto connectionImpl =
		new (std::nothrow) dx::Connection{connection, bufferedOutputConnectionContext, bufferedInputConnectionContext};

	if (!dx_set_subsystem_data(connection, dx_css_connection_impl, static_cast<void*>(connectionImpl))) {
		delete connectionImpl;

		return false;
	}

	return true;
}

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_css_connection_impl) {
	int res = true;
	auto connectionImpl = static_cast<dx::Connection*>(dx_get_subsystem_data(connection, dx_css_connection_impl, &res));

	if (connectionImpl == nullptr) {
		return res;
	}

	delete connectionImpl;

	return res;
}

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_css_connection_impl) { return true; }

void* dx_get_connection_impl(dxf_connection_t connection) {
	return dx_get_subsystem_data(connection, dx_css_connection_impl, nullptr);
}

int dx_connection_create_outgoing_heartbeat(void* connection_impl) {
	if (connection_impl == nullptr) {
		return false;
	}

	auto connection = static_cast<dx::Connection*>(connection_impl);

	return connection->createOutgoingHeartbeat();
}

int dx_connection_process_incoming_heartbeat(void* connection_impl) {
	if (connection_impl == nullptr) {
		return false;
	}

	auto connection = static_cast<dx::Connection*>(connection_impl);

	connection->processIncomingHeartbeat();

	return true;
}
