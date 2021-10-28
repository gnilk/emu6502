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

    // Not using this is compliant with VICE...
    // mstatus.set(CpuFlag::Unused);

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
                // According to the emulators this is not set on reset
                // but in the data-sheet it is said to be '1'
                // Note: In VICE the BRK flag is also set -
                auto current = mstatus;
                current.set(CpuFlag::Unused);
                current.set(CpuFlag::BreakCmd);

                Push8(current.raw());
                SetStepResult("PHP");
            }
    };

    instructions[kCpuOperands::PLP] = {
            kCpuOperands::PLP, 1, "PLP", [this](){
                //status = Pop8();
                auto tmp = static_cast<CpuFlags>(Pop8());
                tmp.set(CpuFlag::Unused, false);
                mstatus = tmp;
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
                RefreshStatusFromValue(reg_a);
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

    instructions[kCpuOperands::CLI] = {
            kCpuOperands::CLI, 1, "CLI", [this](){
                //UpdateStatus(kCpuFlags::kFlag_Overflow, false);
                mstatus.set(CpuFlag::InterruptDisable, false);
                SetStepResult("CLI");
            }
    };

    instructions[kCpuOperands::SEI] = {
            kCpuOperands::SEI, 1, "SEI", [this](){
                //UpdateStatus(kCpuFlags::kFlag_Overflow, false);
                mstatus.set(CpuFlag::InterruptDisable, true);
                SetStepResult("SEI");
            }
    };


    instructions[kCpuOperands::ORA_IMM] = {
            kCpuOperands::ORA_IMM, 2, "ORA", [this](){
                uint8_t val = Fetch8();
                reg_a |= val;
                RefreshStatusFromValue(reg_a);
                SetStepResult("ORA #$%02x", val);
            }
    };

    instructions[kCpuOperands::AND_IMM] = {
            kCpuOperands::AND_IMM, 2, "AND", [this](){
                uint8_t val = Fetch8();
                reg_a &= val;
                RefreshStatusFromValue(reg_a);
                SetStepResult("AND #$%02x", val);
            }
    };

    instructions[kCpuOperands::EOR_IMM] = {
            kCpuOperands::EOR_IMM, 2, "EOR", [this](){
                uint8_t val = Fetch8();
                reg_a ^= val;
                RefreshStatusFromValue(reg_a);
                SetStepResult("EOR #$%02x", val);
            }
    };


    instructions[kCpuOperands::ADC_IMM] = {
            kCpuOperands::ADC_IMM, 2, "SBC", [this](){
                uint8_t val = Fetch8();
                reg_a += val;
                reg_a += mstatus[CpuFlag::Carry]?1:0;
                if (reg_a > 255) {
                    mstatus.set(CpuFlag::Carry, true);
                    reg_a &= 0xff;
                } else {
                    mstatus.set(CpuFlag::Carry, false);
                }

                // TODO: V flag in Status

                RefreshStatusFromValue(reg_a);
                SetStepResult("ADC #$%02x (C:%d)",val, mstatus[CpuFlag::Carry]?1:0);
            }
    };


    instructions[kCpuOperands::SBC_IMM] = {
            kCpuOperands::SBC_IMM, 2, "SBC", [this](){
                uint8_t val = Fetch8();
                reg_a -= val;
                reg_a -= mstatus[CpuFlag::Carry]?0:1;

                if (reg_a > 0) {
                    mstatus.set(CpuFlag::Carry, true);
                } else {
                    mstatus.set(CpuFlag::Carry, false);
                    reg_a &= 0xff;
                }

                // TODO: V flag in Status

                RefreshStatusFromValue(reg_a);
                SetStepResult("SBC #$%02x (C:%d)",val, mstatus[CpuFlag::Carry]?1:0);
            }
    };

    instructions[kCpuOperands::TAY] = {
            kCpuOperands::TAY, 1, "TAY", [this](){
                reg_y = reg_a;
                RefreshStatusFromValue(reg_y);
                SetStepResult("TAY");
            }
    };

    instructions[kCpuOperands::TYA] = {
            kCpuOperands::TYA, 1, "TYA", [this](){
                reg_a = reg_y;
                RefreshStatusFromValue(reg_a);
                SetStepResult("TYA");
            }
    };

    instructions[kCpuOperands::TXA] = {
            kCpuOperands::TXA, 1, "TXA", [this](){
                reg_a = reg_x;
                RefreshStatusFromValue(reg_a);
                SetStepResult("TXA");
            }
    };

    instructions[kCpuOperands::TAX] = {
            kCpuOperands::TAX, 1, "TAX", [this](){
                reg_x = reg_a;
                RefreshStatusFromValue(reg_a);
                SetStepResult("TAX");
            }
    };

    instructions[kCpuOperands::DEY] = {
            kCpuOperands::DEY, 1, "DEY", [this](){
                auto old_y = reg_y;
                reg_y = (reg_y - 1) & 255;
                RefreshStatusFromValue(reg_y);
                SetStepResult("DEY (#$%02x -> #$%02x)", old_y, reg_y);
            }
    };

    instructions[kCpuOperands::DEX] = {
            kCpuOperands::DEY, 1, "DEX", [this](){
                auto old_x = reg_x;
                reg_x = (reg_x - 1) & 255;
                RefreshStatusFromValue(reg_x);
                SetStepResult("DEX (#$%02x -> #$%02x)", old_x, reg_x);
            }
    };

    instructions[kCpuOperands::INY] = {
            kCpuOperands::INY, 1, "INY", [this](){
                auto old_y = reg_y;
                reg_y = (reg_y + 1) & 255;
                RefreshStatusFromValue(reg_y);
                SetStepResult("INY (#$%02x -> #$%02x)", old_y, reg_y);
            }
    };

    instructions[kCpuOperands::INX] = {
            kCpuOperands::INX, 1, "INX", [this](){
                auto old_y = reg_x;
                reg_x = (reg_x + 1) & 255;
                RefreshStatusFromValue(reg_x);
                SetStepResult("INX (#$%02x -> #$%02x)", old_y, reg_x);
            }
    };

    instructions[kCpuOperands::LDA_IMM] = {
            kCpuOperands::LDA_IMM, 2, "LDA", [this](){
                reg_a = Fetch8();
                RefreshStatusFromValue(reg_a);
                SetStepResult("LDA #$%02x", reg_a);
            }
    };

    instructions[kCpuOperands::LDX_IMM] = {
            kCpuOperands::LDX_IMM, 2, "LDX", [this](){
                reg_x = Fetch8();
                RefreshStatusFromValue(reg_x);
                SetStepResult("LDX #$%02x", reg_x);
            }
    };

    instructions[kCpuOperands::LDY_IMM] = {
            kCpuOperands::LDY_IMM, 2, "LDY", [this](){
                reg_y = Fetch8();
                RefreshStatusFromValue(reg_y);
                SetStepResult("LDY #$%02x", reg_y);
            }
    };

    instructions[kCpuOperands::LDA_ABS] = {
            kCpuOperands::LDA_ABS, 3, "LDA", [this](){
                uint16_t ofs = Fetch16();
                uint8_t value = ReadU8(ofs);
                reg_a = value;
                RefreshStatusFromValue(reg_a);
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

    return TryDecode2();

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
        printf("ADDR AR XR YR SP 01 NV-BDIZC\n");
        printf("%04x %02x %02x %02x %02x %02x %s\n",
               ip, reg_a, reg_x, reg_y, sp & 0xff, ReadU8(0x01), ToBinaryU8(mstatus.raw()).c_str());

        // Make this line optional..
        printf("\n");
    }
    return res;
}

#define OPCODE_MASK_BASE     (0b00000011)
#define OPCODE_MASK_ADDRMODE (0b00011100)
#define OPCODE_MASK_EXT      (0b11100000)


struct ascOperandSzAddr {
    static const size_t Invalid      = 0;    // Invalid operand size
    static const size_t Immediate    = 2;    // Immediate mode
    static const size_t Absolute     = 3;    // Absolute
    static const size_t AbsoluteIndX = 3;    // Absolute
    static const size_t AbsoluteIndY = 3;    // Absolute
    static const size_t Zeropage     = 2;    // Zeropage
    static const size_t ZeropageX    = 2;    // Zeropage,x
    static const size_t ZeroPageIndX = 2;    // (Zeropage),x
    static const size_t ZeroPageIndY = 2;    // (Zeropage),y
    static const size_t Accumulator  = 1;    // Directly affecting accumulator
};

enum class OperandSzAddr : uint8_t {
    Invalid      = 0,    // Invalid operand size
    Immediate    = 2,    // Immediate mode
    Absolute     = 3,    // Absolute
    AbsoluteIndX = 3,    // Absolute
    AbsoluteIndY = 3,    // Absolute
    Zeropage     = 2,    // Zeropage
    ZeropageX    = 2,    // Zeropage,x
    ZeroPageIndX = 2,    // (Zeropage),x
    ZeroPageIndY = 2,    // (Zeropage),y
    Accumulator  = 1,    // Directly affecting accumulator
};


struct OperandGroup {
    [[nodiscard]] const std::string &Name(uint8_t idx) const { return names[idx]; }
    [[nodiscard]] OperandSzAddr Size(uint8_t idx) const { return sizes[idx]; }

    std::string names[8];
    OperandSzAddr sizes[8];
};

static OperandGroup opGroup01={
    .names = {"ORA","AND", "EOR", "ADC", "STA", "LDA", "CMP", "SBC"},
    .sizes = {
            OperandSzAddr::ZeroPageIndX,
            OperandSzAddr::Zeropage,
            OperandSzAddr::Immediate,
            OperandSzAddr::Absolute,
            OperandSzAddr::ZeroPageIndY,
            OperandSzAddr::ZeropageX,
            OperandSzAddr::AbsoluteIndY,
            OperandSzAddr::AbsoluteIndY,
    }
};
static OperandGroup opGroup10={
        .names = {"ASL", "ROL", "LSR", "ROR", "STX", "LDX", "DEC", "INC",},
        .sizes = {
                OperandSzAddr::Immediate,       // 000
                OperandSzAddr::Zeropage,        // 001
                OperandSzAddr::Accumulator,     // 010
                OperandSzAddr::AbsoluteIndY,    // 011
                OperandSzAddr::Invalid,         // 100      INVALID
                OperandSzAddr::ZeropageX,       // 101
                OperandSzAddr::Invalid,         // 110      INVALID
                OperandSzAddr::AbsoluteIndX,    // 111
        }
};

static OperandGroup opGroup00={
        .names = {"---", "BIT", "JMP", "JMP", "STY", "LDY", "CPY", "CPX", },
        .sizes = {
                OperandSzAddr::Immediate,       // 000
                OperandSzAddr::Zeropage,        // 001
                OperandSzAddr::Invalid,         // 010      INVALID
                OperandSzAddr::AbsoluteIndY,    // 011
                OperandSzAddr::Invalid,         // 100      INVALID
                OperandSzAddr::ZeropageX,       // 101
                OperandSzAddr::Invalid,         // 110      INVALID
                OperandSzAddr::AbsoluteIndX,    // 111
        }
};



bool CPU::TryDecode2() {
    bool res = true;

    uint8_t incoming = Fetch8();
    if (incoming == 0x00) return false;

    // Handle instructions which are a bit odd...
    if ((incoming & 0x0f) == 0x08) {
        if ((incoming & 0x10) == 0x10) {
            // clear flags
            static std::string names[]={"CLC","SEC","CLI","SEI","TYA","CLV","CLD","SED"};

            auto idxInstr = incoming >> 5;
            printf("%s, idx: %d (%02x)\n", names[idxInstr].c_str(), idxInstr, idxInstr);


            return true;
        } else if ((incoming & 0xf0) < 0x70) {
            static std::string names[]={"PHP", "PLP", "PHA", "PLA"};

            auto idxInstr = (incoming >> 5);

            // Push
            printf("%s (%d, %02x)\n", names[idxInstr].c_str(), idxInstr, idxInstr);
            return true;
        } else if ((incoming & 0xf0) >= 0x80) {
            static std::string names[] = {"TAY", "INY", "INX"};

            auto idxInstr = ((incoming >> 5) & 0x03) - 1;

            printf("%s incoming: %02x - (%d, %02x) - %s\n", names[idxInstr].c_str(), incoming, idxInstr, idxInstr,
                   ToBinaryU8(idxInstr).c_str());
            return true;
        }
        printf("Unsupported op code: %02x\n", incoming);
    }

    // Handle special stuff first
    if ((incoming &0x10) == 0x10) {
        printf("BRANCH\n");
        Fetch8();
        return true;
    }
    // Now handle the following...
    // JSR RTI	RTS
    // 20  40	60
    if (incoming == 0x20) {
        printf("JSR");
        Fetch8();
        Fetch8();
        return true;
    }



    uint8_t op_base = incoming & OPCODE_MASK_BASE;
    uint8_t addrmode = incoming & OPCODE_MASK_ADDRMODE;
    uint8_t addrmode_idx = addrmode >> 2;
    uint8_t op_ext = incoming & OPCODE_MASK_EXT;
    uint8_t op_ext_idx = op_ext >> 5;


    static OperandGroup *opGroups[4]={
            nullptr,
            &opGroup01,
            &opGroup10,
            nullptr
    };

    OperandGroup *opGroup = opGroups[op_base];
    if (opGroup == nullptr) {
        printf("Emulation of opcode %02x not implemented\n", incoming);
        exit(1);
    }
    const std::string &name = opGroup->Name(op_ext_idx);
    auto szOperand = to_underlying(opGroup->Size(addrmode_idx));
    printf("%s, sz: %d\n", name.c_str(), szOperand);
    if (szOperand > 1) {
        // Fetch remaining...
        for (int i=0;i<szOperand-1;i++) {
            Fetch8();
        }
    }
    return true;


//    //
//    // Operand Group 1
//    //
//    static std::string base_01_ext_class_operands[]={
//            "ORA","AND", "EOR", "ADC", "STA", "LDA", "CMP", "SBC"
//    };
//    // Size besides the op-code
//    /*
//    bbb	addressing mode
//    000	(zero page,X)
//    001	zero page
//    010	#immediate
//    011	absolute
//    100	(zero page),Y
//    101	zero page,X
//    110	absolute,Y
//    111	absolute,X
//     */
//    static uint8_t base_01_ext_sz_operand[8] {
//        OperandSzAddr::ZeroPageIndX,
//        OperandSzAddr::Zeropage,
//        OperandSzAddr::Immediate,
//        OperandSzAddr::Absolute,
//        OperandSzAddr::ZeroPageIndY,
//        OperandSzAddr::ZeropageX,
//        OperandSzAddr::AbsoluteIndY,
//        OperandSzAddr::AbsoluteIndY,
//    };
//
//    static std::string base_10_ext_class_operands[]={
//            "ASL", "ROL", "LSR", "ROR", "STX", "LDX", "DEC", "INC",
//    };
//
//    /*
//        bbb	addressing mode
//        000	#immediate
//        001	zero page
//        010	accumulator
//        011	absolute
//        101	zero page,X
//        111	absolute,X
//     */
//    static uint8_t base_10_ext_sz_operand[8] {
//        OperandSzAddr::Immediate,
//        OperandSzAddr::Zeropage,
//        OperandSzAddr::Accumulator,
//        OperandSzAddr::AbsoluteIndY,
//        0,
//        OperandSzAddr::ZeropageX,
//        0,
//        OperandSzAddr::AbsoluteIndX,
//    };
//
//
//    static std::string base_00_ext_class_operands[]={
//            "---", "BIT", "JMP", "JMP", "STY", "LDY", "CPY", "CPX",
//    };
//
//
//    if (op_base == 0x01) {
//        auto szOperand = base_01_ext_sz_operand[addrmode_idx];
//        printf("opcode: %02x (aaa: %s, bbb: %s, cc: %s) -> addrmode:%s (idx: %d)\n",
//               incoming,
//               ToBinaryU8(op_ext).c_str(),
//               ToBinaryU8(addrmode).c_str(),
//               ToBinaryU8(op_base).c_str(),
//               ToBinaryU8(addrmode_idx).c_str(),
//               addrmode_idx);
//        printf("%s, sz: %d\n", base_01_ext_class_operands[op_ext_idx].c_str(), szOperand);
//        if (szOperand > 1) {
//            // Fetch remaining...
//            for (int i=0;i<szOperand-1;i++) {
//                Fetch8();
//            }
//        }
//    }


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

// This will refresh the Zero/Neg flags in the status register...
void CPU::RefreshStatusFromValue(uint8_t reg) {
    if (!reg) {
        //UpdateStatus(kFlag_Zero, true);
        mstatus.set(CpuFlag::Zero, true);
    } else {
        mstatus.set(CpuFlag::Zero, false);
    }

    if (reg & 0x80) {
        //UpdateStatus(kFlag_Negative, true);
        mstatus.set(CpuFlag::Negative, true);
    } else {
        // Clear???
        //UpdateStatus(kFlag_Negative, false);
        mstatus.set(CpuFlag::Negative, false);
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

