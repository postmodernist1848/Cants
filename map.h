#ifndef MAP_H
#define MAP_H 1
#include <stdint.h>
#include <stdbool.h>

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
bool load_map(char *path);
Point find_random_free_spot_on_a_map(void);

enum MAP {MAP_FREE=0, MAP_WALL=1, MAP_ENCLOSED=2, MAP_FOOD, MAP_ANTHILL, MAP_TOTAL};
#endif //MAP_H
