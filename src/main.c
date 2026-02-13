#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <kernel.h>
#include <swis.h>

#include "palette.h"
#include "math3d.h"
#include "cvector.h"

#define rand32(max) (rand() % (max))
#define rand32balanced(max) ((rand() % (max)) - ((max) >> 1))
#define SCREEN_W 320
#define SCREEN_H 256
#define NUM_STARS 128

// ASM Routines
extern void VDUSetup(void);
extern void UpdateMemAddress(int screenStart, int screenMax);
extern void ReserveScreenBanks(void);
extern void SwitchScreenBank(void);
extern void ClearScreen(int color, int fullclear);
extern int KeyPress(int keyCode);
extern void FillEdgeLists(int triList, int color);
extern void ProjectVertex(int vertexPtr);

// SWI access
_kernel_oserror *err;
_kernel_swi_regs rin, rout;

char *gBaseDirectoryPath = NULL;
int *gEdgeList = NULL;
extern unsigned int EdgeList;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

V3D starfield[NUM_STARS];

int main(int argc, char *argv[])
{
    int i, swi_data[10], isRunning = 1;
    int heading = 498, pitch = -53, angle = 0;
    V3D eyePos, direction;
    V3D tmp, tmp2;
    MAT43 mat;
    int mouseX, mouseY;
    unsigned char block[9];
    unsigned char *ptr;

    srand((unsigned int)time(NULL));
    for (i = 0; i < NUM_STARS; ++i)
    {
        starfield[i].x = (rand32(0xFFFF));
        starfield[i].y = (rand32(0xFFFF));
        starfield[i].z = (rand32(0xFFFF));

        // printf("Star %d: X=%d Y=%d Z=%d\n\r", i, starfield[i].x, starfield[i].y, starfield[i].z);
    }

    SetupMathsGlobals(1);

    for (i = 0; i < 1024; ++i)
    {
        g_SineTable[i] = float2fix(sinf((i * M_PI * 2.f) / 1024.f));
        g_oneOver[i] = (i == 0) ? float2fix(1.f) : float2fix(1.f / i);
    }

    // (void)getchar(); // Uncomment to pause here and read data output
    // cvector_reserve(gEdgeList, 256);
    // EdgeList = (unsigned int)(gEdgeList); // For ASM access

    SetupPaletteLookup(1);

    gBaseDirectoryPath = getenv("Game$Dir");
    if (LoadFogLookup() != 0)
    {
        printf("ERROR: Failed to load fog lookup table.\n");
        return 1;
    }

    // Disable the default escape handler
    rin.r[0] = 229;
    rin.r[1] = 0xFFFFFFFF;
    err = _kernel_swi(OS_Byte, &rin, &rout);

    VDUSetup();
    ReserveScreenBanks();
    SwitchScreenBank();
    SetPalette();
    // Save256(); // Uncomment to save the VIDC generated palette to a file

    // Obtain details about the current screen mode
    swi_data[0] = (int)148;         // screen base address
    swi_data[1] = (int)-1;          // terminate query
    rin.r[0] = (int)(&swi_data[0]); // Start of query
    rin.r[1] = (int)(&swi_data[3]); // Results
    err = _kernel_swi(OS_ReadVduVariables, &rin, &rout);
    UpdateMemAddress(swi_data[3], swi_data[4]);

    for (i = 0; i < 2; ++i)
    {
        SwitchScreenBank();             // Swap draw buffer with display buffer
        rin.r[0] = (int)(&swi_data[0]); // Get the new screen start address
        rin.r[1] = (int)(&swi_data[3]); // Results
        err = _kernel_swi(OS_ReadVduVariables, &rin, &rout);
        UpdateMemAddress(swi_data[3], swi_data[4]); // Pass these to the ASM side
        ClearScreen(0, 1);                          // Clear the new draw buffer
    }

    eyePos.x = 0;
    eyePos.y = 0;
    eyePos.z = 0;

#ifdef TIMING_LOG
    gTimerLog.biggestVertex = 0;
#endif // TIMING_LOG

    // Set an infinite screen box so the mouse doesn't stop at the screen edge.
    block[0] = 0x01; // Reason code
    block[1] = 0x00; // Left LSB
    block[2] = 0x80; // Left MSB
    block[3] = 0x00; // Bottom LSB
    block[4] = 0x80; // Bottom MSB
    block[5] = 0xFF; // Right LSB
    block[6] = 0x7F; // Right MSB
    block[7] = 0xFF; // Top LSB
    block[8] = 0x7F; // Top MSB
    rin.r[0] = 21;
    rin.r[1] = (int)(&block[0]);
    err = _kernel_swi(OS_Word, &rin, &rout);

    // Get initial mouse position
    err = _kernel_swi(OS_Mouse, &rin, &rout);
    mouseX = rout.r[0];
    mouseY = rout.r[1];

    if (err == NULL)
    {
        while (isRunning)
        {
            err = _kernel_swi(OS_Mouse, &rin, &rout); // Get the mouse position
            heading += clamp((rout.r[0] - mouseX) >> 7, -32, 32);
            pitch += clamp((rout.r[1] - mouseY) >> 7, -32, 32);
            pitch = clamp(pitch, -120, 120);

            if (KeyPress(112)) // Escape
                isRunning = 0;

            if (rout.r[2] & 4) // Left mouse button - Walk forward
            {
                // --angle;
                eyePos.x += (fixcos(heading)) << 1;
                eyePos.z -= (fixsin(heading)) << 1;
                eyePos.y += (fixsin(pitch)) << 1;
            }
            if (rout.r[2] & 1) // Right mouse button - Walk backward
            {
                // ++angle;
                eyePos.x -= (fixcos(heading));
                eyePos.z += (fixsin(heading));
                eyePos.y -= (fixsin(pitch)) << 1;
            }

            SwitchScreenBank();             // Swap draw buffer with display buffer
            rin.r[0] = (int)(&swi_data[0]); // Get the new screen start address
            rin.r[1] = (int)(&swi_data[3]); // Results
            err = _kernel_swi(OS_ReadVduVariables, &rin, &rout);
            UpdateMemAddress(swi_data[3], swi_data[4]); // Pass these to the ASM side

            direction.x = fixcos(heading);
            direction.y = fixsin(pitch);
            direction.z = -fixsin(heading);

            LookAt(&eyePos, &direction, &mat); // TODO - SLOWWWWWW - 2 cross products in here

#ifdef PAL_256
            ClearScreen(0x0, 1); // Clear the new draw buffer
#else
            ClearScreen(0x0, 1); // Clear the new draw buffer
#endif // PAL_256

            int quantEyeX = eyePos.x >> 8;
            int quantEyeY = eyePos.y >> 8;
            int quantEyeZ = eyePos.z >> 8;

            for (i = 0; i < NUM_STARS; ++i)
            {
                // Wrap star position relative to eye into -64..+63 range (128 unit cube)
                tmp2.x = starfield[i].x;
                tmp2.x -= quantEyeX;
                tmp2.x &= 0xFFFF;
                tmp2.x -= 0x8000;
                tmp2.y = starfield[i].y;
                tmp2.y -= quantEyeY;
                tmp2.y &= 0xFFFF;
                tmp2.y -= 0x8000;
                tmp2.z = starfield[i].z;
                tmp2.z -= quantEyeZ;
                tmp2.z &= 0xFFFF;
                tmp2.z -= 0x8000;
                MultV3DMat_NoTranslate(&tmp2, &tmp, &mat);
                if (tmp.z <= 0)
                    continue;

                ProjectVertex((int)(&tmp));

                if (tmp.x < 0 || tmp.x >= SCREEN_W || tmp.y < 0 || tmp.y >= SCREEN_H)
                    continue;

                ptr = (unsigned char *)(swi_data[3]);
                ptr += (tmp.y * SCREEN_W) + tmp.x;
                *ptr = 255; // White star
            }

            // RenderModel(&mat, &eyePos, heading); // Main render

#ifdef TIMING_LOG
            {
                rin.r[0] = 30;
                err = _kernel_swi(OS_WriteC, &rin, &rout);

                // printf("DISt     :  %d", dist);
                printf("\nTRANSFORM TILES : %d", gTimerLog.transformTiles);
                printf("\nSUBMIT TRIANGLES: %d", gTimerLog.submitRenderTriangles);
                printf("\nCLIPPING QUEUE  : %d", gTimerLog.clippingQueue);
                printf("\n3D PROJECTION   : %d", gTimerLog.project3D);
                printf("\nSCENE RENDER    : %d", gTimerLog.sceneRender);
                printf("\nBIGGEST VERTEX  : %d", gTimerLog.biggestVertex);
                printf("\nCLIPPED COUNT   : %d", gTimerLog.clippedCount);
            }
#endif // TIMING_LOG
        }
    }
    else
    {
        printf("ERROR: %s", err->errmess);
    }

    // Return to text mode
    rin.r[0] = 22;
    err = _kernel_swi(OS_WriteC, &rin, &rout);
    rin.r[0] = 0;
    err = _kernel_swi(OS_WriteC, &rin, &rout);

    // Re-enable the default escape handler
    rin.r[0] = 229;
    rin.r[1] = 0xFFFFFFFF;
    err = _kernel_swi(OS_Byte, &rin, &rout);

    // Free up memory that was allocated
    cvector_free(gEdgeList);
    SetupMathsGlobals(0);
    SetupPaletteLookup(0);
    printf("Heading: %d, Pitch: %d\n", heading, pitch);
    printf("Eyepos: %d, %d, %d\n", eyePos.x, eyePos.y, eyePos.z);

    // (void)getchar(); // Uncomment to pause here and read data output

    return 0;
}
