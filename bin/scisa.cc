#include <scisavm.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

void dumpBits(uint8_t val) {
	char buf[9];
	for (int i = 0; i < 8; ++i) {
		buf[i] = (val & 1 << (7 - i)) ? '1' : '0';
	}
	buf[8] = '\0';
	std::cout << buf;
}

template<typename T>
void dumpCPU(scisavm::CPU<T> &cpu)
{
	std::cout
		<< "PC " << int(cpu.pc) << "; SP " << int(cpu.sp) << '\n'
		<< "ACC " << int(cpu.acc) << "; X " << int(cpu.x)
		<< "; Y " << int(cpu.y) << '\n';
	if (cpu.pc < cpu.pmem.size()) {
		std::cout << "INST ";
		dumpBits(cpu.pmem[cpu.pc]);
		std::cout << '\n';
	} else {
		std::cout << "INSTR OOB\n";
	}
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("Usage: %s <source>\n", argv[0]);
		return 1;
	}

	std::fstream f(argv[1]);
	if (f.bad()) {
		std::cerr << "Failed to open " << argv[1] << '\n';
		return 1;
	}

	std::stringstream ss;
	ss << f.rdbuf();
	f.close();
	if (f.bad()) {
		std::cerr << "Read error\n";
		return 1;
	}

	scisavm::CPU8 cpu;
	std::string pmem = std::move(ss).str();
	cpu.pmem = std::span{ (uint8_t *)pmem.data(), pmem.size() };

	dumpCPU(cpu);
	std::string line;
	while (std::getline(std::cin, line)) {
		cpu.step(1);
		if (cpu.error) {
			std::cout << "Error: " << cpu.error << '\n';
			break;
		}

		dumpCPU(cpu);
	}

	return 0;
}
