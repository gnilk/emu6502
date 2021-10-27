//
// Created by Fredrik Kling on 27/10/2021.
//

#include "cpu.h"
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <map>
#include <functional>

#include <type_traits>

//
// C++ version to properly cast an enum class : T to the underlying type T
//
template <typename E>
constexpr auto to_underlying(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}

//
// Operator for logical AND on debug flags...
//
// Note, trying to get a feel for CPP scoped enumerators
//
kDebugFlags operator & (kDebugFlags lhs, kDebugFlags rhs) {
    return static_cast<kDebugFlags>(static_cast<std::underlying_type<kDebugFlags>::type>(lhs) & static_cast<std::underlying_type<kDebugFlags>::type>(rhs));
}

kDebugFlags operator &= (kDebugFlags &lhs, kDebugFlags rhs) {
    lhs = static_cast<kDebugFlags>(static_cast<std::underlying_type<kDebugFlags>::type>(lhs) & static_cast<std::underlying_type<kDebugFlags>::type>(rhs));
    return lhs;
}


kDebugFlags operator ^ (kDebugFlags lhs, kDebugFlags rhs) {
    return static_cast<kDebugFlags>(static_cast<std::underlying_type<kDebugFlags>::type>(lhs) ^ static_cast<std::underlying_type<kDebugFlags>::type>(rhs));
}

// Note: There is a reason for the signed int!!!
kDebugFlags operator ^ (kDebugFlags lhs, int rhs) {
    return static_cast<kDebugFlags>(static_cast<std::underlying_type<kDebugFlags>::type>(lhs) ^ rhs);
}


kDebugFlags operator | (kDebugFlags lhs, kDebugFlags rhs) {
    return static_cast<kDebugFlags>(static_cast<std::underlying_type<kDebugFlags>::type>(lhs) | static_cast<std::underlying_type<kDebugFlags>::type>(rhs));
}
// we should av aliases for this...
kDebugFlags operator |= (kDebugFlags &lhs, kDebugFlags rhs) {
    lhs = static_cast<kDebugFlags>(static_cast<std::underlying_type<kDebugFlags>::type>(lhs) | static_cast<std::underlying_type<kDebugFlags>::type>(rhs));
    return lhs;

}
//
// ================


CPU::CPU() : ram(nullptr), mstatus(0) {

}
void CPU::Initialize() {
    if (ram != nullptr) {
        free(ram);
    }
    ram = (uint8_t *)malloc(MAX_RAM);
    memset(ram, 0, MAX_RAM);
    ip = 0;
    sp = 0x1ff; // stack pointer, points to first available byte..
    //status = 0x00;
    mstatus.reset();

    debugFlags = kDebugFlags::None;

    //
    // TODO: Consider putting these somewhere else as this will blow up quite heavily...
    //
    instructions[kCpuOperands::CLC] = {
            kCpuOperands::CLC, 1, "CLC", [this](){
                mstatus.set(CpuFlag::Carry, false);
//                UpdateStatus(kCpuFlags::kFlag_Carry, false);
                SetStepResult("CLC");
            }
    };

    instructions[kCpuOperands::SEC] = {
            kCpuOperands::SEC, 1, "SEC", [this](){
                //UpdateStatus(kCpuFlags::kFlag_Carry, true);
                mstatus.set(CpuFlag::Carry);
                SetStepResult("SEC");
            }
    };

    instructions[kCpuOperands::PHP] = {
            kCpuOperands::PHP, 1, "PHP", [this](){
                //Push8(status);
                Push8(mstatus.raw());
                SetStepResult("PHP");
            }
    };

    instructions[kCpuOperands::PLP] = {
            kCpuOperands::PLP, 1, "PLP", [this](){
                //status = Pop8();
                mstatus = Pop8();
                SetStepResult("PLP");
            }
    };

    instructions[kCpuOperands::PHA] = {
            kCpuOperands::PHA, 1, "PHA", [this](){
                Push8(reg_a);
                SetStepResult("PHA");
            }
    };

    instructions[kCpuOperands::PLA] = {
            kCpuOperands::PLA, 1, "PLA", [this](){
                reg_a = Pop8();
                RefreshStatusFromALU(true);
                SetStepResult("PLA");
            }
    };


    instructions[kCpuOperands::CLD] = {
            kCpuOperands::CLD, 1, "CLD", [this](){
                //UpdateStatus(kCpuFlags::kFlag_DecimalMode, false);
                mstatus.set(CpuFlag::DecimalMode, false);
                SetStepResult("CLD");
            }
    };

    instructions[kCpuOperands::SED] = {
            kCpuOperands::SED, 1, "SED", [this](){
                //UpdateStatus(kCpuFlags::kFlag_DecimalMode, true);
                mstatus.set(CpuFlag::DecimalMode, true);
                SetStepResult("SED");
            }
    };

    instructions[kCpuOperands::CLV] = {
            kCpuOperands::CLV, 1, "CLV", [this](){
                //UpdateStatus(kCpuFlags::kFlag_Overflow, false);
                mstatus.set(CpuFlag::Overflow, false);
                SetStepResult("CLV");
            }
    };


    instructions[kCpuOperands::EOR_IMM] = {
            kCpuOperands::EOR_IMM, 2, "EOR", [this](){
                uint8_t val = Fetch8();
                reg_a ^= val;
                RefreshStatusFromALU(true);
                SetStepResult("EOR #$%02x", val);
            }
    };


    instructions[kCpuOperands::ADC_IMM] = {
            kCpuOperands::ADC_IMM, 2, "CLC", [this](){
                uint8_t val = Fetch8();
                reg_a += val;
                //reg_a += IsStatusSet(kCpuFlags::kFlag_Carry)?1:0;
                reg_a += mstatus[CpuFlag::Carry]?1:0;
                if (reg_a > 255) {
                    //UpdateStatus(kCpuFlags::kFlag_Carry, true);
                    mstatus.set(CpuFlag::Carry, true);
                    // TODO: Need to understand the overflow flag a bit better...
                    // UpdateStatus(kCpuFlags::kFlag_Overflow, true);
                    reg_a &= 0xff;
                }

                // TODO: Verify if carry should be cleared in case we don't wrap..

                RefreshStatusFromALU();

                SetStepResult("ADC #$%02x (C:%d)",val, mstatus[CpuFlag::Carry]?1:0);
            }
    };


    // Setup instruction set...
    instructions[kCpuOperands::LDA_IMM] = {
            kCpuOperands::LDA_IMM, 2, "LDA", [this](){
                reg_a = Fetch8();
                RefreshStatusFromALU(true);
                SetStepResult("LDA #$%02x", reg_a);
            }
    };

    instructions[kCpuOperands::LDA_ABS] = {
            kCpuOperands::LDA_ABS, 3, "LDA", [this](){
                uint16_t ofs = Fetch16();
                uint8_t value = ReadU8(ofs);
                reg_a = value;
                RefreshStatusFromALU();
                SetStepResult("LDA $%04x  ($%04x => $02x)", ofs, ofs, value);
            }
    };

    instructions[kCpuOperands::STA] = {
            kCpuOperands::STA, 3, "STA", [this](){
                uint16_t ofs = Fetch16();
                WriteU8(ofs, reg_a);
                SetStepResult("STA $%04x (#$%02x => $%04x)", ofs, reg_a, ofs);
            }
    };

    instructions[kCpuOperands::JSR] = {
            kCpuOperands::JSR, 3, "JSR", [this](){
                uint16_t ofs = Fetch16();
                uint16_t ipReturn = ip;
                SetStepResult("JSR $%04x", ofs);
                ip = ofs;
                Push16(ipReturn);
            }
    };
    instructions[kCpuOperands::RTS] = {
            kCpuOperands::RTS,1,"RTS", [this](){
                uint16_t ofs = Pop16();
                SetStepResult("RTS  (ip will be: $%04x)", ofs);
                ip = ofs;
            }
    };
}

void CPU::Reset(uint32_t ipAddr) {
    ip = ipAddr;
    sp = 0x1ff; // stack pointer, points to first available byte..
    reg_a = 0xaa;
    reg_x = 0x00;
    reg_y = 0x00;
    //status = 0x00;      // should be 0x16 according to: https://www.c64-wiki.com/wiki/Processor_Status_Register
    mstatus.reset();
}

void CPU::Load(const uint8_t *from, uint32_t offset, uint32_t nbytes) {
    printf("Loading %d bytes to offset 0x%04x (%d)\n", nbytes, offset, offset);
    memcpy(&ram[offset], from, nbytes);
}

bool CPU::Step() {
    bool res = true;

    // TODO: Static cast here is probably wrong...
    kCpuOperands opcode = static_cast<kCpuOperands>(Fetch8());
    if (instructions.find(opcode) != instructions.end()) {
        instructions[opcode].exec();
    } else {
        switch (opcode) {
            case kCpuOperands::BRK :
                res = false;
                SetStepResult("BRK");
                break;
            case kCpuOperands::NOP :
                // Do nothing...
                SetStepResult("NOP");
                break;
            default:
                SetStepResult("ERR: Unknown OPCode $%02x at address $%04x", opcode, ip-1);
                res = false;
        }
    }

//    kDebugFlags testFlags = kDebugFlags::kDbg_StepDisAsm | kDebugFlags::kDbg_StepCPUReg;
//    if (testFlags & kDebugFlags::kDbg_StepCPUReg) {
//        printf(":WEfwefwefwef'\n");
//    }

    if((debugFlags & kDebugFlags::StepDisAsm) == kDebugFlags::StepDisAsm) {
        printf("%s\n", lastStepResult.c_str());
    }
    if ((debugFlags & kDebugFlags::StepCPUReg) == kDebugFlags::StepCPUReg) {
        printf("IP=$%04x SP=$%02x A=$%02x X=$%02x Y=$%02x P=%s (NV-BDIZC)\n",
               ip, sp, reg_a, reg_x, reg_y, ToBinaryU8(mstatus.raw()).c_str());
    }
    return res;
}

const std::string CPU::ToBinaryU8(uint8_t byte) {

    std::string str;
    // Want int here, so we decrement below zero and thus break the loop...
    for(int i=7;i>=0;i--) {
        if (byte & (1<<i)) {
            str += '1';
        } else {
            str += '0';
        }
    }
    //00000010
    //12345678
    return str;
}

void CPU::SetDebug(kDebugFlags flag, bool enable) {
    if (enable) {
        debugFlags |= flag;
    } else {
        debugFlags &= flag ^ 0xff;
    }
}


void CPU::SetStepResult(const char *format, ...) {
    va_list values;
    va_start(values, format);
    char outstr[256];
    vsnprintf(outstr, 256, format, values);
    va_end(values);

    lastStepResult = std::string(outstr);

}

void CPU::RefreshStatusFromALU(bool updateNeg) {
    if (!reg_a) {
        //UpdateStatus(kFlag_Zero, true);
        mstatus.set(CpuFlag::Zero, true);
    } else {
        //UpdateStatus(kFlag_Zero, false);
        mstatus.set(CpuFlag::Zero, false);
    }

    // TODO: need argument if update neg
    if (updateNeg) {
        if (reg_a & 0x80) {
            //UpdateStatus(kFlag_Negative, true);
            mstatus.set(CpuFlag::Negative, true);
        } else {
            // Clear???
            //UpdateStatus(kFlag_Negative, false);
            mstatus.set(CpuFlag::Negative, false);
        }
    }
}


uint8_t CPU::Fetch8() {
    auto res = ReadU8(ip);
    ip+=1;
    return res;
}

uint16_t CPU::Fetch16() {
    auto res = ReadU16(ip);
    ip+=sizeof(uint16_t);
    return res;
}

uint32_t CPU::Fetch32() {
    auto res = ReadU32(ip);
    ip+=sizeof(uint32_t);
    return res;
}
// Stack helpers
void CPU::Push8(uint8_t value) {
    WriteU8(sp, value);
    sp -= 1;    // Advance stack to next available
}

void CPU::Push16(uint16_t value) {
    sp -= 1;    // Make room for one more value - we are pushing 16 bits and the stack points to first available
    WriteU16(sp, value);
    sp -= 1;    // Advance stack to next available
}
uint8_t CPU::Pop8() {
    uint8_t value = ReadU8(sp+1);
    sp += sizeof(uint8_t);
    return value;
}

uint16_t CPU::Pop16() {
    uint16_t value = ReadU16(sp+1);
    sp += sizeof(uint16_t);
    return value;
}


uint8_t CPU::ReadU8(uint32_t index) {
    if((debugFlags & kDebugFlags::MemoryRead) == kDebugFlags::MemoryRead) {
        printf("[CPU] Read8 0x%02x from ofs: 0x%04x (%d)\n", ram[index], index, index);
    }
    return ram[index];
}
uint16_t CPU::ReadU16(uint32_t index) {
    auto ptr = reinterpret_cast<uint16_t *>(&ram[index]);
    if((debugFlags & kDebugFlags::MemoryRead) == kDebugFlags::MemoryRead) {
        printf("[CPU] Read16  0x%04x from ofs: 0x%04x (%d)\n", *ptr, index, index);
    }
    return *ptr;
}
uint32_t CPU::ReadU32(uint32_t index) {
    auto ptr = reinterpret_cast<uint32_t *>(&ram[index]);
    if((debugFlags & kDebugFlags::MemoryRead) == kDebugFlags::MemoryRead) {
        printf("[CPU] Read32 0x%08x from ofs: 0x%04x (%d)\n", *ptr, index, index);
    }
    return *ptr;
}

void CPU::WriteU8(uint32_t index, uint8_t value) {
    if((debugFlags & kDebugFlags::MemoryWrite) == kDebugFlags::MemoryWrite) {
        printf("[CPU] WriteU8 0x%02x to ofs: 0x%04x (%d)\n", value, index, index);
    }
    ram[index] = value;
}


void CPU::WriteU16(uint32_t index, uint16_t value) {
    if((debugFlags & kDebugFlags::MemoryWrite) == kDebugFlags::MemoryWrite) {
        printf("[CPU] WriteU16 0x%04x to ofs: 0x%04x (%d)\n", value, index, index);
    }
    auto *ptr = reinterpret_cast<uint16_t *>(&ram[index]);
    *ptr = value;
}

void CPU::WriteU32(uint32_t index, uint32_t value) {
    if((debugFlags & kDebugFlags::MemoryWrite) == kDebugFlags::MemoryWrite) {
        printf("[CPU] WriteU32 0x%08x to ofs: 0x%04x (%d)\n", value, index, index);
    }
    auto *ptr = reinterpret_cast<uint32_t *>(&ram[index]);
    *ptr = value;
}

