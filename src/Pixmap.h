//
// Created by goatman on 11/6/2021.
//

#ifndef EMU6502_PIXMAP_H
#define EMU6502_PIXMAP_H

#include <stdint.h>

using RGBA = struct { uint8_t r, g, b, a; };

class Pixmap {
public:
    static const RGBA White;
    static const RGBA Black;
    static const RGBA Red;
    static const RGBA Green;
    static const RGBA Blue;

public:
    Pixmap(void *ptr, size_t width, size_t height);
    Pixmap(size_t width, size_t height);
    void Clear(RGBA col);
    void PutPixel(uint32_t x, uint32_t y, RGBA col);
    RGBA GetPixel(uint32_t x, uint32_t y);

    size_t Width() const { return w; }
    size_t Height() const { return h; }
    const void *Data() const { return data; }
private:
    void *data;
    size_t w;
    size_t h;
};


#endif //EMU6502_PIXMAP_H
