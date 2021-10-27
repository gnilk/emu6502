cmake_minimum_required(VERSION 3.20)
project(emu6502)

set(CMAKE_CXX_STANDARD 14)


# this compiles the c64 binary through kick-assembler...
set(C64BINARY "c64/bin/test.prg")
add_custom_command(OUTPUT ${C64BINARY}
        COMMAND ${PROJECT_SOURCE_DIR}/cc64.cmd
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "compiling c64 binary"
        VERBATIM)

add_custom_target(c64binary ALL DEPENDS ${C64BINARY})

#
# Create the EMU target
#
add_executable(emu6502 src/main.cpp)

# Let it depend on the test program binary...
add_dependencies(emu6502 c64binary)
