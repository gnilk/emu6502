cmake_minimum_required(VERSION 3.15)
project(emu6502)

set(CMAKE_CXX_STANDARD 20)


# this compiles the c64 binary through kick-assembler...
set(C64BINARY "c64/bin/test.prg")
if (APPLE)
    add_custom_command(OUTPUT ${C64BINARY}
            COMMAND ${PROJECT_SOURCE_DIR}/cc64.sh
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            COMMENT "compiling c64 binary"
            VERBATIM)
elseif (WIN32)
    add_custom_command(OUTPUT ${C64BINARY}
            COMMAND ${PROJECT_SOURCE_DIR}/cc64.cmd
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            COMMENT "compiling c64 binary"
            VERBATIM)
endif()

add_custom_target(c64binary ALL DEPENDS ${C64BINARY})

#
# Create the EMU target
#
list(APPEND src src/cpu.cpp src/cpu.h)
#list(APPEND src src/.cpp src/cpu.h)
list(APPEND src src/main.cpp)

add_executable(emu6502 ${src} src/Memory.h src/Memory.cpp)

# Let it depend on the test program binary...
add_dependencies(emu6502 c64binary)
