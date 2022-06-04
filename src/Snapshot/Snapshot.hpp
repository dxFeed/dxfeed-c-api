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
#include <DXFeed.h>
#include <EventData.h>
}

#include <atomic>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional.hpp>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant.hpp>
#include <vector>

#include "../IdGenerator.hpp"
#include "../Utils/Hash.hpp"
#include "SnapshotKey.hpp"

namespace dx {

using SnapshotRefId = Id;
const SnapshotRefId INVALID_SNAPSHOT_REFERENCE_ID = SnapshotRefId{-1};

struct SnapshotChanges {
	SnapshotKey snapshotKey;

	SnapshotChanges() = default;

	SnapshotChanges(SnapshotKey newSnapshotKey) : snapshotKey{std::move(newSnapshotKey)} {}

	bool isEmpty() const { return true; }
};

struct SnapshotChangesSet {
	SnapshotChanges removals{};
	SnapshotChanges additions{};
	SnapshotChanges updates{};

	SnapshotChangesSet() = default;

	SnapshotChangesSet(SnapshotChanges newRemovals, SnapshotChanges newAdditions, SnapshotChanges newUpdates)
		: removals{std::move(newRemovals)}, additions{std::move(newAdditions)}, updates{std::move(newUpdates)} {}

	bool isEmpty() const { return removals.isEmpty() && additions.isEmpty() && updates.isEmpty(); }
};

struct Snapshot;

struct SnapshotSubscriber {
	using SnapshotHandler = std::function<void(const SnapshotChanges&, void*)>;
	using IncrementalSnapshotHandler = std::function<void(const SnapshotChangesSet&, void*)>;

private:
	std::atomic<SnapshotRefId> referenceId_;
	SnapshotKey filter_;
	void* userData_;
	mutable std::recursive_mutex mutex_;
	std::atomic<bool> isValid_;

	SnapshotHandler onNewSnapshot_{};
	SnapshotHandler onSnapshotUpdate_{};
	IncrementalSnapshotHandler onIncrementalChange_{};

public:
	SnapshotSubscriber(SnapshotRefId referenceId, SnapshotKey filter, void* userData)
		: referenceId_{referenceId}, filter_(std::move(filter)), userData_{userData}, mutex_(), isValid_(true) {}

	SnapshotSubscriber(SnapshotSubscriber&& sub) noexcept : mutex_() {
		referenceId_ = sub.referenceId_.load();
		sub.referenceId_ = INVALID_SNAPSHOT_REFERENCE_ID;
		filter_ = std::move(sub.filter_);
		userData_ = sub.userData_;
		onNewSnapshot_ = std::move(sub.onNewSnapshot_);
		onSnapshotUpdate_ = std::move(sub.onSnapshotUpdate_);
		onIncrementalChange_ = std::move(sub.onIncrementalChange_);
		isValid_ = sub.isValid_.load();
		sub.isValid_ = false;
	}

	SnapshotSubscriber& operator=(SnapshotSubscriber&& sub) noexcept {
		if (this == &sub) {
			return *this;
		}

		referenceId_ = sub.referenceId_.load();
		sub.referenceId_ = INVALID_SNAPSHOT_REFERENCE_ID;
		filter_ = std::move(sub.filter_);
		userData_ = sub.userData_;
		onNewSnapshot_ = std::move(sub.onNewSnapshot_);
		onSnapshotUpdate_ = std::move(sub.onSnapshotUpdate_);
		onIncrementalChange_ = std::move(sub.onIncrementalChange_);
		isValid_ = sub.isValid_.load();
		sub.isValid_ = false;

		return *this;
	}

	void setOnNewSnapshotHandler(SnapshotHandler onNewSnapshotHandler) {
		std::lock_guard<std::recursive_mutex> guard{mutex_};
		onNewSnapshot_ = std::move(onNewSnapshotHandler);
	}

	void setOnSnapshotUpdateHandler(SnapshotHandler onSnapshotUpdateHandler) {
		std::lock_guard<std::recursive_mutex> guard{mutex_};
		onSnapshotUpdate_ = std::move(onSnapshotUpdateHandler);
	}

	void setOnIncrementalChangeHandler(IncrementalSnapshotHandler onIncrementalChangeHandler) {
		std::lock_guard<std::recursive_mutex> guard{mutex_};
		onIncrementalChange_ = std::move(onIncrementalChangeHandler);
	}

	SnapshotRefId getReferenceId() const { return referenceId_; }

	SnapshotKey getFilter() const {
		std::lock_guard<std::recursive_mutex> guard{mutex_};

		return filter_;
	}

	bool isValid() { return isValid_; }
};

struct SnapshotManager;

struct Snapshot {
protected:
	std::unordered_map<SnapshotRefId, std::shared_ptr<SnapshotSubscriber>> subscribers_;
	mutable std::mutex subscribersMutex_;

	friend SnapshotManager;

	bool addSubscriber(const std::shared_ptr<SnapshotSubscriber>& subscriber) {
		std::lock_guard<std::mutex> guard{subscribersMutex_};

		if (!subscriber || !subscriber->isValid() || subscribers_.count(subscriber->getReferenceId()) > 0) {
			return false;
		}

		return subscribers_.emplace(subscriber->getReferenceId(), subscriber).second;
	}

	bool removeSubscriber(SnapshotRefId snapshotRefId) {
		std::lock_guard<std::mutex> guard{subscribersMutex_};

		return subscribers_.erase(snapshotRefId) > 0;
	}

public:
	explicit Snapshot() : subscribers_{}, subscribersMutex_{} {}

	bool hasSubscriptions() const {
		std::lock_guard<std::mutex> guard{subscribersMutex_};

		return !subscribers_.empty();
	}

	virtual ~Snapshot() = default;
};

struct OnlyIndexedEventKey {
	dxf_long_t index;
	dxf_long_t time;
};

}  // namespace dx

namespace std {
template <>
struct hash<dx::OnlyIndexedEventKey> {
	std::size_t operator()(const dx::OnlyIndexedEventKey& key) const noexcept {
		return dx::hashCombine(key.index, key.time);
	}
};
}  // namespace std

namespace dx {

struct OnlyIndexedEventsSnapshot final : public Snapshot {
	using EventType = nonstd::variant<dxf_order_t /*order + spread order*/, dxf_series_t>;

	SnapshotKey key;
	std::unordered_map<OnlyIndexedEventKey, EventType> events;
};

struct TimeSeriesEventsSnapshot final : public Snapshot {};

struct SnapshotManager {
	using ConnectionKey = dxf_connection_t;
	using CommonSnapshotPtr = nonstd::variant<std::shared_ptr<OnlyIndexedEventsSnapshot>,
											  std::shared_ptr<TimeSeriesEventsSnapshot>, nullptr_t>;

private:
	static std::unordered_map<ConnectionKey, std::shared_ptr<SnapshotManager>> managers_;
	// one to one
	std::unordered_map<SnapshotKey, std::shared_ptr<OnlyIndexedEventsSnapshot>> indexedEventsSnapshots_;
	// many to one
	std::unordered_map<SnapshotRefId, std::shared_ptr<OnlyIndexedEventsSnapshot>> indexedEventsSnapshotsByRefId_;
	std::mutex indexedEventsSnapshotsMutex_;
	// one to one
	std::unordered_map<SnapshotKey, std::shared_ptr<TimeSeriesEventsSnapshot>> timeSeriesEventsSnapshots_;
	// many to one
	std::unordered_map<SnapshotRefId, std::shared_ptr<TimeSeriesEventsSnapshot>> timeSeriesEventsSnapshotsByRefId_;
	std::mutex timeSeriesEventsSnapshotsMutex_;
	// one to one
	std::unordered_map<SnapshotRefId, std::shared_ptr<SnapshotSubscriber>> subscribers_;
	std::mutex subscribersMutex_;

	explicit SnapshotManager()
		: indexedEventsSnapshots_{},
		  indexedEventsSnapshotsByRefId_{},
		  indexedEventsSnapshotsMutex_{},
		  timeSeriesEventsSnapshots_{},
		  timeSeriesEventsSnapshotsByRefId_{},
		  timeSeriesEventsSnapshotsMutex_{},
		  subscribers_{},
		  subscribersMutex_{} {}

	template <typename T>
	std::shared_ptr<T> getImpl(const SnapshotKey&) {
		return nullptr;
	}

	template <typename T>
	std::shared_ptr<T> getImpl(SnapshotRefId) {
		return nullptr;
	}

	bool closeImpl(SnapshotRefId snapshotRefId, CommonSnapshotPtr snapshotPtr) {
		// TODO: overloaded "idiom" + generic lambda
		struct CloseImplVisitor {
			SnapshotRefId snapshotRefId;

			bool operator()(std::shared_ptr<OnlyIndexedEventsSnapshot> indexedEventsSnapshotPtr) const { return false; }

			bool operator()(std::shared_ptr<TimeSeriesEventsSnapshot> timeSeriesEventsSnapshotPtr) const {
				return false;
			}

			bool operator()(nullptr_t) const {
				// TODO: warning
				return true;
			}
		};

		return nonstd::visit(CloseImplVisitor{snapshotRefId}, snapshotPtr);
	}

public:
	/// Returns the SnapshotManager instance by a connection key or creates the new one and returns it.
	/// \param connectionKey The connection key (i.e. the pointer to connection context structure)
	/// \return The SnapshotManager instance.
	static std::shared_ptr<SnapshotManager> getInstance(const ConnectionKey& connectionKey) {
		auto found = managers_.find(connectionKey);

		if (found != managers_.end()) {
			return found->second;
		}

		auto inserted = managers_.emplace(connectionKey, std::shared_ptr<SnapshotManager>(new SnapshotManager{}));

		if (inserted.second) {
			return inserted.first->second;
		}

		return managers_[connectionKey];
	}

	/// Returns a pointer to the snapshot instance by the snapshot key (an event id, a symbol, an optional source, an
	/// optional `timeFrom`) \tparam T The Snapshot type (IndexedEventsSnapshot or TimeSeriesEventsSnapshot) \return A
	/// shared pointer to the snapshot instance or std::shared_ptr{nullptr}
	template <typename T>
	std::shared_ptr<T> get(const SnapshotKey&) {
		return nullptr;
	}

	/// Returns a pointer to the snapshot instance (indexed or time series) by the snapshot reference id (1 to 1 for a
	/// SnapshotSubscriber) \param snapshotRefId The snapshot reference id \return A shared pointer to the snapshot
	/// instance or nullptr
	CommonSnapshotPtr get(SnapshotRefId snapshotRefId);

	/// Creates the new snapshot instance and the new SnapshotSubscriber instance or adds new SnapshotSubscriber
	/// instance to the existing snapshot. \tparam T The Snapshot type (IndexedEventsSnapshot or
	/// TimeSeriesEventsSnapshot) \return The pair of shared pointer to the snapshot instance and reference id to the
	/// snapshot (1 to 1 for a SnapshotSubscriber)
	template <typename T>
	std::pair<std::shared_ptr<T>, SnapshotRefId> create(const ConnectionKey&, const SnapshotKey&, void*) {
		return {nullptr, INVALID_SNAPSHOT_REFERENCE_ID};
	}

	bool setOnNewSnapshotHandler(SnapshotRefId snapshotRefId,
								 SnapshotSubscriber::SnapshotHandler onNewSnapshotHandler) {
		std::lock_guard<std::mutex> guard{subscribersMutex_};

		auto found = subscribers_.find(snapshotRefId);

		if (found != subscribers_.end()) {
			found->second->setOnNewSnapshotHandler(std::move(onNewSnapshotHandler));

			return true;
		}

		return false;
	}

	bool setOnSnapshotUpdateHandler(SnapshotRefId snapshotRefId,
									SnapshotSubscriber::SnapshotHandler onSnapshotUpdateHandler) {
		std::lock_guard<std::mutex> guard{subscribersMutex_};

		auto found = subscribers_.find(snapshotRefId);

		if (found != subscribers_.end()) {
			found->second->setOnSnapshotUpdateHandler(std::move(onSnapshotUpdateHandler));

			return true;
		}

		return false;
	}

	bool setOnIncrementalChangeHandler(SnapshotRefId snapshotRefId,
									   SnapshotSubscriber::IncrementalSnapshotHandler onIncrementalChangeHandler) {
		std::lock_guard<std::mutex> guard{subscribersMutex_};

		auto found = subscribers_.find(snapshotRefId);

		if (found != subscribers_.end()) {
			found->second->setOnIncrementalChangeHandler(std::move(onIncrementalChangeHandler));

			return true;
		}

		return false;
	}

	bool close(SnapshotRefId snapshotRefId) {
		auto snapshotPtr = get(snapshotRefId);

		return closeImpl(snapshotRefId, snapshotPtr);
	}
};

std::unordered_map<SnapshotManager::ConnectionKey, std::shared_ptr<SnapshotManager>> SnapshotManager::managers_{};

template <>
std::shared_ptr<OnlyIndexedEventsSnapshot> SnapshotManager::getImpl(const SnapshotKey& snapshotKey) {
	auto found = indexedEventsSnapshots_.find(snapshotKey);

	if (found == indexedEventsSnapshots_.end()) {
		return nullptr;
	}

	return found->second;
}

template <>
std::shared_ptr<TimeSeriesEventsSnapshot> SnapshotManager::getImpl(const SnapshotKey& snapshotKey) {
	auto found = timeSeriesEventsSnapshots_.find(snapshotKey);

	if (found == timeSeriesEventsSnapshots_.end()) {
		return nullptr;
	}

	return found->second;
}

template <>
std::shared_ptr<OnlyIndexedEventsSnapshot> SnapshotManager::getImpl(SnapshotRefId snapshotRefId) {
	auto found = indexedEventsSnapshotsByRefId_.find(snapshotRefId);

	if (found == indexedEventsSnapshotsByRefId_.end()) {
		return nullptr;
	}

	return found->second;
}

template <>
std::shared_ptr<TimeSeriesEventsSnapshot> SnapshotManager::getImpl(SnapshotRefId snapshotRefId) {
	auto found = timeSeriesEventsSnapshotsByRefId_.find(snapshotRefId);

	if (found == timeSeriesEventsSnapshotsByRefId_.end()) {
		return nullptr;
	}

	return found->second;
}

template <>
std::shared_ptr<OnlyIndexedEventsSnapshot> SnapshotManager::get(const SnapshotKey& snapshotKey) {
	std::lock_guard<std::mutex> guard(indexedEventsSnapshotsMutex_);

	return getImpl<OnlyIndexedEventsSnapshot>(snapshotKey);
}

template <>
std::shared_ptr<TimeSeriesEventsSnapshot> SnapshotManager::get(const SnapshotKey& snapshotKey) {
	std::lock_guard<std::mutex> guard(timeSeriesEventsSnapshotsMutex_);

	return getImpl<TimeSeriesEventsSnapshot>(snapshotKey);
}

SnapshotManager::CommonSnapshotPtr SnapshotManager::get(SnapshotRefId snapshotRefId) {
	{
		std::lock_guard<std::mutex> guard(indexedEventsSnapshotsMutex_);

		auto snapshot = getImpl<OnlyIndexedEventsSnapshot>(snapshotRefId);

		if (snapshot) {
			return snapshot;
		}
	}
	{
		std::lock_guard<std::mutex> guard(timeSeriesEventsSnapshotsMutex_);

		auto snapshot = getImpl<TimeSeriesEventsSnapshot>(snapshotRefId);

		if (snapshot) {
			return snapshot;
		}
	}

	return nullptr;
}

template <>
Id IdGenerator<Snapshot>::id{0};

template <>
std::pair<std::shared_ptr<OnlyIndexedEventsSnapshot>, Id> SnapshotManager::create(const ConnectionKey& connectionKey,
																				  const SnapshotKey& snapshotKey,
																				  void* userData) {
	std::lock_guard<std::mutex> guard(indexedEventsSnapshotsMutex_);

	auto snapshot = getImpl<OnlyIndexedEventsSnapshot>(snapshotKey);
	auto id = IdGenerator<Snapshot>::get();

	if (snapshot) {
		std::shared_ptr<SnapshotSubscriber> subscriber(new SnapshotSubscriber(id, snapshotKey, userData));

		if (snapshot->addSubscriber(subscriber)) {
			indexedEventsSnapshotsByRefId_.emplace(id, snapshot);

			std::lock_guard<std::mutex> subscribersGuard(subscribersMutex_);

			subscribers_.emplace(id, subscriber);

			return {snapshot, id};
		} else {
			return {nullptr, INVALID_SNAPSHOT_REFERENCE_ID};
		}
	}

	auto inserted = indexedEventsSnapshots_.emplace(
		snapshotKey, std::shared_ptr<OnlyIndexedEventsSnapshot>(new OnlyIndexedEventsSnapshot{}));

	return {inserted.first->second, id};
}

template <>
std::pair<std::shared_ptr<TimeSeriesEventsSnapshot>, Id> SnapshotManager::create(const ConnectionKey& connectionKey,
																				 const SnapshotKey& snapshotKey,
																				 void* userData) {
	std::lock_guard<std::mutex> guard(timeSeriesEventsSnapshotsMutex_);

	auto snapshot = getImpl<TimeSeriesEventsSnapshot>(snapshotKey);
	auto id = IdGenerator<Snapshot>::get();

	if (snapshot) {
		std::shared_ptr<SnapshotSubscriber> subscriber(new SnapshotSubscriber(id, snapshotKey, userData));

		if (snapshot->addSubscriber(subscriber)) {
			timeSeriesEventsSnapshotsByRefId_.emplace(id, snapshot);

			std::lock_guard<std::mutex> subscribersGuard(subscribersMutex_);

			subscribers_.emplace(id, subscriber);

			return {snapshot, id};
		} else {
			return {nullptr, INVALID_SNAPSHOT_REFERENCE_ID};
		}
	}

	auto inserted = timeSeriesEventsSnapshots_.emplace(
		snapshotKey, std::shared_ptr<TimeSeriesEventsSnapshot>(new TimeSeriesEventsSnapshot{}));

	return {inserted.first->second, id};
}

}  // namespace dx
