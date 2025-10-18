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
		std::string label;
		int offset = 0;
	};
	struct Absolute {
		std::string label;
	};

	size_t index;
	std::variant<Relative, Absolute> substitute;
};

struct Section {
	std::vector<uint8_t> text;
};

struct Assembly {
	struct Section {
		size_t offset = 0;
		std::vector<uint8_t> content;
	};

	struct Label {
		size_t offset;
		Section Assembly::* section;
	};

	Section text;
	Section data;
	Section Assembly::* currentSection = &Assembly::text;
	std::vector<uint8_t> &current() { return (this->*currentSection).content; }

	std::unordered_map<std::string, Label> labels;
	std::unordered_map<std::string, int> defines;
	std::vector<Relocation> relocations;
};

int assemble(std::istream &is, Assembly &a, const char **err);
int link(Assembly &a, const char **err);
void disasm(std::span<const uint8_t> instr, std::string &out);

}

#endif
