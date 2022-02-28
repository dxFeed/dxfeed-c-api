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

#include "DXFeed.h"
#include "EventData.h"
}

#include <algorithm>
#include <cassert>
#include <cmath>
#include <deque>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "StringConverter.hpp"

namespace dx {
static constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

struct OrderData {
	dxf_long_t index = 0;
	double price = NaN;
	double size = NaN;
	dxf_long_t time = 0;
	dxf_order_side_t side = dxf_osd_undefined;

	OrderData() = default;

	explicit OrderData(dxf_long_t newIndex, double newPrice, double newSize, dxf_long_t newTime,
					   dxf_order_side_t newSide)
		: index{newIndex}, price{newPrice}, size{newSize}, time{newTime}, side{newSide} {}
};

struct PriceLevel {
	double price = NaN;
	double size = NaN;
	std::int64_t time = 0;

	PriceLevel() = default;

	PriceLevel(double newPrice, double newSize, std::int64_t newTime)
		: price{newPrice}, size{newSize}, time{newTime} {};

	[[nodiscard]] bool isValid() const { return !std::isnan(price); }

	friend bool operator<(const PriceLevel& a, const PriceLevel& b) {
		if (std::isnan(b.price)) return true;
		if (std::isnan(a.price)) return false;

		return a.price < b.price;
	}
};

struct AskPriceLevel : PriceLevel {
	AskPriceLevel() = default;

	explicit AskPriceLevel(const PriceLevel& priceLevel) : PriceLevel{priceLevel} {}

	AskPriceLevel(double newPrice, double newSize, std::int64_t newTime) : PriceLevel{newPrice, newSize, newTime} {};

	friend bool operator<(const AskPriceLevel& a, const AskPriceLevel& b) {
		if (std::isnan(b.price)) return true;
		if (std::isnan(a.price)) return false;

		return a.price < b.price;
	}
};

struct BidPriceLevel : PriceLevel {
	BidPriceLevel() = default;

	explicit BidPriceLevel(const PriceLevel& priceLevel) : PriceLevel{priceLevel} {}

	BidPriceLevel(double newPrice, double newSize, std::int64_t newTime) : PriceLevel{newPrice, newSize, newTime} {};

	friend bool operator<(const BidPriceLevel& a, const BidPriceLevel& b) {
		if (std::isnan(b.price)) return false;
		if (std::isnan(a.price)) return true;

		return a.price > b.price;
	}
};

struct PriceLevelChanges {
	std::string symbol{};
	std::vector<AskPriceLevel> asks{};
	std::vector<BidPriceLevel> bids{};

	PriceLevelChanges() = default;

	PriceLevelChanges(std::string newSymbol, std::vector<AskPriceLevel> newAsks, std::vector<BidPriceLevel> newBids)
		: symbol{std::move(newSymbol)}, asks{std::move(newAsks)}, bids{std::move(newBids)} {}

	[[nodiscard]] bool isEmpty() const { return asks.empty() && bids.empty(); }
};

struct PriceLevelChangesSet {
	PriceLevelChanges removals{};
	PriceLevelChanges additions{};
	PriceLevelChanges updates{};

	PriceLevelChangesSet() = default;

	PriceLevelChangesSet(PriceLevelChanges newRemovals, PriceLevelChanges newAdditions, PriceLevelChanges newUpdates)
		: removals{std::move(newRemovals)}, additions{std::move(newAdditions)}, updates{std::move(newUpdates)} {}

	[[nodiscard]] bool isEmpty() const { return removals.isEmpty() && additions.isEmpty() && updates.isEmpty(); }
};

class PriceLevelBook final {
	dxf_snapshot_t snapshot_;
	std::string symbol_;
	std::string source_;
	std::size_t levelsNumber_;

	/*
	 * Since we are working with std::set, which does not imply random access, we have to keep the iterators lastAsk,
	 * lastBid for the case with a limit on the number of PLs. These iterators, as well as the look-ahead functions, are
	 * used to find out if we are performing operations on PL within the range given to the user.
	 */
	std::set<AskPriceLevel> asks_;
	std::set<AskPriceLevel>::iterator lastAsk_;
	std::set<BidPriceLevel> bids_;
	std::set<BidPriceLevel>::iterator lastBid_;
	std::unordered_map<dxf_long_t, OrderData> orderDataSnapshot_;
	bool isValid_;
	std::mutex mutex_;

	std::function<void(const PriceLevelChanges&, void*)> onNewBook_;
	std::function<void(const PriceLevelChanges&, void*)> onBookUpdate_;
	std::function<void(const PriceLevelChangesSet&, void*)> onIncrementalChange_;

	void* userData_;

	static bool isZeroPriceLevel(const PriceLevel& pl) {
		return std::abs(pl.size) < std::numeric_limits<double>::epsilon();
	};

	static bool areEqual(double d1, double d2, double epsilon = std::numeric_limits<double>::epsilon()) {
		return std::abs(d1 - d2) < std::numeric_limits<double>::epsilon();
	}

	PriceLevelBook(std::string symbol, std::string source, std::size_t levelsNumber = 0)
		: snapshot_{nullptr},
		  symbol_{std::move(symbol)},
		  source_{std::move(source)},
		  levelsNumber_{levelsNumber},
		  asks_{},
		  lastAsk_{asks_.end()},
		  bids_{},
		  lastBid_{bids_.end()},
		  orderDataSnapshot_{},
		  isValid_{false},
		  mutex_{},
		  userData_{nullptr} {}

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
					if (order.side == foundOrderData.side && areEqual(order.price, foundOrderData.price) &&
						areEqual(order.size, foundOrderData.size)) {
						continue;
					}

					processOrderRemoval(order, foundOrderData);
					processOrderAddition(order);
					orderDataSnapshot_[foundOrderData.index] =
						OrderData{order.index, order.price, order.size, order.time, order.side};
				}
			}
		}

		return {symbol_, std::vector<AskPriceLevel>{askUpdates.begin(), askUpdates.end()},
				std::vector<BidPriceLevel>{bidUpdates.rbegin(), bidUpdates.rend()}};
	}

	// TODO: C++14 lambda
	template <typename PriceLevelUpdateSide, typename PriceLevelStorageSide, typename PriceLevelChangesSide>
	static void generatePriceLevelChanges(const PriceLevelUpdateSide& priceLevelUpdateSide,
										  const PriceLevelStorageSide& priceLevelStorageSide,
										  PriceLevelChangesSide& additions, PriceLevelChangesSide& removals,
										  PriceLevelChangesSide& updates) {
		auto found = priceLevelStorageSide.find(priceLevelUpdateSide);

		if (found == priceLevelStorageSide.end()) {
			additions.push_back(priceLevelUpdateSide);
		} else {
			auto newPriceLevelChange = *found;

			newPriceLevelChange.size += priceLevelUpdateSide.size;
			newPriceLevelChange.time = priceLevelUpdateSide.time;

			if (isZeroPriceLevel(newPriceLevelChange)) {
				removals.push_back(*found);
			} else {
				updates.push_back(newPriceLevelChange);
			}
		}
	}

	template <typename PriceLevelRemovalSide, typename PriceLevelStorageSide, typename PriceLevelChangesSideSet,
			  typename LastElementIter>
	static void processSideRemoval(const PriceLevelRemovalSide& priceLevelRemovalSide,
								   PriceLevelStorageSide& priceLevelStorageSide, PriceLevelChangesSideSet& removals,
								   PriceLevelChangesSideSet& additions, LastElementIter& lastElementIter,
								   std::size_t levelsNumber) {
		if (priceLevelStorageSide.empty()) return;

		auto found = priceLevelStorageSide.find(priceLevelRemovalSide);

		if (levelsNumber == 0) {
			removals.insert(priceLevelRemovalSide);
			priceLevelStorageSide.erase(found);
			lastElementIter = priceLevelStorageSide.end();

			return;
		}

		auto removed = false;

		// Determine what will be the removal given the number of price levels.
		if (priceLevelStorageSide.size() <= levelsNumber || priceLevelRemovalSide < *std::next(lastElementIter)) {
			removals.insert(priceLevelRemovalSide);
			removed = true;
		}

		// Determine what will be the shift in price levels after removal.
		if (removed && priceLevelStorageSide.size() > levelsNumber) {
			additions.insert(*std::next(lastElementIter));
		}

		// Determine the adjusted last price (NaN -- end)
		typename std::decay<decltype(*lastElementIter)>::type newLastPL{};

		if (removed) {														  // removal <= last
			if (std::next(lastElementIter) != priceLevelStorageSide.end()) {  // there is another PL after last
				newLastPL = *std::next(lastElementIter);
			} else {
				if (priceLevelRemovalSide < *lastElementIter) {
					newLastPL = *lastElementIter;
				} else if (lastElementIter != priceLevelStorageSide.begin()) {
					newLastPL = *std::prev(lastElementIter);
				}
			}
		} else {
			newLastPL = *lastElementIter;
		}

		priceLevelStorageSide.erase(found);

		if (!newLastPL.isValid()) {
			lastElementIter = priceLevelStorageSide.end();
		} else {
			lastElementIter = priceLevelStorageSide.find(newLastPL);
		}
	}

	template <typename PriceLevelAdditionSide, typename PriceLevelStorageSide, typename PriceLevelChangesSideSet,
			  typename LastElementIter>
	static void processSideAddition(const PriceLevelAdditionSide& priceLevelAdditionSide,
									PriceLevelStorageSide& priceLevelStorageSide, PriceLevelChangesSideSet& additions,
									PriceLevelChangesSideSet& removals, LastElementIter& lastElementIter,
									std::size_t levelsNumber) {
		if (levelsNumber == 0) {
			additions.insert(priceLevelAdditionSide);
			priceLevelStorageSide.insert(priceLevelAdditionSide);
			lastElementIter = priceLevelStorageSide.end();

			return;
		}

		auto added = false;

		// We determine what will be the addition of the price level, taking into account the possible quantity.
		if (priceLevelStorageSide.size() < levelsNumber || priceLevelAdditionSide < *lastElementIter) {
			additions.insert(priceLevelAdditionSide);
			added = true;
		}

		// We determine what will be the shift after adding
		if (added && priceLevelStorageSide.size() >= levelsNumber) {
			const auto& toRemove = *lastElementIter;

			// We take into account the possibility that the previously added price level will be deleted.
			if (additions.count(toRemove) > 0) {
				additions.erase(toRemove);
			} else {
				removals.insert(toRemove);
			}
		}

		// Determine the adjusted last price (NaN -- end)
		typename std::decay<decltype(*lastElementIter)>::type newLastPL = priceLevelAdditionSide;

		if (lastElementIter != priceLevelStorageSide.end()) {
			if (priceLevelAdditionSide < *lastElementIter) {
				if (priceLevelStorageSide.size() < levelsNumber) {
					newLastPL = *lastElementIter;
				} else if (lastElementIter != priceLevelStorageSide.begin() &&
						   priceLevelAdditionSide < *std::prev(lastElementIter)) {
					newLastPL = *std::prev(lastElementIter);
				}
			} else if (priceLevelStorageSide.size() >= levelsNumber) {
				newLastPL = *lastElementIter;
			}
		}

		priceLevelStorageSide.insert(priceLevelAdditionSide);

		if (!newLastPL.isValid()) {
			lastElementIter = priceLevelStorageSide.end();
		} else {
			lastElementIter = priceLevelStorageSide.find(newLastPL);
		}
	}

	template <typename PriceLevelUpdateSide, typename PriceLevelStorageSide, typename PriceLevelChangesSideSet,
			  typename LastElementIter>
	static void processSideUpdate(const PriceLevelUpdateSide& priceLevelUpdateSide,
								  PriceLevelStorageSide& priceLevelStorageSide, PriceLevelChangesSideSet& updates,
								  LastElementIter& lastElementIter, std::size_t levelsNumber) {
		if (levelsNumber == 0) {
			priceLevelStorageSide.erase(priceLevelUpdateSide);
			priceLevelStorageSide.insert(priceLevelUpdateSide);
			updates.insert(priceLevelUpdateSide);
			lastElementIter = priceLevelStorageSide.end();

			return;
		}

		if (priceLevelStorageSide.count(priceLevelUpdateSide) > 0) {
			updates.insert(priceLevelUpdateSide);
		}

		typename std::decay<decltype(*lastElementIter)>::type newLastPL{};

		if (lastElementIter != priceLevelStorageSide.end()) {
			newLastPL = *lastElementIter;
		}

		priceLevelStorageSide.erase(priceLevelUpdateSide);
		priceLevelStorageSide.insert(priceLevelUpdateSide);

		if (!newLastPL.isValid()) {
			lastElementIter = priceLevelStorageSide.end();
		} else {
			lastElementIter = priceLevelStorageSide.find(newLastPL);
		}
	}

	PriceLevelChangesSet applyUpdates(const PriceLevelChanges& priceLevelUpdates) {
		PriceLevelChanges additions{};
		PriceLevelChanges removals{};
		PriceLevelChanges updates{};

		// We generate lists of additions, updates, removals
		for (const auto& askUpdate : priceLevelUpdates.asks) {
			generatePriceLevelChanges(askUpdate, asks_, additions.asks, removals.asks, updates.asks);
		}

		for (const auto& bidUpdate : priceLevelUpdates.bids) {
			generatePriceLevelChanges(bidUpdate, bids_, additions.bids, removals.bids, updates.bids);
		}

		std::set<AskPriceLevel> askRemovals{};
		std::set<BidPriceLevel> bidRemovals{};
		std::set<AskPriceLevel> askAdditions{};
		std::set<BidPriceLevel> bidAdditions{};
		std::set<AskPriceLevel> askUpdates{};
		std::set<BidPriceLevel> bidUpdates{};

		// O(R)
		for (const auto& askRemoval : removals.asks) {
			// * O(log(N))
			processSideRemoval(askRemoval, asks_, askRemovals, askAdditions, lastAsk_, levelsNumber_);
		}

		for (const auto& askAddition : additions.asks) {
			processSideAddition(askAddition, asks_, askAdditions, askRemovals, lastAsk_, levelsNumber_);
		}

		for (const auto& askUpdate : updates.asks) {
			processSideUpdate(askUpdate, asks_, askUpdates, lastAsk_, levelsNumber_);
		}

		for (const auto& bidRemoval : removals.bids) {
			processSideRemoval(bidRemoval, bids_, bidRemovals, bidAdditions, lastBid_, levelsNumber_);
		}

		for (const auto& bidAddition : additions.bids) {
			processSideAddition(bidAddition, bids_, bidAdditions, bidRemovals, lastBid_, levelsNumber_);
		}

		for (const auto& bidUpdate : updates.bids) {
			processSideUpdate(bidUpdate, bids_, bidUpdates, lastBid_, levelsNumber_);
		}

		return {PriceLevelChanges{symbol_, std::vector<AskPriceLevel>{askRemovals.begin(), askRemovals.end()},
								  std::vector<BidPriceLevel>{bidRemovals.begin(), bidRemovals.end()}},
				PriceLevelChanges{symbol_, std::vector<AskPriceLevel>{askAdditions.begin(), askAdditions.end()},
								  std::vector<BidPriceLevel>{bidAdditions.begin(), bidAdditions.end()}},
				PriceLevelChanges{symbol_, std::vector<AskPriceLevel>{askUpdates.begin(), askUpdates.end()},
								  std::vector<BidPriceLevel>{bidUpdates.begin(), bidUpdates.end()}}};
	}

	[[nodiscard]] std::vector<AskPriceLevel> getAsks() const {
		return {asks_.begin(), lastAsk_ == asks_.end() ? lastAsk_ : std::next(lastAsk_)};
	}

	[[nodiscard]] std::vector<BidPriceLevel> getBids() const {
		return {bids_.begin(), lastBid_ == bids_.end() ? lastBid_ : std::next(lastBid_)};
	}

	void clear() {
		asks_.clear();
		lastAsk_ = asks_.end();
		bids_.clear();
		lastBid_ = bids_.end();
		orderDataSnapshot_.clear();
	}

public:
	// TODO: move to another thread
	void processSnapshotData(const dxf_snapshot_data_ptr_t snapshotData, int newSnapshot) {
		std::lock_guard<std::mutex> lk(mutex_);

		auto newSnap = newSnapshot != 0;

		if (newSnap) {
			clear();
		}

		if (snapshotData->records_count == 0) {
			if (newSnap && onNewBook_) {
				onNewBook_({}, userData_);
			}

			return;
		}

		auto updates = convertToUpdates(snapshotData);
		auto resultingChangesSet = applyUpdates(updates);

		if (newSnap) {
			if (onNewBook_) {
				onNewBook_(PriceLevelChanges{symbol_, getAsks(), getBids()}, userData_);
			}
		} else if (!resultingChangesSet.isEmpty()) {
			if (onIncrementalChange_) {
				onIncrementalChange_(resultingChangesSet, userData_);
			}

			if (onBookUpdate_) {
				onBookUpdate_(PriceLevelChanges{symbol_, getAsks(), getBids()}, userData_);
			}
		}
	}

	~PriceLevelBook() {
		if (isValid_) {
			dxf_close_price_level_book(snapshot_);
		}
	}

	static PriceLevelBook* create(dxf_connection_t connection, const std::string& symbol, const std::string& source,
								  std::size_t levelsNumber) {
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

	void setUserData(void* newUserData) {
		std::lock_guard<std::mutex> lk(mutex_);
		userData_ = newUserData;
	}

	void setOnNewBook(std::function<void(const PriceLevelChanges&, void*)> onNewBookHandler) {
		std::lock_guard<std::mutex> lk(mutex_);
		onNewBook_ = std::move(onNewBookHandler);
	}

	void setOnBookUpdate(std::function<void(const PriceLevelChanges&, void*)> onBookUpdateHandler) {
		std::lock_guard<std::mutex> lk(mutex_);
		onBookUpdate_ = std::move(onBookUpdateHandler);
	}

	void setOnIncrementalChange(std::function<void(const PriceLevelChangesSet&, void*)> onIncrementalChangeHandler) {
		std::lock_guard<std::mutex> lk(mutex_);
		onIncrementalChange_ = std::move(onIncrementalChangeHandler);
	}
};

struct PriceLevelBookDataBundle {
	std::wstring wSymbol{};
	dxf_price_level_book_data_t priceLevelBookData{};
	std::vector<dxf_price_level_element> asks{};
	std::vector<dxf_price_level_element> bids{};

	PriceLevelBookDataBundle() = delete;
	PriceLevelBookDataBundle(const PriceLevelBookDataBundle&) = delete;
	PriceLevelBookDataBundle(PriceLevelBookDataBundle&&) = delete;
	PriceLevelBookDataBundle& operator= (const PriceLevelBookDataBundle&) = delete;
	PriceLevelBookDataBundle&& operator= (PriceLevelBookDataBundle&&) = delete;

	explicit PriceLevelBookDataBundle(const dx::PriceLevelChanges& changes) {
		wSymbol = dx::StringConverter::utf8ToWString(changes.symbol);

		if (!changes.asks.empty()) {
			asks.reserve(changes.asks.size());

			for (const auto& ask : changes.asks) {
				asks.push_back({ask.price, ask.size, ask.time});
			}
		}

		if (!changes.bids.empty()) {
			bids.reserve(changes.bids.size());

			for (const auto& bid : changes.bids) {
				bids.push_back({bid.price, bid.size, bid.time});
			}
		}

		priceLevelBookData.symbol = wSymbol.c_str();
		priceLevelBookData.bids_count = bids.size();
		priceLevelBookData.bids = bids.data();
		priceLevelBookData.asks_count = asks.size();
		priceLevelBookData.asks = asks.data();
	}
};

}  // namespace dx
