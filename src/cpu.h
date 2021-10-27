//
// Created by Fredrik Kling on 27/10/2021.
//

#ifndef EMU6502_CPU_H
#define EMU6502_CPU_H

#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <bitset>
#include <type_traits>


#define MAX_RAM (64*1024)
enum class kCpuOperands : uint8_t {
    BRK = 0x00,
    PHP = 0x08,
    CLC = 0x18,
    JSR = 0x20,
    PLP = 0x28,
    SEC = 0x38,
    PHA = 0x48,
    EOR_IMM = 0x49,
    RTS = 0x60,
    PLA = 0x68,
    ADC_IMM = 0x69,
    STA = 0x8d,
    NOP = 0xea,
    LDA_IMM = 0xa9,
    CLD = 0xd8,
    CLV = 0xB8,
    LDA_ABS = 0xad,
    SED = 0xf8,
};

// The good thing with scoped is naming - is becomes much more simplified as we can reuse names
// and usage must be fully-qualified...
// The bad thing is that we don't get implicit type conversion to the underyling type for binary operands for flags..
// Could try with 'bitset' as well...
enum class kDebugFlags : uint8_t {
    None = 0x00,
    MemoryRead = 0x01,
    MemoryWrite = 0x02,
    StepDisAsm = 0x04,
    StepCPUReg = 0x08,
};


//
// Template for std::bitset based enum types
//
template <typename EnumT>
class Flags {
    static_assert(std::is_enum_v<EnumT>, "Flags can only be specialized for enum types");

    using UnderlyingT = typename std::make_unsigned_t<typename std::underlying_type_t<EnumT>>;

public:
    Flags(UnderlyingT val) {
        bits_ = val;
    }
    Flags& set(EnumT e, bool value = true) noexcept {
        bits_.set(underlying(e), value);
        return *this;
    }

    Flags& reset(EnumT e) noexcept {
        set(e, false);
        return *this;
    }

    Flags& reset() noexcept {
        bits_.reset();
        return *this;
    }

    [[nodiscard]] bool all() const noexcept {
        return bits_.all();
    }

    [[nodiscard]] unsigned long to_ulong() const {
        return bits_.to_ulong();
    }

    [[nodiscard]] UnderlyingT raw() const {
        return bits_.to_ulong();
    }


    [[nodiscard]] bool any() const noexcept {
        return bits_.any();
    }

    [[nodiscard]] bool none() const noexcept {
        return bits_.none();
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept {
        return bits_.size();
    }

    [[nodiscard]] std::size_t count() const noexcept {
        return bits_.count();
    }

    constexpr bool operator[](EnumT e) const {
        return bits_[underlying(e)];
    }

private:
    static constexpr UnderlyingT underlying(EnumT e) {
        return static_cast<UnderlyingT>(e);
    }

private:
    std::bitset<underlying(EnumT::size)> bits_;
};

//
// Define the flags
//
enum class CpuFlag : uint8_t {
    Carry,
    Zero,
    InterruptDisable,
    DecimalMode,
    BreakCmd,
    Unused,
    Overflow,
    Negative,
    // ...
    size
};
// Create an enum for those flags...
using CpuFlags = Flags<CpuFlag>;



class CPU {
public:
    using CPUInstruction = struct {
        //uint8_t opCode;
        kCpuOperands opCode;
        uint8_t bytes;
        std::string name;
        std::function<void()> exec;
    };
public:
    CPU();
    void Initialize();
    void Reset(uint32_t ipAddr);
    void Load(const uint8_t *from, uint32_t offset, uint32_t nbytes);
    bool Step();
    const uint8_t *RAMPtr() const { return ram; }

    void SetDebug(kDebugFlags flag, bool enable);

protected:
    void RefreshStatusFromALU(bool updateNeg = false);
    void SetStepResult(const char *format, ...);

    static const std::string ToBinaryU8(uint8_t byte);

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
    CpuFlags mstatus;
    uint32_t ip;    // instruction pointer, index in RAM
    uint32_t sp;    // stack point, index in RAM
    uint32_t reg_a;
    uint32_t reg_x;
    uint32_t reg_y;
    // RAM Memory
    uint8_t *ram;

    // Not releated to 6502
    kDebugFlags debugFlags;
    std::string lastStepResult;
    std::map<kCpuOperands, CPUInstruction > instructions;
};


#endif //EMU6502_CPU_H