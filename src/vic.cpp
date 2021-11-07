//
// Created by goatman on 11/6/2021.
//

#include <cstdio>
#include <stdint.h>
#include "vic.h"


static VicType vic6569 = {
        .nVerticalLines = 312,
        .vblBegin = 300,
        .vblEnd = 15,
};

// Based off PEPTO-PAL from Vice 3.5
static RGBA palette[]={
        // 0:Black
        {0,0,0, 255},
        // 1:White
        {0xff, 0xff, 0xff, 255},
        // 2:Red
        {0x68,0x37,0x2b,255},
        // 3:Cyan
        { 0x70, 0xa4, 0xb2, 255},
        // 4:Purple
        { 0x6f, 0x3d, 0x86, 255},
        // 5:Green
        { 0x58, 0x8d, 0x43, 255},
        // 6:Blue
        { 0x35, 0x28, 0x79, 255},
        // 7:Yellow
        {0xb8, 0xc7, 0x6f, 255},
        // 8:Orange
        {0x6f, 0x4f, 0x25, 255},
        // 9:Brown
        {0x43, 0x39, 0x00, 255},
        // 10:Light red
        { 0x9a, 0x67,0x59,255},
        // 11:Dark Gray
        {0x44, 0x44, 0x44, 255},
        // 12:Medium gray
        { 0x6c, 0x6c, 0x6c, 255},
        // 13:Light Green
        { 0x9a, 0xd2, 0x84, 255},
        // 14:Light Blue
        { 0x6c, 0x5e, 0xb5, 255},
        // 15:Light gray
        {0x95, 0x95,0x95, 255}

};


VIC::VIC(Memory &memory) :
    ram(memory),
    // width 512 is too wide...  probably around 403
    screen(512,312),
    rasterY(0),
    rasterX(0),
    rasterYState(InsideVBL),
    rasterXState(InsideHBL)
{
    // Reset some vars
    ram[BorderCol] = LightBlue;
    ram[BackgroundCol] = Blue;
    screen.Clear(Pixmap::White);
}

#define NUM_RAS_LINES_PAL 312

void VIC::Tick() {

    UpdateHorizontalState();
    UpdateVerticalState();

    // No need to do any drawing...
    RenderToScreen();
    // Tick CPU here...

}
void VIC::RenderToScreen() {
    // Set to black, initially
    RGBA col = Pixmap::Black;
    if (rasterYState == InsideVBL) {
        col = Pixmap::Red;
    } else {
        switch (rasterXState) {
            case InsideBorder :
                col = palette[ram[BorderCol] & 0x0f];
                break;
            case InsideMain :
                col = palette[ram[BackgroundCol] & 0x0f];
                break;
        }
    }
    // Draw this tick...
    for(int i=0;i<8;i++) {
        screen.PutPixel(rasterX * 8 + i, rasterY, col);
    }
}

void VIC::UpdateHorizontalState() {
    rasterX++;
    if (rasterX == 63) {
        rasterX = 0;
    }

    if (rasterYState == InsideVBL) {
        // Don't track raster X states if we are within VBL
        rasterXState = Invalid;
    }

    if ((rasterX >= 11) && (rasterX < 16)) {
        rasterXState = InsideBorder;
    } else if ((rasterX >= 16) && (rasterX < 56)) {
        if (IsInVerticalBorder()) {
            rasterXState = InsideBorder;
        } else {
            rasterXState = InsideMain;
        }
    } else if ((rasterX >= 56) && (rasterX < 61)) {
        rasterXState = InsideBorder;
    } else if (rasterX >= 61) {
        rasterXState = InsideHBL;
    } else {
        // < 11
        rasterXState = InsideHBL;
    }
}
bool VIC::IsVBL() {
    if (rasterY >= vic6569.vblBegin) {
        return true;
    }
    if (rasterY <= vic6569.vblEnd) {
        return true;
    }
    return false;
}

bool VIC::IsInVerticalBorder() {
    // NOTE: This depends on RSEL
    if (rasterY < 50) return true;
    if (rasterY > 250) return true;
    return false;
}

void VIC::UpdateVerticalState() {
    if (rasterX == 0) {
        rasterY++;
    }

    if (rasterY > vic6569.nVerticalLines) {
        rasterY = 0;
    }
    if (IsVBL()) {
        rasterYState = InsideVBL;
    } else {
        rasterYState = OutsideVBL;
    }

    // Update raster counters
    auto raster = GetReg<VICRegRaster>(Raster);
    raster->COUNTER = rasterY;

    // Upper bit of CNTRL1 is MSB of rater counter
    auto cntrl1 = GetReg<VICRegControl1>(Control1);
    cntrl1->RST8 = (rasterY & 0x100)?1:0;

}

