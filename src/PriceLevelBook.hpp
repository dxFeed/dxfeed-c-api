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

extern "C" {

#include "DXFeed.h"
}

#include <algorithm>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>
#include <cassert>
#include <cmath>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "StringConverter.hpp"

namespace dx {

struct OrderData {
	dxf_long_t index = 0;
	double price = std::numeric_limits<double>::quiet_NaN();
	double size = std::numeric_limits<double>::quiet_NaN();
	dxf_long_t time = 0;
	dxf_order_side_t side = dxf_osd_undefined;

	OrderData() = default;

	explicit OrderData(dxf_long_t newIndex, double newPrice, double newSize, dxf_long_t newTime,
					   dxf_order_side_t newSide)
		: index{newIndex}, price{newPrice}, size{newSize}, time{newTime}, side{newSide} {}
};

struct PriceLevel {
	double price = std::numeric_limits<double>::quiet_NaN();
	double size = std::numeric_limits<double>::quiet_NaN();
	std::int64_t time = 0;

	PriceLevel() = default;

	PriceLevel(double newPrice, double newSize, std::int64_t newTime)
		: price{newPrice}, size{newSize}, time{newTime} {};

	friend bool operator<(const PriceLevel& a, const PriceLevel& b) {
		if (std::isnan(b.price)) return true;
		if (std::isnan(a.price)) return false;

		return a.price < b.price;
	}
};

struct PriceLevelChanges {
	std::vector<PriceLevel> asks{};
	std::vector<PriceLevel> bids{};

	PriceLevelChanges() = default;

	PriceLevelChanges(std::vector<PriceLevel> newAsks, std::vector<PriceLevel> newBids)
		: asks{std::move(newAsks)}, bids{std::move(newBids)} {}
};

struct PriceLevelChangesSet {
	PriceLevelChanges additions{};
	PriceLevelChanges updates{};
	PriceLevelChanges removals{};

	PriceLevelChangesSet() = default;

	PriceLevelChangesSet(PriceLevelChanges newAdditions, PriceLevelChanges newUpdates, PriceLevelChanges newRemovals)
		: additions{std::move(newAdditions)}, updates{std::move(newUpdates)}, removals{std::move(newRemovals)} {}
};

namespace bmi = boost::multi_index;

using PriceLevelContainer = bmi::multi_index_container<
	PriceLevel,
	bmi::indexed_by<bmi::random_access<>, bmi::ordered_unique<bmi::member<PriceLevel, double, &PriceLevel::price>>>>;

class PriceLevelBook final {
	dxf_snapshot_t snapshot_;
	std::string symbol_;
	std::string source_;
	std::size_t levelsNumber_;
	PriceLevelContainer asks_;
	PriceLevelContainer bids_;
	std::unordered_map<dxf_long_t, OrderData> orderDataSnapshot_;
	bool isValid_;
	std::mutex mutex_;

	std::function<void(const PriceLevelChanges&)> onNewBook_;
	std::function<void(const PriceLevelChanges&)> onBookUpdate_;
	std::function<void(const PriceLevelChangesSet&)> onIncrementalChange_;

	static bool isZeroPriceLevel(const PriceLevel& pl) {
		return std::abs(pl.size) < std::numeric_limits<double>::epsilon();
	};

	static bool areEqualPrices(double price1, double price2) {
		return std::abs(price1 - price2) < std::numeric_limits<double>::epsilon();
	}

	PriceLevelBook(std::string symbol, std::string source, std::size_t levelsNumber = 0)
		: snapshot_{nullptr},
		  symbol_{std::move(symbol)},
		  source_{std::move(source)},
		  levelsNumber_{levelsNumber},
		  asks_{},
		  bids_{},
		  orderDataSnapshot_{},
		  isValid_{false},
		  mutex_{} {}

	// Process the tx\snapshot data, converts it to PL changes. Also, changes the orderDataSnapshot_
	PriceLevelChanges convertToUpdates(const dxf_snapshot_data_ptr_t snapshotData) {
		assert(snapshotData->records_count != 0);
		assert(snapshotData->event_type != dx_eid_order);

		std::set<PriceLevel> askUpdates{};
		std::set<PriceLevel> bidUpdates{};

		auto orders = reinterpret_cast<const dxf_order_t*>(snapshotData->records);

		auto isOrderRemoval = [](const dxf_order_t& o) {
			return (o.event_flags & dxf_ef_remove_event) != 0 || o.size == 0 || std::isnan(o.size);
		};

		auto processOrderAddition = [&bidUpdates, &askUpdates](const dxf_order_t& order) {
			auto& updatesSide = order.side == dxf_osd_buy ? bidUpdates : askUpdates;
			auto priceLevelChange = PriceLevel{order.price, order.size, order.time};
			auto foundPriceLevel = updatesSide.find(priceLevelChange);

			if (foundPriceLevel != updatesSide.end()) {
				priceLevelChange.size = foundPriceLevel->size + priceLevelChange.size;
				updatesSide.erase(foundPriceLevel);
			}

			updatesSide.insert(priceLevelChange);
		};

		auto processOrderRemoval = [&bidUpdates, &askUpdates](const dxf_order_t& order,
															  const OrderData& foundOrderData) {
			auto& updatesSide = foundOrderData.side == dxf_osd_buy ? bidUpdates : askUpdates;
			auto priceLevelChange = PriceLevel{foundOrderData.price, -foundOrderData.size, order.time};
			auto foundPriceLevel = updatesSide.find(priceLevelChange);

			if (foundPriceLevel != updatesSide.end()) {
				priceLevelChange.size = foundPriceLevel->size + priceLevelChange.size;
				updatesSide.erase(foundPriceLevel);
			}

			if (!isZeroPriceLevel(priceLevelChange)) {
				updatesSide.insert(priceLevelChange);
			}
		};

		for (std::size_t i = 0; i < snapshotData->records_count; i++) {
			auto order = orders[i];
			auto removal = isOrderRemoval(order);
			auto foundOrderDataIt = orderDataSnapshot_.find(order.index);

			if (foundOrderDataIt == orderDataSnapshot_.end()) {
				if (removal) {
					continue;
				}

				processOrderAddition(order);
				orderDataSnapshot_[order.index] =
					OrderData{order.index, order.price, order.size, order.time, order.side};
			} else {
				const auto& foundOrderData = foundOrderDataIt->second;

				if (removal) {
					processOrderRemoval(order, foundOrderData);
					orderDataSnapshot_.erase(foundOrderData.index);
				} else {
					if (order.side != foundOrderData.side) {
						processOrderRemoval(order, foundOrderData);
					}

					processOrderAddition(order);
					orderDataSnapshot_[foundOrderData.index] =
						OrderData{order.index, order.price, order.size, order.time, order.side};
				}
			}
		}

		return {std::vector<PriceLevel>{askUpdates.begin(), askUpdates.end()},
				std::vector<PriceLevel>{bidUpdates.rbegin(), bidUpdates.rend()}};
	}

	// TODO: price levels number
	PriceLevelChangesSet applyUpdates(const PriceLevelChanges& priceLevelUpdates) {
		PriceLevelChanges additions{};
		PriceLevelChanges updates{};
		PriceLevelChanges removals{};

		// We generate lists of additions, updates, removals
		for (const auto& updateAsk : priceLevelUpdates.asks) {
			auto found = std::lower_bound(asks_.begin(), asks_.end(), updateAsk);

			if (found == asks_.end() || !areEqualPrices(found->price, updateAsk.price)) {
				additions.asks.push_back(updateAsk);
			} else {
				auto newPriceLevelChange = *found;

				newPriceLevelChange.size += updateAsk.size;
				newPriceLevelChange.time = updateAsk.time;

				if (isZeroPriceLevel(newPriceLevelChange)) {
					removals.asks.push_back(*found);
				} else {
					updates.asks.push_back(newPriceLevelChange);
				}
			}
		}

		for (const auto& updateBid : priceLevelUpdates.bids) {
			auto found = std::lower_bound(bids_.begin(), bids_.end(), updateBid);

			if (found == bids_.end() || !areEqualPrices(found->price, updateBid.price)) {
				additions.bids.push_back(updateBid);
			} else {
				auto newPriceLevelChange = *found;

				newPriceLevelChange.size += updateBid.size;
				newPriceLevelChange.time = updateBid.time;

				if (isZeroPriceLevel(newPriceLevelChange)) {
					removals.bids.push_back(*found);
				} else {
					updates.bids.push_back(newPriceLevelChange);
				}
			}
		}

		std::set<PriceLevel> askRemovals{};
		std::set<PriceLevel> bidRemovals{};
		std::set<PriceLevel> askAdditions{};
		std::set<PriceLevel> bidAdditions{};
		std::set<PriceLevel> askUpdates{};
		std::set<PriceLevel> bidUpdates{};

		for (const auto& askRemoval : removals.asks) {
			if (asks_.empty()) continue;

			// Determine what will be the removal given the number of price levels.
			if (levelsNumber_ == 0 || asks_.size() <= levelsNumber_ || askRemoval.price < asks_[levelsNumber_].price) {
				askRemovals.insert(askRemoval);
			}

			// Determine what will be the shift in price levels after removal.
			if (levelsNumber_ != 0 && asks_.size() > levelsNumber_ && askRemoval.price < asks_[levelsNumber_].price) {
				askAdditions.insert(asks_[levelsNumber_]);
			}

			// remove price level by price
			asks_.get<1>().erase(askRemoval.price);
		}

		for (const auto& askAddition : additions.asks) {
			// We determine what will be the addition of the price level, taking into account the possible quantity.
			if (levelsNumber_ == 0 || asks_.size() < levelsNumber_ ||
				askAddition.price < asks_[levelsNumber_ - 1].price) {
				askAdditions.insert(askAddition);
			}

			// We determine what will be the shift after adding
			if (levelsNumber_ != 0 && asks_.size() >= levelsNumber_ &&
				askAddition.price < asks_[levelsNumber_ - 1].price) {
				const auto& toRemove = asks_[levelsNumber_ - 1];

				// We take into account the possibility that the previously added price level will be deleted.
				if (askAdditions.count(toRemove) > 0) {
					askAdditions.erase(toRemove);
				} else {
					askRemovals.insert(toRemove);
				}
			}

			asks_.get<1>().insert(askAddition);
		}

		for (const auto& askUpdate : updates.asks) {
			if (levelsNumber_ == 0 || asks_.get<1>().count(askUpdate.price) > 0) {
				askUpdates.insert(askUpdate);
			}

			asks_.get<1>().erase(askUpdate.price);
			asks_.get<1>().insert(askUpdate);
		}

		for (const auto& bidRemoval : removals.bids) {
			if (bids_.empty()) continue;

			// Determine what will be the removal given the number of price levels.
			if (levelsNumber_ == 0 || bids_.size() <= levelsNumber_ ||
				bidRemoval.price > bids_[bids_.size() - 1 - levelsNumber_].price) {
				bidRemovals.insert(bidRemoval);
			}

			// Determine what will be the shift in price levels after removal.
			if (levelsNumber_ != 0 && bids_.size() > levelsNumber_ &&
				bidRemoval.price > bids_[bids_.size() - 1 - levelsNumber_].price) {
				bidAdditions.insert(bids_[bids_.size() - 1 - levelsNumber_]);
			}

			// remove price level by price
			bids_.get<1>().erase(bidRemoval.price);
		}

		for (const auto& bidAddition : additions.bids) {
			// We determine what will be the addition of the price level, taking into account the possible quantity.
			if (levelsNumber_ == 0 || bids_.size() < levelsNumber_ ||
				bidAddition.price > bids_[bids_.size() - levelsNumber_].price) {
				bidAdditions.insert(bidAddition);
			}

			// We determine what will be the shift after adding
			if (levelsNumber_ != 0 && bids_.size() >= levelsNumber_ &&
				bidAddition.price > bids_[bids_.size() - levelsNumber_].price) {
				const auto& toRemove = bids_[bids_.size() - levelsNumber_];

				// We take into account the possibility that the previously added price level will be deleted.
				if (bidAdditions.count(toRemove) > 0) {
					bidAdditions.erase(toRemove);
				} else {
					bidRemovals.insert(toRemove);
				}
			}

			bids_.get<1>().insert(bidAddition);
		}

		for (const auto& bidUpdate : updates.bids) {
			if (levelsNumber_ == 0 || bids_.get<1>().count(bidUpdate.price) > 0) {
				bidUpdates.insert(bidUpdate);
			}

			bids_.get<1>().erase(bidUpdate.price);
			bids_.get<1>().insert(bidUpdate);
		}

		return {PriceLevelChanges{std::vector<PriceLevel>{askAdditions.begin(), askAdditions.end()},
								  std::vector<PriceLevel>{bidAdditions.rbegin(), bidAdditions.rend()}},
				PriceLevelChanges{std::vector<PriceLevel>{askUpdates.begin(), askUpdates.end()},
								  std::vector<PriceLevel>{bidUpdates.rbegin(), bidUpdates.rend()}},
				PriceLevelChanges{std::vector<PriceLevel>{askRemovals.begin(), askRemovals.end()},
								  std::vector<PriceLevel>{bidRemovals.rbegin(), bidRemovals.rend()}}};
	}

	[[nodiscard]] std::vector<PriceLevel> getAsks() const {
		return {asks_.begin(),
				(levelsNumber_ == 0 || asks_.size() <= levelsNumber_) ? asks_.end() : asks_.begin() + levelsNumber_};
	}

	[[nodiscard]] std::vector<PriceLevel> getBids() const {
		return {bids_.rbegin(),
				(levelsNumber_ == 0 || bids_.size() <= levelsNumber_) ? bids_.rend() : bids_.rbegin() + levelsNumber_};
	}

public:
	// TODO: move to another thread
	void processSnapshotData(const dxf_snapshot_data_ptr_t snapshotData, int newSnapshot) {
		std::lock_guard<std::mutex> lk(mutex_);

		auto newSnap = newSnapshot != 0;

		if (newSnap) {
			asks_.clear();
			bids_.clear();
			orderDataSnapshot_.clear();
		}

		if (snapshotData->records_count == 0) {
			if (newSnap && onNewBook_) {
				onNewBook_({});
			}

			return;
		}

		auto updates = convertToUpdates(snapshotData);
		auto resultingChangesSet = applyUpdates(updates);

		if (newSnap) {
			if (onNewBook_) {
				onNewBook_(PriceLevelChanges{getAsks(), getBids()});
			}
		} else {
			if (onIncrementalChange_) {
				onIncrementalChange_(resultingChangesSet);
			}

			if (onBookUpdate_) {
				onBookUpdate_(PriceLevelChanges{getAsks(), getBids()});
			}
		}
	}

	~PriceLevelBook() {
		if (isValid_) {
			dxf_close_price_level_book(snapshot_);
		}
	}

	static PriceLevelBook* create(dxf_connection_t connection, const std::string& symbol,
												  const std::string& source, std::size_t levelsNumber) {
		auto plb = new PriceLevelBook(symbol, source, levelsNumber);
		auto wSymbol = StringConverter::utf8ToWString(symbol);
		dxf_snapshot_t snapshot = nullptr;

		dxf_create_order_snapshot(connection, wSymbol.c_str(), source.c_str(), 0, &snapshot);

		dxf_attach_snapshot_inc_listener(
			snapshot,
			[](const dxf_snapshot_data_ptr_t snapshot_data, int new_snapshot, void* user_data) {
				static_cast<PriceLevelBook*>(user_data)->processSnapshotData(snapshot_data, new_snapshot);
			},
			plb);
		plb->isValid_ = true;

		return plb;
	}

	void setOnNewBook(std::function<void(const PriceLevelChanges&)> onNewBookHandler) {
		onNewBook_ = std::move(onNewBookHandler);
	}

	void setOnBookUpdate(std::function<void(const PriceLevelChanges&)> onBookUpdateHandler) {
		onBookUpdate_ = std::move(onBookUpdateHandler);
	}

	void setOnIncrementalChange(std::function<void(const PriceLevelChangesSet&)> onIncrementalChangeHandler) {
		onIncrementalChange_ = std::move(onIncrementalChangeHandler);
	}
};

}  // namespace dx
