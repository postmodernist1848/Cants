#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "map.h"

Map g_map = {0};

bool load_map(char *path) {

    SDL_RWops *map_file = SDL_RWFromFile(path, "rb");
    if (map_file == NULL) return false;

    //check the signature
    {
        char signature[sizeof CANTS_MAP_SIGNATURE / sizeof(char)] = {0};
        if (SDL_RWread(map_file, &signature, sizeof(char), sizeof CANTS_MAP_SIGNATURE / sizeof(char) - 1) != sizeof CANTS_MAP_SIGNATURE / sizeof(char) - 1) {
            fprintf(stderr, "Failed reading from file.\n");
            return false;
        }
        if (strcmp(signature, CANTS_MAP_SIGNATURE) != 0) {
            fprintf(stderr, "Given file is not a cants map.\n");
            return false;
        }
    }

    //read width and height
    if (SDL_RWread(map_file, &g_map.width, sizeof g_map.width, 1) == 0 ||
    SDL_RWread(map_file, &g_map.height, sizeof g_map.height, 1) == 0) return false;

    if ((g_map.matrix = malloc(g_map.height * sizeof(int8_t *))) == NULL) return false;
    for (int i = 0; i < g_map.height; i++) {
        if ((g_map.matrix[i] = malloc(g_map.width * sizeof(int8_t))) == NULL) {
            //if failed, free everything allocated
            for (int j = 0; j < i; j++) {
                free(g_map.matrix[j]);
            }
            free(g_map.matrix);
            g_map.matrix = NULL;
            return false;
        }
    }
    for (int i = 0; i < g_map.height; i++) {
        if (SDL_RWread(map_file, g_map.matrix[i], sizeof(int8_t), g_map.width) == 0) {
            for (int j = 0; j < g_map.height; j++) {
                free(g_map.matrix[j]);
            }
            free(g_map.matrix);
            g_map.matrix = NULL;
            return false;
        }
    }

    return true;
}

Point find_random_free_spot_on_a_map(void) {
    short x, y;
    while (g_map.matrix[y = rand() % g_map.height][x = rand() % g_map.width] != MAP_FREE);
    Point point = {x, y};
    return point;
}

//not the most efficient way, but allows to check if there are free points at all
//TODO: index all free spaces and make this function extra fast and safe or find a better way to do this
Point find_random_free_spot_on_a_map_safe(void) {
    Point free_points[g_map.height * g_map.width];
    int sp = 0;
    Point point;
    for (int i = 0; i < g_map.height; i++) {
        for (int j = 0; j < g_map.width; j++) {
            if (g_map.matrix[i][j] == MAP_FREE) {
                point.y = i;
                point.x = j;
                free_points[sp++] = point;
            }
        }
    }
    assert(sp > 0);
    return free_points[rand() % sp];
}
