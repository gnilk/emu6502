//
// Skeleton for 6502 CPU emulator by Gnilk
//
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <string>
#include <map>
#include <functional>

static void HexDump(const uint8_t *ptr, size_t ofs, size_t len);

#define MAX_RAM (64*1024)
typedef enum {
    kOpCode_BRK = 0x00,
    kOpCode_JSR = 0x20,
    kOpCode_RTS = 0x60,
    kOpCode_STA = 0x8d,
    kOpCode_NOP = 0xea,
    kOpCode_LDA_IMM = 0xa9,
    kOpCode_LDA_ABS = 0xad,
} kCpuOperands;

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

protected:
    void SetALU(uint8_t value);
    void UpdateStatus(uint8_t setFlags, bool enable);
    uint8_t Fetch8();
    uint16_t Fetch16();
    uint32_t Fetch32();
    uint8_t ReadU8(uint32_t index);
    uint16_t ReadU16(uint32_t index);
    uint32_t ReadU32(uint32_t index);
    void WriteU8(uint32_t index, uint8_t value);
    void WriteU16(uint32_t index, uint16_t value);
    void WriteU32(uint32_t index, uint32_t value);

    void Push16(uint16_t value);
    uint16_t Pop16();

private:
    uint8_t status;
    uint32_t ip;    // instruction pointer, index in RAM
    uint32_t sp;    // stack point, index in RAM
    uint32_t reg_a;
    uint32_t reg_x;
    uint32_t reg_y;
    uint8_t *ram;

    std::map<uint8_t, CPUInstruction > instructions;
};

CPU::CPU() : ram(nullptr) {

}
void CPU::Initialize() {
    if (ram != nullptr) {
        free(ram);
    }
    ram = (uint8_t *)malloc(MAX_RAM);
    ip = 0;
    sp = 0x1ff; // stack pointer, points to first available byte..
    status = 0x00;


    // Setup instruction set...
    instructions[kOpCode_LDA_IMM] = {
            kOpCode_LDA_IMM, 2, "LDA", [this](){
                SetALU(Fetch8());
                printf("LDA #$%02x\n", reg_a);
            }
    };

    instructions[kOpCode_LDA_ABS] = {
            kOpCode_LDA_ABS, 3, "LDA", [this](){
                uint16_t ofs = Fetch16();
                uint8_t value = ReadU8(ofs);
                SetALU(value);
                printf("LDA $%04x  ($%04x => $02x)\n", ofs, ofs, value);
            }
    };

    instructions[kOpCode_STA] = {
            kOpCode_STA, 3, "STA", [this](){
                uint16_t ofs = Fetch16();
                WriteU8(ofs, reg_a);
                printf("STA $%04x (#$%02x => $%04x)\n", ofs, reg_a, ofs);
            }
    };

    instructions[kOpCode_JSR] = {
            kOpCode_JSR, 3, "JSR", [this](){
                uint16_t ofs = Fetch16();
                uint16_t ipReturn = ip;
                printf("JSR $%04x\n", ofs);
                ip = ofs;
                Push16(ipReturn);
            }
    };
    instructions[kOpCode_RTS] = {
            kOpCode_RTS,1,"RTS", [this](){
                uint16_t ofs = Pop16();
                printf("RTS  (ip will be: $%04x)\n", ofs);
                ip = ofs;
            }
    };
}

void CPU::Reset(uint32_t ipAddr) {
    ip = ipAddr;
    sp = 0x1ff; // stack pointer, points to first available byte..
}

void CPU::Load(const uint8_t *from, const uint32_t offset, uint32_t nbytes) {
    printf("Loading %d bytes to offset 0x%04x (%d)\n", nbytes, offset, offset);
    memcpy(&ram[offset], from, nbytes);
}

bool CPU::Step() {
    bool res = true;
    uint8_t opcode = Fetch8();
    printf("OPCode: $%02x\n", opcode);
    if (instructions.find(opcode) != instructions.end()) {
        instructions[opcode].exec();
    } else {
        switch (opcode) {
            case kOpCode_BRK :
                res = false;
                break;
            case kOpCode_NOP :
                // Do nothing...
                break;
            default:
                res = false;
        }
    }
    return res;
}

void CPU::SetALU(uint8_t value) {
    reg_a = value;

    if (!reg_a) {
        UpdateStatus(kFlag_Zero, true);
    } else {
        UpdateStatus(kFlag_Zero, false);
    }
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
void CPU::Push16(uint16_t value) {
    sp -= 1;    // Make room for one more value - we are pushing 16 bits and the stack points to first available
    WriteU16(sp, value);
    sp -= 1;    // Advance stack to next available
}

uint16_t CPU::Pop16() {
    uint16_t value = ReadU16(sp+1);
    sp += sizeof(uint16_t);
    return value;
}


uint8_t CPU::ReadU8(uint32_t index) {
    printf("[CPU] Read8 from ofs: 0x%04x (%d)\n", index, index);
    return ram[index];
}
uint16_t CPU::ReadU16(uint32_t index) {
    printf("[CPU] Read16 from ofs: 0x%04x (%d)\n", index, index);
    auto ptr = reinterpret_cast<uint16_t *>(&ram[index]);
    return *ptr;
}
uint32_t CPU::ReadU32(uint32_t index) {
    printf("[CPU] Read32 from ofs: 0x%04x (%d)\n", index, index);
    auto ptr = reinterpret_cast<uint32_t *>(&ram[index]);
    return *ptr;
}

void CPU::WriteU8(uint32_t index, uint8_t value) {
    printf("[CPU] WriteU8 0x%02x to ofs: 0x%04x (%d)\n", value, index, index);
    ram[index] = value;
}


void CPU::WriteU16(uint32_t index, uint16_t value) {
    printf("[CPU] WriteU16 0x%04x to ofs: 0x%04x (%d)\n", value, index, index);
    auto *ptr = reinterpret_cast<uint16_t *>(&ram[index]);
    *ptr = value;
}

void CPU::WriteU32(uint32_t index, uint32_t value) {
    printf("[CPU] WriteU32 0x%08x to ofs: 0x%04x (%d)\n", value, index, index);
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
    HexDump(cpu.RAMPtr(), offset, 32);
    while(cpu.Step()) {}
    HexDump(cpu.RAMPtr(), 0xd000, 48);
    return 0;
}
