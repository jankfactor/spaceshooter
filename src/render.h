#ifndef RENDER_H
#define RENDER_H

#define SCREEN_W 320
#define SCREEN_H 200
#define MAXDEPTH 256

#include "math3d.h"
#include "mesh.h"

void SetupRender(int allocating);
void RenderStarfield(MAT43 *viewMat, V3D eyePos, unsigned char *ptr);
void RenderModel(MAT43 *viewMat, Mesh *mesh, V3D *outModelPos, int flash);
void ExplodingModel(MAT43 *viewMat, Mesh *mesh, V3D *outModelPos, int offset);

extern TRI *g_RenderQueue[MAXDEPTH];
extern TRI *queuePtr;

// Uncomment the following to enable the timing log.
// #define TIMING_LOG 1

#ifdef TIMING_LOG
typedef struct TimerLog
{
    int transformTiles;
    int submitRenderTriangles;
    int clippingQueue;
    int project3D;
    int sceneRender;
    int biggestVertex;
    int clippedCount;
} TimerLog;

extern TimerLog gTimerLog;

// The following SWIs are for David Ruck's TimerMod.
// Which can be found at https://armclub.org.uk/free/
#define SWI_Timer_Start 0x000490C0
#define SWI_Timer_Stop 0x000490C1
#define SWI_Timer_Value 0x000490C2

int GetRenderDelta(void);

#endif // TIMING_LOG

#endif // RENDER_H
