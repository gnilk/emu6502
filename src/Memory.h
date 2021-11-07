//
// Created by goatman on 10/31/2021.
//

#ifndef EMU6502_MEMORY_H
#define EMU6502_MEMORY_H

#ifndef EMU6502_RAM_SIZE
#define EMU6502_RAM_SIZE 65536
#endif

class Memory {
public:
    Memory(size_t szRam = EMU6502_RAM_SIZE);
    ~Memory();

    void CopyTo(uint32_t dstIndex, const void *src, size_t nBytes);
    const uint8_t *RawPtr() { return ram; }
    uint8_t *PtrAt(uint32_t index) { return &ram[index]; }
    uint8_t ReadU8(uint32_t index);
    uint16_t ReadU16(uint32_t index);
    uint32_t ReadU32(uint32_t index);
    void WriteU8(uint32_t index, uint8_t value);
    void WriteU16(uint32_t index, uint16_t value);
    void WriteU32(uint32_t index, uint32_t value);

    inline uint8_t &operator[](const size_t index) noexcept {
        return ram[index];
    }

    // TODO: Support debug flags...
private:
    size_t szRamBuffer;
    uint8_t *ram;
};

#endif //EMU6502_MEMORY_H
