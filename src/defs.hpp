#pragma once

#include "src/defs.hpp"
#include <cstdint>

// Execution flag masks
namespace EF {
  inline constexpr uint64_t CARRY = 0x1;
  inline constexpr uint64_t OVERFLOW = 0x2;
  inline constexpr uint64_t ZERO = 0x4;
  inline constexpr uint64_t NEGATIVE = 0x8;

  inline constexpr uint64_t PROTECTED_ENABLE = 0x8000000000000000;
  inline constexpr uint64_t PAGING_ENABLE = 0x4000000000000000;
  inline constexpr uint64_t INTERRUPT_ENABLE = 0x2000000000000000;
}

// Page entry masks
namespace PE {
  inline constexpr uint32_t FRAME = 0xFFFFF000;
  inline constexpr uint32_t OCCUPIED = 0x1;
  inline constexpr uint32_t PROTECTED = 0x2;
  inline constexpr uint32_t MODIFIED = 0x4;
  inline constexpr uint32_t WRITABLE = 0x8;
  inline constexpr uint32_t EXECUTABLE = 0x10; 
  inline constexpr uint32_t ACCESSED = 0x20;
}


// Decoded binary register operation instruction
struct Inst {
  uint8_t opcode;
  uint8_t r0; // Only 4 bits of the registers are used
  uint8_t r1;
  int16_t offset;
};

// Register names (4bit max)
enum Reg : uint8_t {
  // GP Registers
  A = 0b0000,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  X,
  Y,
  SP = 0b1101, // Stack head
  BP = 0b1110, // Stack base
  Z = 0b1111,  // Zero register (unwritable)
};

enum ProtectedReg : uint8_t {
  EFLAGS, // Execiton flags
  USP, // User stack pointer
  PSP, // Privileged stack pointer
  IJT, // Interrupt jump table pointer
  RPT, // Root page table pointer
};

// Opcodes occupying the upper byte of an instruction
// Of form: (rroooooo) where (r) = reserved, (o) = opcode
enum Op : uint8_t {
  MOV = 0b00000000,
  GEF, // Get execution flags

  LB, // Load instructions
  LBU,
  LH,
  LHU,
  LW,
  LWU,
  LD,

  SB, // Store instructions
  SH,
  SW,
  SD,

  PUSH, // Stack operations
  POP,

  JMP, // Control flow
  JLT,
  JGT,
  JZR,
  JIF,

  AND, // Logical operations
  OR,
  XOR,
  SHL,
  SHR,

  ADD, // Arithmetic operations
  SUB,
  MUL,
  SMUL,
  DIV,
  SDIV,
  SSHR,

  INT, // Software interrupt

  // Privileged instructions
  PMOV, // Privileged register move
  IRET, // Interrupt return
};

enum IntCode : uint8_t {
  // Faults, all resume at the faulting instruction
  FAULT_START = 0x0,
  PAGE_FAULT = 0x0,
  INSTRUCTION_FAULT,  
  ALU_FAULT,

  FAULT_END = 0x1F,

  // HW Interrupts, resume at the next instruction
  HW_INTERRUPT_START = 0x20, 
  TIMER_CLOCK = 0x20,  

  HW_INTERRUPT_END = 0x9F,

  SOFTWARE_INTERUPT_START = 0xA0,
  SOFTWARE_INTERUPT_END = 0xFF
};


// To be thrown as an exception
struct Interrupt {
  IntCode code;
  uint64_t info{0};
  Interrupt(const IntCode code, const uint64_t info) : code{code}, info{info}
  {};
};