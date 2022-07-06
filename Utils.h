#pragma once
#include <atomic>

template <class K, class V>
class Minimum {
	K initialKey;
	V initialValue;
	
public:
	Minimum(K&& key, V&& value) {
		initialKey = move(key);
		initialValue = move(value);
	}

	void Update(K&& key, V&& value) {
		if (key < initialKey) {
			initialKey = std::move(key);
			initialValue = std::move(value);
		}
	}

	auto& getKey() {
		return initialKey;
	}

	auto& getValue() {
		return initialValue;
	}
};

template <class K, class V>
class AtomicMinimum {
	atomic<K> initialKey;
	atomic<V> initialValue;

public:
	AtomicMinimum(K key, V value) {
		initialKey = key;
		initialValue = value;
	}

	void Update(K key, V value) {
		if (key < initialKey) {
			initialKey = key;
			initialValue = value;
		}
	}

	K getKey() {
		return initialKey;
	}

	V getValue() {
		return initialValue;
	}
};