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

#define NUM_STARS 128
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
        cvector_reserve(gEdgeList, SCREEN_H);
        EdgeList = (unsigned int)(gEdgeList); // For ASM access

        for (i = 0; i < NUM_STARS; ++i)
        {
            starfield[i].x = (rand32(0x7FFF));
            starfield[i].y = (rand32(0x7FFF));
            starfield[i].z = (rand32(0x7FFF));
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

    eyePos.x >>= 7;
    eyePos.y >>= 7;
    eyePos.z >>= 7;

    for (i = 0; i < NUM_STARS; ++i)
    {
        tmp2.x = ((vPtr->x - eyePos.x) & 0x7FFF) - 0x4000;
        tmp2.y = ((vPtr->y - eyePos.y) & 0x7FFF) - 0x4000;
        tmp2.z = ((vPtr->z - eyePos.z) & 0x7FFF) - 0x4000;
        vPtr++;

        MultV3DMat_NoTranslate(&tmp2, &tmp, viewMat);
        if (tmp.z <= 0)
            continue;

        ProjectVertex((int)(&tmp));

        if (tmp.x < 0 || tmp.x >= SCREEN_W || tmp.y < 0 || tmp.y >= SCREEN_H)
            continue;

        *(ptr + (tmp.y * SCREEN_W) + tmp.x) = 242; // colors[15 - min(tmp.z >> 12, 15)];
    }
}

void RenderModel(MAT43 *viewMat, Mesh *mesh, V3D *outModelPos, int flash)
{
    MAT43 modelMat, modelViewMat;
    V3D _verts[4], tmpVec;
    V3D *vPtr;
    int i;
    size_t vec_i;

    // Build local model matrix from mesh eulers and position
    EulerToMat(&modelMat, mesh->eulers.x, mesh->eulers.y, mesh->eulers.z);

    // Cache the forward vector (3rd column) for position update after rendering
    mesh->forward.x = modelMat.m13;
    mesh->forward.y = modelMat.m23;
    mesh->forward.z = modelMat.m33;

    modelMat.tx = mesh->position.x;
    modelMat.ty = mesh->position.y;
    modelMat.tz = mesh->position.z;

    // Combine model and view into a single matrix (column-vector: viewMat * modelMat)
    MultMatMat(&modelViewMat, viewMat, &modelMat);

    // Get the position of the model in view space for the radar
    if (outModelPos)
    {
        outModelPos->x = modelViewMat.tx;
        outModelPos->y = modelViewMat.ty;
        outModelPos->z = modelViewMat.tz;
    }

    // Transform vertices: object space -> view space in one step
    for (vec_i = 0; vec_i < cvector_size(mesh->verts); ++vec_i)
    {
        vPtr = &mesh->verts_transformed[vec_i];
        MultV3DMat(&mesh->verts[vec_i], vPtr, &modelViewMat);
        ProjectVertex((int)(vPtr)); // Perspective projection (skips if Z <= 0)
    }

    // Backface cull and insert visible faces into the depth-sorted render queue
    for (vec_i = 0; vec_i < cvector_size(mesh->faces); ++vec_i)
    {
        mesh->faces[vec_i].next = NULL;
        _verts[0] = mesh->verts_transformed[mesh->faces[vec_i].a];
        _verts[1] = mesh->verts_transformed[mesh->faces[vec_i].b];
        _verts[2] = mesh->verts_transformed[mesh->faces[vec_i].c];

        // Skip faces with any vertex behind the camera (not projected)
        if (_verts[0].z <= 0 || _verts[1].z <= 0 || _verts[2].z <= 0)
            continue;

        if (orient2dint(_verts[0], _verts[1], _verts[2]) > 0)
        {
            // Rotate face normal by model rotation for correct lighting.
            // Use fixmult (not fixmult_lessThanOne) to avoid 32-bit overflow
            // when rotation matrix entries or normal components reach 1.0.
            const V3D *n = &mesh->faces[vec_i].normal;
            tmpVec.x = fixmult(n->x, modelMat.m11) + fixmult(n->y, modelMat.m12) + fixmult(n->z, modelMat.m13);
            tmpVec.y = fixmult(n->x, modelMat.m21) + fixmult(n->y, modelMat.m22) + fixmult(n->z, modelMat.m23);
            tmpVec.z = fixmult(n->x, modelMat.m31) + fixmult(n->y, modelMat.m32) + fixmult(n->z, modelMat.m33);
            mesh->faces[vec_i].d = (mesh->faces[vec_i].d & 0xFFFFFF80) + clamp((tmpVec.z >> 10), 0, 63);
            mesh->faces[vec_i].depth = min((_verts[0].z + _verts[1].z + _verts[2].z) >> 8, MAXDEPTH - 1);

            // Push the previous triangle (if any) onto the stack for this depth.
            mesh->faces[vec_i].next = g_RenderQueue[mesh->faces[vec_i].depth];
            g_RenderQueue[mesh->faces[vec_i].depth] = &mesh->faces[vec_i];
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

            FillEdgeLists((unsigned int)(&_verts[0]), flash ? ((flash & 1) ? 63 : 0) : queuePtr->d);

            g_RenderQueue[i] = (TRI *)queuePtr->next; // Next face
        }
    }
}

void ExplodingModel(MAT43 *viewMat, Mesh *mesh, V3D *outModelPos, int offset)
{
    MAT43 modelMat, modelViewMat;
    V3D _verts[4], tmpVec, tmpVec2;
    V3D *vPtr;
    int i;
    size_t vec_i;

    // Build local model matrix from mesh eulers and position
    EulerToMat(&modelMat, mesh->eulers.x, mesh->eulers.y, mesh->eulers.z);

    // Cache the forward vector (3rd column) for position update after rendering
    mesh->forward.x = modelMat.m13;
    mesh->forward.y = modelMat.m23;
    mesh->forward.z = modelMat.m33;

    modelMat.tx = mesh->position.x;
    modelMat.ty = mesh->position.y;
    modelMat.tz = mesh->position.z;

    // Combine model and view into a single matrix (column-vector: viewMat * modelMat)
    MultMatMat(&modelViewMat, viewMat, &modelMat);

    // Get the position of the model in view space for the radar
    if (outModelPos)
    {
        outModelPos->x = modelViewMat.tx;
        outModelPos->y = modelViewMat.ty;
        outModelPos->z = modelViewMat.tz;
    }

    // Transform vertices: object space -> view space in one step
    for (vec_i = 0; vec_i < cvector_size(mesh->verts); ++vec_i)
    {
        vPtr = &mesh->verts_transformed[vec_i];
        MultV3DMat(&mesh->verts[vec_i], vPtr, &modelViewMat);
        ProjectVertex((int)(vPtr)); // Perspective projection (skips if Z <= 0)
    }

    // Backface cull and insert visible faces into the depth-sorted render queue
    for (vec_i = 0; vec_i < cvector_size(mesh->faces); ++vec_i)
    {
        mesh->faces[vec_i].next = NULL;
        _verts[0] = mesh->verts_transformed[mesh->faces[vec_i].a];
        _verts[1] = mesh->verts_transformed[mesh->faces[vec_i].b];
        _verts[2] = mesh->verts_transformed[mesh->faces[vec_i].c];

        // Offset by the first face normal
        const V3D *n = &mesh->faces[0].normal;

        _verts[2].z += fixmult(n->z, offset);

        // Skip faces with any vertex behind the camera (not projected)
        if (_verts[0].z <= 0 || _verts[1].z <= 0 || _verts[2].z <= 0)
            continue;

        //if (orient2dint(_verts[0], _verts[1], _verts[2]) > 0)
        {
            // Rotate face normal by model rotation for correct lighting.
            // Use fixmult (not fixmult_lessThanOne) to avoid 32-bit overflow
            // when rotation matrix entries or normal components reach 1.0.
            const V3D *n = &mesh->faces[vec_i].normal;
            tmpVec.x = fixmult(n->x, modelMat.m11) + fixmult(n->y, modelMat.m12) + fixmult(n->z, modelMat.m13);
            tmpVec.y = fixmult(n->x, modelMat.m21) + fixmult(n->y, modelMat.m22) + fixmult(n->z, modelMat.m23);
            tmpVec.z = fixmult(n->x, modelMat.m31) + fixmult(n->y, modelMat.m32) + fixmult(n->z, modelMat.m33);
            mesh->faces[vec_i].d = (mesh->faces[vec_i].d & 0xFFFFFF80) + clamp((tmpVec.z >> 10), 0, 63);
            mesh->faces[vec_i].depth = min((_verts[0].z + _verts[1].z + _verts[2].z) >> 8, MAXDEPTH - 1);

            // Push the previous triangle (if any) onto the stack for this depth.
            mesh->faces[vec_i].next = g_RenderQueue[mesh->faces[vec_i].depth];
            g_RenderQueue[mesh->faces[vec_i].depth] = &mesh->faces[vec_i];
        }
    }


    // Project the object center to screen space for outward displacement
    tmpVec.x = modelViewMat.tx;
    tmpVec.y = modelViewMat.ty;
    tmpVec.z = modelViewMat.tz;
    ProjectVertex((int)&tmpVec);

    // Painter's algorithm. Proceed from furthest to nearest.
    for (i = MAXDEPTH - 1; i >= 0; i--)
    {
        while (g_RenderQueue[i])
        { // Render faces with current depth
            queuePtr = g_RenderQueue[i];

            _verts[0] = mesh->verts_transformed[queuePtr->a];
            tmpVec2.x = _verts[0].x - tmpVec.x;
            tmpVec2.y = _verts[0].y - tmpVec.y;
            _verts[0].x += (tmpVec2.x * offset) >> 12;
            _verts[0].y += (tmpVec2.y * offset) >> 12;

            _verts[1] = mesh->verts_transformed[queuePtr->b];
            _verts[1].x += (tmpVec2.x * offset) >> 12;
            _verts[1].y += (tmpVec2.y * offset) >> 12;

            _verts[2] = mesh->verts_transformed[queuePtr->c];
            _verts[2].x += (tmpVec2.x * offset) >> 12;
            _verts[2].y += (tmpVec2.y * offset) >> 12;

            FillEdgeLists((unsigned int)(&_verts[0]), queuePtr->d);

            g_RenderQueue[i] = (TRI *)queuePtr->next; // Next face
        }
    }
}
