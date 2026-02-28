#include <kernel.h>

#include "render.h"
#include "math3d.h"
#include "mesh.h"
#include "palette.h"

extern void FillEdgeLists(int triList, int color);
extern void ProjectVertex(int vertexPtr);

TRI *g_RenderQueue[MAXDEPTH];
TRI *queuePtr = NULL;
int *gEdgeList = NULL;
extern unsigned int EdgeList;

int gDebug = 0;

#define MAX_EXTRA_VERTS 192
#define MAX_EXTRA_TRIS 64

TRI *clippedQueue1 = 0;
V3D clippedNearVerts[MAX_EXTRA_VERTS];
int clippedNearVertIndex = 0;
TRI clippedNearTris[MAX_EXTRA_TRIS];
int clippedNearTrisIndex = 0;

#ifdef TIMING_LOG
TimerLog gTimerLog;

int GetRenderDelta(void)
{
    _kernel_oserror *err;
    _kernel_swi_regs rin, rout;
    int deltaTime = 0;

    err = _kernel_swi(SWI_Timer_Stop, &rin, &rout);
    deltaTime = rout.r[1];
    err = _kernel_swi(SWI_Timer_Start, &rin, &rout);

    return deltaTime;
}
#endif // TIMING_LOG

void SetupRender(int allocating)
{
    memset(g_RenderQueue, 0, MAXDEPTH * sizeof(TRI *));
    if (allocating)
    {
        cvector_reserve(gEdgeList, 256);
        EdgeList = (unsigned int)(gEdgeList); // For ASM access
    }
    else
    {
        // Free up memory that was allocated
        cvector_free(gEdgeList);
    }
}

#define interpolate(a, b)                                         \
    do                                                            \
    {                                                             \
        t = ((a)->z - (b)->z) >> 8;                               \
        if (t != 0)                                               \
        {                                                         \
            t = ((a)->z << 8) / t;                                \
            tmp.x = (((b)->x - (a)->x) >> 8) * (t >> 8) + (a)->x; \
            tmp.y = (((b)->y - (a)->y) >> 8) * (t >> 8) + (a)->y; \
            tmp.z = 0;                                            \
            inside.verts[inside.numVerts++] = tmp;                \
        }                                                         \
        else                                                      \
        {                                                         \
            (a)->z = 0;                                           \
        }                                                         \
    } while (0);

// void ClipPolyonListToNearPlane(POLYGON *p)
// {
//     static POLYGON inside;
//     static V3D *previous, *current;
//     static V3D tmp;
//     register int i, t;

//     inside.numVerts = 0;
//     previous = &p->verts[p->numVerts - 1];

//     for (i = 0; i < p->numVerts; ++i)
//     {
//         current = &p->verts[i];

//         // Have we crossed into near plane?
//         if ((previous->z ^ current->z) & 0x80000000)
//         {
//             if (previous->z < 0)
//             {
//                 interpolate(current, previous);
//             }
//             else
//             {
//                 interpolate(previous, current);
//             }
//         }

//         if (current->z >= 0)
//             inside.verts[inside.numVerts++] = *current;

//         previous = current;
//     }

//     *p = inside;
// }

void RenderModel(MAT43 *viewMat, Mesh *mesh)
{
    MAT43 modelMat;
    V3D _verts[4], tmpVec, worldPos;
    V3D *vPtr;
    int i;

    // Build local model matrix from mesh eulers and position
    EulerToMat(&modelMat, mesh->eulers.x, mesh->eulers.y, mesh->eulers.z);
    modelMat.tx = mesh->position.x;
    modelMat.ty = mesh->position.y;
    modelMat.tz = mesh->position.z;

    // Transform vertices: object space -> world space -> view space
    for (i = 0; i < cvector_size(mesh->verts); ++i)
    {
        vPtr = &mesh->verts_transformed[i];
        MultV3DMat(&mesh->verts[i], &worldPos, &modelMat); // Local rotation + world position
        MultV3DMat(&worldPos, vPtr, viewMat);               // World -> view space
        ProjectVertex((int)(vPtr));                         // Perspective projection (skips if Z <= 0)
    }

    // Backface cull and insert visible faces into the depth-sorted render queue
    for (i = 0; i < cvector_size(mesh->faces); ++i)
    {
        mesh->faces[i].next = NULL;
        _verts[0] = mesh->verts_transformed[mesh->faces[i].a];
        _verts[1] = mesh->verts_transformed[mesh->faces[i].b];
        _verts[2] = mesh->verts_transformed[mesh->faces[i].c];

        // Skip faces with any vertex behind the camera (not projected)
        if (_verts[0].z <= 0 || _verts[1].z <= 0 || _verts[2].z <= 0)
            continue;

        if (orient2dint(_verts[0], _verts[1], _verts[2]) >= 0)
        {
            // Rotate face normal by model rotation for correct lighting.
            // Use fixmult (not fixmult_lessThanOne) to avoid 32-bit overflow
            // when rotation matrix entries or normal components reach 1.0.
            const V3D *n = &mesh->faces[i].normal;
            tmpVec.x = fixmult(n->x, modelMat.m11) + fixmult(n->y, modelMat.m12) + fixmult(n->z, modelMat.m13);
            tmpVec.y = fixmult(n->x, modelMat.m21) + fixmult(n->y, modelMat.m22) + fixmult(n->z, modelMat.m23);
            tmpVec.z = fixmult(n->x, modelMat.m31) + fixmult(n->y, modelMat.m32) + fixmult(n->z, modelMat.m33);

            // Add face to the destination list if it is facing us
            mesh->faces[i].d = (256 << 6) + clamp((tmpVec.x >> 10), 0, 63);
            // Important to invert the depth here as the camera looks into -Z
            // but our g_RenderQueue is indexed by positive numbers
            mesh->faces[i].depth = min((_verts[0].z + _verts[1].z + _verts[2].z) >> 8, MAXDEPTH - 1);

            // Push the previous triangle (if any) onto the stack for this depth.
            mesh->faces[i].next = g_RenderQueue[mesh->faces[i].depth];
            g_RenderQueue[mesh->faces[i].depth] = &mesh->faces[i];
        }
    }

    // Painter's algorithm. Proceed from furthest to nearest.
    for (i = MAXDEPTH - 1; i >= 0; i--)
    {
        while (g_RenderQueue[i])
        { // Render faces with current depth
            queuePtr = g_RenderQueue[i];
            _verts[0] = mesh->verts_transformed[queuePtr->a];
            _verts[1] = mesh->verts_transformed[queuePtr->b];
            _verts[2] = mesh->verts_transformed[queuePtr->c];

            FillEdgeLists((unsigned int)(&_verts[0]), queuePtr->d);

            g_RenderQueue[i] = (TRI *)queuePtr->next; // Next face
        }
    }
}
