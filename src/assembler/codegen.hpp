#pragma once

#include "parser.hpp"
#include "tokenizer.hpp"
#include <vector>
#include <cstdint>
#include <string_view>
#include <optional>
#include <utility>



struct CodeRegion {
  std::string_view label{};
  std::optional<uint32_t> startingAddress{};
  uint32_t nBytes{};
  std::vector<Statement> code{};

  CodeRegion(std::string_view label) : label{label} {};
  CodeRegion() = default;
};

struct AssemblyOptions {
  bool packingEnabled{false};
};

std::vector<uint8_t> generateCode(std::vector<std::pair<Statement, tokenizedLine>> &statements, AssemblyOptions opts);
std::vector<uint8_t> littleEndian(uint64_t val, uint8_t nBytes);