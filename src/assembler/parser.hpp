#pragma once

#include "src/common/defs.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

struct Token {
  enum class tokenType {
    IDENTIFIER,
    PLUS,
    MINUS,
    COLON,
    POUND,
    OPEN_BRACKET,
    CLOSE_BRACKET,
    INT_LITERAL,
    DELIMITER,
  };

  tokenType type;
  std::string_view contents;

  Token(tokenType type, std::string_view contents)
    : type{type}, contents{contents}
  {};

  std::string print(void){
    std::string ret{};

    switch(type){
      case tokenType::IDENTIFIER:
        ret = "IDENTIFIER";
        break;
      case tokenType::PLUS:
        ret = "PLUS";
        break;
      case tokenType::MINUS:
        ret = "MINUS";
        break;
      case tokenType::DELIMITER:
        ret = "DELIMITER";
        break;
      case tokenType::OPEN_BRACKET:
        ret = "OPEN_BRACKET";
        break;
      case tokenType::CLOSE_BRACKET:
        ret = "CLOSE_BRACKET";
        break;
      case tokenType::POUND:
        ret = "POUND";
        break;
      case tokenType::COLON:
        ret = "COLON";
        break;
      case tokenType::INT_LITERAL:
        ret = "INT_LITERAL";
        break;
    }

    return ret + " " + std::string(contents);
  }
};

// Instruction mnemonic to opcode + number of operands
static std::unordered_map<std::string, std::pair<Op, uint8_t>> instNames{
  {"MOV", {Op::MOV, 2}},
  {"GEF", {Op::GEF, 1}},
  {"LB", {Op::LB, 2}},
  {"LBU", {Op::LBU, 2}},
  {"LH", {Op::LH, 2}},
  {"LHU", {Op::LHU, 2}},
  {"LW", {Op::LW, 2}},
  {"LWU", {Op::LWU, 2}},
  {"LD", {Op::LD, 2}},
  {"SB", {Op::SB, 2}},
  {"SH", {Op::SH, 2}},
  {"SW", {Op::SW, 2}},
  {"SD", {Op::SD, 2}},
  {"PUSH", {Op::PUSH, 1}},
  {"POP", {Op::POP, 1}},
  {"JMP", {Op::JMP, 1}},
  {"JLT", {Op::JLT, 1}},
  {"JGT", {Op::JGT, 1}},
  {"JZR", {Op::JZR, 1}},
  {"JIF", {Op::JIF, 2}},
  {"AND", {Op::AND, 2}},
  {"OR", {Op::OR, 2}},
  {"XOR", {Op::XOR, 2}},
  {"SHL", {Op::SHL, 2}},
  {"SHR", {Op::SHR, 2}},
  {"ADD", {Op::ADD, 2}},
  {"SUB", {Op::SUB, 2}},
  {"MUL", {Op::MUL, 2}},
  {"SMUL", {Op::SMUL, 2}},
  {"SDIV", {Op::SDIV, 2}},
  {"SSHR", {Op::SSHR, 2}},
  {"INT", {Op::INT, 1}},
  {"PMOV", {Op::PMOV, 2}},
  {"IRET", {Op::IRET, 0}},
};

std::vector<std::vector<Token>> tokenize(std::string_view input);
std::vector<Token> tokenizeLine(std::string_view line, size_t lineNum);
