#pragma once

#include "src/common/defs.hpp"
#include <cstdint>
#include <vector>

struct CPU {
  struct State {
    uint64_t registers[16]{};
    uint64_t protectedReg[16]{};

    uint64_t ip{0}; // Current instruction pointer
  };

  State st;         // Internal state of CPU at start of clock
  std::vector<uint8_t> memory;

  uint64_t nip{};     // New instruction pointer
  bool nipSet{false}; // Should nip be used?
  bool intSet{false}; // Interrupt requested
  bool handlingInterrupt{false}; // Is an interrupt being handled?

  CPU(State s, const size_t memSize);

  void progressClock(void);
  uint32_t fetchInstruction(void);
  void dispatchInstruction(const uint32_t inst);
  Inst decodeBinRegInst(const uint32_t inst);

  void executeBinaryRegOp(const Inst &inst);
  void executeConditional(const Inst &inst);
  void executeLoad(const Inst &inst);
  void executeStore(const Inst &inst);
  void executeStack(const Inst &inst);
  void executePriviliged(const Inst &inst);
  void executeMisc(const Inst &inst);
  bool didOverflow(const uint64_t a, const uint64_t b,
                   const uint64_t res);

  void handleInterrupt(Interrupt &i);
  
  uint32_t resolveAddress(const uint32_t address, const bool write = false,
                          const bool jump = false);

  // Stack helpers
  void stackPush(const uint64_t value);
  uint64_t stackPop(void);

  // Helpers for loading and storing respecting endianness
  uint64_t mLoad(const uint32_t physicalAddress, const uint8_t nBytes);
  void mStore(const uint32_t physicalAddress, uint64_t data,
              const uint8_t nBytes);
};

