# SCISA: SWAN Computer ISA

An ISA which can work for either 8 or 16 bit CPUs.

## Registers and flags

SCISA defines 3 general purpose registers: X, Y and A (accumulator).

In addition, there are special purpose registers:

* PC: The address of the current instruction.
* SP: The stack pointer.

All instructions 

Some instructions update the following flags:

* C: Carry. This is set if the carry out of the ALU is 1.
* Z: Zero. This is set if the output of the ALU is all 0s.
* V: Overflow. This is set if add/adc/sub/cmp results in a signed overflow.
* N: Negative. This is set if the high bit of the ALU's output is set.

## Instruction format

SWISA instructions take exactly 1 parameter.

The first byte of every instruction is:

	xxxxx yyy

Where `xxxxx` is a 5-bit op code, and `yyy` specifies parameter mode.

Instructions where `yyy` starts with a `0` bit are 1 byte.
Instructions where `yyy` starts with a `1` bit are 2-byte.

## Parameter modes

* `000`: The parameter is the value 0.
* `001`: The parameter is the value in register X.
* `010`: The parameter is the value in register Y.
* `011`: The parameter is the value in register A.
* `100`: The parameter is the next byte in the instruction stream.
* `101`: The parameter is the value in register X + the next byte in the instruction stream.
* `110`: The parameter is the value in register Y + the next byte in the instruction stream.
* `111`: The parameter is the value in register A + the next byte in the instruction stream.

## Instructions

* `00000`: Special; the parameter bits specify the instruction
* `00001`: ADD\*; A = A + param
* `00010`: SUB\*; A = A - param
* `00011`: ADC\*, Add with carry; A = A + param + C
* `00100`: XOR\*; A = A XOR param
* `00101`: AND\*; A = A AND param
* `00110`: OR\*; A = A OR param
* `00111`: CMP\*, Compare; A - param
* `01000`: MVX, Move X; X = param
* `01001`: MVY, Move Y; Y = param
* `01010`: MVA, Move A; A = param
* `01011`: MHA\*\*, Move High A; A = param << 8
* `01100`: SPS, Stack Pointer Set; SP = param
* `01101`: LDX, Load X; X = \[value at memory address specified by param\]
* `01110`: LDY, Load Y; Y = \[value at memory address specified by param\]
* `01111`: LDA, Load A; A = \[value at memory address specified by param\]
* `10000`: STX, Store X; \[value at memory address specified by param\] = X
* `10001`: STY, Store Y; \[value at memory address specified by param\] = Y
* `10010`: STA, Store A; \[value at memory address specified by param\] = A
* `10011`: JMP; Jump to \[param\].
* `10100`: JLR, Jump and Link Register; Jump to \[param\], storing the return address in register Y
* `10101`: B, Branch; Jump to \[PC + param\]
* `10110`: BCC, Branch if Carry Clear; Jump to \[PC + param\] if C is clear
* `10111`: BCS, Branch if Carry Set; Jump to \[PC + param\] if C is set
* `11000`: BEQ, Branch if Equal; Jump to \[PC + param\] if Z is set
* `11001`: BNE, Branch if Not Equal; Jump to \[PC + param\] if Z is clear
* `11010`: BMI, Branch if Minus; Jump to \[PC + param\] if N is set
* `11011`: BPL, Branch if Positive; Jump to \[PC + param\] if N is clear
* `11100`: BVS, Branch if Overflow Set; Jump to \[PC + param\] if V is set
* `11101`: BVC, Branch if Overflow Clear; Jump to \[PC + param\] if V is clear
* `11110`: PUSH; \[value at SP\] = param, SP += 1 or 2
* `11111`: POP\*\*\*; SP -= 1 or 2, param = \[value at SP\]

Special instructions:

* `00000 000`: NOP, do nothing.
* `00000 001`: LSR\*, logical Shift Right; A = A >> 1, C = shifted-out bit
* `00000 010`: ROR\*, Rotate Right; A = A >> 1, set low bit to C, C = shifted-out bit

Aliases:

* `00001 011`: LSL\*, Logical Shift Left; Equivalent to ADD A
* `00011 011`: ROL\*, Rotate Left; Equivalent to ADC A
* `10110`: BLT, Branch if Less Than; Jump to \[PC + param\] if C is clear
* `10111`: BGE, Branch if Less Than or Equal; Jump to \[PC + param\] if C is set

\*: Instruction updates flags

\*\*: Instruction is only valid for 16-bit CPUs.

\*\*\*: The parameter mode is treated as a destination.
Only values `000`, `001`, `010` and `011` are valid,
representing void, X, Y or A respectively.
