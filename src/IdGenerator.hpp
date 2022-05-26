#pragma once

namespace dx {
template <typename T>
class IdGenerator {
	static long long id;

public:
	static long long get() { return id++; }
};

template <typename T>
long long IdGenerator<T>::id{0};
}  // namespace dx