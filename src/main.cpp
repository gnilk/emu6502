//
// Skeleton for 6502 CPU emulator by Gnilk
//
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <string>
#include <cstdarg>
#include <map>
#include <functional>

static void HexDump(const uint8_t *ptr, size_t ofs, size_t len);

#define MAX_RAM (64*1024)
typedef enum {
    kOpCode_BRK = 0x00,
    kOpCode_PHP = 0x08,
    kOpCode_CLC = 0x18,
    kOpCode_JSR = 0x20,
    kOpCode_PLP = 0x28,
    kOpCode_SEC = 0x38,
    kOpCode_PHA = 0x48,
    kOpCode_EOR_IMM = 0x49,
    kOpCode_RTS = 0x60,
    kOpCode_PLA = 0x68,
    kOpCode_ADC_IMM = 0x69,
    kOpCode_STA = 0x8d,
    kOpCode_NOP = 0xea,
    kOpCode_LDA_IMM = 0xa9,
    kOpCode_CLD = 0xd8,
    kOpCode_CLV = 0xB8,
    kOpCode_LDA_ABS = 0xad,
    kOpCode_SED = 0xf8,
} kCpuOperands;

typedef enum {
    kDdb_None = 0x00,
    kDbg_MemoryRead = 0x01,
    kDbg_MemoryWrite = 0x02,
    kDbg_StepDisAsm = 0x04,
    kDbg_StepCPUReg = 0x08,
} kDebugFlags;

typedef enum {
    kFlag_Carry = 0x01,
    kFlag_Zero = 0x02,
    kFlag_InterruptDisable = 0x04,
    kFlag_DecimalMode = 0x08,
    kFlag_BreakCmd = 0x10,
    kFlag_Unused = 0x20,
    kFlag_Overflow = 0x40,
    kFlag_Negative = 0x80,
} kCpuFlags;



class CPU {
public:
    using CPUInstruction = struct {
        uint8_t opCode;
        uint8_t bytes;
        std::string name;
        std::function<void()> exec;
    };
public:
    CPU();
    void Initialize();
    void Reset(uint32_t ipAddr);
    void Load(const uint8_t *from, const uint32_t offset, uint32_t nbytes);
    bool Step();
    const uint8_t *RAMPtr() const { return ram; }

    void SetDebug(kDebugFlags flag, bool enable);

protected:
    void RefreshStatusFromALU(bool updateNeg = false);
    bool IsStatusSet(kCpuFlags flag);
    void UpdateStatus(uint8_t setFlags, bool enable);
    void SetStepResult(const char *format, ...);

    static const std::string CPU::ToBinaryU8(const uint8_t byte);

    uint8_t Fetch8();
    uint16_t Fetch16();
    uint32_t Fetch32();
    uint8_t ReadU8(uint32_t index);
    uint16_t ReadU16(uint32_t index);
    uint32_t ReadU32(uint32_t index);
    void WriteU8(uint32_t index, uint8_t value);
    void WriteU16(uint32_t index, uint16_t value);
    void WriteU32(uint32_t index, uint32_t value);

    void Push8(uint8_t value);
    void Push16(uint16_t value);
    uint8_t Pop8();
    uint16_t Pop16();

private:
    uint8_t status;
    uint32_t ip;    // instruction pointer, index in RAM
    uint32_t sp;    // stack point, index in RAM
    uint32_t reg_a;
    uint32_t reg_x;
    uint32_t reg_y;
    // RAM Memory
    uint8_t *ram;

    // Not releated to 6502
    uint8_t debugFlags;
    std::string lastStepResult;
    std::map<uint8_t, CPUInstruction > instructions;
};

CPU::CPU() : ram(nullptr) {

}
void CPU::Initialize() {
    if (ram != nullptr) {
        free(ram);
    }
    ram = (uint8_t *)malloc(MAX_RAM);
    memset(ram, 0, MAX_RAM);
    ip = 0;
    sp = 0x1ff; // stack pointer, points to first available byte..
    status = 0x00;

    debugFlags = kDebugFlags::kDdb_None;

    instructions[kOpCode_CLC] = {
            kOpCode_CLC, 1, "CLC", [this](){
                UpdateStatus(kCpuFlags::kFlag_Carry, false);
                SetStepResult("CLC");
            }
    };

    instructions[kOpCode_SEC] = {
            kOpCode_SEC, 1, "SEC", [this](){
                UpdateStatus(kCpuFlags::kFlag_Carry, true);
                SetStepResult("SEC");
            }
    };

    instructions[kOpCode_PHP] = {
            kOpCode_PHP, 1, "PHP", [this](){
                Push8(status);
                SetStepResult("PHP");
        }
    };

    instructions[kOpCode_PLP] = {
            kOpCode_PLP, 1, "PLP", [this](){
                status = Pop8();
                SetStepResult("PLP");
            }
    };

    instructions[kOpCode_PHA] = {
            kOpCode_PHA, 1, "PHA", [this](){
                Push8(reg_a);
                SetStepResult("PHA");
            }
    };

    instructions[kOpCode_PLA] = {
            kOpCode_PLA, 1, "PLA", [this](){
                reg_a = Pop8();
                RefreshStatusFromALU(true);
                SetStepResult("PLA");
            }
    };


    instructions[kOpCode_CLD] = {
            kOpCode_CLD, 1, "CLD", [this](){
                UpdateStatus(kCpuFlags::kFlag_DecimalMode, false);
                SetStepResult("CLD");
            }
    };

    instructions[kOpCode_SED] = {
            kOpCode_SED, 1, "SED", [this](){
                UpdateStatus(kCpuFlags::kFlag_DecimalMode, true);
                SetStepResult("SED");
            }
    };

    instructions[kOpCode_CLV] = {
            kOpCode_CLV, 1, "CLV", [this](){
                UpdateStatus(kCpuFlags::kFlag_Overflow, false);
                SetStepResult("CLV");
            }
    };


    instructions[kOpCode_EOR_IMM] = {
            kOpCode_EOR_IMM, 2, "EOR", [this](){
                uint8_t val = Fetch8();
                reg_a ^= val;
                RefreshStatusFromALU(true);
                SetStepResult("EOR #$%02x", val);
            }
    };


    instructions[kOpCode_ADC_IMM] = {
            kOpCode_ADC_IMM, 2, "CLC", [this](){
                uint8_t val = Fetch8();
                reg_a += val;
                reg_a += IsStatusSet(kCpuFlags::kFlag_Carry)?1:0;
                if (reg_a > 255) {
                    UpdateStatus(kCpuFlags::kFlag_Carry, true);
                    // TODO: Need to understand the overflow flag a bit better...
                    // UpdateStatus(kCpuFlags::kFlag_Overflow, true);
                    reg_a &= 0xff;
                }

                // TODO: Verify if carry should be cleared in case we don't wrap..

                RefreshStatusFromALU();

                SetStepResult("ADC #$%02x (C:%d)",val, (status & kFlag_Carry)?1:0);
            }
    };


    // Setup instruction set...
    instructions[kOpCode_LDA_IMM] = {
            kOpCode_LDA_IMM, 2, "LDA", [this](){
                reg_a = Fetch8();
                RefreshStatusFromALU();
                SetStepResult("LDA #$%02x", reg_a);
            }
    };

    instructions[kOpCode_LDA_ABS] = {
            kOpCode_LDA_ABS, 3, "LDA", [this](){
                uint16_t ofs = Fetch16();
                uint8_t value = ReadU8(ofs);
                reg_a = value;
                RefreshStatusFromALU();
                SetStepResult("LDA $%04x  ($%04x => $02x)", ofs, ofs, value);
            }
    };

    instructions[kOpCode_STA] = {
            kOpCode_STA, 3, "STA", [this](){
                uint16_t ofs = Fetch16();
                WriteU8(ofs, reg_a);
                SetStepResult("STA $%04x (#$%02x => $%04x)", ofs, reg_a, ofs);
            }
    };

    instructions[kOpCode_JSR] = {
            kOpCode_JSR, 3, "JSR", [this](){
                uint16_t ofs = Fetch16();
                uint16_t ipReturn = ip;
                SetStepResult("JSR $%04x", ofs);
                ip = ofs;
                Push16(ipReturn);
            }
    };
    instructions[kOpCode_RTS] = {
            kOpCode_RTS,1,"RTS", [this](){
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
    status = 0x00;      // should be 0x16 according to: https://www.c64-wiki.com/wiki/Processor_Status_Register
}

void CPU::Load(const uint8_t *from, const uint32_t offset, uint32_t nbytes) {
    printf("Loading %d bytes to offset 0x%04x (%d)\n", nbytes, offset, offset);
    memcpy(&ram[offset], from, nbytes);
}

bool CPU::Step() {
    bool res = true;
    uint8_t opcode = Fetch8();
    if (instructions.find(opcode) != instructions.end()) {
        instructions[opcode].exec();
    } else {
        switch (opcode) {
            case kOpCode_BRK :
                res = false;
                SetStepResult("BRK");
                break;
            case kOpCode_NOP :
                // Do nothing...
                SetStepResult("NOP");
                break;
            default:
                SetStepResult("ERR: Unknown OPCode $%02x at address $%04x", opcode, ip-1);
                res = false;
        }
    }
    if(debugFlags & kDebugFlags::kDbg_StepDisAsm) {
        printf("%s\n", lastStepResult.c_str());
    }
    if (debugFlags & kDebugFlags::kDbg_StepCPUReg) {
        printf("IP=$%04x SP=$%02x A=$%02x X=$%02x Y=$%02x P=%s (NV-BDIZC)\n",
               ip, sp, reg_a, reg_x, reg_y, ToBinaryU8(status).c_str());
    }
    return res;
}

const std::string CPU::ToBinaryU8(const uint8_t byte) {

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
        UpdateStatus(kFlag_Zero, true);
    } else {
        UpdateStatus(kFlag_Zero, false);
    }

    // TODO: need argument if update neg
    if (updateNeg) {
        if (reg_a & 0x80) {
            UpdateStatus(kFlag_Negative, true);
        } else {
            // Clear???
            UpdateStatus(kFlag_Negative, false);
        }
    }
}

bool CPU::IsStatusSet(kCpuFlags flag) {
    if (status & flag) {
        return true;
    }
    return false;
}
void CPU::UpdateStatus(uint8_t setFlags, bool enable) {
    if (enable) {
        status |= setFlags;
    } else {
        status &= (setFlags ^ 0xff);
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
    if(debugFlags & kDebugFlags::kDbg_MemoryRead) {
        printf("[CPU] Read8 0x%02x from ofs: 0x%04x (%d)\n", ram[index], index, index);
    }
    return ram[index];
}
uint16_t CPU::ReadU16(uint32_t index) {
    auto ptr = reinterpret_cast<uint16_t *>(&ram[index]);
    if(debugFlags & kDebugFlags::kDbg_MemoryRead) {
        printf("[CPU] Read16  0x%04x from ofs: 0x%04x (%d)\n", *ptr, index, index);
    }
    return *ptr;
}
uint32_t CPU::ReadU32(uint32_t index) {
    auto ptr = reinterpret_cast<uint32_t *>(&ram[index]);
    if(debugFlags & kDebugFlags::kDbg_MemoryRead) {
        printf("[CPU] Read32 0x%08x from ofs: 0x%04x (%d)\n", *ptr, index, index);
    }
    return *ptr;
}

void CPU::WriteU8(uint32_t index, uint8_t value) {
    if(debugFlags & kDebugFlags::kDbg_MemoryWrite) {
        printf("[CPU] WriteU8 0x%02x to ofs: 0x%04x (%d)\n", value, index, index);
    }
    ram[index] = value;
}


void CPU::WriteU16(uint32_t index, uint16_t value) {
    if(debugFlags & kDebugFlags::kDbg_MemoryWrite) {
        printf("[CPU] WriteU16 0x%04x to ofs: 0x%04x (%d)\n", value, index, index);
    }
    auto *ptr = reinterpret_cast<uint16_t *>(&ram[index]);
    *ptr = value;
}

void CPU::WriteU32(uint32_t index, uint32_t value) {
    if(debugFlags & kDebugFlags::kDbg_MemoryWrite) {
        printf("[CPU] WriteU32 0x%08x to ofs: 0x%04x (%d)\n", value, index, index);
    }
    auto *ptr = reinterpret_cast<uint32_t *>(&ram[index]);
    *ptr = value;
}


static void HexDump(const uint8_t *ptr, size_t ofs, size_t len) {
    char tmp[64];
    int counter = 0;
    for(size_t i=0;i<len;i++) {
        if ((i & 15) == 0)  { printf("%04x  ", i+ofs); }
        printf("%02x ", ptr[i+ofs]);
        if (ptr[i] > 31 && ptr[i+ofs] < 127) {
            tmp[counter] = ptr[i+ofs];
        } else {
            tmp[counter] = '.';
        }
        if ((i & 7) == 7) {
            printf("  ");
        }
        if ((i & 15) == 15) {
            tmp[counter] = '\0';
            printf("  %s\n", tmp);
        }
    }
}

static long fsize(FILE *f) {
    long curPos = ftell(f);
    fseek(f,0,SEEK_END);
    auto szfile = ftell(f);
    fseek(f,curPos,SEEK_SET);
    return szfile;
}

//
// Loads a PRG file to correct location and returns the address
//
static uint16_t LoadPRG(const std::string &filename, CPU &cpu) {
    auto f = fopen(filename.c_str(), "rb");
    if (!f) {
        printf("ERR: Unable to open file: %s\n", filename.c_str());
        return 0;
    }
    auto szfile = fsize(f);
    uint8_t *buffer = reinterpret_cast<uint8_t *>(malloc(szfile - 2));
    if (buffer == nullptr) {
        fclose(f);
        return 0;
    }
    uint16_t offset = 0;
    fread(&offset, 2, 1, f);
    // Now read the rest...
    printf("Offset: $%02x, reading: %zd bytes\n", offset, szfile-2);
    fread(buffer, szfile-2, 1, f);
    fclose(f);

    cpu.Load(buffer, offset, szfile-2);
    return offset;
}

static uint8_t bincode[]={
        0xa9,0xff,
        0x8d,0x80,0x00,
        0x00,
};

int main(int argc, char **argv) {
    CPU cpu;
    cpu.Initialize();

    uint16_t offset = 0;
    if (argc > 1) {
        printf("Loading PRG: %s\n", argv[1]);
        offset = LoadPRG(argv[1], cpu);
        if (!offset) {
            printf("Err: Unable to load %s\n", argv[1]);
            return 0;
        }
    } else {
        cpu.Load(bincode, offset, sizeof(bincode));
    }

    // Reset CPU and set instruction pointer offset..
    cpu.Reset(offset);
    cpu.SetDebug(kDebugFlags::kDbg_StepDisAsm, true);
    cpu.SetDebug(kDebugFlags::kDbg_StepCPUReg, true);
    HexDump(cpu.RAMPtr(), 0x4100, 16);
    while(cpu.Step()) {}
    HexDump(cpu.RAMPtr(), 0x4100, 16);
    return 0;
}
