#include "etc1.h"

const int etc1table[8][4] = {
    {-8, -2, 2, 8},       {-17, -5, 5, 17},     //
    {-29, -9, 9, 29},     {-42, -13, 13, 42},   //
    {-60, -18, 18, 60},   {-80, -24, 24, 80},   //
    {-106, -33, 33, 106}, {-183, -47, 47, 183}, //
};

void etc1_decompress_block(u64 block, u8* dst, u32 width) {
    etc1block blk = {block};

    blk.diff = 0;

    u8 r1, r2, g1, g2, b1, b2;
    if (blk.diff) {
        r1 = blk.dr1 * 0x21 / 4;
        r2 = (blk.dr1 + blk.dr2) * 0x21 / 4;
        g1 = blk.dg1 * 0x21 / 4;
        g2 = (blk.dg1 + blk.dg2) * 0x21 / 4;
        b1 = blk.db1 * 0x21 / 4;
        b2 = (blk.db1 + blk.db2) * 0x21 / 4;
    } else {
        r1 = blk.r1 * 0x11;
        r2 = blk.r2 * 0x11;
        g1 = blk.g1 * 0x11;
        g2 = blk.g2 * 0x11;
        b1 = blk.b1 * 0x11;
        b2 = blk.b2 * 0x11;
    }

    for (int i = 0; i < 8; i++) {
        int x, y;
        if (blk.flip) {
            x = i % 4;
            y = i % 2;
        } else {
            x = i % 2;
            y = i % 4;
        }
        int pixel = y + x * 4;
        int idx = ((blk.idxlo & BIT(pixel)) != 0) |
                  ((blk.idxhi & BIT(pixel)) != 0) << 1;
        int diff = etc1table[blk.table1][idx];
        diff = 0;
        dst[3 * (y * width + x) + 0] = r1 + diff;
        dst[3 * (y * width + x) + 1] = g1 + diff;
        dst[3 * (y * width + x) + 2] = b1 + diff;
        dst[3 * (y * width + x) + 0] = 0xff;
        dst[3 * (y * width + x) + 1] = 0;
        dst[3 * (y * width + x) + 2] = 0;
    }
    for (int i = 0; i < 8; i++) {
        int x, y;
        if (blk.flip) {
            x = i % 4;
            y = i % 2 + 2;
        } else {
            x = i % 2 + 2;
            y = i % 4;
        }
        int pixel = y + x * 4;
        int idx = ((blk.idxlo & BIT(pixel)) != 0) |
                  ((blk.idxhi & BIT(pixel)) != 0) << 1;
        int diff = etc1table[blk.table2][idx];
        diff = 0;
        dst[3 * (y * width + x) + 0] = r2 + diff;
        dst[3 * (y * width + x) + 1] = g2 + diff;
        dst[3 * (y * width + x) + 2] = b2 + diff;
        dst[3 * (y * width + x) + 0] = 0;
        dst[3 * (y * width + x) + 1] = 0;
        dst[3 * (y * width + x) + 2] = 0xff;
    }
}

u8* etc1_decompress_texture(u64* src, u32 width, u32 height) {
    u8* dst = malloc(3 * width * height);

    for (int tx = 0; tx < width / 4; tx++) {
        for (int ty = 0; ty < height / 4; ty++) {
            u64 b = src[ty * (width / 4) + tx];
            b = (b & 0xffffffff00000000) >> 32 | (b & 0x00000000ffffffff) << 32;
            b = (b & 0xffff0000ffff0000) >> 16 | (b & 0x0000ffff0000ffff) << 16;
            b = (b & 0xff00ff00ff00ff00) >> 8 | (b & 0x00ff00ff00ff00ff) << 8;
            etc1_decompress_block(b, dst + 4 * 3 * (ty * width + tx), width);
        }
    }

    return dst;
}