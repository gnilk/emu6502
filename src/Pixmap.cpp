//
// Created by goatman on 11/6/2021.
//

#include "Pixmap.h"
#include <stdlib.h>

const RGBA Pixmap::White = {255,255,255,255};
const RGBA Pixmap::Black = {0,0,0,255};
const RGBA Pixmap::Red = {255,0,0 ,255};
const RGBA Pixmap::Green = {0,255,0 ,255};
const RGBA Pixmap::Blue = {0,0,255 ,255};


Pixmap::Pixmap(void *ptr, size_t width, size_t height) :
    data(ptr),
    w(width),
    h(height) {
}

Pixmap::Pixmap(size_t width, size_t height) : w(width), h(height) {
    data = malloc(sizeof(RGBA) * w * h);
}


void Pixmap::Clear(RGBA col) {
    RGBA *pImage = reinterpret_cast<RGBA *>(data);
    for(size_t i=0;i<w*h;i++) {
        pImage[i] = col;
    }
}
void Pixmap::PutPixel(uint32_t x, uint32_t y, RGBA col) {
    if ((x >= 0) && (x < w)) {
        if ((y>=0) && (y<h)) {
            RGBA *pImage = reinterpret_cast<RGBA *>(data);
            pImage[x + y * w] = col;
        }
    }
}
RGBA Pixmap::GetPixel(uint32_t x, uint32_t y) {
    if ((x >= 0) && (x < w)) {
        if ((y>=0) && (y<h)) {
            RGBA *pImage = reinterpret_cast<RGBA *>(data);
            return pImage[x + y * w];
        }
    }
    return Pixmap::Black;
}

