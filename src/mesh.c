#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mesh.h"

#define rand32(max) (((rand() << 16) | rand()) % (max))
#define rand32balanced(max) ((((rand() << 16) | rand()) % (max)) - ((max) >> 1))
