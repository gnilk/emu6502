cmake_minimum_required(VERSION 3.15)
project(emu6502)

set(CMAKE_CXX_STANDARD 20)

include(CheckIncludeFile)

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
# main emu6502 src
#

list(APPEND src src/cpu.cpp src/cpu.h)
list(APPEND src src/memory.cpp src/memory.h)
list(APPEND src src/main.cpp)


#
# Imgui src
#
list(APPEND imgui ext/imgui/imgui.cpp ext/imgui/imgui.h)
list(APPEND imgui ext/imgui/imconfig.h)
list(APPEND imgui ext/imgui/imgui_demo.cpp)
list(APPEND imgui ext/imgui/imgui_draw.cpp)
list(APPEND imgui ext/imgui/imgui_tables.cpp)
list(APPEND imgui ext/imgui/imgui_widgets.cpp)

list(APPEND libs "")

if (WIN32)
    list(APPEND src src/Win32/ui.cpp)

    check_include_file(d3d10.h HAS_D3D10)
    if (HAS_D3D10)
        list(APPEND imgui_backend ext/imgui/backends/imgui_impl_dx10.cpp)
        list(APPEND imgui_backend ext/imgui/backends/imgui_impl_win32.cpp)


        list(APPEND libs d3d10)
#        target_link_libraries(imgui_backend INTERFACE imgui d3d10)
 #       target_include_directories(imgui_backend INTERFACE ${IMGUI_BACKENDS_DIR})
    else ()
        message (STATUS "IMGUI_IMPL_DX10 set to ON but DirectX 10 could not be found.")
    endif ()

elseif(APPLE)
    # TODO fix this...
    list(APPEND src src/macOS/ui.cpp)
endif()


#
# Create the EMU target
#

add_executable(emu6502 ${src} ${imgui} ${imgui_backend})
target_link_libraries(emu6502 ${libs})
target_include_directories(emu6502 PUBLIC ext/imgui/)
target_include_directories(emu6502 PUBLIC ext/imgui/backends)

add_custom_command(TARGET emu6502 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/ $<TARGET_FILE_DIR:emu6502>
        COMMENT "Copying assets files...."
        VERBATIM)


        # Let it depend on the test program binary...
add_dependencies(emu6502 c64binary)
