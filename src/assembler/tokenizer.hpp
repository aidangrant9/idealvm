#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

struct Token {
  enum class tokenType : uint8_t{
    IDENTIFIER,
    PLUS,
    MINUS,
    COLON,
    POUND,
    OPEN_BRACKET,
    CLOSE_BRACKET,
    INT_LITERAL,
    DELIMITER,
    END
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
      case tokenType::END:
        ret = "END";
        break;
    }

    return ret + " " + std::string(contents);
  }
};

struct tokenizedLine{
  std::string_view line{};
  std::vector<Token> toks{};
  size_t lineNum{};
};


std::vector<tokenizedLine> tokenize(std::string_view input);
std::vector<Token> tokenizeLine(std::string_view line, size_t lineNum);