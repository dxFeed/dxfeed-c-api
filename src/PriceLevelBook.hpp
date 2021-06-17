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

#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "EventSubscription.h"

extern "C" {
#include <EventData.h>
}

namespace dx {

enum class PriceLevelBookStatus { Unknown = 0, Begin, Full, Pending };

struct PriceLevelBookListenerContext {
	dxf_price_level_book_listener_t listener;
	void* userData;
};

struct PriceLevelBook;

struct PriceLevelBookSourceConsumer {
	PriceLevelBook* consumer;
	int sourceIndex;
};

struct PriceLevelSide {
	size_t count;
	std::vector<dxf_price_level_element_t> levels;
	std::function<int(dxf_price_level_element_t, dxf_price_level_element_t)> cmp;
	int rebuild;
	int updated;
};

struct PriceLevelBookSource {
	dx_mutex_t guard;

	dxf_ulong_t hash;
	dxf_string_t symbol;
	dxf_char_t source[DXF_RECORD_SUFFIX_SIZE];

	dxf_subscription_t subscription;

	std::vector<dxf_order_t> snapshot;

	PriceLevelBookStatus snapshotStatus;

	std::deque<PriceLevelBookSourceConsumer> consumers;

	PriceLevelSide bids;
	PriceLevelSide finalBids;
	PriceLevelSide asks;
	PriceLevelSide finalAsks;
};

}

namespace std {

//TODO: find plb source by source, symbol and price levels number
template <>
struct hash<dx::PriceLevelBookSource> {
	std::size_t operator()(const dx::PriceLevelBookSource& priceLevelBookSource) const noexcept {
		dxf_ulong_t h1 = dx_symbol_name_hasher(priceLevelBookSource.symbol);
		dxf_ulong_t h2 = dx_symbol_name_hasher(priceLevelBookSource.source);

		return (h1 << 32UL) | h2;
	}
};

}  // namespace std

namespace dx {

struct PriceLevelBookConnectionContext {
	dxf_connection_t connection;
	std::recursive_mutex mutex{};
	std::unordered_set<PriceLevelBookSource> sources;
};

struct PriceLevelBook {
	std::recursive_mutex mutex{};

	dxf_string_t symbol;

	std::vector<PriceLevelBookSource> sources;
	std::vector<PriceLevelBookListenerContext> listeners;

	/* Aliases for source's data structures */
	std::vector<PriceLevelSide*> sourceBids;
	std::vector<PriceLevelSide*> sourceAsks;

	PriceLevelSide bids;
	PriceLevelSide asks;

	/* Public alias for our own data structure */
	dxf_price_level_book_data_t book;

	void *context;
};

}
