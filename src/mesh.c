#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include "mesh.h"
#include "math.h"
#include "utils.h"

#define rand32(max) (((rand() << 16) | rand()) % (max))
#define rand32balanced(max) ((((rand() << 16) | rand()) % (max)) - ((max) >> 1))

Mesh g_Mesh = {};

static int IsSpaceChar(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int IsDigitChar(char c)
{
    return c >= '0' && c <= '9';
}

static const char *SkipSpaces(const char *p)
{
    while (*p && IsSpaceChar(*p))
    {
        ++p;
    }

    return p;
}

static int ParseI32(const char **input, int *out)
{
    const char *p = SkipSpaces(*input);
    int sign = 1;
    int32_t value = 0;

    if (*p == '+' || *p == '-')
    {
        sign = (*p == '-') ? -1 : 1;
        ++p;
    }

    if (!IsDigitChar(*p))
    {
        return 0;
    }

    if (sign > 0)
    {
        while (IsDigitChar(*p))
        {
            int digit = *p - '0';

            if (value > (INT32_MAX - digit) / 10)
            {
                return 0;
            }

            value = value * 10 + digit;
            ++p;
        }
    }
    else
    {
        while (IsDigitChar(*p))
        {
            int digit = *p - '0';

            if (value < (INT32_MIN + digit) / 10)
            {
                return 0;
            }

            value = value * 10 - digit;
            ++p;
        }
    }

    *out = (int)value;
    *input = p;
    return 1;
}

// Minimal sscanf-style parser for signed 32-bit integers (%d only).
static int ScanI32(const char *input, const char *format, ...)
{
    const char *f = format;
    const char *p = input;
    int parsed = 0;
    va_list args;

    va_start(args, format);

    while (*f)
    {
        if (IsSpaceChar(*f))
        {
            while (*f && IsSpaceChar(*f))
            {
                ++f;
            }

            p = SkipSpaces(p);
            continue;
        }

        if (*f != '%')
        {
            if (*p != *f)
            {
                break;
            }

            ++f;
            ++p;
            continue;
        }

        ++f;

        if (*f != 'd')
        {
            break;
        }

        if (!ParseI32(&p, va_arg(args, int *)))
        {
            break;
        }

        ++parsed;
        ++f;
    }

    va_end(args);
    return parsed;
}

// SDK workaround: avoid fgets() and read lines manually.
static int ReadLineSafe(FILE *file, char *buffer, unsigned int bufferSize)
{
    unsigned int i = 0;
    int ch;

    if (!file || !buffer || bufferSize < 2)
    {
        return 0;
    }

    while (1)
    {
        ch = fgetc(file);

        if (ch == EOF)
        {
            if (i == 0)
            {
                return 0;
            }

            break;
        }

        if (ch == '\n')
        {
            break;
        }

        if (ch == '\r')
        {
            int next = fgetc(file);

            if (next != '\n' && next != EOF)
            {
                ungetc(next, file);
            }

            break;
        }

        if (ch == '\0')
        {
            continue;
        }

        if (i < (bufferSize - 1))
        {
            buffer[i++] = (char)ch;
        }
        else
        {
            // Discard remainder of an overlong line.
            do
            {
                ch = fgetc(file);

                if (ch == '\n')
                {
                    break;
                }

                if (ch == '\r')
                {
                    int next = fgetc(file);

                    if (next != '\n' && next != EOF)
                    {
                        ungetc(next, file);
                    }

                    break;
                }

                if (ch == '\0')
                {
                    continue;
                }
            } while (ch != EOF);

            break;
        }
    }

    buffer[i] = '\0';
    return 1;
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
    int normal_indices[3];
    int i;
    V3D vertex;
    TRI face;
    V3D *imported_normals = NULL;
    int vert_count = 0;
    int normal_count = 0;
    int face_count = 0;

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

    printf("Loading mesh from %s\r\n", ptr);

    // Read counts from first 3 lines
    if (!ReadLineSafe(file, line, sizeof(line)) || !ScanI32(line, "%d", &vert_count))
    {
        printf("ERROR - Failed to read vertex count.\r\n");
        fclose(file);
        return 1;
    }

    if (!ReadLineSafe(file, line, sizeof(line)) || !ScanI32(line, "%d", &normal_count))
    {
        printf("ERROR - Failed to read normal count.\r\n");
        fclose(file);
        return 1;
    }

    if (!ReadLineSafe(file, line, sizeof(line)) || !ScanI32(line, "%d", &face_count))
    {
        printf("ERROR - Failed to read face count.\r\n");
        fclose(file);
        return 1;
    }

    printf("Loading: %d vertices, %d normals, %d faces\r\n", vert_count, normal_count, face_count);

    // Reserve space for all arrays
    cvector_reserve(g_Mesh.verts, vert_count);
    cvector_reserve(g_Mesh.faces, face_count);
    cvector_reserve(imported_normals, normal_count);

    // Read vertices (already in fixed-point from conversion script)
    for (i = 0; i < vert_count; ++i)
    {
        if (!ReadLineSafe(file, line, sizeof(line)))
        {
            printf("ERROR - Failed to read vertex %d.\r\n", i);
            fclose(file);
            cvector_free(imported_normals);
            return 1;
        }

        if (ScanI32(line, "%d %d %d", &vertex.x, &vertex.y, &vertex.z) != 3)
        {
            printf("ERROR - Failed to parse vertex %d.\r\n", i);
            fclose(file);
            cvector_free(imported_normals);
            return 1;
        }

        cvector_push_back(g_Mesh.verts, vertex);
    }

    // Read normals (already in fixed-point from conversion script)
    for (i = 0; i < normal_count; ++i)
    {
        if (!ReadLineSafe(file, line, sizeof(line)))
        {
            printf("ERROR - Failed to read normal %d.\r\n", i);
            fclose(file);
            cvector_free(imported_normals);
            return 1;
        }

        if (ScanI32(line, "%d %d %d", &vertex.x, &vertex.y, &vertex.z) != 3)
        {
            printf("ERROR - Failed to parse normal %d.\r\n", i);
            fclose(file);
            cvector_free(imported_normals);
            return 1;
        }

        cvector_push_back(imported_normals, vertex);
    }

    // Read faces (format: v1 n1 v2 n2 v3 n3)
    for (i = 0; i < face_count; ++i)
    {
        int matIndex = 0;

        if (!ReadLineSafe(file, line, sizeof(line)))
        {
            printf("ERROR - Failed to read face %d.\r\n", i);
            fclose(file);
            cvector_free(imported_normals);
            return 1;
        }

        if (ScanI32(line, "%d %d %d %d %d %d %d",
                &matIndex,
                &vertex_indices[0], &normal_indices[0],
                &vertex_indices[1], &normal_indices[1],
                &vertex_indices[2], &normal_indices[2]) != 7)
        {
            printf("ERROR - Failed to parse face %d.\r\n", i);
            fclose(file);
            cvector_free(imported_normals);
            return 1;
        }

        face.a = vertex_indices[0] - 1;
        face.b = vertex_indices[1] - 1;
        face.c = vertex_indices[2] - 1;
        face.d = (matIndex << 7); // Pre-shift depth for lighting calculation
        face.next = NULL;

        // Use the first normal index (all 3 should be the same for flat shading)
        face.normal = imported_normals[normal_indices[0] - 1];

        cvector_push_back(g_Mesh.faces, face);
    }
    
    if (imported_normals)
    {
        cvector_free(imported_normals);
    }

    printf("Mesh loaded successfully.\r\n");

    cvector_copy(g_Mesh.verts, g_Mesh.verts_transformed);

    fclose(file);

    g_Mesh.position = (V3D){0, 0, 0};
    g_Mesh.eulers = (V3D){0, 0, 0};
    g_Mesh.speed = float2fix(0.3);
    g_Mesh.rollPerFrame = 0;
    g_Mesh.pitchPerFrame = 0;

    return 0;
}