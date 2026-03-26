#pragma once

#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <unordered_map>
#include <string>
#include <optional>
#include "../common/defs.hpp"
#include "tokenizer.hpp"

struct Offset {
  int8_t labelMultiplier; // to be multiplied with address of label (used only for sign as of now)
  std::optional<std::string_view> label;
  int16_t offset; // offset to be added to label
};

struct Instruction {
  uint8_t opcode;
  uint8_t r0;
  uint8_t r1;
  Offset offset;
};

struct Label {
  std::optional<uint32_t> position{};
  std::string_view name{};
};

struct DataImperative {
  uint8_t nBytes{};
  uint64_t data{};
  std::optional<std::string_view> label{};
};

struct Statement {
  enum class statementType : uint8_t {
    LABEL,
    INSTRUCTION,
    DATA_IMPERATIVE,
  };

  union Data{
    Instruction inst;
    Label label;
    DataImperative imp;
  };

  // Could probably use rvalue instead since we want to return as a statement
  Statement(Instruction &i) : type{statementType::INSTRUCTION}, data{.inst = i} {}
  Statement(DataImperative &d) : type{statementType::DATA_IMPERATIVE}, data{.imp = d} {}  
  Statement(Label &l) : type{statementType::LABEL}, data{.label = l} {}

  statementType type{};
  Data data{};
};

struct Parser {
  struct Error{
    std::string message;
    Token offendingToken;

    Error(std::string message, Token offending)
      : message{message}, offendingToken{offending}
    {}
  };

  std::vector<Token> &toks;
  std::optional<Error> error{};
  size_t cur;

  Parser(std::vector<Token> &ts, size_t pos) : toks{ts}{
    setPos(pos);
  }

  Parser(Parser &p, size_t seekPos) : toks{p.toks} {
    setPos(seekPos);
  }

  void setPos(size_t pos){
    if(pos >= toks.size()){
      std::cerr << "Error setting parser position: position out of token stream range";
      std::exit(EXIT_FAILURE);
    }
    cur = pos;
  }

  Token peek(){
    return toks[cur];
  }

  std::optional<Token> get(Token::tokenType t){
    if(t == toks[cur].type){
      return toks[cur++];
    }
    return std::nullopt;
  }

  std::optional<Token> getAny(std::initializer_list<Token::tokenType> li){
    for(const Token::tokenType &t : li){
      if(t == toks[cur].type){
        return toks[cur++];
      }
    }
    return std::nullopt;
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
  {"DIV", {Op::DIV, 2}},
  {"SDIV", {Op::SDIV, 2}},
  {"SSHR", {Op::SSHR, 2}},
  {"INT", {Op::INT, 1}},
  {"PMOV", {Op::PMOV, 2}},
  {"IRET", {Op::IRET, 0}},
};

static std::unordered_map<std::string, uint8_t> regNames {
  {"A", Reg::A},
  {"B", Reg::B},
  {"C", Reg::C},
  {"D", Reg::D},
  {"E", Reg::E},
  {"F", Reg::F},
  {"G", Reg::G},
  {"H", Reg::H},
  {"I", Reg::I},
  {"J", Reg::J},
  {"K", Reg::K},
  {"X", Reg::X},
  {"Y", Reg::Y},
  {"Z", Reg::Z},
  {"SP", Reg::SP},
  {"BP", Reg::BP},
};

static std::unordered_map<std::string, uint8_t> protectedRegNames {
  {"EFLAGS", ProtectedReg::EFLAGS},
  {"USP", ProtectedReg::USP},
  {"PSP", ProtectedReg::PSP},
  {"IJT", ProtectedReg::IJT},
  {"RPT", ProtectedReg::RPT},
};

std::vector<std::pair<Statement,tokenizedLine>> parse(std::vector<tokenizedLine> &input);
Statement parseLine(tokenizedLine &line);
bool isLabelForm(std::vector<Token> &line);
bool isDataImperativeForm(std::vector<Token> &line);
bool isEnd(Parser &p);

std::optional<Statement> parseInstruction(Parser &p);
std::optional<Statement> parseDataImperative(Parser &p);
std::optional<Statement> parseLabel(Parser &p);
std::optional<uint8_t> parseGPReg(Parser &p);
std::optional<uint8_t> parseProtectedReg(Parser &p);
std::optional<Offset> parseOffset(Parser &p);
std::optional<int16_t> parse16BitInt(Parser &p, Token::tokenType t);