#include <scisasm.h>
#include <scisavm.h>

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

class TextIO: public scisavm::MemoryIO {
public:
	virtual void store(size_t, uint8_t val)
	{
		std::cerr << char(val);
	}
};

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

template<typename T>
static int debugCPU(scisavm::CPU<T> &cpu)
{
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

template<typename T>
static int runCPU(scisavm::CPU<T> &cpu)
{
	while (!cpu.error) {
		cpu.step(1024);
	}

	std::cout << "Error: " << cpu.error << '\n';
	return 1;
}

struct Computer {
	scisavm::CPU8 cpu;
	std::vector<uint8_t> text;
	std::vector<uint8_t> data;
	TextIO textIO;
};

static int setupComputer(Computer &comp, char *path)
{
	std::fstream f(path);
	if (f.bad()) {
		std::cerr << "Failed to open " << path << '\n';
		return 1;
	}

	uint8_t word[4];
	f.read((char *)word, 4);
	if (f.gcount() != 4) {
		std::cerr << "Short file\n";
		return 1;
	}

	if (std::string_view((char *)word, 4) != "\033SCE") {
		std::cerr << "Missing SCE magic\n";
		return 1;
	}

	while (true) {
		f.read((char *)word, 4);
		if (f.gcount() == 0) {
			break;
		} else if (f.gcount() != 4) {
			std::cerr << "Short section name read\n";
			return 1;
		}

		std::string_view name((char *)word, 4);
		std::vector<uint8_t> *section;
		if (name == "TEXT") {
			section = &comp.text;
		} else if (name == "DATA") {
			section = &comp.data;
		} else {
			std::cerr << "Unknown section name: '" << name << "'\n";
			return 1;
		}

		f.read((char *)word, 4);
		if (f.gcount() != 4) {
			std::cerr << "Short section size read\n";
			return 1;
		}

		uint32_t size =
			(uint32_t(word[0]) << 0) |
			(uint32_t(word[1]) << 8) |
			(uint32_t(word[2]) << 16) |
			(uint32_t(word[3]) << 24);

		section->resize(size);
		f.read((char *)section->data(), size);
		if (f.gcount() != size) {
			std::cerr << "Short section data read";
			return 1;
		}
	}

	std::cerr << "Loaded SCE:\n";
	std::cerr << "* TEXT: " << comp.text.size() << " bytes\n";
	std::cerr << "* DATA: " << comp.data.size() << " bytes\n";
	std::cerr << '\n';

	comp.cpu.pmem = comp.text;

	comp.data.resize(256);
	comp.cpu.dmem.push_back({
		.start = 0,
		.data = comp.data,
	});

	comp.cpu.io.push_back({
		.start = 255,
		.size = 1,
		.io = &comp.textIO,
	});

	return 0;
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

	// Magic SCE header
	os.write("\033SCE", 4);

	// Write a section
	auto writeSection = [&os](std::string_view name, std::span<uint8_t> content) {
		assert(name.size() == 4);
		os.write(name.data(), 4);

		uint32_t size = content.size();
		uint8_t sizeBytes[] = {
			uint8_t((size & 0x000000ffu) >> 0),
			uint8_t((size & 0x0000ff00u) >> 8),
			uint8_t((size & 0x00ff0000u) >> 16),
			uint8_t((size & 0xff000000u) >> 24),
		};
		os.write((const char *)sizeBytes, 4);
		os.write((const char *)content.data(), content.size());
	};

	writeSection("TEXT", a.text.content);
	writeSection("DATA", a.data.content);

	std::cerr << "Written SCE:\n";
	std::cerr << "* TEXT: " << a.text.content.size() << " bytes\n";
	std::cerr << "* DATA: " << a.data.content.size() << " bytes\n";

	if (os.bad()) {
		std::cerr << "Output error\n";
		return 1;
	}

	return 0;
}

static void usage(const char *argv0)
{
	printf("Usage: %s run <file>\n", argv0);
	printf("Usage: %s dbg <file>\n", argv0);
	printf("Usage: %s asm [infile] [outfile]\n", argv0);
}

int main(int argc, char **argv)
{
	using namespace std::literals;

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	if (argv[1] == "dbg"sv && argc == 3) {
		Computer comp;
		setupComputer(comp, argv[2]);
		return debugCPU(comp.cpu);
	}

	if (argv[1] == "run"sv && argc == 3) {
		Computer comp;
		setupComputer(comp, argv[2]);
		return runCPU(comp.cpu);
	}

	if (argv[1] == "asm"sv && argc <= 4) {
		if (argc == 2) {
			return assemble(std::cin, std::cout);
		}

		std::fstream is(argv[2]);
		if (argc == 3) {
			return assemble(is, std::cout);
		}

		std::fstream os(argv[3], std::fstream::out | std::fstream::trunc);
		return assemble(is, os);
	}

	if (argv[1] == "dis"sv && argc == 3) {
		Computer comp;
		setupComputer(comp, argv[2]);
		return runCPU(comp.cpu);
	}

	usage(argv[0]);
	return 0;
}
