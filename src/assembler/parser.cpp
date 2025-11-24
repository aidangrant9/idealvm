#include "parser.hpp"
#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <format>
#include <iostream>

std::vector<std::vector<Token>> tokenize(std::string_view input){
  std::vector<std::vector<Token>> ret{};
  size_t lineNum{1};

  size_t pos{0};
  while(pos < input.size()){
    size_t end = input.find('\n', pos);

    if(pos == end){ // Line is empty
      pos = end+1;
      lineNum++;
      continue;
    }
    else if(end == input.npos){
      end = input.size(); 
    }

    ret.push_back(tokenizeLine(input.substr(pos,end-pos), lineNum));

    pos = end+1;
    lineNum++;
  }

  return ret;
}

std::vector<Token> tokenizeLine(std::string_view line, size_t lineNum){
  using enum Token::tokenType;

  std::vector<Token> ret{};
  std::string errorMessage{};

  size_t pos{0};
  while(pos < line.size()){
    char cur = line.at(pos);
    size_t processed{0};

    if(std::isspace(cur)){
      goto PROGRESS_ONE;
    }

    if(!isalnum(cur) && std::string("_,-+():#").find(cur) == std::string::npos){
      errorMessage = std::format("Illegal character \'{}\'", cur);
      break;
    }

    switch(cur){
      case ',':
        ret.emplace_back(DELIMITER, line.substr(pos,1));
        goto PROGRESS_ONE;
      case '-':
        ret.emplace_back(MINUS, line.substr(pos,1));
        goto PROGRESS_ONE;
      case '+':
        ret.emplace_back(PLUS, line.substr(pos, 1));
        goto PROGRESS_ONE;
      case '(':
        ret.emplace_back(OPEN_BRACKET, line.substr(pos, 1));
        goto PROGRESS_ONE;
      case ')':
        ret.emplace_back(CLOSE_BRACKET, line.substr(pos,1));
        goto PROGRESS_ONE;
      case ':':
        ret.emplace_back(COLON, line.substr(pos,1));
        goto PROGRESS_ONE;
      case '#':
        ret.emplace_back(POUND, line.substr(pos,1));
        goto PROGRESS_ONE;
    }

    try{
      std::stoull(std::string(line.substr(pos, line.npos)), &processed, 0);
      ret.emplace_back(INT_LITERAL, line.substr(pos,processed));
      pos = pos+processed;
      continue;
    }
    catch(std::invalid_argument &e){
      // Do nothing, can still be a valid identifier
    }
    catch(std::out_of_range &e){
      errorMessage = std::format("Integer literal too large");
      pos = pos+processed;
      break;
    }


    // Process identifier using alphanum and _
    processed = 0;
    while(pos+processed < line.size()){
      if(!std::isalpha(line.at(pos+processed)) && line.at(pos+processed) != '_')
        break;
      processed++;
    }

    // Do not allow a digit directly after an identifier
    if(pos+processed < line.size() && std::isdigit(line.at(pos+processed))){
      errorMessage = "Identifier contains a digit";
      pos = pos+processed;
      break;
    }

    ret.emplace_back(IDENTIFIER, line.substr(pos, processed));
    pos = pos+processed;
    continue;

PROGRESS_ONE: 
    pos++;
    continue;
  }

  // Exits immediately since this is an irrecoverable position
  // IF used as a library, this may want to be changed to use an error
  // flag or exceptions.
  if(!errorMessage.empty()){
    std::cerr << "Tokenization error on line " << lineNum << ":\n";
    std::cerr << line << "\n";
    std::cerr << std::string(pos, ' ') << "^ " << errorMessage << "\n";
    std::exit(EXIT_FAILURE);
  }

  return ret;
}