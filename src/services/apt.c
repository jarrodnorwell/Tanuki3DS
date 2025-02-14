#include "apt.h"

#include "../3ds.h"

DECL_PORT(apt) {
    u32* cmdbuf = PTR(cmd_addr);
    switch (cmd.command) {
        case 0x0001:
            cmdbuf[0] = IPCHDR(3, 2);
            cmdbuf[1] = 0;
            cmdbuf[2] = 0;
            cmdbuf[3] = 0;
            cmdbuf[4] = 0;
            cmdbuf[5] = srvobj_make_handle(s, &s->services.apt.lock.hdr);
            linfo("GetLockHandle is %x", cmdbuf[5]);
            break;
        case 0x0002:
            cmdbuf[0] = IPCHDR(1, 3);
            cmdbuf[1] = 0;
            cmdbuf[2] = 0x04000000;
            cmdbuf[3] = srvobj_make_handle(s, &s->services.apt.notif_event.hdr);
            cmdbuf[4] =
                srvobj_make_handle(s, &s->services.apt.resume_event.hdr);
            linfo("Initialize with notif event %x and resume event %x",
                  cmdbuf[3], cmdbuf[4]);
            break;
        case 0x0003:
            linfo("Enable");
            cmdbuf[0] = IPCHDR(1, 0);
            cmdbuf[1] = 0;
            break;
        case 0x0006: {
            u32 appid = cmdbuf[1];
            linfo("GetAppletInfo for 0x%x", appid);
            cmdbuf[0] = IPCHDR(7, 0);
            cmdbuf[1] = 0;
            cmdbuf[5] = 1;
            cmdbuf[6] = 1;
            break;
        }
        case 0x0009: {
            u32 appid = cmdbuf[1];
            linfo("IsRegistered for 0x%x", appid);
            cmdbuf[0] = IPCHDR(2, 0);
            cmdbuf[1] = 0;
            cmdbuf[2] = 1;
            break;
        }
        case 0x000c: {
            u32 appid = cmdbuf[2];
            u32 appcommand = cmdbuf[3];
            u32 handleparam = cmdbuf[6];
            linfo("SendParameter to app %x, command=%x, handleparam=%x", appid,
                  appcommand, handleparam);

            if (appcommand == APTCMD_REQUEST) {
                s->services.apt.nextparam.appid = appid;
                s->services.apt.nextparam.cmd = APTCMD_RESPONSE;
                s->services.apt.nextparam.kobj =
                    &s->services.apt.capture_block.hdr;
                event_signal(s, &s->services.apt.resume_event);
            } else {
                lwarn("unknown appcommand %x", appcommand);
            }

            cmdbuf[0] = IPCHDR(1, 0);
            cmdbuf[1] = 0;
            break;
        }
        case 0x000d:
            linfo("ReceiveParameter");
            cmdbuf[0] = IPCHDR(4, 4);
            cmdbuf[1] = 0;
            cmdbuf[2] = s->services.apt.nextparam.appid;
            cmdbuf[3] = s->services.apt.nextparam.cmd;
            if (s->services.apt.nextparam.kobj) {
                cmdbuf[6] =
                    srvobj_make_handle(s, s->services.apt.nextparam.kobj);
            } else {
                cmdbuf[6] = 0;
            }
            cmdbuf[8] = cmdbuf[0x41];
            break;
        case 0x000e:
            linfo("GlanceParameter");
            cmdbuf[0] = IPCHDR(4, 4);
            cmdbuf[1] = 0;
            cmdbuf[2] = s->services.apt.nextparam.appid;
            cmdbuf[3] = s->services.apt.nextparam.cmd;
            if (s->services.apt.nextparam.kobj) {
                cmdbuf[6] =
                    srvobj_make_handle(s, s->services.apt.nextparam.kobj);
            } else {
                cmdbuf[6] = 0;
            }
            cmdbuf[8] = cmdbuf[0x41];
            break;
        case 0x0016: {
            u32 appid = cmdbuf[1];
            linfo("PreloadLibraryApplet %x", appid);
            if (appid == APPID_ERRDISP)
                lerror("trying to launch ErrDisp applet");
            cmdbuf[0] = IPCHDR(1, 0);
            cmdbuf[1] = 0;
            break;
        }
        case 0x001e: {
            u32 appid = cmdbuf[1];
            linfo("StartLibraryApplet %x", appid);

            s->services.apt.nextparam.appid = appid;
            s->services.apt.nextparam.cmd = APTCMD_WAKEUP;
            s->services.apt.nextparam.kobj = nullptr;
            event_signal(s, &s->services.apt.resume_event);

            cmdbuf[0] = IPCHDR(1, 0);
            cmdbuf[1] = 0;
            break;
        }
        case 0x0043:
            linfo("NotifyToWait");
            cmdbuf[0] = IPCHDR(1, 0);
            cmdbuf[1] = 0;
            break;
        case 0x0044: {
            linfo("GetSharedFont");
            cmdbuf[0] = IPCHDR(2, 2);
            cmdbuf[1] = 0;
            cmdbuf[2] = s->services.apt.shared_font.mapaddr;
            cmdbuf[4] = srvobj_make_handle(s, &s->services.apt.shared_font.hdr);
            break;
        }
        case 0x004b: {
            u32 utility = cmdbuf[1];
            u32 insize = cmdbuf[2];
            u32 outsize = cmdbuf[3];
            void* input = PTR(cmdbuf[5]);
            void* output = PTR(cmdbuf[0x41]);
            linfo("AppletUtility %d in at %08x size %x out at %08x size %x",
                  utility, cmdbuf[5], insize, cmdbuf[0x41], outsize);
            cmdbuf[0] = IPCHDR(2, 0);
            cmdbuf[1] = 0;
            cmdbuf[2] = 0;
            break;
        }
        case 0x004f: {
            int percent = cmdbuf[2];
            linfo("SetApplicationCpuTimeLimit to %d%%", percent);
            s->services.apt.application_cpu_time_limit = percent;
            cmdbuf[0] = IPCHDR(1, 0);
            cmdbuf[1] = 0;
            break;
        }
        case 0x0050: {
            linfo("GetApplicationCpuTimeLimit");
            cmdbuf[0] = IPCHDR(2, 0);
            cmdbuf[1] = 0;
            cmdbuf[2] = s->services.apt.application_cpu_time_limit;
            break;
        }
        case 0x0101: {
            linfo("GetTargetPlatform");
            cmdbuf[0] = IPCHDR(2, 0);
            cmdbuf[1] = 0;
            cmdbuf[2] = 0; // 0 for old 3ds
            break;
        }
        case 0x0102: {
            linfo("CheckNew3DS");
            cmdbuf[0] = IPCHDR(2, 0);
            cmdbuf[1] = 0;
            cmdbuf[2] = 0; // 0 for old 3ds
            break;
        }
        default:
            lwarn("unknown command 0x%04x (%x,%x,%x,%x,%x)", cmd.command,
                  cmdbuf[1], cmdbuf[2], cmdbuf[3], cmdbuf[4], cmdbuf[5]);
            cmdbuf[0] = IPCHDR(1, 0);
            cmdbuf[1] = 0;
            break;
    }
}
