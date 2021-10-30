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
enum class CpuOperands : uint8_t {
    BRK = 0x00,
    PHP = 0x08,
    ORA_IMM = 0x09,
    CLC = 0x18,
    JSR = 0x20,
    PLP = 0x28,
    AND_IMM = 0x29,
    SEC = 0x38,
    RTI = 0x40,
    PHA = 0x48,
    EOR_IMM = 0x49,
    CLI = 0x48,
    RTS = 0x60,
    PLA = 0x68,
    ADC_IMM = 0x69,
    SEI = 0x78,
    DEY = 0x88,
    TXA = 0x8a,
    STA = 0x8d,
    TYA = 0x98,
    LDY_IMM = 0xa0,
    LDX_IMM = 0xa2,
    TAY = 0xa8,
    LDA_IMM = 0xa9,
    TAX = 0xaa,
    CLD = 0xd8,
    CLV = 0xB8,
    INY = 0xC8,
    DEX = 0xCA,
    LDA_ABS = 0xad,
    INX = 0xe8,
    SBC_IMM = 0xe9,
    NOP = 0xea,
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

enum class OperandAddrMode : uint8_t {
    Invalid      = 0,    // Invalid operand size
    Immediate    = 1,    // Immediate mode
    Absolute     = 2,    // Absolute
    AbsoluteIndX = 3,    // Absolute
    AbsoluteIndY = 4,    // Absolute
    Zeropage     = 5,    // Zeropage
    ZeropageX    = 6,    // Zeropage,x
    ZeroPageIndX = 7,    // (Zeropage),x
    ZeroPageIndY = 8,    // (Zeropage),y
    Accumulator  = 9,    // Directly affecting accumulator
};

class CPU {
public:
    using CPUInstruction = struct {
        //uint8_t opCode;
        CpuOperands opCode;
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

    // TEMP - remove this to protected later..
    void WriteU8(uint32_t index, uint8_t value);

protected:
    bool TryDecode();
    bool TryDecodeInternal();
    bool TryDecodeOddities(uint8_t incoming);
    bool TryDecodeBranches(uint8_t incoming);
    bool TryDecodeOpGroup(uint8_t incoming);
    bool TryDecodeLeftovers(uint8_t incoming);


    void RefreshStatusFromValue(uint8_t reg);
    void SetStepResult(const char *format, ...);

    static const std::string ToBinaryU8(uint8_t byte);

    uint8_t Fetch8();
    uint16_t Fetch16();
    uint32_t Fetch32();
    uint8_t ReadU8(uint32_t index);
    uint16_t ReadU16(uint32_t index);
    uint32_t ReadU32(uint32_t index);
//    void WriteU8(uint32_t index, uint8_t value);
    void WriteU16(uint32_t index, uint16_t value);
    void WriteU32(uint32_t index, uint32_t value);

    void Push8(uint8_t value);
    void Push16(uint16_t value);
    uint8_t Pop8();
    uint16_t Pop16();
private:
    using OpHandlerActionDelegate = std::function<void(uint16_t index, uint8_t v)>;
    void OperandResolveAddressAndExecute(const std::string &name, OperandAddrMode addrMode, OpHandlerActionDelegate Action);
    void InitializeOpGroup01();
    void InitializeOpGroup10();

    void OpHandler_LDA(OperandAddrMode addrMode);
    void OpHandler_STA(OperandAddrMode addrMode);

private:
    CpuFlags mstatus;
    uint32_t ip;    // instruction pointer, index in RAM
    uint32_t sp;    // stack point, index in RAM
    int32_t reg_a;
    uint32_t reg_x;
    uint32_t reg_y;
    // RAM Memory
    uint8_t *ram;

    // Not releated to 6502
    kDebugFlags debugFlags;
    std::string lastStepResult;
    std::map<CpuOperands, CPUInstruction > instructions;
};


#endif //EMU6502_CPU_H
