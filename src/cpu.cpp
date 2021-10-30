//
// Simple 6502 CPU Emulator
//
// TODO:
//   - Overflow ('V') flags is not implemented
//   - Consider using a 'Tick' based system instead which would add support for peripherals (VIC, SID) and enable them to be cycle exact..
//
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
// Logical operators for debug flags enables us to use them as bit-operands...
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
    InitializeOpGroup01();
    InitializeOpGroup10();
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

    return TryDecode();

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



//
// Return full size (incl. opcode byte) of an operand based on the addressing mode
//
static size_t OpAddrModeToSize(OperandAddrMode addressingMode) {
//    Invalid      = 0,    // Invalid operand size
//    Immediate    = 2,    // Immediate mode
//    Absolute     = 3,    // Absolute
//    AbsoluteIndX = 3,    // Absolute
//    AbsoluteIndY = 3,    // Absolute
//    Zeropage     = 2,    // Zeropage
//    ZeropageX    = 2,    // Zeropage,x
//    ZeroPageIndX = 2,    // (Zeropage),x
//    ZeroPageIndY = 2,    // (Zeropage),y
//    Accumulator  = 1,    // Directly affecting accumulator


    // Note: There are instructions that don't take any argument (ASL, LSR, ROL, ROR, etc...)
    //       They have instruction size '1'
    static std::map<OperandAddrMode, size_t> modeToSize = {
            { OperandAddrMode::Invalid, 0 },
            { OperandAddrMode::Immediate, 2 },
            { OperandAddrMode::Absolute, 3 },
            { OperandAddrMode::AbsoluteIndX, 3 },
            { OperandAddrMode::AbsoluteIndY, 3 },
            { OperandAddrMode::Zeropage, 2 },
            { OperandAddrMode::ZeropageX, 2 },
            { OperandAddrMode::ZeroPageIndX, 2 },
            { OperandAddrMode::ZeroPageIndY, 2 },
            { OperandAddrMode::Accumulator, 1 },
    };

    return modeToSize[addressingMode];
}


struct OperandGroup {
    [[nodiscard]] const std::string &Name(uint8_t idx) const { return names[idx]; }
    [[nodiscard]] OperandAddrMode AddrMode(uint8_t idx) const { return addrModes[idx]; }

    std::string names[8];
    OperandAddrMode addrModes[8];
    std::function<void(OperandAddrMode addrMode)> handlers[8];
};



static OperandGroup opGroup01={
        .names = {"ORA","AND", "EOR", "ADC", "STA", "LDA", "CMP", "SBC"},
        .addrModes = {
                OperandAddrMode::ZeroPageIndX,
                OperandAddrMode::Zeropage,
                OperandAddrMode::Immediate,
                OperandAddrMode::Absolute,
                OperandAddrMode::ZeroPageIndY,
                OperandAddrMode::ZeropageX,
                OperandAddrMode::AbsoluteIndY,
                OperandAddrMode::AbsoluteIndX,
        },
        .handlers = {},
};
static OperandGroup opGroup10={
        .names = {"ASL", "ROL", "LSR", "ROR", "STX", "LDX", "DEC", "INC",},
        .addrModes = {
                OperandAddrMode::Immediate,       // 000
                OperandAddrMode::Zeropage,        // 001
                OperandAddrMode::Accumulator,     // 010
                OperandAddrMode::Absolute,        // 011
                OperandAddrMode::Invalid,         // 100
                OperandAddrMode::ZeropageX,       // 101
                OperandAddrMode::Invalid,         // 110      INVALID
                OperandAddrMode::AbsoluteIndX,    // 111
        }
};

static OperandGroup opGroup00={
        .names = {"---", "BIT", "JMP", "JMP", "STY", "LDY", "CPY", "CPX", },
        .addrModes = {
                OperandAddrMode::Immediate,       // 000
                OperandAddrMode::Zeropage,        // 001
                OperandAddrMode::Invalid,         // 010      INVALID
                OperandAddrMode::AbsoluteIndY,    // 011
                OperandAddrMode::Invalid,         // 100      INVALID
                OperandAddrMode::ZeropageX,       // 101
                OperandAddrMode::Invalid,         // 110      INVALID
                OperandAddrMode::AbsoluteIndX,    // 111
        }
};

struct OperandSpecial {
    uint8_t size;
    std::string name;
    // TODO: Add lambda here???
};

static std::map<uint8_t, OperandSpecial> opSpecial={
        { 0x20, { .size = 3, .name = "JSR" }},
        { 0x40, { .size = 1, .name = "RTI" }},
        { 0x60, { .size = 1, .name = "RTS" }}
};

bool CPU::TryDecode() {

    if (!TryDecodeInternal()) {
        return false;
    }

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
    return true;
}

//
// More generic 6502 disassembler code
//
bool CPU::TryDecodeInternal() {
    bool res = true;

    uint8_t incoming = Fetch8();
    printf("%02x\n",incoming);
    if (incoming == 0x00) return false;

    // Handle instructions which are a bit odd...
    if (TryDecodeOddities(incoming)) {
        return true;
    }
    if (TryDecodeBranches(incoming)) {
        return true;
    }
    if (TryDecodeOpGroup(incoming)) {
        return true;
    }
    if (TryDecodeLeftovers(incoming)) {
        return true;
    }


    printf("ERROR: Invalid or unhandled op-code: %02x\n", incoming);
    return false;
}


// This decodes all (hopefully) instructions
bool CPU::TryDecodeOddities(uint8_t incoming) {
    if ((incoming & 0x0f) != 0x08) {
        return false;
    }

    if ((incoming & 0x10) == 0x10) {
        // clear flags
        static std::string names[]={"CLC","SEC","CLI","SEI","TYA","CLV","CLD","SED"};

        auto idxInstr = incoming >> 5;
        printf("%s, idx: %d (%02x)\n", names[idxInstr].c_str(), idxInstr, idxInstr);
        return true;

    } else if ((incoming & 0xf0) < 0x70) {
        // Push/Pop instructions
        static std::string names[]={"PHP", "PLP", "PHA", "PLA"};

        auto idxInstr = (incoming >> 5);
        // Push
        printf("%s (%d, %02x)\n", names[idxInstr].c_str(), idxInstr, idxInstr);
        return true;

    } else if ((incoming & 0xf0) >= 0x80) {
        // Special other stuff...
        static std::string names[] = {"TAY", "INY", "INX"};

        auto idxInstr = ((incoming >> 5) & 0x03) - 1;

        printf("%s incoming: %02x - (%d, %02x) - %s\n", names[idxInstr].c_str(), incoming, idxInstr, idxInstr,
               ToBinaryU8(idxInstr).c_str());
        return true;
    }
    return false;
}
// 189, 0xbd, %010111101
// 16, 0x10,  %000010000
bool CPU::TryDecodeBranches(uint8_t incoming) {
    //
    // Handle conditional branches...
    // The conditional branch instructions all have the form xxy10000.
    // The flag indicated by xx is compared with y, and the branch is taken if they are equal.
    // 87654321
    // xxy10000
    //
    if ((incoming &0x1F) != 0x10) {
        return false;
    }
    static std::string names[]={
            "BPL","BMI","BVC","BVC","BCC","BCS","BNE","BEQ"
    };
    //  xxy10000
    // %00110000 - 0x30
    // %00100000 - 0x20
    bool testFlag = (incoming & 0x20)?true:false;
    uint8_t idxBrandOp = ((incoming >> 6)<<1) | (testFlag?1:0);
    printf("%s  (Branch, flag: %s, incoming: %02x, idxBrandOp: %02x)\n",
           names[idxBrandOp].c_str(), testFlag?"0":"1",incoming, idxBrandOp);
    uint8_t relativeAddr = Fetch8();

    return true;
}

void CPU::InitializeOpGroup01() {
    //
    // Setup opGroup handlers, these are called during decoding from 'TryDecodeOpGroup' when processing op-codes in
    // group 01. The handler essentially calls OperandResolveAddressAndExecute which resolves the addressing depending
    // on the addressing mode then it calls the 'action' which performs the actual load/store or any other operand...
    //

    // OpGroup operands:
    //  0: "ORA"
    //  1: "AND"
    //  2: "EOR"
    //  3: "ADC"
    //  4: "STA
    //  5: "LDA"
    //  6: "CMP"
    //  7: "SBC"

    // Consider passing around and operand structure instead...
    opGroup01.handlers[0] = [this](OperandAddrMode addrMode){
        OperandResolveAddressAndExecute("ORA", addrMode, [&](uint16_t index, uint8_t v) {
            if (addrMode == OperandAddrMode::Immediate) {
                reg_a |= v;
            } else {
                // Any non-immediate mode operand will load from memory..
                reg_a |= ReadU8(index);
            }
            RefreshStatusFromValue(reg_a);
        });
    };

    opGroup01.handlers[1] = [this](OperandAddrMode addrMode){
        OperandResolveAddressAndExecute("AND", addrMode, [&](uint16_t index, uint8_t v) {
            if (addrMode == OperandAddrMode::Immediate) {
                reg_a &= v;
            } else {
                // Any non-immediate mode operand will load from memory..
                reg_a &= ReadU8(index);
            }
            RefreshStatusFromValue(reg_a);
        });
    };

    opGroup01.handlers[2] = [this](OperandAddrMode addrMode){
        OperandResolveAddressAndExecute("EOR", addrMode, [&](uint16_t index, uint8_t v) {
            if (addrMode == OperandAddrMode::Immediate) {
                reg_a ^= v;
            } else {
                // Any non-immediate mode operand will load from memory..
                reg_a ^= ReadU8(index);
            }
            RefreshStatusFromValue(reg_a);
        });
    };

    opGroup01.handlers[3] = [this](OperandAddrMode addrMode){
        OperandResolveAddressAndExecute("ADC", addrMode, [&](uint16_t index, uint8_t v) {
            if (addrMode == OperandAddrMode::Immediate) {
                reg_a += v;
            } else {
                // Any non-immediate mode operand will load from memory..
                reg_a += ReadU8(index);
            }

            reg_a += mstatus[CpuFlag::Carry] ? 1 : 0;
            if (reg_a > 255) {
                mstatus.set(CpuFlag::Carry, true);
                reg_a &= 0xff;
            } else {
                mstatus.set(CpuFlag::Carry, false);
            }
            RefreshStatusFromValue(reg_a);
        });
    };

    opGroup01.handlers[4] = [this](OperandAddrMode addrMode){
        OperandResolveAddressAndExecute("STA", addrMode, [&](uint16_t index, uint8_t v) {
            // No immediate mode - just write whatever...
            WriteU8(index, reg_a);
        });
    };
    opGroup01.handlers[5] = [this](OperandAddrMode addrMode){
        OperandResolveAddressAndExecute("LDA", addrMode, [&](uint16_t index, uint8_t v) {
            if (addrMode == OperandAddrMode::Immediate) {
                reg_a = v;
            } else {
                // Any non-immediate mode operand will load from memory..
                reg_a = ReadU8(index);
            }
            RefreshStatusFromValue(reg_a);
        });
    };

    // TODO: opGroup01 [6] = CMP

    opGroup01.handlers[7] = [this](OperandAddrMode addrMode){
        OperandResolveAddressAndExecute("SBC", addrMode, [&](uint16_t index, uint8_t v) {
            if (addrMode == OperandAddrMode::Immediate) {
                reg_a -= v;
            } else {
                // Any non-immediate mode operand will load from memory..
                reg_a -= ReadU8(index);
            }

            reg_a -= mstatus[CpuFlag::Carry] ? 1 : 0;
            if (reg_a > 0) {
                mstatus.set(CpuFlag::Carry, true);
            } else {
                mstatus.set(CpuFlag::Carry, false);
                reg_a &= 0xff;
            }
            RefreshStatusFromValue(reg_a);
        });
    };
}

// Initialize operand group for {"ASL", "ROL", "LSR", "ROR", "STX", "LDX", "DEC", "INC",},
void CPU::InitializeOpGroup10() {

    opGroup10.handlers[0] = [this](OperandAddrMode addrMode){
        OperandResolveAddressAndExecute("ASL", addrMode, [&](uint16_t index, uint8_t v) {
            if (addrMode == OperandAddrMode::Accumulator) {
                reg_a = reg_a << 1;
                if (reg_a > 255) {
                    mstatus.set(CpuFlag::Carry, true);
                } else {
                    mstatus.set(CpuFlag::Carry, false);
                }
                reg_a = reg_a & 255;
                RefreshStatusFromValue(reg_a);
            } else {
                // Any non-immediate mode operand will load from/write to memory..
                uint16_t val = ReadU8(index);
                val = val << 1;
                if (val > 255) {
                    mstatus.set(CpuFlag::Carry, true);
                } else {
                    mstatus.set(CpuFlag::Carry, false);
                }
                v = val & 255;

                WriteU8(index, v);
                RefreshStatusFromValue(v);
            }
        });
    };

}


bool CPU::TryDecodeOpGroup(uint8_t incoming) {
    // Split up the op-code in to the logical options
    // see: https://llx.com/Neil/a2/opcodes.html
    //
    // If the bitpattern is 'aaabbbcc' (MSB left, LSB right)
    // The aaa and cc bits determine the opcode, and the bbb bits determine the addressing mode.
    //
    // Two groups can be solved in a bulk like fashion as they all use more or less same addressing scheme...
    //
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
        return false;
    }

    const std::string &name = opGroup->Name(op_ext_idx);
    auto nBytes = OpAddrModeToSize(opGroup->AddrMode(addrmode_idx));
    auto szOperand = OpAddrModeToSize(opGroup->AddrMode(addrmode_idx));
    printf("%s, sz: %d (op_base: %d)\n", name.c_str(), szOperand,op_base);

    if (opGroup->handlers[op_ext_idx] != nullptr) {
        opGroup->handlers[op_ext_idx](opGroup->AddrMode(addrmode_idx));
    } else {
        if (szOperand > 1) {
            // Fetch remaining...
            for (int i=0;i<szOperand-1;i++) {
                Fetch8();
            }
        }
    }
    return true;
}
bool CPU::TryDecodeLeftovers(uint8_t incoming) {

    // Handle left overs
    if (opSpecial.find(incoming) == opSpecial.end()) {
        return false;
    }

    auto op = opSpecial[incoming];
    printf("%s\n", op.name.c_str());
    if (op.size > 1) {
        for (int i=1; i<op.size;i++) {
            Fetch8();
        }
    }
    return true;
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

// This handles both LDA/STA/ORA/AND/EOR
void CPU::OperandResolveAddressAndExecute(const std::string &name, OperandAddrMode addrMode, OpHandlerActionDelegate Action) {
    auto szOperand = OpAddrModeToSize(addrMode);
    if ((szOperand == 1) && (addrMode == OperandAddrMode::Accumulator)) {
        SetStepResult("%s a", name.c_str());
        Action(0,0);
    } else if (szOperand == 2) {
        uint8_t v = Fetch8();
        switch(addrMode) {
            case OperandAddrMode::Immediate :
                SetStepResult("%s #$%02x", name.c_str(), v);
                Action(0, v);
                break;
            case OperandAddrMode::Zeropage :
                SetStepResult("%s $%02x", name.c_str(), v);
                Action(v,v);
                break;
            case OperandAddrMode::ZeropageX :
                SetStepResult("%s $%02x,x",name.c_str(), v);
                v += reg_x;
                Action(v,v);
                break;
            case OperandAddrMode::ZeroPageIndX :
                {
                    SetStepResult("%s $(%02x,x)", name.c_str(), v);
                    // Compute index in ZeroPage relative X
                    v += reg_x;
                    // Read final address as 16 bit from Zeropage
                    uint16_t finalAddr = ReadU16(v);
                    // Now perform action with final address...
                    Action(finalAddr, v);
                }
                break;
            case OperandAddrMode::ZeroPageIndY :
                SetStepResult("%s $(%02x),y", name.c_str(), v);
                v = ReadU16(v);
                v += reg_y;
                Action(v,ReadU8(v));
                break;
        }
    } else if (szOperand == 3) {
        uint16_t v = Fetch16();
        switch(addrMode) {
            case OperandAddrMode::Absolute :
                SetStepResult("%s $%04x", name.c_str(), v);
                Action(v,0);
                break;
            case OperandAddrMode::AbsoluteIndX :
                SetStepResult("%s $%04x,x", name.c_str(), v);
                v += reg_x;
                Action(v, 0);
                break;
            case OperandAddrMode::AbsoluteIndY :
                SetStepResult("%s $%04x,y", name.c_str(), v);
                v += reg_y;
                Action(v, 0);
                break;
        }

    }

}

/// Template testing
void CPU::OpHandler_LDA(OperandAddrMode addrMode) {

    OperandResolveAddressAndExecute("LDA", addrMode, [&](uint16_t index, uint8_t v) {
        if (addrMode == OperandAddrMode::Immediate) {
            reg_a = v;
        } else {
            reg_a = ReadU8(index);
        }
        RefreshStatusFromValue(reg_a);
    });
}

void CPU::OpHandler_STA(OperandAddrMode addrMode) {
    OperandResolveAddressAndExecute("STA", addrMode, [&](uint16_t index, uint8_t v) {
        WriteU8(index, reg_a);
    });
}

