#pragma once

namespace dx {
using Id = long long;

template <typename T>
class IdGenerator {
	static Id id;

public:
	static Id get() { return id++; }
};

template <typename T>
Id IdGenerator<T>::id{0};
}  // namespace dx