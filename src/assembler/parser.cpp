#include "parser.hpp"
#include "tokenizer.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>

using enum Token::tokenType;
using Error = Parser::Error; 

std::vector<Statement> parse(std::vector<tokenizedLine> &input){
  std::vector<Statement> ret{};

  for(auto &vt : input){
    if(vt.toks.empty())
      continue;

    Statement s = parseLine(vt);
    ret.push_back(s);
  }

  return ret;
}

Statement parseLine(tokenizedLine &line){
  Parser p(line.toks, 0);
  std::optional<Statement> ret{};

  // Check first ident, inst ->     parse inst
  //                    data_imp -> parse data imp
  //                    arb ->      parse label
  // else: expect identifier failure

  Token firstTok = p.peek();

  if(firstTok.type != IDENTIFIER){
    p.error.emplace("Expected an identifier", firstTok);
  }

  if(isLabelForm(line.toks)){
    ret = parseLabel(p);
  }
  else if(instNames.contains(std::string(firstTok.contents))){
    ret = parseInstruction(p); 
  }
  else if(firstTok.contents == "DD" || firstTok.contents == "DW"
          || firstTok.contents == "DH" || firstTok.contents == "DB"){
    ret = parseDataImperative(p);
  }
  else{
    p.error.emplace("Unknown identifier", firstTok);
  }

  if(ret && !isEnd(p)){
    p.error.emplace("Expected end of sequence", p.peek());
    ret = std::nullopt;
  }

  if(!ret){
    if(p.error){
      std::cerr << "Parse error on line " << line.lineNum << ":\n";
      std::cerr << line.line << "\n";

      size_t charIn{0};

      if(p.error->offendingToken.type == END){
        p.setPos(line.toks.size()-2); // If we reach end token then failure must have been before
        charIn = line.line.size();
      }
      else{
        const char* epoint = p.error->offendingToken.contents.data();
        charIn = epoint - line.line.data();
      }

      std::cerr <<  std::string(charIn, ' ') << "^ " << p.error->message << "\n";
    }
    else{
      std::cerr << "Unknown parse error on line " << line.lineNum << "\n";
    }
    std::exit(EXIT_FAILURE);
  }

  return ret.value();
}

bool isLabelForm(std::vector<Token> &line){
  Parser p(line, 0);

  if(!(p.get(IDENTIFIER) && p.get(OPEN_BRACKET))){
    return false;
  }

  p.get(INT_LITERAL); // Optional

  if(p.get(CLOSE_BRACKET) && p.get(COLON)){
    return isEnd(p);
  }
  else{
    return false;
  }
}

bool isDataImperativeForm(std::vector<Token> &line){
  Parser p(line, 0);

  if(!p.get(IDENTIFIER)){
    return false;
  }

  auto l1 = p.getAny({INT_LITERAL, IDENTIFIER});

  if(!l1 || !isEnd(p)){
    return false;
  }

  return true;
}

std::optional<Statement> parseInstruction(Parser &p){
  Instruction ret{};

  std::optional<Token> name = p.get(IDENTIFIER);

  std::pair<Op, uint8_t> instAttributes;

  if(name.has_value()){
    instAttributes = instNames[std::string(name->contents)];
  }
  else{
    return std::nullopt;
  }

  ret.opcode = instAttributes.first;
  
  if(instAttributes.second == 2){
    std::optional<uint8_t> r0;

    if(ret.opcode != Op::PMOV)
      r0 = parseGPReg(p);
    else
      r0 = parseProtectedReg(p);

    if(!r0)
      return std::nullopt;

    ret.r0 = r0.value();

    auto delim = p.get(DELIMITER);

    if(!delim){
      p.error.emplace("Expected \",\"", p.peek());
      return std::nullopt;
    }

    auto r1 = parseGPReg(p);
    

    if(!r1)
      return std::nullopt;

    ret.r1 = r1.value();

    if(isEnd(p)){
      return Statement(ret);
    }

    auto offset = parseOffset(p);

    if(!offset){
      return std::nullopt;
    }

    ret.offset = offset.value();
    return Statement(ret);
  }
  else if(instAttributes.second == 1){ // Instructions with 1 operand utilise r1, must parse offset
    auto r1 = parseGPReg(p); // TODO: This sequence is entirely duplicated above, can fix

    if(!r1)
      return std::nullopt;

    ret.r1 = r1.value();

    if(isEnd(p)){
      return Statement(ret);
    }

    auto offset = parseOffset(p);

    if(!offset){
      return std::nullopt;
    }

    ret.offset = offset.value();
    return Statement(ret);
  }

  return Statement(ret);
}


std::optional<uint8_t> parseGPReg(Parser &p){
  auto id = p.get(IDENTIFIER);

  if(!id){
    p.error.emplace("Expected a register argument", p.peek());
    return std::nullopt;
  }

  std::string name = std::string(id->contents);

  if(regNames.contains(name))
    return regNames[name];
  else{
    p.error.emplace("Expected a general-purpose register, got unknown identifier \"" + name + "\"",
                    id.value());
    return std::nullopt;
  }
}

std::optional<uint8_t> parseProtectedReg(Parser &p){
  auto id = p.get(IDENTIFIER);

  if(!id){
    p.error.emplace("Expected a register argument", p.peek());
    return std::nullopt;
  }

  std::string name = std::string(id->contents);

  if(protectedRegNames.contains(name))
    return protectedRegNames[name];
  else{
    p.error.emplace("Expected a protected register, got unknown identifier \"" + name + "\"",
                    id.value());
    return std::nullopt;
  }
}

// TODO: This function is messy, fix would be nice
std::optional<Offset> parseOffset(Parser &p){
  Offset ret;

  auto sign = p.getAny({PLUS, MINUS});

  if(!sign){
    p.error.emplace("Expected a sign \"+/-\" or end of sequence", p.peek());
    return std::nullopt;
  }

  auto op0 = p.peek();

  if(op0.type == INT_LITERAL){
    auto num = parse16BitInt(p, sign->type);

    if(!num)
      return std::nullopt;

    ret.offset = num.value();
  }
  else if(op0.type == IDENTIFIER){
    ret.label = op0.contents;

    if(sign->type == PLUS){
      ret.labelMultiplier = 1;
    }
    else{
      ret.labelMultiplier = -1;
    }

    p.get(IDENTIFIER);
  }
  else{
    p.error.emplace("Expected a label or int literal", p.peek());
    return std::nullopt;
  }

  if(isEnd(p)){
    return ret;
  }

  auto sign2 = p.getAny({PLUS, MINUS});

  if(!sign2){
    p.error.emplace("Expected a sign \"+/-\" or end of sequence", p.peek());
    return std::nullopt;
  }

  auto op1 = p.peek();


  if(op0.type == INT_LITERAL){
    if(op1.type == INT_LITERAL){
      p.error.emplace("Expected a label", p.peek());
      return std::nullopt; 
    }

    ret.label = op1.contents;

    if(sign2->type == PLUS){
      ret.labelMultiplier = 1;
    }
    else{
      ret.labelMultiplier = -1;
    }

    p.get(IDENTIFIER);
  }
  else if(op0.type == IDENTIFIER){
    if(op1.type == IDENTIFIER){
      p.error.emplace("Expected an int literal", p.peek());
      return std::nullopt;
    }
    
    auto num = parse16BitInt(p, sign->type);

    if(!num)
      return std::nullopt;

    ret.offset = num.value();
  }
  else{
    p.error.emplace("Expected a label or int literal", p.peek());
    return std::nullopt; 
  }


  return ret;
}

std::optional<int16_t> parse16BitInt(Parser &p, Token::tokenType t){
  auto op0 = p.get(INT_LITERAL);

  int64_t num = std::stoull(std::string(op0->contents), nullptr, 0);

  if(num > INT16_MAX){
    p.error.emplace("Integer must be representable in 16 bits", op0.value());
    return std::nullopt;
  }

  if(t == MINUS){
    num = -num;
    if(num < INT16_MIN){
      p.error.emplace("Integer must be representable in 16 bits", op0.value());
      return std::nullopt;
    }
  }

  return static_cast<int16_t>(num);
}

std::optional<Statement> parseDataImperative(Parser &p){
  DataImperative ret;

  auto ident = p.get(IDENTIFIER);

  if(!ident){
    p.error.emplace("Expected identifier", p.peek());
    return std::nullopt;
  }

  std::string i = std::string(ident->contents);

  if(instNames.contains(i) || regNames.contains(i) || protectedRegNames.contains(i)){
    p.error.emplace("Identifier \"" + i + "\" is a reserved keyword", ident.value());
    return std::nullopt;
  }

  if(i != "DB" && i != "DH" && i != "DW" && i != "DD"){
    p.error.emplace("Expected data imperative keyword \"DB\"/\"DH\"/\"DW\"/\"DD\", got " + i, ident.value());
    return std::nullopt;
  }

  auto op0 = p.getAny({INT_LITERAL, IDENTIFIER});

  if(!op0){
    p.error.emplace("Expected argument", p.peek());
    return std::nullopt;
  }

  ret.data = 0;

  if(op0->type == IDENTIFIER){
    ret.label = op0->contents;
  }
  else{
    ret.data = std::stoull(std::string(op0->contents), nullptr, 0);
  }

  bool badSize{false};

  if(i == "DB"){
    ret.nBytes = 1;
    badSize = ret.data > UINT8_MAX;
  }
  else if(i == "DH"){
    ret.nBytes = 2;
    badSize = ret.data > UINT16_MAX;
  }
  else if(i == "DW"){
    ret.nBytes = 4;
    badSize = ret.data > UINT32_MAX;
  }
  else{
    ret.nBytes = 8;
  }

  if(badSize){
    p.error.emplace("Integer literal " + std::string(op0->contents) + " too large for desired data type",
                    op0.value());
    return std::nullopt;
  }

  return Statement(ret);
}

std::optional<Statement> parseLabel(Parser &p){
  Label ret;

  auto ident = p.get(IDENTIFIER);

  if(!ident){
    p.error.emplace("Expected identifier", p.peek());
    return std::nullopt;
  }

  std::string i = std::string(ident->contents);

  if(instNames.contains(i) || regNames.contains(i) || protectedRegNames.contains(i)){
    p.error.emplace("Identifier \"" + i + "\" is a reserved keyword", ident.value());
    return std::nullopt;
  }

  if(!p.get(OPEN_BRACKET)){
    p.error.emplace("Expected \"(\"", p.peek());
    return std::nullopt;
  }

  auto offset = p.get(INT_LITERAL);

  if(offset){
    uint64_t num = std::stoull(std::string(offset->contents), nullptr, 0);

    if(num > UINT32_MAX){
      p.error.emplace("Integer must be representable in 32 bits", offset.value());
      return std::nullopt;
    }

    ret.position = num; 
  }

  if(!p.get(CLOSE_BRACKET)){
    p.error.emplace( "Expected \")\"", p.peek());
    return std::nullopt;
  }

  if(!p.get(COLON)){
    p.error.emplace( "Expected \":\"", p.peek());
    return std::nullopt;
  }

  return Statement(ret);
}

bool isEnd(Parser &p){
  if(p.peek().type != END)
    return false;
  return true;
}