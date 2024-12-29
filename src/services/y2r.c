#include "y2r.h"

#include "../3ds.h"

enum {
    INPUT_YUV422_INDIV_8 = 0,
    INPUT_YUV420_INDIV_8 = 1,
    INPUT_YUV422_INDIV_16 = 2,
    INPUT_YUV420_INDIV_16 = 3,
    INPUT_YUV422_BATCH = 4
};

enum {
    OUTPUT_RGB_32 = 0,
    OUTPUT_RGB_24 = 1,
    OUTPUT_RGB_16_555 = 2,
    OUTPUT_RGB_16_565 = 4
};

enum {
    ROTATION_NONE = 0,
    ROTATION_CLOCKWISE_90 = 1,
    ROTATION_CLOCKWISE_180 = 2,
    ROTATION_CLOCKWISE_270 = 3
};

enum { BLOCK_LINE = 0, BLOCK_8_BY_8 = 1 };

typedef struct {
    u16 Y_A;
    u16 R_V, G_V;
    u16 G_U, B_U;
    u16 R_Offset, G_Offset, B_Offset;
} CoefficientParams;

typedef struct {
    u16 w0_xEven_yEven;
    u16 w0_xOdd_yEven;
    u16 w0_xEven_yOdd;
    u16 w0_xOdd_yOdd;

    u16 w1_xEven_yEven;
    u16 w1_xOdd_yEven;
    u16 w1_xEven_yOdd;
    u16 w1_xOdd_yOdd;

    u16 w2_xEven_yEven;
    u16 w2_xOdd_yEven;
    u16 w2_xEven_yOdd;
    u16 w2_xOdd_yOdd;

    u16 w3_xEven_yEven;
    u16 w3_xOdd_yEven;
    u16 w3_xEven_yOdd;
    u16 w3_xOdd_yOdd;
} DitheringWeightParams;

enum {
    COEFFICIENT_ITU_R_BT_601 = 0,
    COEFFICIENT_ITU_R_BT_709 = 1,
    COEFFICIENT_ITU_R_BT_601_SCALING = 2,
    COEFFICIENT_ITU_R_BT_709_SCALING = 3
};

typedef struct {
    u8 input_format;
    u8 output_format;
    u8 rotation;
    u8 block_alignment;

    s16 input_line_width;
    s16 input_lines;

    u8 standard_coefficient;
    u8 padding;

    s16 alpha;
} PackageParamater;

PackageParamater package_parameter;

DECL_PORT(y2r) {
    u32* cmdbuf = PTR(cmd_addr);
    switch (cmd.command) {
        default:
            lwarn("unknown command 0x%04x (%x,%x,%x,%x,%x)", cmd.command,
                  cmdbuf[1], cmdbuf[2], cmdbuf[3], cmdbuf[4], cmdbuf[5]);
            cmdbuf[0] = IPCHDR(1, 0);
            cmdbuf[1] = 0;
            break;
    }
}