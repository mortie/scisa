#ifndef SCISASM_H
#define SCISASM_H

#include <istream>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace scisasm {

struct Relocation {
	struct Relative {
		std::string name;
		int offset;
	};
	struct Absolute {
		std::string name;
	};

	size_t index;
	std::variant<Relative, Absolute> substitute;
};

struct Assembly {
	std::vector<uint8_t> output;
	std::vector<Relocation> relocations;
	std::unordered_map<std::string, size_t> labels;
};

int assemble(std::istream &is, Assembly &a, const char **err);
int link(Assembly &a, const char **err);

void disasm(std::span<const uint8_t> instr, std::string &out);

}

#endif
