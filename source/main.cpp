/*
    NOTE: sd card must contain the following directories:
    /_test, /_test2

    the following blank file must exist for the sysmodule to activate:
    /_test2/enabled.flag

    you can toggle the sysmodule on and off at runtime using this file and e.g. sys-ftpd

    logs are written to /_test/
*/

// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Include the main libnx system header, for Switch development
#include <switch.h>

#include "dmnt/dmntcht.h"
#include <string>
#include <sstream>

// Size of the inner heap (adjust as necessary).
#define INNER_HEAP_SIZE 0x80000

#ifdef __cplusplus
extern "C" {
#endif

// Sysmodules should not use applet*.
u32 __nx_applet_type = AppletType_None;

// Sysmodules will normally only want to use one FS session.
u32 __nx_fs_num_sessions = 1;

// Newlib heap configuration function (makes malloc/free work).
void __libnx_initheap(void)
{
    static u8 inner_heap[INNER_HEAP_SIZE];
    extern void* fake_heap_start;
    extern void* fake_heap_end;

    // Configure the newlib heap.
    fake_heap_start = inner_heap;
    fake_heap_end   = inner_heap + sizeof(inner_heap);
}

// Service initialization.
void __appInit(void)
{
    Result rc;

    // Open a service manager session.
    rc = smInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));

    // Retrieve the current version of Horizon OS.
    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion fw;
        rc = setsysGetFirmwareVersion(&fw);
        if (R_SUCCEEDED(rc))
            hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        setsysExit();
    }

    // Enable this if you want to use HID.
    /*rc = hidInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_HID));*/

    // Enable this if you want to use time.
    /*rc = timeInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_Time));

    __libnx_init_time();*/

    // Disable this if you don't want to use the filesystem.
    rc = fsInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_FS));

    // Disable this if you don't want to use the SD card filesystem.
    fsdevMountSdmc();

    // Add other services you want to use here.

    // TODO: change error code
    rc = dmntchtInitialize();
    if (R_FAILED(rc)) { diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM)); }
    rc = pmdmntInitialize();
    if (R_FAILED(rc)) { diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM)); }
    rc = pminfoInitialize();
    if (R_FAILED(rc)) { diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM)); }

    // Close the service manager session.
    smExit();
}

// Service deinitialization.
void __appExit(void)
{
    // Close extra services you added to __appInit here.
    fsdevUnmountAll(); // Disable this if you don't want to use the SD card filesystem.
    fsExit(); // Disable this if you don't want to use the filesystem.
    //timeExit(); // Enable this if you want to use time.
    //hidExit(); // Enable this if you want to use HID.

    pmdmntExit();
    pminfoExit();

    dmntchtExit();
}

#ifdef __cplusplus
}
#endif

/*
    boilerplate ends here
*/

const u64 SPLATOON_2_TID = 72123289858342912ull;

// get the current app's title id, or NULL
u64 getCurrentAppTID() {
    Result rc;

    u64 pid;
    rc = pmdmntGetApplicationProcessId(&pid);
    if (R_FAILED(rc)) { return 0; }
    u64 tid;
    rc = pminfoGetProgramId(&tid, pid);
    if (R_FAILED(rc)) { return 0; }
    return tid;
}

int main()
{
    FILE* tf = fopen("/_test/test_sysmodule_loaded", "w"); if (tf == NULL) return 1;
    fprintf(tf, "\n");
    fclose(tf);

    FILE* tf2 = fopen("/_test/log.txt", "a"); if (tf2 == NULL) return 1;
    fprintf(tf2, "[ log begin ]\n"); fflush(tf2);
    fclose(tf2);

    Result rc;

    int numDumps = 1;

    while (true) {
        FILE* enabled = fopen("/_test2/enabled.flag", "r");
        if (enabled != NULL) {
            FILE* f = fopen("/_test/log.txt", "a"); if (f == NULL) return 1;

            u64 tid = getCurrentAppTID();
            if (tid == SPLATOON_2_TID) {
                fprintf(f, "found splatoon 2\n"); fflush(f);
                bool hasChtProc;
                rc = dmntchtHasCheatProcess(&hasChtProc); if (R_FAILED(rc)) { return 1; }
                if (!hasChtProc) {
                    fprintf(f, "opening debugger\n"); fflush(f);
                    rc = dmntchtForceOpenCheatProcess(); if (R_FAILED(rc)) { return 1; }
                }
                DmntCheatProcessMetadata metadata;
                rc = dmntchtGetCheatProcessMetadata(&metadata); if (R_FAILED(rc)) { return 1; }

                // global variable containing a pointer to the "StaticMem" instance (containing current game state)
                u64 splatStaticMemInstAddr = metadata.main_nso_extents.base + 0x02D590F0;

                u64 staticMemPtr; // allocate 8 bytes
                rc = dmntchtReadCheatProcessMemory(splatStaticMemInstAddr, &staticMemPtr, 8); if (R_FAILED(rc)) return 1;
                if (staticMemPtr != 0) {
                    // staticMemPtr->mPlayerInfoAry (pointer to PlayerInfoAry obj)
                    u64 playerInfoAryPtr;
                    rc = dmntchtReadCheatProcessMemory(staticMemPtr+0x38, &playerInfoAryPtr, 8); if (R_FAILED(rc)) return 1;

                    if (playerInfoAryPtr != 0) {
                        u64 playerInfoPtr;
                        rc = dmntchtReadCheatProcessMemory(playerInfoAryPtr+0x8, &playerInfoPtr, 8); if (R_FAILED(rc)) return 1;

                        if (playerInfoPtr != 0) {
                            /* 
                                create bindump of all PlayerInfo objects onto sd card (uncomment to activate)
                            */

                            // fprintf(f, "dumping %d...\n", numDumps); fflush(f);
                            // int dumpSize = 448 * 10;
                            // unsigned char* dump = (unsigned char*) malloc(dumpSize); if (!dump) return 1;
                            // rc = dmntchtReadCheatProcessMemory(playerInfoPtr, dump, dumpSize); if (R_FAILED(rc)) return 1;
                            // std::stringstream path;
                            // path << "/_test/dump" << numDumps << ".bin";
                            // FILE* dumpFile = fopen(path.str().c_str(), "wb"); if (dumpFile == NULL) return 1;
                            // fwrite(dump, 1, dumpSize, dumpFile);
                            // fclose(dumpFile);
                            // free(dump); dump = nullptr;
                            // numDumps++;

                            /*
                                parse the color data and dump to log file
                            */

                            // 10 players max in a game, so 10 PlayerInfos
                            for (int i = 0; i < 10; i++) {
                                float r, g, b, a;
                                rc = dmntchtReadCheatProcessMemory(playerInfoPtr+(i*448)+0xcc, &r, 4); if (R_FAILED(rc)) return 1;
                                rc = dmntchtReadCheatProcessMemory(playerInfoPtr+(i*448)+0xcc+4, &g, 4); if (R_FAILED(rc)) return 1;
                                rc = dmntchtReadCheatProcessMemory(playerInfoPtr+(i*448)+0xcc+8, &b, 4); if (R_FAILED(rc)) return 1;
                                rc = dmntchtReadCheatProcessMemory(playerInfoPtr+(i*448)+0xcc+12, &a, 4); if (R_FAILED(rc)) return 1;

                                int r_i = (int)(r*256.0f);
                                if (r_i > 255) { r_i = 255; }
                                int g_i = (int)(g*256.0f);
                                if (g_i > 255) { g_i = 255; }
                                int b_i = (int)(b*256.0f);
                                if (b_i > 255) { b_i = 255; }
                                int a_i = (int)(a*256.0f);
                                if (a_i > 255) { a_i = 255; }

                                fprintf(f, "player %d: %d, %d, %d, %d\n", i+1, r_i, g_i, b_i, a_i); fflush(f);
                            }
                        }
                    }
                }
            }
            else {
                fprintf(f, "splat 2 not found\n");
            }
            fclose(f);
            fclose(enabled);
        }
        else {
            FILE* f = fopen("/_test/log.txt", "a"); if (f == NULL) return 1;
            fprintf(f, "/_test2/enabled.flag not found\n"); fflush(f);
            fclose(f);
        }

        svcSleepThread(5000000000ull); // 5 secs
    }
}
