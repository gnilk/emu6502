//
// Created by goatman on 11/6/2021.
//

#ifndef EMU6502_VIC_H
#define EMU6502_VIC_H

#include "Pixmap.h"
#include "memory.h"


typedef struct {
    // Raster Y
    int32_t nVerticalLines;
    int32_t vblBegin;
    int32_t vblEnd;
    // Raster X
} VicType;


#pragma pack(push, 1)
typedef struct {
    uint8_t YScroll : 3;    // 0..7
    uint8_t RSEL : 1;   // Row Select
    uint8_t DEN : 1;    // Display ENable
    uint8_t BMM : 1;    //
    uint8_t ECM : 1;
    uint8_t RST8 : 1;
} VICRegControl1;

typedef struct {
    uint8_t COUNTER;
} VICRegRaster;

#pragma pack(pop)

// http://www.zimmers.net/cbmpics/cbm/c64/vic-ii.txt
class VIC {
public:
    enum Color {
        Black = 0,
        White = 1,
        Red = 2,
        Cyan = 3,
        Purple = 4,
        Green = 5,
        Blue = 6,
        Yellow = 7,
        Orange = 8,
        Brown = 9,
        LightRed = 10,
        DarkGray = 11,
        MediumGray = 12,
        LightGreen = 13,
        LightBlue = 14,
        LightGray = 15
    };
    enum RasterYState {
        OutsideVBL = 0,
        InsideVBL = 1,
    };
    enum RasterXState {
        Invalid = 0,
        InsideHBL = 1,
        InsideBorder = 2,
        InsideMain = 3,
    };
    enum Regs {
        Control1 = 0xd011,
        Raster = 0xd012,
        Control2 = 0xd016,
        BorderCol = 0xd020,
        BackgroundCol = 0xd021,
    };
public:
    VIC(Memory &memory);
    void Tick();
    const Pixmap &Screen() const { return screen; }
public:// Getters
    inline uint32_t RasterX() const { return rasterX; };
    inline uint32_t RasterY() const { return rasterY; }
private:
    template<typename T>
    inline T *GetReg(Regs reg) {
        return reinterpret_cast<T *>(ram.PtrAt(reg));
    }

    bool IsInVerticalBorder();
    bool IsVBL();
    bool IsBadLine();
    void HandleBadLine();
    void UpdateHorizontalState();
    void UpdateVerticalState();
    void RenderToScreen();
private:
    Memory &ram;
    Pixmap screen;
private:
    uint32_t rasterY;
    uint32_t rasterX;
    RasterYState rasterYState;
    RasterXState rasterXState;
    uint16_t videoMatrixAddress;
    uint32_t videoMatrixCounter;
private:
    bool cpuStunned;
    uint8_t stunCycleCount;
    uint8_t chars[40];      // bad-line cache
private:
    uint32_t videoRowCounter;
private:

};


#endif //EMU6502_VIC_H
