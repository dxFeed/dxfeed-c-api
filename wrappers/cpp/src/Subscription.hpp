#pragma once

#include <memory>
#include <vector>
#include <DXFeed.h>
#include <nonstd/string_view.hpp>
#include <nonstd/variant.hpp>
#include "StringConverter.hpp"
#include "Event.hpp"

///Not thread-safe subscription wrapper
struct Subscription : std::enable_shared_from_this<Subscription> {
	using Event = nonstd::variant<Greeks, Underlying, Quote>;
	using ListenerType = std::function<void(std::string, std::vector<Event>)>;
	using Ptr = std::shared_ptr<Subscription>;

private:

	dxf_subscription_t handle_;
	ListenerType listener_;

	Subscription() = default;

	Subscription(const Subscription &) = default;

	explicit Subscription(dxf_subscription_t handle) : handle_{handle} {}

public:

	static Ptr create(dxf_connection_t connection, int eventTypes) {
		if (!connection) {
			return nullptr;
		}

		dxf_subscription_t subscription;

		if (dxf_create_subscription(connection, eventTypes, &subscription) == DXF_SUCCESS) {
			return std::shared_ptr<Subscription>(new Subscription(subscription));
		}

		return nullptr;
	}

	bool addSymbol(nonstd::string_view symbol) {
		auto wideSymbol = StringConverter::toWString(symbol.to_string());

		return dxf_add_symbol(handle_, wideSymbol.data()) == DXF_SUCCESS;
	}

	static void
	nativeEventsListener(int eventType, dxf_const_string_t symbolName, const dxf_event_data_t *data, int dataCount, void *userData) {
		auto subscription = reinterpret_cast<Subscription *>(userData);
		auto listener = subscription->getListener();

		if (!listener) {
			return;
		}

		std::string symbol = StringConverter::toString(std::wstring(symbolName));
		std::vector<Event> events(static_cast<std::size_t>(dataCount));

		for (std::size_t i = 0; i < events.size(); i++) {
			if (eventType == DXF_ET_GREEKS) {
				auto greeksArray = reinterpret_cast<const dxf_greeks_t *>(data);
				events[i] = Greeks(symbol, greeksArray[i]);
			} else if (eventType == DXF_ET_UNDERLYING) {
				auto underlyingArray = reinterpret_cast<const dxf_underlying_t *>(data);
				events[i] = Underlying(symbol, underlyingArray[i]);
			} else if (eventType == DXF_ET_QUOTE) {
				auto quoteArray = reinterpret_cast<const dxf_quote_t *>(data);
				events[i] = Quote(symbol, quoteArray[i]);
			}
		}

		listener(symbol, events);
	}

	bool attachListener(ListenerType listener) {
		if (!listener) {
			return false;
		}

		if (listener_) {
			listener_ = listener;

			return true;
		}

		auto result = dxf_attach_event_listener(
				handle_, &Subscription::nativeEventsListener,
				reinterpret_cast<void *>(this));

		if (result == DXF_SUCCESS) {
			listener_ = listener;
		}

		return result == DXF_SUCCESS;
	}

	void close() {
		dxf_close_subscription(handle_);
	}

	dxf_subscription_t getHandle() const {
		return handle_;
	}

	ListenerType getListener() const {
		return listener_;
	}
};



