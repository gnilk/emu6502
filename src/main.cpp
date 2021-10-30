//
// Skeleton for 6502 CPU emulator by Gnilk
//
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <string>
#include <cstdarg>

#include "cpu.h"

static void HexDump(const uint8_t *ptr, size_t ofs, size_t len);




static void HexDump(const uint8_t *ptr, size_t ofs, size_t len) {
    char tmp[64];
    int counter = 0;
    for(size_t i=0;i<len;i++) {
        if ((i & 15) == 0)  { printf("%04x  ", (int)(i+ofs)); }
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
    cpu.SetDebug(kDebugFlags::StepDisAsm, true);
    cpu.SetDebug(kDebugFlags::StepCPUReg, true);
    //cpu.SetDebug(kDebugFlags::MemoryRead, true);
    HexDump(cpu.RAMPtr(), 0x4100, 16);
    while(cpu.Step()) {
        HexDump(cpu.RAMPtr(), 0x4100, 16);
    }
    HexDump(cpu.RAMPtr(), 0x4100, 16);
    return 0;
}
