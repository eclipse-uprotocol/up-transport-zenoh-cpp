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
		} else {
			return std::nullopt;
		}
	}

	template <typename Predicate>
	std::optional<Value> find_if(Predicate pred) {
		std::lock_guard<std::mutex> lock(mutex_);
		Iterator it = std::find_if(map_.begin(), map_.end(), pred);
		if (it != map_.end()) {
			return it->second;
		} else {
			return std::nullopt;
		}
	}

private:
	MapType map_;
	mutable std::mutex mutex_;
};
