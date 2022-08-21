#ifndef MAP_H
#define MAP_H 1
#include <stdint.h>
#include <stdbool.h>
#include "cants_config.h"

typedef struct {
    int8_t **matrix;
    uint8_t width;
    uint8_t height;
} Map;

typedef struct {
    short x;
    short y;
} Point;

extern Map g_map;
bool load_map(char *path);
Point find_random_free_spot_on_a_map(void);

enum MAP { MAP_FREE, 
           MAP_WALL, 
           MAP_ENCLOSED, 
           MAP_FOOD, 
           MAP_ANTHILL, 
           MAP_TOTAL};
#endif //MAP_H
