#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mesh.h"
#include "math.h"
#include "utils.h"

#define rand32(max) (((rand() << 16) | rand()) % (max))
#define rand32balanced(max) ((((rand() << 16) | rand()) % (max)) - ((max) >> 1))

Mesh g_Mesh = {};

// Parse string directly to fixed-point (16.16) to avoid buggy floating point library
// Updates *endptr to point to the first character after the parsed number
int ParseToFixed(char *str, char **endptr)
{
    int integer_part = 0;
    int fractional_part = 0;
    int sign = 1;
    int decimal_divisor = 1;
    int in_decimal = 0;
    
    // Skip whitespace
    while (*str == ' ' || *str == '\t') str++;
    
    // Handle sign
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Parse digits
    while ((*str >= '0' && *str <= '9') || *str == '.') {
        if (*str == '.') {
            in_decimal = 1;
            str++;
            continue;
        }
        
        if (in_decimal) {
            // Cap at 4 decimal digits to avoid overflow in fractional_part * 65536
            if (decimal_divisor < 10000) {
                fractional_part = fractional_part * 10 + (*str - '0');
                decimal_divisor *= 10;
            }
        } else {
            integer_part = integer_part * 10 + (*str - '0');
        }
        str++;
    }
    
    if (endptr) *endptr = str;
    
    // Convert to 16.16 fixed point
    // Integer part: shift left 16 bits
    // Fractional part: (frac * 65536) / divisor
    int result = (integer_part << 16);
    if (fractional_part > 0) {
        result += (fractional_part * 65536) / decimal_divisor;
    }
    
    return result * sign;
}

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
            // Parse directly to fixed-point to avoid buggy floating point library
            char *parse_ptr = line + 2; // Skip "v "
            vertex.x = ParseToFixed(parse_ptr, &parse_ptr);
            vertex.y = ParseToFixed(parse_ptr, &parse_ptr);
            vertex.z = ParseToFixed(parse_ptr, &parse_ptr);
            cvector_push_back(g_Mesh.verts, vertex);
        }
        // // Face information
        if (strncmp(line, "f ", 2) == 0)
        {
            sscanf(
                line, "f %d/%d/%d %d/%d/%d %d/%d/%d",
                &vertex_indices[0], &texture_indices[0], &normal_indices[0],
                &vertex_indices[1], &texture_indices[1], &normal_indices[1],
                &vertex_indices[2], &texture_indices[2], &normal_indices[2]);

            face.a = vertex_indices[0] - 1;
            face.b = vertex_indices[1] - 1;
            face.c = vertex_indices[2] - 1;
            face.next = NULL;

            cvector_push_back(g_Mesh.faces, face);
        }
    }

    getchar();

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