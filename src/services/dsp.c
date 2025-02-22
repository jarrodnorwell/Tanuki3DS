#include "dsp.h"

#include "3ds.h"

u16 dsp_addrs[15] = {
    0xBFFF, 0x9E92, 0x8680, 0xA792, 0x9430, 0x8400, 0x8540, 0x9492,
    0x8710, 0x8410, 0xA912, 0xAA12, 0xAAD2, 0xAC52, 0xAC5C,
};

DECL_PORT(dsp) {
    u32* cmdbuf = PTR(cmd_addr);
    switch (cmd.command) {
        case 0x0001: {
            int reg = cmdbuf[1];
            linfo("RecvData %d", reg);
            cmdbuf[0] = IPCHDR(2, 0);
            cmdbuf[1] = 0;
            cmdbuf[2] = 1;
            break;
        }
        case 0x0002:
            linfo("RecvDataIsReady");
            // stub
            cmdbuf[0] = IPCHDR(2, 0);
            cmdbuf[1] = 0;
            cmdbuf[2] = 1;
            break;
        case 0x000c: {
            linfo("ConvertProcessAddressFromDspDram");
            cmdbuf[0] = IPCHDR(2, 0);
            cmdbuf[2] = DSPRAM_VBASE + 0x40000 + (cmdbuf[1] << 1);
            cmdbuf[1] = 0;
            break;
        }
        case 0x0010: {
            u32 chan = cmdbuf[1];
            u32 size = (u16) cmdbuf[3];
            void* buf = PTR(cmdbuf[0x41]);
            cmdbuf[0] = IPCHDR(2, 0);
            cmdbuf[1] = 0;
            cmdbuf[2] = size;

            // the dsp code first reads 2 bytes containing the number of entries
            // (15) then it reads 15 shorts (30 bytes) containing dsp addresses
            // of each dsp firmware memory region

            if (size == 2) {
                *(u16*) buf = 15;
            }
            if (size == 30) {
                memcpy(buf, dsp_addrs, sizeof dsp_addrs);
            }

            // but you can also read it all at once
            if (size == 32) {
                *(u16*) buf = 15;
                memcpy(buf + 2, dsp_addrs, sizeof dsp_addrs);
            }

            linfo("ReadPipeIfPossible chan=%d with size 0x%x", chan, size);
            break;
        }
        case 0x0011:
            linfo("LoadComponent");
            cmdbuf[0] = IPCHDR(2, 2);
            cmdbuf[1] = 0;
            cmdbuf[2] = true;
            cmdbuf[3] = 0;
            cmdbuf[4] = 0;
            break;
        case 0x0013:
            linfo("FlushDataCache");
            cmdbuf[0] = IPCHDR(1, 0);
            cmdbuf[1] = 0;
            break;
        case 0x0014:
            linfo("InvalidateDCache");
            cmdbuf[0] = IPCHDR(1, 0);
            cmdbuf[1] = 0;
            break;
        case 0x0015: {
            int interrupt = cmdbuf[1];
            int channel = cmdbuf[2];
            linfo("RegisterInterruptEvents int=%d,ch=%d with handle %x",
                  interrupt, channel, cmdbuf[4]);
            if (interrupt >= 3 || channel >= 4) {
                lerror("invalid channel");
                break;
            }

            KEvent** event = &s->services.dsp.events[interrupt][channel];

            // unregister an existing event
            if (*event) {
                (*event)->hdr.refcount--;
                *event = nullptr;
            }

            *event = HANDLE_GET_TYPED(cmdbuf[4], KOT_EVENT);
            if (*event) {
                (*event)->hdr.refcount++;
            }

            cmdbuf[0] = IPCHDR(1, 0);
            cmdbuf[1] = 0;
            break;
        }
        case 0x0016:
            linfo("GetSemaphoreEventHandle");
            cmdbuf[0] = IPCHDR(1, 2);
            cmdbuf[1] = 0;
            cmdbuf[2] = 0;
            cmdbuf[3] = srvobj_make_handle(s, &s->services.dsp.semEvent.hdr);
            break;
        case 0x001f:
            linfo("GetHeadphoneStatus");
            cmdbuf[0] = IPCHDR(2, 0);
            cmdbuf[1] = 0;
            cmdbuf[2] = false;
            break;
        default:
            lwarn("unknown command 0x%04x (%x,%x,%x,%x,%x)", cmd.command,
                  cmdbuf[1], cmdbuf[2], cmdbuf[3], cmdbuf[4], cmdbuf[5]);
            cmdbuf[0] = IPCHDR(1, 0);
            cmdbuf[1] = 0;
            break;
    }
}
