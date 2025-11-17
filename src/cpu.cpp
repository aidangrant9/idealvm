#include "cpu.hpp"
#include "defs.hpp"
#include <cstdint>
#include <algorithm>
#include <stdexcept>

constexpr uint64_t msbMask = 0x8000000000000000;  

CPU::CPU(CPU::State s, const size_t memSize) 
  : st{s}, memory(memSize) {};

void CPU::progressClock(void){
  try{
    const uint32_t instruction = fetchInstruction();
    dispatchInstruction(instruction);
  }
  catch(Interrupt &i){
    handleInterrupt(i);
  }
  catch(std::out_of_range &e){

  }

  if(nipSet){
    nipSet = false;
    st.ip = nip;
  }
  else{
    st.ip += 4;
  }

  st.registers[Reg::Z] = 0;
}

void CPU::handleInterrupt(Interrupt &i){
  if(handlingInterrupt){
    throw std::runtime_error("Double fault");
  }

  handlingInterrupt = true;
  
  uint64_t eflags = st.protectedReg[EFLAGS];
  uint64_t rip = st.ip;

  if(i.code > IntCode::FAULT_END){
    rip += 4;
  }

  // Disable protection & interrupts
  st.protectedReg[EFLAGS] &= ~EF::PROTECTED_ENABLE;
  st.protectedReg[EFLAGS] &= ~EF::INTERRUPT_ENABLE;

  // Save user SP and swap to privileged stack
  st.protectedReg[USP] = st.registers[SP];
  st.registers[SP] = st.protectedReg[PSP];

  stackPush(rip);
  stackPush(eflags);

  const uint64_t jumpTableEntry = st.protectedReg[IJT] + i.code * 8;
  
  const uint32_t jumpAddress = mLoad(resolveAddress(jumpTableEntry), 4);

  nipSet = true;
  nip = jumpAddress;

  return;
}

uint32_t CPU::fetchInstruction(){
  uint32_t physicalAddress = resolveAddress(st.ip);
  uint32_t ret{};
  for(int i{4}; i >= 0; i--){
    ret |= memory[physicalAddress+i];
    ret <<= 8; 
  }
  return ret;
}

void CPU::dispatchInstruction(const uint32_t inst){
  // Only one instruction form, upper 2 bits of opcode may be used to define others
  Inst decoded = decodeBinRegInst(inst);

  if (decoded.opcode >= Op::LB && decoded.opcode <= Op::LD){
    executeLoad(decoded);
  }
  else if(decoded.opcode >= Op::SB && decoded.opcode <= Op::SD){
    executeStore(decoded);
  }
  else if(decoded.opcode >= Op::AND && decoded.opcode <= Op::SSHR){
    executeBinaryRegOp(decoded);
  } 
  else if(decoded.opcode >= Op::JMP && decoded.opcode <= Op::JIF){
    executeConditional(decoded);
  }
  else if(decoded.opcode >= Op::PMOV && decoded.opcode <= Op::IRET){
    executePriviliged(decoded);
  }
  else if (decoded.opcode <= Op::PMOV){
    executeMisc(decoded);
  }
  else{
    throw Interrupt(IntCode::INSTRUCTION_FAULT, 0x0);
  }
}

void CPU::executePriviliged(const Inst &inst){
  if(st.protectedReg[EFLAGS] & EF::PROTECTED_ENABLE){
    throw Interrupt(IntCode::INSTRUCTION_FAULT, 0x3);
  }

  if(inst.opcode == Op::PMOV){
    st.protectedReg[inst.r0] = st.registers[inst.r1] + inst.offset;
  }
  else if(inst.opcode == Op::IRET){
    const uint64_t eflags = stackPop();
    const uint64_t rip = stackPop();

    nipSet = true;
    nip = rip;

    st.protectedReg[PSP] = st.registers[SP];
    st.registers[SP] = st.protectedReg[USP];

    st.protectedReg[EFLAGS] = eflags;
    st.protectedReg[EFLAGS] |= EF::PROTECTED_ENABLE | EF::INTERRUPT_ENABLE;
  }
}

void CPU::executeMisc(const Inst &inst){
  if(inst.opcode == Op::MOV){
    st.registers[inst.r0] = st.registers[inst.r1]+inst.offset;
  }
  else if(inst.opcode == Op::GEF){
    st.registers[inst.r0] = st.protectedReg[EFLAGS];
  }
  else if(inst.opcode == Op::INT){
    uint64_t code = st.registers[inst.r1]+inst.offset;
    if(code < IntCode::SOFTWARE_INTERUPT_START || code > SOFTWARE_INTERUPT_END)
      throw Interrupt(IntCode::INSTRUCTION_FAULT, 0x03);
    throw Interrupt(static_cast<IntCode>(code), 0x0);
  }
}

void CPU::executeLoad(const Inst &inst){
  const uint32_t logicalAddress = st.registers[inst.r1] + inst.offset;
  const uint32_t physicalAddress = resolveAddress(logicalAddress);
  uint64_t result{};

  auto signExtend = [] (const uint64_t val, const uint8_t nBytes) -> uint64_t {
    const bool sign = static_cast<int64_t>(val) < 0;
    uint64_t ret = val;
    if(sign){
      ret |= UINT64_MAX << 8*nBytes;
    }
    return ret;
  };

  uint8_t nBytes{};
  bool hasSign{(inst.opcode - Op::LB) % 2 == 1};

  if(inst.opcode <= Op::LBU)
    nBytes = 1;
  else if(inst.opcode <= Op::LHU)
    nBytes = 2;
  else if(inst.opcode <= Op::LWU)
    nBytes = 4;
  else
    nBytes = 8;

  if(hasSign){
    result = signExtend(mLoad(physicalAddress,nBytes), nBytes);
  }
  else{
    result = mLoad(physicalAddress, nBytes);
  }

  st.registers[inst.r0] = result;
}

void CPU::executeStore(const Inst &inst){
  const uint32_t logicalAddress = st.registers[inst.r1] + inst.offset;
  const uint32_t physicalAddress = resolveAddress(logicalAddress);

  if(inst.opcode == Op::SB){
    mStore(physicalAddress, st.registers[inst.r0], 1);
  }
  else if(inst.opcode == Op::SH){
    mStore(physicalAddress, st.registers[inst.r0], 2);
  }
  else if(inst.opcode == Op::SW){
    mStore(physicalAddress, st.registers[inst.r0], 4);
  }
  else if(inst.opcode == Op::SD){
    mStore(physicalAddress, st.registers[inst.r0], 8);
  }
}

void CPU::executeStack(const Inst &inst){
  if(inst.opcode == Op::PUSH){
    stackPush(st.registers[inst.r1] + inst.offset);
  }
  else if(inst.opcode == Op::POP){
    st.registers[inst.r0] = stackPop();
  }
}

void CPU::stackPush(const uint64_t value){
  st.registers[Reg::SP] -= 8;
  mStore(st.registers[Reg::SP], value, 8);
}

uint64_t CPU::stackPop(void){
  uint64_t ret = mLoad(st.registers[Reg::SP], 8);
  st.registers[Reg::SP] += 8;
  return ret;
}

void CPU::executeConditional(const Inst &inst){
  nip = st.registers[inst.r1] + inst.offset;
  const bool negative = st.protectedReg[EFLAGS] & EF::NEGATIVE;
  const bool zero = st.protectedReg[EFLAGS] & EF::ZERO;

  switch(inst.opcode){
    case(Op::JMP):
      nipSet = true;  
      break;
    case(Op::JGT):
      nipSet = !(zero || negative);
      break;
    case(Op::JLT):
      nipSet = negative; 
      break;
    case(Op::JZR):
      nipSet = zero;
      break;
    case(Op::JIF):
      nipSet = st.registers[inst.r0];
      break;
    }
}

void CPU::executeBinaryRegOp(const Inst &inst){
  uint64_t result{};
  const uint64_t o1{st.registers[inst.r0]};
  const uint64_t o2{st.registers[inst.r1] + inst.offset};
  const auto so1 = static_cast<int64_t>(o1);
  const auto so2 = static_cast<int64_t>(o2);

  st.protectedReg[EFLAGS] &= (EF::CARRY | EF::OVERFLOW);

  switch(inst.opcode){
    case(Op::ADD):
      result = o1 + o2;
      if(didOverflow(o1, o2, result)){
        st.protectedReg[EFLAGS] |= EF::OVERFLOW;
      } 
      if(result < o1){
        st.protectedReg[EFLAGS] |= EF::CARRY;
      }
      break;
    case(Op::SUB):
      result = o1 - o2;
      if(didOverflow(o1, o2, result)){
        st.protectedReg[EFLAGS] |= EF::OVERFLOW;
      }  
      if(o2 > o1){
        st.protectedReg[EFLAGS] |= EF::CARRY; // Carry = 1 if we needed to borrow (x86 behaviour)
      }
      break;
    case(Op::MUL):
      result = o1 * o2;
      break;
    case(Op::SMUL):
      result = static_cast<uint64_t>(so1*so2);
      break;
    case(Op::DIV):
      if(o2 == 0){
        throw Interrupt(ALU_FAULT, 0x0);
      }
      result = o1 / o2;
      st.registers[inst.r1] = o1 % o2;      
      break; 
    case(Op::SDIV):
      if(o2 == 0){
        throw Interrupt(ALU_FAULT, 0x0);
      }
      result = static_cast<uint64_t>(so1/so2);
      st.registers[inst.r1] = static_cast<uint64_t>(so1%so2);
      break;
    case(Op::SSHR):
      if(o1 & msbMask)
        result = (o1 >> o2) | (UINT64_MAX << std::max<uint64_t>(64, 64-o2));
      else
        result = o1 >> o2;
      break;
    case(Op::AND):
      result = o1 & o2;
      break;
    case(Op::OR):
      result = o1 | o2;
      break;
    case(Op::XOR):
      result = o1 ^ o2;
      break;
    case(Op::SHL):
      result = o1 << o2;
      break;
    case(Op::SHR):
      result = o1 >> o2;
      break;
  }

  if(result == 0){
    st.protectedReg[EFLAGS] |= EF::ZERO;
  }
  if(result & msbMask){
    st.protectedReg[EFLAGS] |= EF::NEGATIVE;
  }

  st.registers[inst.r0] = result;
}

bool CPU::didOverflow(const uint64_t a, const uint64_t b,
                             const uint64_t res){
  auto aUb = a & msbMask;
  auto bUb = b & msbMask;
  auto rUb = res & msbMask;

  if(aUb == bUb){
    return aUb != rUb;
  }
  return false;
}

uint64_t CPU::mLoad(const uint32_t physicalAddress, const uint8_t nBytes){
  uint64_t ret{};

  for(int i{nBytes-1}; i >= 0; i--){
    ret |= memory[physicalAddress+i];
    ret <<= 8;
  }

  return ret;
}

void CPU::mStore(const uint32_t physicalAddress, uint64_t data,
                 const uint8_t nBytes){
  for(int i{0}; i < nBytes; i++){
    memory[physicalAddress+i] = static_cast<uint8_t>(data);
    data >>= 8;
  } 
}

uint32_t CPU::resolveAddress(const uint32_t address, const bool write, const bool jump){
  if(!(st.protectedReg[EFLAGS] & EF::PAGING_ENABLE)){
    return address;
  }

  const uint16_t rootIndex = (address & 0xFFC00000) >> 22;
  const uint16_t pageIndex = (address & 0x3FF000) >> 12;
  const uint16_t offset = address & 0xFFF;
  

  auto getPageMap = [&] (const uint32_t entry) -> uint32_t {
    uint64_t errorType{};

    if(!(entry & PE::OCCUPIED)){ // Page is not mapped or not present
      errorType = PE::OCCUPIED; 
    }
    else if((entry & PE::PROTECTED) && (st.protectedReg[EFLAGS] & EF::PROTECTED_ENABLE)){
      errorType = PE::PROTECTED;
    } 
    // Write and execution protection only with protection enabled
    else if(write && !(entry & PE::WRITABLE) && (st.protectedReg[EFLAGS] & EF::PROTECTED_ENABLE)){
      errorType = PE::WRITABLE; 
    }
    else if(jump && !(entry & PE::EXECUTABLE) && (st.protectedReg[EFLAGS] & EF::PROTECTED_ENABLE)){
      errorType = PE::EXECUTABLE; 
    }

    // Failing bit is used as the error code stored in upper 32 bits
    if(errorType){
      throw Interrupt(IntCode::PAGE_FAULT, (address & 0xFFFFFFFF) | (errorType << 32));
    }

    return entry & PE::FRAME;
  };

  uint32_t rootEntry = static_cast<uint32_t>(mLoad(st.protectedReg[RPT]+rootIndex, 4));
  const uint32_t pageTable = getPageMap(rootEntry);
  uint32_t tableEntry = static_cast<uint32_t>(mLoad(pageTable+pageIndex, 4));
  const uint32_t physicalFrame = getPageMap(tableEntry);

  tableEntry |= PE::ACCESSED;
  rootEntry |= PE::ACCESSED;
  if(write)
    tableEntry |= PE::MODIFIED;

  mStore(st.protectedReg[RPT]+rootIndex, tableEntry, 4);
  mStore(pageTable+pageIndex, tableEntry, 4);

  return physicalFrame+offset;
}

Inst CPU::decodeBinRegInst(const uint32_t inst){
  Inst decoded{};
  decoded.opcode = static_cast<uint8_t>(inst >> 24);
  decoded.r0 = static_cast<uint8_t>((inst & 0x00F00000) >> 20);
  decoded.r1 = static_cast<uint8_t>((inst & 0x000F0000) >> 16);
  decoded.offset = static_cast<int16_t>(inst);
  return decoded;
}