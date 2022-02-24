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

#include <memory>
#include <vector>

#include <nonstd/optional.hpp>
#include <nonstd/string_view.hpp>

#include <DXFeed.h>
#include "Subscription.hpp"

///Not thread-safe connection wrapper
struct Connection final : std::enable_shared_from_this<Connection> {
	using Ptr = std::shared_ptr<Connection>;

private:

	dxf_connection_t handle_{};
	std::vector<Subscription::Ptr> subscriptions_;

	Connection() = default;
	Connection(const Connection&) = default;
	explicit Connection(dxf_connection_t handle) : handle_{handle} {}

public:

	static Ptr create(nonstd::string_view address) {
		dxf_connection_t handle;

		if (dxf_create_connection(address.data(), nullptr, nullptr, nullptr, nullptr, nullptr, &handle) ==
			DXF_SUCCESS) {
			return std::shared_ptr<Connection>(new Connection(handle));
		}

		return nullptr;
	}

	nonstd::optional<dxf_connection_status_t> getStatus() const {
		dxf_connection_status_t status;

		if (dxf_get_current_connection_status(handle_, &status) == DXF_SUCCESS) {
			return status;
		}

		return {};
	}

	dxf_connection_t getHandle() const {
		return handle_;
	}

	Subscription::Ptr createSubscription(int eventTypes) {
		auto subscription = Subscription::create(handle_, eventTypes);

		subscriptions_.push_back(subscription);

		return subscription;
	}

	Subscription::Ptr createSubscription(std::initializer_list<dx_event_id_t> eventIds) {
		unsigned eventTypes = 0;

		for (auto type : eventIds) {
			eventTypes |= DX_EVENT_BIT_MASK(type);
		}

		return createSubscription(static_cast<int>(eventTypes));
	}

	~Connection() {
		dxf_close_connection(handle_);
	}
};
