#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mesh.h"
#include "math.h"
#include "utils.h"

#define rand32(max) (((rand() << 16) | rand()) % (max))
#define rand32balanced(max) ((((rand() << 16) | rand()) % (max)) - ((max) >> 1))

Mesh g_Mesh = {};

void FreeMesh(void)
{
    cvector_free(g_Mesh.verts);
    cvector_free(g_Mesh.faces);
    cvector_free(g_Mesh.verts_transformed);
}

int LoadOBJ(char *filename)
{
    FILE *file;
    char line[256];
    char *ptr;
    int vertex_indices[3];
    int texture_indices[3];
    int normal_indices[3];
    float vertex_float;
    int i;
    V3D vertex;
    V3D _verts[4];
    TRI face;
    V3D *imported_normals = NULL;

    g_Mesh.verts = NULL;
    g_Mesh.faces = NULL;
    g_Mesh.verts_transformed = NULL;


    sprintf(&line[0], "%s.%s", gBaseDirectoryPath, filename);
    ptr = &line[0];

    file = fopen(ptr, "r");
    if (file == NULL)
    {
        printf("ERROR - Unable to find file.\n");
        return 1;
    }

    while (fgets(line, 256, file))
    {
        // Vertex information
        if (strncmp(line, "v ", 2) == 0)
        {
            // Parse fixed-point vertex coordinates directly
            sscanf(line, "v %d %d %d", &vertex.x, &vertex.y, &vertex.z);
            vertex.x <<= 4;
            vertex.y <<= 4;
            vertex.z <<= 4;

            cvector_push_back(g_Mesh.verts, vertex);
        }

        if (!imported_normals)
        {
            cvector_reserve(imported_normals, cvector_size(g_Mesh.verts));
        }

        // Normal information
        if (strncmp(line, "vn ", 3) == 0)
        {
            // Parse fixed-point vertex coordinates directly
            sscanf(line, "vn %d %d %d", &vertex.x, &vertex.y, &vertex.z);

            cvector_push_back(imported_normals, vertex);
        }

        // Face information
        if (strncmp(line, "f ", 2) == 0)
        {
            sscanf(
                line, "f %d//%d %d//%d %d//%d",
                &vertex_indices[0], &normal_indices[0],
                &vertex_indices[1], &normal_indices[1],
                &vertex_indices[2], &normal_indices[2]);

            face.a = vertex_indices[0] - 1;
            face.b = vertex_indices[1] - 1;
            face.c = vertex_indices[2] - 1;
            face.next = NULL;

            // In theory, all 3 normals should be the same indice so let's just pick 1
            face.normal = imported_normals[face.b];

            printf("TRIS: %d, %d, %d\n", face.a, face.b, face.c);

            cvector_push_back(g_Mesh.faces, face);
        }
    }

    if (imported_normals)
    {
        cvector_free(imported_normals);
    }

    // getchar();

    // for (i = 0; i < cvector_size(g_Mesh.faces); ++i)
    // {
    //     _verts[0] = g_Mesh.verts[g_Mesh.faces[i].a];
    //     _verts[1] = g_Mesh.verts[g_Mesh.faces[i].b];
    //     _verts[2] = g_Mesh.verts[g_Mesh.faces[i].c];
    //     Normal(&_verts[0], &_verts[1], &_verts[2], &g_Mesh.faces[i].normal);
    //     Normalize(&g_Mesh.faces[i].normal);
    // }

    cvector_copy(g_Mesh.verts, g_Mesh.verts_transformed);

    fclose(file);

    return 0;
}