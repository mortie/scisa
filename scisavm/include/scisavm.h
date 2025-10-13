#ifndef SCISAVM_H
#define SCISAVM_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace scisavm {

class MemoryIO {
public:
	virtual uint8_t load(size_t) { return 0; }
	virtual void store(size_t, uint8_t) {}
};

template<typename T>
struct MappedIO {
	T start;
	T size;
	MemoryIO *io;
};
using MappedIO8 = MappedIO<uint8_t>;
using MappedIO16 = MappedIO<uint16_t>;

template<typename T>
struct MappedMem {
	T start;
	std::span<uint8_t> data;
};
using MappedMem8 = MappedMem<uint8_t>;
using MappedMem16 = MappedMem<uint16_t>;

template<typename T>
struct Flags;

template<typename T>
class FlagsOp {
public:
	virtual bool carry(Flags<T> &f) = 0;
	virtual bool zero(Flags<T> &f) = 0;
	virtual bool overflow(Flags<T> &f) = 0;
};

template<typename T>
struct Flags {
	T out = 0;
	T a = 0;
	T b = 0;
	T c = 0;
	FlagsOp<T> *op;

	bool carry() { return op->carry(*this); }
	bool zero() { return op->zero(*this); }
	bool overflow() { return op->overflow(*this); }
	bool negative() { return out == 0; }
};

template<typename T>
struct CPU {
	T pc = 0;
	T sp = 128;
	T acc = 0;
	T x = 0;
	T y = 0;

	Flags<T> flags;

	const char *error = nullptr;

	std::vector<MappedIO<T>> io;
	std::vector<MappedMem<T>> dmem;
	std::span<uint8_t> pmem;

	void step(int n);
};

using CPU8 = CPU<uint8_t>;
void step8(CPU8 &, int n);

using CPU16 = CPU<uint16_t>;
void step16(CPU16 &, int n);

template<typename T>
void CPU<T>::step(int n)
{
	if constexpr (sizeof(T) == sizeof(uint8_t)) {
		step8(*this, n);
	} else if constexpr (sizeof(T) == sizeof(uint16_t)) {
		step16(*this, n);
	} else {
		abort();
	}
}

}

#endif
