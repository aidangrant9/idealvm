#include "parser.hpp"

#include <cstdlib>
#include <ios>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
#include <filesystem>
#include <optional>
#include <fstream>

#define TOKEN_DEBUG

int main(int argc, char *argv[]){
  // Could change to use a hash table with more arguments
  // Linear is fine for now

  const std::filesystem::path thisExecutable(argv[0]);
  const std::string usage = "Usage: " + thisExecutable.filename().string() + " [flags] input.asm\n"
                            "Use --help flag for further information\n";
  const std::string help = "Flags:\n"
                           "-o [filepath]: Set output filepath\n"
                           "-s [imagesize]: Set the output image size in bytes.";

  std::vector<std::string> positionalArguments{};
  std::filesystem::path outputPath{};
  std::optional<std::size_t> imageSize{};

  for(int i{1}; i < argc; i++){
    const std::string arg = argv[i];
    const bool hasNext = i+1 < argc;

    if(arg == "-o"){
      if(!hasNext){
        std::cerr << usage << "-o: No output filepath provided\n";
        return EXIT_FAILURE;
      }
      try{
        outputPath = argv[++i];
      }
      catch(...){
        std::cerr << usage << "-o: Provided filepath is invalid\n";
        return EXIT_FAILURE;
      }
    }
    else if(arg == "-s"){
      if(!hasNext){
        std::cerr << usage << "-s: No image size provided\n";
        return EXIT_FAILURE;
      }

      bool conversionFailure{false};
      try{
        imageSize = std::stoull(argv[++i], nullptr, 0);     
      }
      catch(...){
        conversionFailure = true;
      }

      if(conversionFailure || imageSize < 1 || imageSize > 0x800000){
        std::cerr << usage << "-s: Invalid image size, value must be a decimal integer between 1 and 8MiB\n";
        return EXIT_FAILURE;
      }
    }
    else if(arg == "--help"){
      std::cout << help;
      return EXIT_SUCCESS;
    }
    else{
      positionalArguments.push_back(argv[i]);
    }
  }

  if(positionalArguments.size() < 1){
    std::cerr << usage;
    return EXIT_FAILURE;
  }

  std::filesystem::path inputPath{};

  try{
    inputPath = positionalArguments[0];
    if(!std::filesystem::is_regular_file(inputPath))
      throw std::invalid_argument("Not a regular file");
  }
  catch(...){
    std::cerr << "Invalid input file: " + positionalArguments[0] + "\n";
    return EXIT_FAILURE;
  }

  std::ifstream inputFile(inputPath, std::ios::binary);

  if(!inputFile){
    std::cerr << "Failed to open input file: " + positionalArguments[0] + "\n";
    return EXIT_FAILURE;
  }


  // Read entire file into buffer string
  std::string buffer;
  inputFile.seekg(0, std::ios_base::end);
  buffer.resize(inputFile.tellg());
  inputFile.seekg(0);
  inputFile.read(buffer.data(), buffer.size());

  if(!inputFile){
    std::cerr << "Error reading input file: " + positionalArguments[0] + "\n";
    inputFile.close();
    return EXIT_FAILURE;
  }

  inputFile.close();

  // Tokenize input at this point we should not have any file handles
  // open as this function can terminate
  std::vector<std::vector<Token>> tokens = tokenize(buffer);


  #ifdef TOKEN_DEBUG
  for(auto &vt : tokens){
    std::cout << "\n LINE \n\n";
    for(auto &t : vt){
      std::cout << t.print() << "\n";
    }
  }
  #endif
  
  return EXIT_SUCCESS;
}
