#include "scisasm/include/scisasm.h"
#include <scisavm.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

template<typename T>
static void dumpCPU(scisavm::CPU<T> &cpu)
{
	std::cout
		<< "PC " << int(cpu.pc) << "; SP " << int(cpu.sp) << '\n'
		<< "ACC " << int(cpu.acc) << "; X " << int(cpu.x)
		<< "; Y " << int(cpu.y) << '\n';
	std::cout
		<< 'Z' << cpu.flags.zero() << ' '
		<< 'C' << cpu.flags.carry() << ' '
		<< 'N' << cpu.flags.negative() << ' '
		<< 'V' << cpu.flags.overflow() << '\n';

	std::string disasm;
	scisasm::disasm(cpu.pmem.subspan(cpu.pc), disasm);
	std::cout << disasm << '\n';
}

static int runCPU(std::span<uint8_t> pmem)
{
	scisavm::CPU8 cpu;
	cpu.pmem = pmem;

	uint8_t dmem[256];
	cpu.dmem.push_back({
		.start = 0,
		.data = dmem,
	});

	dumpCPU(cpu);
	std::string line;
	while (std::getline(std::cin, line)) {
		cpu.step(1);
		if (cpu.error) {
			std::cout << "Error: " << cpu.error << '\n';
			return 1;
		}

		dumpCPU(cpu);
	}

	return 1;
}

static int runFile(char *path)
{
	std::fstream f(path);
	if (f.bad()) {
		std::cerr << "Failed to open " << path << '\n';
		return 1;
	}

	std::stringstream ss;
	ss << f.rdbuf();
	f.close();
	if (f.bad()) {
		std::cerr << "Read error\n";
		return 1;
	}

	std::string str = std::move(ss).str();
	auto pmem = std::span((uint8_t *)str.data(), str.size());
	return runCPU(pmem);
}

static int assemble(std::istream &is, std::ostream &os)
{
	if (is.bad()) {
		std::cerr << "Input error\n";
		return 1;
	}

	if (os.bad()) {
		std::cerr << "Output error\n";
		return 1;
	}

	scisasm::Assembly a;
	const char *err;
	if (scisasm::assemble(is, a, &err) < 0) {
		std::cerr << "Assembler error: " << err << '\n';
		return 1;
	}

	if (scisasm::link(a, &err) < 0) {
		std::cerr << "Linker error: " << err << '\n';
		return 1;
	}

	os.write((const char *)a.output.data(), a.output.size());
	if (os.bad()) {
		std::cerr << "Output error\n";
		return 1;
	}

	return 0;
}

static void usage(const char *argv0)
{
	printf("Usage: %s run <file>\n", argv0);
	printf("Usage: %s asm [infile] [outfile]\n", argv0);
}

int main(int argc, char **argv)
{
	using namespace std::literals;

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	if (argv[1] == "run"sv && argc == 3) {
		return runFile(argv[2]);
	}

	if (argv[1] == "asm"sv && argc <= 4) {
		if (argc == 2) {
			return assemble(std::cin, std::cout);
		}

		std::fstream is(argv[2]);
		if (argc == 3) {
			return assemble(is, std::cout);
		}

		std::fstream os(argv[3]);
		return assemble(is, os);
	}

	usage(argv[0]);
	return 0;
}
