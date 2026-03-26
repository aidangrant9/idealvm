#include "codegen.hpp"
#include "parser.hpp"
#include <cstdint>
#include <cstdlib>
#include <unordered_map>

std::vector<uint8_t> generateCode(std::vector<std::pair<Statement, tokenizedLine>> &statements, AssemblyOptions opts){
  std::vector<CodeRegion> placedRegions;
  std::vector<CodeRegion> unplacedRegions;

  /*
    The code below places code "regions", which are labeled (and the first code that may be unlabled)
    into lists depending on whether they are explicitly placed by the user or not.

    In the case of linear assembly (packing disabled), unplaced regions will implicity take the address
    of the next available space and placed into the placedRegions list.

    It is then assumed that these regions are in order

    For non-linear assembly, no assumptions are made and placed regions should be sorted and checked for
    conflicts, then unplaced regions packed into the gaps.

    During the placing procedure, the map from label name to address should be generated.

    After regions have been placed, assembly can take place.
  */

  CodeRegion current{};

  uint32_t addressCounter{0};

  auto commitRegion = [&](void){
    if(current.nBytes == 0){
      current = CodeRegion{};
    }
    else if(current.startingAddress.has_value()){
      placedRegions.emplace_back(std::move(current));
    }
    else{
      unplacedRegions.emplace_back(std::move(current));
    }

    current = CodeRegion{};
  };
  
  if(opts.packingEnabled == false){
    current.startingAddress = addressCounter;
  }
  
  for(const auto &[st, line] : statements){
    if(st.type == Statement::statementType::LABEL){
      commitRegion();
      const Label &label = st.data.label;
      current.label = label.name;

      if(opts.packingEnabled == false){
        if(label.position.has_value()){
          const auto labelPos = label.position.value();
          if(labelPos < addressCounter){
            // TODO: Clean up code
            // In this scenario, we will be doing linear assembly so throw an error
            std::cerr << "Codegen error: Region \"" << current.label << "\" needs to be placed at 0x" << std::hex << labelPos
              << " but address counter is at 0x" << std::hex << addressCounter << ", enable packing for nonlinear assembly\n"; 

            std::exit(EXIT_FAILURE);
          }
          current.startingAddress = labelPos;
          addressCounter = labelPos;
        }
        else{
          current.startingAddress = addressCounter;
        }
      }
      else{
        if(label.position.has_value()){
          current.startingAddress = label.position.value();
        }
      }
    }
    else if(st.type == Statement::statementType::DATA_IMPERATIVE){
      const DataImperative &imp = st.data.imp;
      current.code.push_back(st);
      current.nBytes += imp.nBytes;
      addressCounter += imp.nBytes;
    }
    else{

      current.code.push_back(st);
      current.nBytes += 4;
      addressCounter += 4;
    }
  }

  commitRegion();
 
  /*
    Here we place regions if using non-linear assembly
  */


  std::vector<CodeRegion> finalPlacement{};

  if(opts.packingEnabled){
    // Sort placed regions by starting address
    std::sort(placedRegions.begin(), placedRegions.end(),
      [](const auto &a, const auto &b){ return a.startingAddress < b.startingAddress; });

    // Sort unplaced regions by decreasing size
    std::sort(unplacedRegions.begin(), unplacedRegions.end(),
      [](const auto &a, const auto &b){ return a.nBytes > b.nBytes;});

    if(placedRegions.empty()){
      std::cout << "Warning: Packing is enabled and no placed regions exist. Entrypoint will be the largest region.\n";
    }

    addressCounter = 0;
    int64_t gapSize = INT64_MAX;

    if(!placedRegions.empty()){
      gapSize = placedRegions[0].startingAddress.value();
    }

    size_t nUnplacedProcessed = 0;

    // Fill unplaced regions into the gap
    auto fillGap = [&](){
      while(nUnplacedProcessed < unplacedRegions.size() && unplacedRegions[nUnplacedProcessed].nBytes <= gapSize){
        auto &region = unplacedRegions[nUnplacedProcessed];
        region.startingAddress = addressCounter;
        addressCounter += region.nBytes;
        gapSize -= region.nBytes;

        finalPlacement.push_back(region);

        nUnplacedProcessed++;
      }
    };

    fillGap();

    for(const auto &placedRegion : placedRegions){
      const auto &startAddress = placedRegion.startingAddress.value();

      if(startAddress < addressCounter){
        std::cerr << "Codegen error: Region \"" << placedRegion.label << "\" needs to be placed at 0x" << std::hex << startAddress
              << " but address counter is at 0x" << std::hex << addressCounter << "\n"; 

        std::exit(EXIT_FAILURE);
      }

      gapSize = placedRegion.startingAddress.value() - addressCounter;
      fillGap();
      finalPlacement.push_back(placedRegion);
      addressCounter = placedRegion.startingAddress.value() + placedRegion.nBytes;
    }

    gapSize = INT64_MAX;
    fillGap();
  }
  else{
    finalPlacement = std::move(placedRegions); 
  }

  #ifdef REGION_DEBUG
  for(auto region : finalPlacement){
    auto startAddress = region.startingAddress.value();
    std::cout << region.label << ":\t 0x" << std::hex << startAddress << " - 0x" << std::hex <<
      startAddress + region.nBytes-1 << "\n";
  }

  #endif

  // Generate mappings
  std::unordered_map<std::string, uint32_t> labelMap{};

  for(const auto &region : finalPlacement){
    if(region.label.empty()){
      continue;
    }

    const std::string label(region.label);

    // Check for collision
    if(labelMap.contains(label)){
      std::cerr << "Codegen error: Multiple definitions for region \"" + label << "\"\n";
      std::exit(EXIT_FAILURE); 
    }

    labelMap[label] = region.startingAddress.value();
  }


  // Generate byte stream

  std::vector<uint8_t> ret{};
  addressCounter = 0;

  for(const auto &region : finalPlacement){
    // Fill gap with zeroes
    ret.insert(ret.end(), region.startingAddress.value() - addressCounter, 0);

    // TODO: This whole codegen should really be in a class or at least shared structures, this is terrible
    for(const auto &statement : region.code){
      if(statement.type == Statement::statementType::DATA_IMPERATIVE){
        const DataImperative &imp = statement.data.imp;
        if(imp.label){ // We know from parsing that nBytes already must be 4
          if(!labelMap.contains(std::string(imp.label.value()))){
            std::cerr << "Codegen error: Label \"" << imp.label.value() << "\" doesn't exist\n";
            std::exit(EXIT_FAILURE);
          }
          uint32_t address = labelMap[std::string(imp.label.value())];

          const auto endianAddress = littleEndian(address, 4);
          ret.insert(ret.end(), endianAddress.begin(), endianAddress.end());
        }
        else{
          const auto endianValue = littleEndian(imp.data, imp.nBytes);
          ret.insert(ret.end(), endianValue.begin(), endianValue.end());
        }
      }
      else if(statement.type == Statement::statementType::INSTRUCTION){
        const Instruction &inst = statement.data.inst;
        ret.push_back(inst.opcode);
        uint8_t regs = inst.r0 << 4;
        regs |= inst.r1;
        ret.push_back(regs);

        int64_t offset = 0;
        // Compute offset
        if(inst.offset.label){
          if(!labelMap.contains(std::string(inst.offset.label.value()))){
            std::cerr << "Codegen error: Label \"" << inst.offset.label.value() << "\" doesn't exist\n";
            std::exit(EXIT_FAILURE);
          }

          offset = inst.offset.labelMultiplier * labelMap[std::string(inst.offset.label.value())];
        }
        offset += inst.offset.offset;


        // TODO: need to retain line mappings for errors, not descriptive
        if(offset > INT16_MAX || offset < INT16_MIN){
          std::cerr << "Codegen error: Computed offset is too large for instruction\n";
          std::exit(EXIT_FAILURE);
        }

        int16_t instOffset = static_cast<int16_t>(offset);

        ret.push_back((instOffset >> 8) & 0xFF);
        ret.push_back((instOffset) & 0xFF);
      } 
    }

    addressCounter = region.startingAddress.value() + region.nBytes;
  }

  return ret;
}

std::vector<uint8_t> littleEndian(uint64_t val, uint8_t nBytes){
  if(nBytes > 8){
    std::exit(EXIT_FAILURE);
  }

  std::vector<uint8_t> ret{};

  for(int i{0}; i < nBytes; i++){
    ret.push_back(val & 0xFF);
    val >>= 8;
  }

  return ret;
}

