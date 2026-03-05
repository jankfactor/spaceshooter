#ifndef MESH_H
#define MESH_H

#include "math3d.h"
#include "cvector.h"

typedef struct Mesh
{
    V3D position;
    V3D eulers;
    V3D forward;  // cached forward vector (3rd column of model rotation matrix)
    fix speed;
    fix rollPerFrame;
    fix pitchPerFrame;

    cvector_vector_type(V3D) verts;
    cvector_vector_type(TRI) faces;
    cvector_vector_type(V3D) verts_transformed;
} Mesh;

extern Mesh g_Mesh;

int LoadOBJ(char* filename);
void FreeMesh(void);

#endif // MESH_H

