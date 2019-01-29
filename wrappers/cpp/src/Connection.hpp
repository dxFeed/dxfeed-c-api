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

	dxf_connection_t handle_;
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

	~Connection() {
		dxf_close_connection(handle_);
	}
};



