#include "scisavm.h"

namespace scisavm {

enum class Op {
	SPECIAL = 0b00000,
	ADD = 0b00001,
	SUB = 0b00010,
	ADC = 0b00011,
	XOR = 0b00100,
	AND = 0b00101,
	OR  = 0b00110,
	CMP = 0b00111,
	MVX = 0b01000,
	MVY = 0b01001,
	MVA = 0b01010,
	MHA = 0b01011,
	SPS = 0b01100,
	LDX = 0b01101,
	LDW = 0b01110,
	LDA = 0b01111,
	STX = 0b10000,
	STW = 0b10001,
	STA = 0b10010,
	JMP = 0b10011,
	JLR = 0b10100,
	B   = 0b10101,
	BCC = 0b10110,
	BCS = 0b10111,
	BEQ = 0b11000,
	BNE = 0b11001,
	BMI = 0b11010,
	BPL = 0b11011,
	BVS = 0b11100,
	BVC = 0b11101,
	PUSH = 0b11110,
	POP = 0b11111,
};

enum class SpecOp {
	NOP = 0b000,
	LSR = 0b001,
	ROR = 0b010,
	INC = 0b011,
};

template<typename T>
class AddOp: public FlagsOp<T> {
public:
	static AddOp<T> self;

	bool carry(Flags<T> &f) override
	{
		static_assert(sizeof(uint32_t) > sizeof(T));
		uint32_t out = uint32_t(f.a) + uint32_t(f.b) + uint32_t(f.c);
		return out & (1 << (8 * sizeof(T)));
	}

	bool overflow(Flags<T> &f) override
	{
		auto aSign = f.a & (1 << (sizeof(T) * 8 - 1));
		auto bSign = f.b & (1 << (sizeof(T) * 8 - 1));
		if (aSign == bSign) {
			auto outSign = f.out & (1 << (sizeof(T) * 8 - 1));
			return aSign != outSign;
		} else {
			return false;
		}
	}
};
template<typename T>
AddOp<T> AddOp<T>::self;

template<typename T>
class ZOp: public FlagsOp<T> {
public:
	static ZOp<T> self;

	bool carry(Flags<T> &f) override { return f.c; }
	bool overflow(Flags<T> &) override { return false; }
};
template<typename T>
ZOp<T> ZOp<T>::self;

template<typename T>
uint8_t loadByte(CPU<T> &cpu, T addr)
{
	for (MappedIO<T> &io: cpu.io) {
		if (addr >= io.start && addr < io.start + io.size) {
			return io.io->load(addr - io.start);
		}
	}

	for (MappedMem<T> &mem: cpu.dmem) {
		if (addr >= mem.start && addr < mem.start + mem.data.size()) {
			return mem.data[addr - mem.start];
		}
	}

	cpu.error = "Illegal load";
	return 0;
}

template<typename T>
T loadWord(CPU<T> &cpu, T addr)
{
	for (MappedMem<T> &mem: cpu.dmem) {
		if (addr >= mem.start && addr + sizeof(T) <= mem.start + mem.data.size()) {
			T val = mem.data[addr - mem.start];
			if constexpr (sizeof(T) > 1) {
				val |= T(mem.data[addr - mem.start + 1]) << 8;
			}
			return val;
		}
	}

	cpu.error = "Illegal load";
	return 0;
}

template<typename T>
void storeByte(CPU<T> &cpu, T addr, uint8_t val)
{
	for (MappedIO<T> &io: cpu.io) {
		if (addr >= io.start && addr < io.start + io.size) {
			io.io->store(addr - io.start, val);
			return;
		}
	}

	for (MappedMem<T> &mem: cpu.dmem) {
		if (addr >= mem.start && addr < mem.start + mem.data.size()) {
			mem.data[addr - mem.start] = val;
			return;
		}
	}

	cpu.error = "Illegal store";
}

template<typename T>
void storeWord(CPU<T> &cpu, T addr, T val)
{
	for (MappedMem<T> &mem: cpu.dmem) {
		if (addr >= mem.start && addr + sizeof(T) <= mem.start + mem.data.size()) {
			mem.data[addr - mem.start] = val & 0x00ff;
			if (sizeof(T) > 1) {
				mem.data[addr - mem.start + 1] = (val & 0xff00) >> 8;
			}
			return;
		}
	}

	cpu.error = "Illegal store";
}

template<typename T>
T getParam(CPU<T> &cpu, uint8_t paramMode, uint8_t second)
{
	switch (paramMode) {
	case 0b000:
		return 0;

	case 0b001:
		return cpu.x;

	case 0b010:
		return cpu.y;

	case 0b011:
		return cpu.acc;

	case 0b100:
		return second;

	case 0b101:
		return cpu.x + second;

	case 0b110:
		return cpu.y + second;

	case 0b111:
		return cpu.acc + second;
	}

	// We should never get here,
	// paramMode is literally & 0x07
	abort();
}

template<typename T>
void step(CPU<T> &cpu, int n)
{
	if (cpu.error) {
		return;
	}

	for (int i = 0; i < n; ++i) {
		if (cpu.pc >= cpu.pmem.size()) {
			cpu.error = "PC out of bounds";
			return;
		}

		auto pc = cpu.pc;

		// Load instruction
		uint8_t instr = cpu.pmem[cpu.pc++];
		auto op = Op(instr >> 3);
		uint8_t paramMode = instr & 0x07;

		// Load second byte of instruction, if it's a 2-byte instruction
		uint8_t second = 0;
		bool hasSecond = instr & 0b00000'100;
		if (hasSecond) {
			if (cpu.pc >= cpu.pmem.size()) {
				cpu.error = "PC out of bounds";
				return;
			}

			second = cpu.pmem[cpu.pc++];
		}

		// We don't *always* need the param,
		// but we *almost* always need the param.
		// It never *hurts* to call getParam,
		// so I'm hoping that by pulling the call outside of the switch,
		// we get better code gen.
		T param = getParam(cpu, paramMode, second);

		T out, carry;
		switch (op) {
		case Op::SPECIAL:
			switch (SpecOp(paramMode)) {
			case SpecOp::NOP:
				break;
			case SpecOp::LSR:
				out = cpu.acc >> 1;
				carry = cpu.acc & 0x01;
				cpu.flags = { out, 0, 0, carry, &ZOp<T>::self };
				cpu.acc = out;
				break;
			case SpecOp::ROR:
				carry = cpu.acc & 0x01;
				out = (cpu.acc >> 1) | (cpu.flags.carry() << (sizeof(T) * 8 - 1));
				cpu.flags = { out, 0, 0, carry, &ZOp<T>::self };
				cpu.acc = out;
				break;
			case SpecOp::INC:
				out = cpu.acc + 1;
				cpu.flags = { out, cpu.acc, 1, 0, &AddOp<T>::self };
				cpu.acc = out;
				break;
			default:
				cpu.error = "Bad special";
				return;
			}

			break;

		case Op::ADD:
			out = cpu.acc + param;
			cpu.flags = { out, cpu.acc, param, 0, &AddOp<T>::self };
			cpu.acc = out;
			break;

		case Op::SUB:
			out = cpu.acc - param;
			cpu.flags = { out, cpu.acc, T(~param), 1, &AddOp<T>::self };
			cpu.acc = out;
			break;

		case Op::ADC:
			carry = cpu.flags.carry();
			out = cpu.acc + param + carry;
			cpu.flags = { out, cpu.acc, param, carry, &AddOp<T>::self };
			cpu.acc = out;
			break;

		case Op::XOR:
			out = cpu.acc ^ param;
			cpu.flags = { out, 0, 0, 0, &ZOp<T>::self };
			cpu.acc = out;
			break;

		case Op::AND:
			out = cpu.acc & param;
			cpu.flags = { out, 0, 0, 0, &ZOp<T>::self };
			cpu.acc = cpu.acc & param;
			break;

		case Op::OR:
			out = cpu.acc | param;
			cpu.flags = { out, 0, 0, 0, &ZOp<T>::self };
			cpu.acc = out;
			break;

		case Op::CMP:
			out = cpu.acc - param;
			cpu.flags = { out, cpu.acc, T(~param), 1, &AddOp<T>::self };
			break;

		case Op::MVX:
			cpu.x = param;
			break;

		case Op::MVY:
			cpu.y = param;
			break;

		case Op::MVA:
			cpu.acc = param;
			break;

		case Op::MHA:
			if constexpr (sizeof(T) > sizeof(uint8_t)) {
				cpu.acc = param << 8;
			} else {
				cpu.error = "Invalid instruction for bitness";
				return;
			}
			break;

		case Op::SPS:
			cpu.sp = param;
			break;

		case Op::LDX:
			cpu.x = loadByte(cpu, param);
			cpu.flags = { cpu.x, 0, 0, 0, &ZOp<T>::self };
			break;

		case Op::LDW:
			if constexpr (sizeof(T) > sizeof(uint8_t)) {
				cpu.acc = loadWord(cpu, param);
				cpu.flags = { cpu.y, 0, 0, 0, &ZOp<T>::self };
			} else {
				cpu.error = "Invalid instruction for bitness";
				return;
			}
			break;

		case Op::LDA:
			cpu.acc = loadByte(cpu, param);
			cpu.flags = { cpu.acc, 0, 0, 0, &ZOp<T>::self };
			break;

		case Op::STX:
			storeByte(cpu, param, cpu.x);
			break;

		case Op::STW:
			if constexpr (sizeof(T) > sizeof(uint8_t)) {
				storeWord(cpu, param, cpu.acc);
			} else {
				cpu.error = "Invalid instruction for bitness";
				return;
			}
			break;

		case Op::STA:
			storeByte(cpu, param, cpu.acc);
			break;

		case Op::JMP:
			cpu.pc = param;
			break;

		case Op::JLR:
			cpu.y = cpu.pc;
			cpu.pc = param;
			break;

		case Op::B:
			cpu.pc = pc + param;
			break;

		case Op::BCC:
			if (!cpu.flags.carry()) {
				cpu.pc = pc + param;
			}
			break;

		case Op::BCS:
			if (cpu.flags.carry()) {
				cpu.pc = pc + param;
			}
			break;

		case Op::BEQ:
			if (cpu.flags.zero()) {
				cpu.pc = pc + param;
			}
			break;

		case Op::BNE:
			if (!cpu.flags.zero()) {
				cpu.pc = pc + param;
			}
			break;

		case Op::BMI:
			if (cpu.flags.negative()) {
				cpu.pc = pc + param;
			}
			break;

		case Op::BPL:
			if (!cpu.flags.negative()) {
				cpu.pc = pc + param;
			}
			break;

		case Op::BVS:
			if (cpu.flags.overflow()) {
				cpu.pc = pc + param;
			}
			break;

		case Op::BVC:
			if (!cpu.flags.overflow()) {
				cpu.pc = pc + param;
			}
			break;

		case Op::PUSH:
			storeWord(cpu, cpu.sp, param);
			cpu.sp += sizeof(T);
			break;

		case Op::POP:
			cpu.sp -= sizeof(T);
			out = loadWord(cpu, cpu.sp);
			switch (paramMode) {
			case 0b000:
				break;
			case 0b001:
				cpu.x = out;
				break;
			case 0b010:
				cpu.y = out;
				break;
			case 0b011:
				cpu.acc = out;
				break;
			default:
				cpu.error = "Invalid pop";
				return;
			}

			break;
		}
	}
}

template<typename T>
void init(CPU<T> &cpu)
{
	cpu.flags.op = &ZOp<T>::self;
}

void step8(CPU8 &cpu, int n) { step(cpu, n); }
void init8(CPU8 &cpu) { init(cpu); }

void step16(CPU16 &cpu, int n) { step(cpu, n); }
void init16(CPU16 &cpu) { init(cpu); }

}
