cmake_minimum_required(VERSION 3.22)
project(idealVM LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

add_library(common_flags INTERFACE)

if(MSVC)
    target_compile_options(common_flags INTERFACE /Wall)
else()
    target_compile_options(common_flags INTERFACE -Wall -Wextra)
endif()

add_library(common INTERFACE
            src/common/defs.hpp)       

file(GLOB EMULATOR_SRC CONFIGURE_DEPENDS src/emulator/*.cpp)
file(GLOB ASSEMBLER_SRC CONFIGURE_DEPENDS src/assembler/*.cpp)

add_executable(emulator ${EMULATOR_SRC}) 
add_executable(assembler ${ASSEMBLER_SRC})

target_link_libraries(emulator PRIVATE common common_flags)
target_link_libraries(assembler PRIVATE common common_flags)

target_include_directories(emulator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(assembler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
