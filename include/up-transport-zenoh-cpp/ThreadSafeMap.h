// SPDX-FileCopyrightText: 2024 Contributors to the Eclipse Foundation
//
// See the NOTICE file(s) distributed with this work for additional
// information regarding copyright ownership.
//
// This program and the accompanying materials are made available under the
// terms of the Apache License Version 2.0 which is available at
// https://www.apache.org/licenses/LICENSE-2.0
//
// SPDX-License-Identifier: Apache-2.0

#ifndef UP_TRANSPORT_ZENOH_CPP_THREADSAFEMAP_H
#define UP_TRANSPORT_ZENOH_CPP_THREADSAFEMAP_H

#include <algorithm>
#include <map>
#include <mutex>
#include <optional>

template <typename Key, typename Value>
class ThreadSafeMap {
public:
	using MapType = std::map<Key, Value>;
	using Iterator = typename MapType::iterator;

	template <typename... Args>
	std::pair<Iterator, bool> emplace(Args&&... args) {
		std::lock_guard<std::mutex> lock(mutex_);
		return map_.emplace(std::forward<Args>(args)...);
	}

	size_t erase(const Key& key) {
		std::lock_guard<std::mutex> lock(mutex_);
		return map_.erase(key);
	}

	std::optional<Value> find(const Key& key) {
		std::lock_guard<std::mutex> lock(mutex_);
		Iterator it = map_.find(key);
		if (it != map_.end()) {
			return it->second;
		}
		return std::nullopt;
	}

	template <typename Predicate>
	std::optional<Value> find_if(Predicate pred) {
		std::lock_guard<std::mutex> lock(mutex_);
		Iterator it = std::find_if(map_.begin(), map_.end(), pred);
		if (it != map_.end()) {
			return it->second;
		}
		return std::nullopt;
	}

private:
	MapType map_;
	mutable std::mutex mutex_;
};

#endif  // UP_TRANSPORT_ZENOH_CPP_THREADSAFEMAP_H