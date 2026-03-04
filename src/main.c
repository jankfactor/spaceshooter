#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <kernel.h>
#include <swis.h>

#include "palette.h"
#include "math3d.h"
#include "mesh.h"
#include "cvector.h"
#include "render.h"
#include "utils.h"

// ASM Routines
extern void VDUSetup(void);
extern void UpdateMemAddress(int screenStart, int screenMax);
extern void ReserveScreenBanks(void);
extern void SwitchScreenBank(void);
extern void ClearScreen(int color, int fullclear);
extern int KeyPress(int keyCode);
extern void FillEdgeLists(int triList, int color);
extern void ProjectVertex(int vertexPtr);
extern int GetMonotonicTime(void);

// SWI access
_kernel_oserror *err;
_kernel_swi_regs rin, rout;

char *gBaseDirectoryPath = NULL;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

int main(int argc, char *argv[])
{
    int i = 0, j = 0, swi_data[10], isRunning = 1;
    int rollRate = 0, pitchRate = 0, delta = 0, lastTime = 0;
    V3D eyePos;
    V3D tmp, tmp2;
    V3D camRight, camUp, camForward;
    MAT43 mat;
    int mouseX = 0, mouseY = 0;
    unsigned char block[9];
    unsigned char *ptr;

    srand((unsigned int)time(NULL));

    SetupRender(1);
    SetupMathsGlobals(1);
    SetupPaletteLookup(1);

    for (i = 0; i < 1024; ++i)
    {
        g_SineTable[i] = float2fix(sinf((i * M_PI * 2.f) / 1024.f));
        g_oneOver[i] = (i == 0) ? float2fix(1.f) : float2fix(1.f / i);
    }

    gBaseDirectoryPath = getenv("Game$Dir");
    if (LoadOBJ("assets.ship_obj") != 0)
    {
        printf("Unable to load file. Please use the !Run script.\n");
        return 1;
    }

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

    eyePos.x = 100 << 16;
    eyePos.y = 0;
    eyePos.z = 0;

    camRight.x = 0;
    camRight.y = 0;
    camRight.z = int2fix(1);
    camUp.x = 0;
    camUp.y = int2fix(1);
    camUp.z = 0;
    camForward.x = -int2fix(1);
    camForward.y = 0;
    camForward.z = 0;

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
        delta = 0;
        lastTime = GetMonotonicTime();
        while (isRunning)
        {
            err = _kernel_swi(OS_Mouse, &rin, &rout); // Get the mouse position
            rollRate = clamp((mouseX - rout.r[0]) >> 7, -32, 32);
            pitchRate = clamp((mouseY - rout.r[1]) >> 7, -32, 32);

            // Apply roll: small-angle Minsky circle approximation.
            // cos(a) ~= 1 dropped; update camUp first, reuse it for camRight
            // (Minsky trick: using the intermediate value improves rotational stability).
            // Saves: 2 trig lookups (fixcos), 12 fixmults, 2 temp V3D structs vs full rotation.
            {
                const fix alpha = fixsin(rollRate);
                camUp.x -= fixmult(alpha, camRight.x);
                camUp.y -= fixmult(alpha, camRight.y);
                camUp.z -= fixmult(alpha, camRight.z);
                camRight.x += fixmult(alpha, camUp.x);
                camRight.y += fixmult(alpha, camUp.y);
                camRight.z += fixmult(alpha, camUp.z);
            }

            // Apply pitch: same small-angle Minsky approximation.
            // Update camUp first, reuse updated camUp for camForward.
            {
                const fix beta = fixsin(pitchRate);
                camUp.x -= fixmult(beta, camForward.x);
                camUp.y -= fixmult(beta, camForward.y);
                camUp.z -= fixmult(beta, camForward.z);
                camForward.x += fixmult(beta, camUp.x);
                camForward.y += fixmult(beta, camUp.y);
                camForward.z += fixmult(beta, camUp.z);
            }

            // Re-orthogonalize periodically (Gram-Schmidt, forward is primary axis).
            // Minsky rotation is self-stabilizing so every-frame correction is unnecessary;
            // amortize the cost of two floating-point Normalize calls across 64 frames.
            if ((j % 256) == 0)
            {
                fix d;
                Normalize(&camForward);
                d = DotProduct(&camRight, &camForward);
                camRight.x -= fixmult(d, camForward.x);
                camRight.y -= fixmult(d, camForward.y);
                camRight.z -= fixmult(d, camForward.z);
                Normalize(&camRight);
                camUp = CrossProductV3D(&camForward, &camRight);

                g_Mesh.rollPerFrame = rand() % 5 - 2;
                g_Mesh.pitchPerFrame = rand() % 5 - 2;
            }

            if (KeyPress(KEY_ESCAPE)) // Escape
                isRunning = 0;
            else if (KeyPress(KEY_A)) // A - Strafe left
            {
                eyePos.x -= camRight.x >> 1;
                eyePos.y -= camRight.y >> 1;
                eyePos.z -= camRight.z >> 1;
            }
            else if (KeyPress(KEY_D)) // D - Strafe right
            {
                eyePos.x += camRight.x >> 1;
                eyePos.y += camRight.y >> 1;
                eyePos.z += camRight.z >> 1;
            }
            else if (KeyPress(KEY_W)) // W - Strafe up
            {
                eyePos.x += camUp.x >> 1;
                eyePos.y += camUp.y >> 1;
                eyePos.z += camUp.z >> 1;
            }
            else if (KeyPress(KEY_S)) // S - Strafe down
            {
                eyePos.x -= camUp.x >> 1;
                eyePos.y -= camUp.y >> 1;
                eyePos.z -= camUp.z >> 1;
            }

            // if (rout.r[2] & 4) // Left mouse button - Thrust forward
            // {
            //     eyePos.x += camForward.x << 1;
            //     eyePos.y += camForward.y << 1;
            //     eyePos.z += camForward.z << 1;
            // }
            // else if (rout.r[2] & 1) // Right mouse button - Thrust backward
            // {
            //     eyePos.x -= camForward.x >> 1;
            //     eyePos.y -= camForward.y >> 1;
            //     eyePos.z -= camForward.z >> 1;
            // }

            SwitchScreenBank();             // Swap draw buffer with display buffer
            rin.r[0] = (int)(&swi_data[0]); // Get the new screen start address
            rin.r[1] = (int)(&swi_data[3]); // Results
            err = _kernel_swi(OS_ReadVduVariables, &rin, &rout);
            UpdateMemAddress(swi_data[3], swi_data[4]); // Pass these to the ASM side

            // Build view matrix from camera orientation vectors
            mat.m11 = camRight.x;
            mat.m12 = camRight.y;
            mat.m13 = camRight.z;
            mat.m21 = camUp.x;
            mat.m22 = camUp.y;
            mat.m23 = camUp.z;
            mat.m31 = camForward.x;
            mat.m32 = camForward.y;
            mat.m33 = camForward.z;
            mat.tx = -DotProduct(&camRight, &eyePos);
            mat.ty = -DotProduct(&camUp, &eyePos);
            mat.tz = -DotProduct(&camForward, &eyePos);

            ClearScreen(0x0E0E0E0E, 0); // Clear the new draw buffer

            int quantEyeX = eyePos.x >> 8;
            int quantEyeY = eyePos.y >> 8;
            int quantEyeZ = eyePos.z >> 8;

            ptr = (unsigned char *)(swi_data[3]);

            RenderStarfield(&mat, eyePos, ptr);
            RenderModel(&mat, &g_Mesh, delta);

            // printf("DISt     :  %d", dist);
            delta = max(GetMonotonicTime() - lastTime, 1);
            lastTime = GetMonotonicTime();

            g_Mesh.eulers.x += g_Mesh.pitchPerFrame * delta;
            g_Mesh.eulers.z += g_Mesh.rollPerFrame * delta;

            if (!(rout.r[2] & 1)) // Right mouse button not pressed - Thrust forward
            {
                eyePos.x += fixmult(camForward.x, 20000) * delta;
                eyePos.y += fixmult(camForward.y, 20000) * delta;
                eyePos.z += fixmult(camForward.z, 20000) * delta;
            }

            ++j;
            // rin.r[0] = 30;
            // err = _kernel_swi(OS_WriteC, &rin, &rout);
            // printf("\nMONOTONIC TIME : %d", GetMonotonicTime());

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

    SetupMathsGlobals(0);
    FreeMesh();
    SetupRender(0);
    SetupPaletteLookup(0);
    printf("Forward: %d, %d, %d\n", camForward.x, camForward.y, camForward.z);
    printf("Eyepos: %d, %d, %d\n", eyePos.x, eyePos.y, eyePos.z);

    // (void)getchar(); // Uncomment to pause here and read data output

    return 0;
}
