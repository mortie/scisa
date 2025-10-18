#include "scisasm.h"

namespace scisasm {

class Reader {
public:
	Reader(const std::string &str): str_(str) {}

	int peek()
	{
		if (index_ < str_.size()) {
			return str_[index_];
		}

		return EOF;
	}

	bool eof() { return index_ >= str_.size(); }

	std::string_view rest() { return std::string_view(str_).substr(index_); }

	void consume() { index_ += 1; }

private:
	size_t index_ = 0;
	const std::string &str_;
};

static bool chIsWhitespace(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

static bool chIsInitialIdent(char ch)
{
	return
		(ch >= 'a' && ch <= 'z') ||
		(ch >= 'A' && ch <= 'Z') ||
		ch == '_' || ch == '-' || ch == '$';
}

static bool chIsIdent(char ch)
{
	return
		chIsInitialIdent(ch) ||
		(ch >= '0' && ch <= '9');
}

static bool strIsIdent(std::string_view str)
{
	if (str.size() == 0) {
		return false;
	}

	if (!chIsInitialIdent(str[0])) {
		return false;
	}

	for (char ch: str.substr(1)) {
		if (!chIsIdent(ch)) {
			return false;
		}
	}

	return true;
}

static bool chIsNumeric(char ch)
{
	return ch >= '0' && ch <= '9';
}

static bool strIsNumeric(std::string_view str)
{
	if (str.size() == 0) {
		return false;
	}

	if (str[0] == '-' || str[0] == '+') {
		str = str.substr(1);
	}

	for (char ch: str) {
		if (!chIsNumeric(ch)) {
			return false;
		}
	}

	return true;
}

static int parseNumeric(std::string_view str)
{
	if (str.size() == 0) {
		return 0;
	}

	int sign = 1;
	if (str[0] == '-') {
		sign = -1;
		str = str.substr(1);
	} else if (str[0] == '+') {
		str = str.substr(1);
	}

	int num = 0;
	for (char ch: str) {
		num *= 10;
		num += ch - '0';
	}

	return num * sign;
}

static void skipSpace(Reader &r)
{
	while (chIsWhitespace(r.peek())) {
		r.consume();
	}
}

static void upper(std::string &str)
{
	for (auto it = str.begin(); it != str.end(); ++it) {
		char ch = *it;
		// a..z - 32 maps to A..Z in ASCII
		if (ch >= 'a' && ch <= 'z') {
			*it = ch -= 32;
		}
	}
}

static void error(const char **err, const char *msg)
{
	if (err) {
		*err = msg;
	}
}

static int emitSpecial(
	uint8_t lo, const std::string &param,
	Assembly &a, const char **err)
{
	if (param != "") {
		error(err, "No parameter expected");
		return -1;
	}

	a.current().push_back(lo);
	return 0;
}

enum Relativity {
	RELATIVE,
	ABSOLUTE,
};

static int emitNormal(
	uint8_t hi, const std::string &param, Relativity rel,
	Assembly &a, const char **err)
{
	hi <<= 3;

	if (param == "") {
		error(err, "Parameter expected");
		return -1;
	}

	if (param == "%X") {
		a.current().push_back(hi | 0b001);
		return 0;
	}

	if (param == "%Y") {
		a.current().push_back(hi | 0b010);
		return 0;
	}

	if (param == "%A") {
		a.current().push_back(hi | 0b011);
		return 0;
	}

	// Handle a constant number literal
	if (strIsNumeric(param)) {
		int num = parseNumeric(param);
		if (num == 0) {
			a.current().push_back(hi | 0b000);
			return 0;
		}

		a.current().push_back(hi | 0b100);
		a.current().push_back(uint8_t(num));
		return 0;
	}

	// Handle a constant label
	if (strIsIdent(param)) {
		a.current().push_back(hi | 0b100);

		auto it = a.defines.find(param);
		if (it != a.defines.end()) {
			a.current().push_back(uint8_t(it->second));
			return 0;
		}

		Relocation reloc = {
			.index = a.current().size(),
		};

		switch (rel) {
		case RELATIVE:
			reloc.substitute = Relocation::Relative {
				.label = param,
				.offset = -1,
			};
			break;
		case ABSOLUTE:
			reloc.substitute = Relocation::Absolute {
				.label = param,
			};
			break;
		};

		a.relocations.push_back(std::move(reloc));
		a.current().push_back(0);
		return 0;
	}

	// Handle register + constant
	if (param[0] == '%') {
		Reader r(param);
		r.consume();
		switch (r.peek()) {
		case 'X':
			a.current().push_back(hi | 0b101);
			break;
		case 'Y':
			a.current().push_back(hi | 0b110);
			break;
		case 'A':
			a.current().push_back(hi | 0b111);
			break;
		default:
			error(err, "Bad register");
			return -1;
		}

		r.consume();
		skipSpace(r);

		if (r.peek() != '+') {
			error(err, "Unsupported parameter");
			return -1;
		}
		r.consume();

		skipSpace(r);
		auto rest = r.rest();

		if (strIsIdent(rest)) {
			std::string restStr = std::string(rest);
			auto it = a.defines.find(restStr);
			if (it != a.defines.end()) {
				a.current().push_back(uint8_t(it->second));
				return 0;
			}

			Relocation reloc = {
				.index = a.current().size(),
				.substitute = Relocation::Absolute {
					.label = param,
				},
			};
			a.relocations.push_back(std::move(reloc));
			a.current().push_back(0);
			return 0;
		}

		if (strIsNumeric(rest)) {
			int num = parseNumeric(rest);
			if (num == 0) {
				a.current().push_back(hi | 0b000);
				return 0;
			}

			a.current().push_back(uint8_t(num));
			return 0;
		}
	}

	error(err, "Unsupported parameter");
	return -1;
}

static int emitInstr(
	const std::string &op,
	const std::string &param,
	Assembly &a, const char **err)
{
	if (op == "NOP") {
		return emitSpecial(0b000, param, a, err);
	}

	if (op == "LSR") {
		return emitSpecial(0b001, param, a, err);
	}

	if (op == "LSL") {
		return emitSpecial(0b00001'011, param, a, err);
	}

	if (op == "ROR") {
		return emitSpecial(0b010, param, a, err);
	}

	if (op == "INC") {
		return emitSpecial(0b011, param, a, err);
	}

	if (op == "ROL") {
		return emitSpecial(0b00011'011, param, a, err);
	}

	if (op == "ADD") {
		return emitNormal(0b00001, param, ABSOLUTE, a, err);
	}

	if (op == "SUB") {
		return emitNormal(0b00010, param, ABSOLUTE, a, err);
	}

	if (op == "ADC") {
		return emitNormal(0b00011, param, ABSOLUTE, a, err);
	}

	if (op == "XOR") {
		return emitNormal(0b00100, param, ABSOLUTE, a, err);
	}

	if (op == "AND") {
		return emitNormal(0b00101, param, ABSOLUTE, a, err);
	}

	if (op == "OR") {
		return emitNormal(0b00110, param, ABSOLUTE, a, err);
	}

	if (op == "CMP") {
		return emitNormal(0b00111, param, ABSOLUTE, a, err);
	}

	if (op == "MVX") {
		return emitNormal(0b01000, param, ABSOLUTE, a, err);
	}

	if (op == "MVY") {
		return emitNormal(0b01001, param, ABSOLUTE, a, err);
	}

	if (op == "MVA") {
		return emitNormal(0b01010, param, ABSOLUTE, a, err);
	}

	if (op == "MHA") {
		return emitNormal(0b01011, param, ABSOLUTE, a, err);
	}

	if (op == "SPS") {
		return emitNormal(0b01100, param, ABSOLUTE, a, err);
	}

	if (op == "LDX") {
		return emitNormal(0b01101, param, ABSOLUTE, a, err);
	}

	if (op == "LDY") {
		return emitNormal(0b01110, param, ABSOLUTE, a, err);
	}

	if (op == "LDA") {
		return emitNormal(0b01111, param, ABSOLUTE, a, err);
	}

	if (op == "STX") {
		return emitNormal(0b10000, param, ABSOLUTE, a, err);
	}

	if (op == "STY") {
		return emitNormal(0b10001, param, ABSOLUTE, a, err);
	}

	if (op == "STA") {
		return emitNormal(0b10010, param, ABSOLUTE, a, err);
	}

	if (op == "JMP") {
		return emitNormal(0b10011, param, ABSOLUTE, a, err);
	}

	if (op == "JLR") {
		return emitNormal(0b10100, param, ABSOLUTE, a, err);
	}

	if (op == "B") {
		return emitNormal(0b10101, param, RELATIVE, a, err);
	}

	if (op == "BCC" || op == "BGE") {
		return emitNormal(0b10110, param, RELATIVE, a, err);
	}

	if (op == "BCS" || op == "BLT") {
		return emitNormal(0b10111, param, RELATIVE, a, err);
	}

	if (op == "BEQ" || op == "BZS") {
		return emitNormal(0b11000, param, RELATIVE, a, err);
	}

	if (op == "BNE" || op == "BZC") {
		return emitNormal(0b11001, param, RELATIVE, a, err);
	}

	if (op == "BMI") {
		return emitNormal(0b11010, param, RELATIVE, a, err);
	}

	if (op == "BPL") {
		return emitNormal(0b11011, param, RELATIVE, a, err);
	}

	if (op == "BVS") {
		return emitNormal(0b11100, param, RELATIVE, a, err);
	}

	if (op == "BVC") {
		return emitNormal(0b11101, param, RELATIVE, a, err);
	}

	if (op == "PUSH") {
		return emitNormal(0b11110, param, ABSOLUTE, a, err);
	}

	if (op == "POP") {
		if (param == "VOID") {
			a.current().push_back(0b11111'000);
			return 0;
		}

		if (param == "%X") {
			a.current().push_back(0b11111'001);
			return 0;
		}

		if (param == "%Y") {
			a.current().push_back(0b11111'010);
			return 0;
		}

		if (param == "%A") {
			a.current().push_back(0b11111'011);
			return 0;
		}

		error(err, "Unknown POP parameter");
		return -1;
	}

	error(err, "Unknown instruction");
	return -1;
}

static int handleDirective(
	const std::string &op,
	const std::string &param,
	Assembly &a,
	const char **err)
{
	if (op == ".TEXT") {
		if (param != "") {
			error(err, "No parameter expected");
			return -1;
		}

		a.currentSection = &Assembly::text;
		return 0;
	}

	if (op == ".DATA") {
		if (param != "") {
			error(err, "No parameter expected");
			return -1;
		}

		a.currentSection = &Assembly::data;
		return 0;
	}

	if (op == ".ASCII" || op == ".STRING") {
		Reader r(param);
		if (r.peek() != '"') {
			error(err, "Expected '\"'");
			return -1;
		}
		r.consume();

		auto &data = a.current();
		while (true) {
			if (r.eof()) {
				error(err, "Unexpected EOF");
				return -1;
			}

			char ch = r.peek();
			r.consume();
			if (ch == '"') {
				break;
			}

			if (ch == '\\') {
				if (r.eof()) {
					error(err, "Unexpected EOF");
					return -1;
				}

				ch = r.peek();
				r.consume();
				if (ch == '\\' || ch == '"') {
					data.push_back(ch);
				} else if (ch == 'n') {
					data.push_back('\n');
				} else if (ch == 'r') {
					data.push_back('\r');
				} else if (ch == 't') {
					data.push_back('\t');
				} else if (ch == '0') {
					data.push_back(0);
				} else {
					error(err, "Unexpected escape");
					return -1;
				}
			} else {
				data.push_back(ch);
			}
		}

		skipSpace(r);
		if (!r.eof()) {
			error(err, "Trailing garbage");
			return -1;
		}

		if (op == ".STRING") {
			data.push_back(0);
		}

		return 0;
	}

	if (op == ".BYTE") {
		if (!strIsNumeric(param)) {
			error(err, "Invalid parameter");
			return -1;
		}

		a.current().push_back(parseNumeric(param));
		return 0;
	}

	if (op == ".WORD") {
		if (!strIsNumeric(param)) {
			error(err, "Invalid parameter");
			return -1;
		}

		uint16_t num = uint16_t(parseNumeric(param));
		a.current().push_back(num & 0x00ff);
		a.current().push_back((num & 0xff00) >> 8);
		return 0;
	}

	if (op == ".DEFINE") {
		Reader r(param);
		if (!chIsInitialIdent(r.peek())) {
			error(err, "Invalid identifier");
			return -1;
		}

		std::string key;
		do {
			key += r.peek();
			r.consume();
		} while (chIsIdent(r.peek()));
		upper(key);

		skipSpace(r);
		std::string val;
		while (!r.eof()) {
			val += r.peek();
			r.consume();
		}

		if (!strIsNumeric(val)) {
			error(err, "Invalid value");
			return -1;
		}

		if (a.defines.contains(key)) {
			error(err, "Duplicate define");
			return -1;
		}

		a.defines[key] = parseNumeric(val);
		return 0;
	}

	error(err, "Invalid directive");
	return -1;
}

int assemble(std::istream &is, Assembly &a, const char **err)
{
	std::string line;
	std::string op;
	std::string param;
	while (std::getline(is, line)) {
		for (size_t i = 0; i < line.size(); ++i) {
			if (line[i] == ';') {
				line.erase(i);
				break;
			}
		}

		Reader r(line);

		skipSpace(r);
		if (r.eof()) {
			continue;
		}

		op.clear();
		if (chIsInitialIdent(r.peek()) || r.peek() == '.') {
			do {
				op += r.peek();
				r.consume();
			} while (chIsIdent(r.peek()));
		}
		upper(op);

		skipSpace(r);

		if (r.peek() == ':') {
			if (!strIsIdent(op)) {
				error(err, "Invalid label name");
				return -1;
			}

			r.consume();
			skipSpace(r);
			if (!r.eof()) {
				error(err, "Unexpected trailing garbage after label");
				return -1;
			}

			if (a.labels.contains(op)) {
				error(err, "Duplicate label");
				return -1;
			}

			a.labels[op] = {
				.section = a.currentSection,
				.offset = a.current().size(),
			};
			continue;
		}

		param.clear();
		size_t lastNonWhitespace = 0;
		while (!r.eof()) {
			char ch = r.peek();
			if (!chIsWhitespace(ch)) {
				lastNonWhitespace = param.size();
			}

			param += ch;
			r.consume();
		}

		if (lastNonWhitespace < param.size()) {
			param.erase(lastNonWhitespace + 1);
		}

		skipSpace(r);
		if (!r.eof()) {
			error(err, "Unexpected trailing garbage after instruction");
			return -1;
		}

		if (op[0] == '.') {
			if (handleDirective(op, param, a, err) < 0) {
				return -1;
			}

			continue;
		}

		upper(param);
		if (emitInstr(op, param, a, err) < 0) {
			return -1;
		}
	}

	return 0;
}

int link(Assembly &a, const char **err)
{
	for (auto &reloc: a.relocations) {
		auto &sub = reloc.substitute;
		if (auto *r = std::get_if<Relocation::Relative>(&sub); r) {
			auto it = a.labels.find(r->label);
			if (it == a.labels.end()) {
				error(err, "Invalid relative relocation");
				return -1;
			}

			auto &section = a.*it->second.section;
			int rel =
				int(it->second.offset) +
				int(section.offset) -
				(int(reloc.index) + r->offset);

			if (rel > 127 || rel < -128) {
				error(err, "Relative relocation out of range");
				return -1;
			}

			a.text.content[reloc.index] = uint8_t(rel);
			continue;
		}

		if (auto *r = std::get_if<Relocation::Absolute>(&sub); r) {
			auto it = a.labels.find(r->label);
			if (it == a.labels.end()) {
				error(err, "Invalid absolute relocation");
				return -1;
			}

			auto &section = a.*it->second.section;
			size_t abs = it->second.offset + section.offset;

			if (abs > 255) {
				error(err, "Absolute relocation out of range");
				return -1;
			}

			a.text.content[reloc.index] = uint8_t(abs);
			continue;
		}

		// We should never get here
		error(err, "Invalid relocation type");
		return -1;
	}

	return 0;
}

int disasm(std::span<const uint8_t> instr, std::string &out)
{
	if (instr.size() == 0) {
		out = "OOB";
		return 1;
	}

	uint8_t op = instr[0] >> 3;
	uint8_t param = instr[0] & 0x07;

	switch (op) {
	case 0b00000:
		switch (param) {
		case 0b000:
			out = "NOP";
			return 1;
		case 0b001:
			out = "LSR";
			return 1;
		case 0b010:
			out = "ROR";
			return 1;
		case 0b011:
			out = "INC";
			return 1;
		}

		out = "BAD SPECIAL";
		return 1;
	case 0b00001:
		out = "ADD";
		break;
	case 0b00010:
		out = "SUB";
		break;
	case 0b00011:
		out = "ADC";
		break;
	case 0b00100:
		out = "XOR";
		break;
	case 0b00101:
		out = "AND";
		break;
	case 0b00110:
		out = "OR ";
		break;
	case 0b00111:
		out = "CMP";
		break;
	case 0b01000:
		out = "MVX";
		break;
	case 0b01001:
		out = "MVY";
		break;
	case 0b01010:
		out = "MVA";
		break;
	case 0b01011:
		out = "MHA";
		break;
	case 0b01100:
		out = "SPS";
		break;
	case 0b01101:
		out = "LDX";
		break;
	case 0b01110:
		out = "LDY";
		break;
	case 0b01111:
		out = "LDA";
		break;
	case 0b10000:
		out = "STX";
		break;
	case 0b10001:
		out = "STY";
		break;
	case 0b10010:
		out = "STA";
		break;
	case 0b10011:
		out = "JMP";
		break;
	case 0b10100:
		out = "JLR";
		break;
	case 0b10101:
		out = "B";
		break;
	case 0b10110:
		out = "BCC";
		break;
	case 0b10111:
		out = "BCS";
		break;
	case 0b11000:
		out = "BEQ";
		break;
	case 0b11001:
		out = "BNE";
		break;
	case 0b11010:
		out = "BMI";
		break;
	case 0b11011:
		out = "BPL";
		break;
	case 0b11100:
		out = "BVS";
		break;
	case 0b11101:
		out = "BVC";
		break;
	case 0b11110:
		out = "PUSH";
		break;
	case 0b11111:
		switch (param) {
		case 0b000:
			out = "POP VOID";
			return 1;
		case 0b001:
			out = "POP %X";
			return 1;
		case 0b010:
			out = "POP %Y";
			return 1;
		case 0b011:
			out = "POP %A";
			return 1;
		}

		out = "BAD POP";
		return 1;
	}

	uint8_t next = 0;
	if (param & 0b100) {
		if (instr.size() < 2) {
			out += " OOB";
			return 1;
		}

		next = instr[1];
	}

	switch (param) {
	case 0b000:
		out += " 0";
		return 1;
	case 0b001:
		out += " %X";
		return 1;
	case 0b010:
		out += " %Y";
		return 1;
	case 0b011:
		out += " %A";
		return 1;
	case 0b100:
		out += " ";
		out += std::to_string(next);
		return 2;
	case 0b101:
		out += " %X + ";
		out += std::to_string(next);
		return 2;
	case 0b110:
		out += " %Y + ";
		out += std::to_string(next);
		return 2;
	case 0b111:
		out += " %A + ";
		out += std::to_string(next);
		return 2;
	}

	return 1;
}

}
