#ifndef MAP_H
#define MAP_H 1
#include <stdint.h>
#include <stdbool.h>
#include "cants_config.h"

typedef struct {
    int8_t **matrix;
    int width;
    int height;
} Map;

typedef struct {
    short x;
    short y;
} Point;

extern Map g_map;
#if ANDROID_BUILD
bool load_map(void);
#else
bool load_map(char *path);
#endif
Point find_random_free_spot_on_a_map(void);

enum MAP {MAP_FREE=0, MAP_WALL=1, MAP_ENCLOSED=2, MAP_FOOD, MAP_ANTHILL, MAP_TOTAL};
#endif //MAP_H
