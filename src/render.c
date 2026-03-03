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

#define NUM_STARS 256
#define rand32(max) (rand() % (max))
#define rand32balanced(max) ((rand() % (max)) - ((max) >> 1))

V3D starfield[NUM_STARS];
const unsigned char colors[16] = {128, 131, 136, 169, 170, 171, 161, 172, 205, 221, 222, 219, 220, 247, 246, 242};

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
    int i;

    memset(g_RenderQueue, 0, MAXDEPTH * sizeof(TRI *));
    if (allocating)
    {
        cvector_reserve(gEdgeList, 256);
        EdgeList = (unsigned int)(gEdgeList); // For ASM access

        for (i = 0; i < NUM_STARS; ++i)
        {
            starfield[i].x = (rand32(0xFFFF));
            starfield[i].y = (rand32(0xFFFF));
            starfield[i].z = (rand32(0xFFFF));
        }
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

void RenderStarfield(MAT43 *viewMat, V3D eyePos, unsigned char *ptr)
{
    V3D tmp, tmp2;
    V3D *vPtr;
    int i;

    vPtr = &starfield[0];

    eyePos.x >>= 8;
    eyePos.y >>= 8;
    eyePos.z >>= 8;

    for (i = 0; i < NUM_STARS; ++i)
    {
        tmp2.x = ((vPtr->x - eyePos.x) & 0xFFFF) - 0x8000;
        tmp2.y = ((vPtr->y - eyePos.y) & 0xFFFF) - 0x8000;
        tmp2.z = ((vPtr->z - eyePos.z) & 0xFFFF) - 0x8000;       
        vPtr++;

        MultV3DMat_NoTranslate(&tmp2, &tmp, viewMat);
        if (tmp.z <= 0)
            continue;

        ProjectVertex((int)(&tmp));

        if (tmp.x < 0 || tmp.x >= SCREEN_W || tmp.y < 0 || tmp.y >= SCREEN_H)
            continue;

        *(ptr + (tmp.y * SCREEN_W) + tmp.x) = 242;//colors[15 - min(tmp.z >> 12, 15)];
    }
}

void RenderModel(MAT43 *viewMat, Mesh *mesh, int delta)
{
    MAT43 modelMat, modelViewMat;
    V3D _verts[4], tmpVec;
    V3D *vPtr;
    int i;

    // Build local model matrix from mesh eulers and position
    EulerToMat(&modelMat, mesh->eulers.x, mesh->eulers.y, mesh->eulers.z);

    // Advance mesh along its forward vector (3rd column of rotation matrix)
    mesh->position.x -= fixmult(modelMat.m13, mesh->speed * delta);
    mesh->position.y -= fixmult(modelMat.m23, mesh->speed * delta);
    mesh->position.z -= fixmult(modelMat.m33, mesh->speed * delta);

    modelMat.tx = mesh->position.x;
    modelMat.ty = mesh->position.y;
    modelMat.tz = mesh->position.z;

    // Combine model and view into a single matrix (column-vector: viewMat * modelMat)
    MultMatMat(&modelViewMat, viewMat, &modelMat);

    // Transform vertices: object space -> view space in one step
    for (i = 0; i < cvector_size(mesh->verts); ++i)
    {
        vPtr = &mesh->verts_transformed[i];
        MultV3DMat(&mesh->verts[i], vPtr, &modelViewMat);
        ProjectVertex((int)(vPtr)); // Perspective projection (skips if Z <= 0)
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

        if (orient2dint(_verts[0], _verts[1], _verts[2]) > 0)
        {
            // Rotate face normal by model rotation for correct lighting.
            // Use fixmult (not fixmult_lessThanOne) to avoid 32-bit overflow
            // when rotation matrix entries or normal components reach 1.0.
            const V3D *n = &mesh->faces[i].normal;
            tmpVec.x = fixmult(n->x, modelMat.m11) + fixmult(n->y, modelMat.m12) + fixmult(n->z, modelMat.m13);
            tmpVec.y = fixmult(n->x, modelMat.m21) + fixmult(n->y, modelMat.m22) + fixmult(n->z, modelMat.m23);
            tmpVec.z = fixmult(n->x, modelMat.m31) + fixmult(n->y, modelMat.m32) + fixmult(n->z, modelMat.m33);

            mesh->faces[i].d = (mesh->faces[i].d & 0xFFFFFF80) + clamp((tmpVec.z >> 10), 0, 63);
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
