//
// Created by goatman on 10/31/2021.
//
#include <cstdint>
#include <cassert>
#include <cstring>

#include "memory.h"

Memory::Memory(size_t szRam /*= EMU6502_RAM_SIZE*/) : szRamBuffer(szRam) {
    ram = new uint8_t[szRam];
}

Memory::~Memory() {
    delete ram;
}

void Memory::CopyTo(uint32_t dstIndex, const void *src, size_t nBytes) {
    assert(ram != nullptr);
    assert((dstIndex + nBytes) < szRamBuffer);
    memcpy(&ram[dstIndex], src, nBytes);
}


uint8_t Memory::ReadU8(uint32_t index) {
    assert(ram != nullptr);
    assert(index < szRamBuffer);
    return ram[index];
}

uint16_t Memory::ReadU16(uint32_t index) {
    assert(ram != nullptr);
    assert(index < szRamBuffer);
    auto p16 = reinterpret_cast<uint16_t *>(&ram[index]);
    return *p16;

}

uint32_t Memory::ReadU32(uint32_t index) {
    assert(ram != nullptr);
    assert(index < szRamBuffer);
    auto p32 = reinterpret_cast<uint32_t *>(&ram[index]);
    return *p32;
}

void Memory::WriteU8(uint32_t index, uint8_t value) {
    assert(ram != nullptr);
    assert(index < szRamBuffer);
    ram[index] = value;
}

void Memory::WriteU16(uint32_t index, uint16_t value) {
    assert(ram != nullptr);
    assert(index < szRamBuffer);
    auto p16 = reinterpret_cast<uint16_t *>(&ram[index]);
    *p16 = value;

}

void Memory::WriteU32(uint32_t index, uint32_t value) {
    assert(ram != nullptr);
    assert(index < szRamBuffer);
    auto p32 = reinterpret_cast<uint32_t *>(&ram[index]);
    *p32 = value;
}


