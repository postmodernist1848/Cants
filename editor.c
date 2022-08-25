/* Cants map editor.
 * A cants map is a binary format that consists of:
 * a byte for height (uint8)
 * a byte for width (uint8)
 * binary data of the map (width * height bytes, because a single tile is an int8_t)
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include "map.h"
#include <errno.h>
#include <ctype.h>
#include <string.h>

#define scp(pointer, message) {                                               \
    if (pointer == NULL) {                                                    \
        fprintf(stderr, "Error: %s! SDL_Error: %s", message, SDL_GetError()); \
        exit(1);                                                              \
    }                                                                         \
}

#define scc(code, message) {                                                  \
    if (code < 0) {                                                           \
        fprintf(stderr, "Error: %s! SDL_Error: %s", message, SDL_GetError()); \
        exit(1);                                                              \
    }                                                                         \
}

#define imgcp(pointer, message) { if (pointer == NULL) {fprintf(stderr, "Error: %s! IMG_Error: %s", message, IMG_GetError()); exit(1);}}
#define imgcc(code, message) { if (code < 0) {fprintf(stderr, "Error: %s! IMG_Error: %s", message, IMG_GetError()); exit(1);}}
#define ttfcp(pointer, message) { if (pointer == NULL) {fprintf(stderr, "Error: %s! TTF_Error: %s", message, TTF_GetError()); exit(1);}}
#define ttfcc(code, message) { if (code < 0) {fprintf(stderr, "Error: %s! TTF_Error: %s", message, TTF_GetError()); exit(1);}}


typedef struct {
    SDL_Texture *texture_proper;
    int width;
    int height;
} Texture;

int screen_width = 1280;
int screen_height = 720;

int level_width;
int level_height;

int cur_mode = -1;

#define INIT_CELL_SIZE 25
int CELL_SIZE = INIT_CELL_SIZE;

SDL_Window *g_window;
SDL_Renderer *g_renderer;
Texture g_background_texture;
Texture g_leaf_texture;
Texture g_anthill_texture;
TTF_Font *g_font;
Texture g_mode_texture;

SDL_Rect g_camera = {
    0,
    0,
    0,
    0
};

SDL_Rect g_anthill = {-1, 0, 3 * INIT_CELL_SIZE, 3 * INIT_CELL_SIZE};

Texture load_texture(const char *path) {
	//The final texture
	SDL_Texture *new_texture = NULL;
    SDL_Surface *loaded_surface = NULL;
    Texture texture_struct = {0};

	//Load image at specified path
	imgcp((loaded_surface = IMG_Load(path)), "Could not load image");
    //Create texture from surface pixels
    scp((new_texture = SDL_CreateTextureFromSurface(g_renderer, loaded_surface)), "Could not create texture from surface");

    texture_struct.texture_proper = new_texture;
    texture_struct.width = loaded_surface->w;
    texture_struct.height = loaded_surface->h;

    //Get rid of old loaded surface
    SDL_FreeSurface(loaded_surface);

	return texture_struct;
}

Texture load_text_texture(const char *text){
	//The final texture
	SDL_Texture *new_texture = NULL;
    SDL_Surface *text_surface = NULL;


#define OUTLINE_SIZE 2

    /* load font and its outline */
    TTF_SetFontOutline(g_font, OUTLINE_SIZE);

    /* render text and text outline */
    SDL_Color white = {0xFF, 0xFF, 0xFF, 0xFF};
    SDL_Color black = {0x00, 0x00, 0x00, 0xFF};
    text_surface = TTF_RenderText_Blended(g_font, text, black);
    SDL_Surface *fg_surface = TTF_RenderText_Blended(g_font, text, white);
    SDL_Rect rect = {OUTLINE_SIZE, OUTLINE_SIZE, fg_surface->w, fg_surface->h};

    /* blit text onto its outline */
    SDL_SetSurfaceBlendMode(fg_surface, SDL_BLENDMODE_BLEND);
    SDL_BlitSurface(fg_surface, NULL, text_surface, &rect);
    SDL_FreeSurface(fg_surface);

    scp((new_texture = SDL_CreateTextureFromSurface(g_renderer, text_surface)), "Could not create texture from surface");

    const Texture texture_struct = {
        new_texture,
        text_surface->w,
        text_surface->h

    };
    SDL_FreeSurface(text_surface);

	return texture_struct;
}

char *tile_to_string(enum MAP tile) {

    switch (tile) {
        case MAP_WALL:
            return "Wall";
        case MAP_FREE:
            return "Free";
        case MAP_ENCLOSED:
            return "Enclosed";
        case MAP_FOOD:
            return "Food";
        case MAP_ANTHILL:
            return "Anthill";
        default:
            return "Unknown";
    }
}

int min(int a, int b) {
    return (a < b) ? a: b;
}

void setmode(int mode) {
    if (mode == cur_mode) return;
    cur_mode = mode;
    g_mode_texture = load_text_texture(tile_to_string(cur_mode));
}
void init(void) {

    g_camera.w = screen_width;
    g_camera.h = screen_height;
	scc(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER), "Could not initialize SDL");
    if( !SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "1" ) ) {
        fprintf(stderr, "Warning: Linear texture filtering not enabled!");
    }
    ttfcc(TTF_Init(), "Could not initialize SDL_ttf");
    scp((g_window = SDL_CreateWindow("Cants map editor", 0, 0, screen_width, screen_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE)),
            "Could not create window");
    //Create renderer for window
    scp((g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)),
            "Could not create renderer");
    g_background_texture = load_texture("assets/grass500x500.png");
    g_leaf_texture = load_texture("assets/leaf.png");
    g_anthill_texture = load_texture("assets/anthill.png");
    g_font = TTF_OpenFont("assets/OpenSans-Regular.ttf", 50);
    setmode(MAP_WALL);
}

void render_texture(Texture texture, int x, int y, float scale) {
    SDL_Rect render_rect;
    render_rect.x = x;
    render_rect.y = y;
    render_rect.h = texture.height * scale;
    render_rect.w = texture.width * scale;
    SDL_RenderCopy(g_renderer, texture.texture_proper, NULL, &render_rect);
}

bool check_collision(SDL_Rect a, SDL_Rect b) {
    if(a.y + a.h <= b.y  ||
        a.y >= b.y + b.h ||
        a.x + a.w <= b.x ||
        a.x >= b.x + b.w)
        return false;
    return true;
}

bool write_map_to_file(char *path) {
    SDL_RWops* map_file = SDL_RWFromFile(path, "wb");
    if (map_file == NULL) {
        fprintf(stderr, "Failed to open %s for writing: %s\n", path, strerror(errno));
        return false;
    }
    SDL_RWwrite(map_file, &g_map.width,  sizeof g_map.width, 1);
    SDL_RWwrite(map_file, &g_map.height, sizeof g_map.height, 1);

    int8_t buf[9];
    int gm_x = 0, gm_y = 0;

    if (g_anthill.x != -1) {
        gm_x = g_anthill.x / CELL_SIZE;
        gm_y = g_anthill.y / CELL_SIZE;
        buf[0] = g_map.matrix[gm_y + 0][gm_x + 0];
        buf[1] = g_map.matrix[gm_y + 1][gm_x + 0];
        buf[2] = g_map.matrix[gm_y + 2][gm_x + 0];
        buf[3] = g_map.matrix[gm_y + 0][gm_x + 1];
        buf[4] = g_map.matrix[gm_y + 1][gm_x + 1];
        buf[5] = g_map.matrix[gm_y + 2][gm_x + 1];
        buf[6] = g_map.matrix[gm_y + 0][gm_x + 2];
        buf[7] = g_map.matrix[gm_y + 1][gm_x + 2];
        buf[8] = g_map.matrix[gm_y + 2][gm_x + 2];
        g_map.matrix[gm_y + 0][gm_x + 0] = MAP_ANTHILL;
        g_map.matrix[gm_y + 1][gm_x + 0] = MAP_ANTHILL;
        g_map.matrix[gm_y + 2][gm_x + 0] = MAP_ANTHILL;
        g_map.matrix[gm_y + 0][gm_x + 1] = MAP_ANTHILL;
        g_map.matrix[gm_y + 1][gm_x + 1] = MAP_ANTHILL;
        g_map.matrix[gm_y + 2][gm_x + 1] = MAP_ANTHILL;
        g_map.matrix[gm_y + 0][gm_x + 2] = MAP_ANTHILL;
        g_map.matrix[gm_y + 1][gm_x + 2] = MAP_ANTHILL;
        g_map.matrix[gm_y + 2][gm_x + 2] = MAP_ANTHILL;
    }

    for (int i = 0; i < g_map.height; i++) {
        SDL_RWwrite(map_file, g_map.matrix[i], sizeof(int8_t), g_map.width);
    }
    SDL_RWclose(map_file);

    if (g_anthill.x != -1) {
        g_map.matrix[gm_y + 0][gm_x + 0] = buf[0];
        g_map.matrix[gm_y + 1][gm_x + 0] = buf[1];
        g_map.matrix[gm_y + 2][gm_x + 0] = buf[2];
        g_map.matrix[gm_y + 0][gm_x + 1] = buf[3];
        g_map.matrix[gm_y + 1][gm_x + 1] = buf[4];
        g_map.matrix[gm_y + 2][gm_x + 1] = buf[5];
        g_map.matrix[gm_y + 0][gm_x + 2] = buf[6];
        g_map.matrix[gm_y + 1][gm_x + 2] = buf[7];
        g_map.matrix[gm_y + 2][gm_x + 2] = buf[8];
    }
    return true;
}

void usage(void) {
    printf("Usage: editor <file> | create <filename> <width> <height> | info <file> | translate <file> <x> <y> | resize <file> <dx> <dy>\nSee README for details\n");
    exit(0);
}

void translate(int x, int y) {
    if (g_anthill.x != -1) {
        g_anthill.x += x * CELL_SIZE;
        g_anthill.y -= y * CELL_SIZE;
    }
    if (y > 0) {
        int8_t *buf[y];
        memcpy(buf, g_map.matrix, y * sizeof(int8_t *));
        memmove(g_map.matrix, g_map.matrix + y, (g_map.height - y) * sizeof(int8_t *));
        memcpy(g_map.matrix + g_map.height - y, buf, y * sizeof(int8_t *));
    }
    else if (y < 0) {
        y = -y;
        int8_t *buf[y];
        memcpy(buf, g_map.matrix + g_map.height - y, y * sizeof(int8_t *));
        memmove(g_map.matrix + y, g_map.matrix, (g_map.height - y) * sizeof(int8_t *));
        memcpy(g_map.matrix, buf, y * sizeof(int8_t *));
    }
    if (x > 0)
        for (int i = 0; i < g_map.height; i++) {
            int8_t buf[x];
            memcpy(buf, g_map.matrix[i] + g_map.width - x, x * sizeof(int8_t));
            memmove(g_map.matrix[i] + x, g_map.matrix[i], (g_map.width - x) * sizeof(int8_t));
            memcpy(g_map.matrix[i], buf, x * sizeof(int8_t));
        }
    else if (x < 0) {
        x = -x;
        for (int i = 0; i < g_map.height; i++) {
            int8_t buf[x];
            memcpy(buf, g_map.matrix[i], x * sizeof(int8_t));
            memmove(g_map.matrix[i], g_map.matrix[i] + x, (g_map.width - x) * sizeof(int8_t));
            memcpy(g_map.matrix[i] + g_map.width - x, buf, x * sizeof(int8_t));
        }
    }
}


void edit(char *map_path) {
    printf("Loading map...\n");
    if (!load_map(map_path)) {
        fprintf(stderr, "Failed to load map '%s' for editing\n", map_path);
        exit(1);
    }
    else
        printf("Map %dx%d loaded successfully!\n", g_map.width, g_map.height);

    level_height = g_map.height * CELL_SIZE;
    level_width = g_map.width * CELL_SIZE;

    init();
    bool quit = false;
    bool mmb_pressed = false;
    bool lmb_pressed = false;
    bool rmb_pressed = false;
    float world_scale = 1;

    //check if the anthill is present on the map
    for (int i = 0; i < g_map.height; i++) {
        for (int j = 0; j < g_map.width; j++) {
            if (g_map.matrix[i][j] == MAP_ANTHILL) {
                if (g_map.matrix[i + 1][j + 0] == MAP_ANTHILL &&
                    g_map.matrix[i + 2][j + 0] == MAP_ANTHILL &&
                    g_map.matrix[i + 0][j + 1] == MAP_ANTHILL &&
                    g_map.matrix[i + 1][j + 1] == MAP_ANTHILL &&
                    g_map.matrix[i + 2][j + 1] == MAP_ANTHILL &&
                    g_map.matrix[i + 0][j + 2] == MAP_ANTHILL &&
                    g_map.matrix[i + 1][j + 2] == MAP_ANTHILL &&
                    g_map.matrix[i + 2][j + 2] == MAP_ANTHILL) {
                        g_anthill.y = i * CELL_SIZE;
                        g_anthill.x = j * CELL_SIZE;
                        g_map.matrix[i + 0][j + 0] = MAP_FREE;
                        g_map.matrix[i + 1][j + 0] = MAP_FREE;
                        g_map.matrix[i + 2][j + 0] = MAP_FREE;
                        g_map.matrix[i + 0][j + 1] = MAP_FREE;
                        g_map.matrix[i + 1][j + 1] = MAP_FREE;
                        g_map.matrix[i + 2][j + 1] = MAP_FREE;
                        g_map.matrix[i + 0][j + 2] = MAP_FREE;
                        g_map.matrix[i + 1][j + 2] = MAP_FREE;
                        g_map.matrix[i + 2][j + 2] = MAP_FREE;
                        goto out;
            }
                else {
                    fprintf(stderr, "Error: Something is wrong with the anthill in the map.");
                    exit(1);
                }
            }
        }
    }
    out:;

    SDL_Event event;
    while (!quit) {
        while(SDL_PollEvent(&event) != 0) {
            switch (event.type) {
                case SDL_QUIT:
                    quit = true;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_MIDDLE) {
                        mmb_pressed = true;
                        int x = event.button.x + g_camera.x, y = event.button.y + g_camera.y; 
                        printf("%d, %d was pressed\n", x / CELL_SIZE, y / CELL_SIZE);
                    }                                 
                    else if (event.button.button == SDL_BUTTON_LEFT) {
                        lmb_pressed = true;
                        int x = event.button.x + g_camera.x, y = event.button.y + g_camera.y; 
                        if (x > 0 && x < level_width && y > 0 && y < level_height) {
                            if (cur_mode == MAP_ANTHILL) {
                                if (x + 2 * CELL_SIZE < g_map.width * CELL_SIZE && y + 2 * CELL_SIZE < g_map.height * CELL_SIZE) {
                                g_anthill.x = x - x % CELL_SIZE;
                                g_anthill.y = y - y % CELL_SIZE;
                                }
                            }
                            else {
                                g_map.matrix[y / CELL_SIZE][x / CELL_SIZE] = cur_mode;
                            }
                        }
                            
                    }
                    else if (event.button.button == SDL_BUTTON_RIGHT) {
                        rmb_pressed = true;
                        int x = event.button.x + g_camera.x, y = event.button.y + g_camera.y; 
                        if (x > 0 && x < level_width && y > 0 && y < level_height)
                            g_map.matrix[y / CELL_SIZE][x / CELL_SIZE] = MAP_FREE;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_MIDDLE) mmb_pressed = false;
                    else if (event.button.button == SDL_BUTTON_LEFT) lmb_pressed = false;
                    else if (event.button.button == SDL_BUTTON_RIGHT) rmb_pressed = false;
                    break;

                case SDL_MOUSEMOTION:
                    if (mmb_pressed) {
                        g_camera.x += event.motion.xrel;
                        g_camera.y += event.motion.yrel;
                        if (g_camera.x < 0 || g_camera.x + g_camera.w > g_map.width * CELL_SIZE) 
                            g_camera.x -= event.motion.xrel;
                        if (g_camera.y < 0 || g_camera.y + g_camera.h > g_map.height * CELL_SIZE)
                            g_camera.y -= event.motion.yrel;
                    }
                    else if (lmb_pressed && cur_mode != MAP_ANTHILL) {
                        int x = event.motion.x + g_camera.x, y = event.motion.y + g_camera.y; 
                        if (x > 0 && x < level_width && y > 0 && y < level_height)
                            g_map.matrix[y / CELL_SIZE][x / CELL_SIZE] = cur_mode;
                    }
                    else if (rmb_pressed) {
                        int x = event.motion.x + g_camera.x, y = event.motion.y + g_camera.y; 
                        if (x > 0 && x < level_width && y > 0 && y < level_height)
                            g_map.matrix[y / CELL_SIZE][x / CELL_SIZE] = MAP_FREE;
                    }
                    break;
                case SDL_KEYDOWN:
                    switch (event.key.keysym.scancode) {
                        case SDL_SCANCODE_1:
                            setmode(MAP_FREE);
                            break;
                        case SDL_SCANCODE_2:
                            setmode(MAP_WALL);
                            break;
                        case SDL_SCANCODE_3:
                            setmode(MAP_ENCLOSED);
                            break;
                        case SDL_SCANCODE_4:
                            setmode(MAP_FOOD);
                            break;
                        case SDL_SCANCODE_5:
                            setmode(MAP_ANTHILL);
                            break;
                        case SDL_SCANCODE_S:
                            if (event.key.keysym.mod & KMOD_LCTRL)
                                if (write_map_to_file(map_path))
                                    printf("Successfully saved the map!\n");
                            break;
#define WORLD_SCALE_INC 0.1f
                        case SDL_SCANCODE_KP_PLUS:
                            if (world_scale < 3) {
                                world_scale += WORLD_SCALE_INC;
                                CELL_SIZE = INIT_CELL_SIZE * world_scale;
                            }
                            break;
                        case SDL_SCANCODE_KP_MINUS:
                            if (world_scale > 0.1f) {
                                world_scale -= WORLD_SCALE_INC;
                                CELL_SIZE = INIT_CELL_SIZE * world_scale;
                            }
                            break;

                        case SDL_SCANCODE_UP:
                            translate(0, 1);
                            break;
                        case SDL_SCANCODE_RIGHT:
                            translate(1, 0);
                            break;
                        case SDL_SCANCODE_DOWN:
                            translate(0, -1);
                            break;
                        case SDL_SCANCODE_LEFT:
                            translate(-1, 0);
                            break;
                    }
                    break;
                case SDL_WINDOWEVENT:
                      if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        screen_width = event.window.data1;
                        screen_height = event.window.data2;
                        g_camera.w = screen_width;
                        g_camera.h = screen_height;
                      }
                    break;
            }
        }
        SDL_SetRenderDrawColor(g_renderer, 0x00, 0x60, 0x00, 0xFF);
        SDL_RenderClear(g_renderer);
        for (int y = 0; y < level_height; y += g_background_texture.height) {
            for (int x = 0; x < level_width; x += g_background_texture.width) {
                SDL_Rect coords = {
                    x,
                    y,
                    g_background_texture.width,
                    g_background_texture.height
                };
                if (check_collision(coords, g_camera)) {
                    render_texture(g_background_texture, x - g_camera.x, y - g_camera.y, 1);
                }
            }
        }

        SDL_SetRenderDrawColor(g_renderer, 0x00, 0x90, 0x00, 0xFF);
        for (int i = 0; i < min((g_camera.y + g_camera.h + CELL_SIZE) / CELL_SIZE, g_map.height); i++) {
            for (int j = 0; j < min((g_camera.x + g_camera.w + CELL_SIZE) / CELL_SIZE, g_map.width); j++) {
                if (g_map.matrix[i][j] == MAP_WALL) {
                    SDL_Rect coords = {
                        j * CELL_SIZE - g_camera.x,
                        i * CELL_SIZE - g_camera.y,
                        CELL_SIZE,
                        CELL_SIZE
                    };
                    SDL_RenderFillRect(g_renderer, &coords);
                }
                else if (g_map.matrix[i][j] == MAP_FOOD) {
                    render_texture(g_leaf_texture, j * CELL_SIZE - g_camera.x, i * CELL_SIZE - g_camera.y, (float) CELL_SIZE / g_leaf_texture.width * world_scale);
                }
                else if (g_map.matrix[i][j] == MAP_ENCLOSED) {
                    SDL_Rect coords = {
                        j * CELL_SIZE - g_camera.x,
                        i * CELL_SIZE - g_camera.y,
                        CELL_SIZE,
                        CELL_SIZE
                    };
                    SDL_SetRenderDrawColor(g_renderer, 0x00, 0x90, 0x90, 0xFF);
                    SDL_RenderFillRect(g_renderer, &coords);
                    SDL_SetRenderDrawColor(g_renderer, 0x00, 0x90, 0x00, 0xFF);
                }
                else if (g_map.matrix[i][j] == MAP_ENCLOSED) {
                    SDL_Rect coords = {
                        j * CELL_SIZE - g_camera.x,
                        i * CELL_SIZE - g_camera.y,
                        CELL_SIZE,
                        CELL_SIZE
                    };
                    SDL_SetRenderDrawColor(g_renderer, 0x96, 0x4B, 0x00, 0xFF);
                    SDL_RenderFillRect(g_renderer, &coords);
                    SDL_SetRenderDrawColor(g_renderer, 0x00, 0x90, 0x00, 0xFF);
                }
            }
        }

        if (g_anthill.x != -1) {
            render_texture(g_anthill_texture, g_anthill.x * world_scale - g_camera.x, g_anthill.y * world_scale - g_camera.y, (float) g_anthill.w / g_anthill_texture.width * world_scale);
        }

        //drawing tile
        render_texture(g_mode_texture, 0, 0, 1);

        SDL_RenderPresent(g_renderer);
    }


    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    IMG_Quit();
    SDL_Quit();
}

int *get_map_info() {

    int *tile_counts = malloc(MAP_TOTAL * sizeof(int));
    memset(tile_counts, 0, MAP_TOTAL * sizeof(int));
    for (int i = 0; i < g_map.height; i++)
        for (int j = 0; j < g_map.width; j++)
            tile_counts[g_map.matrix[i][j]]++;
    return tile_counts;
}


bool isnumber(char *str) {
    while (*str) {
        if (!isdigit(*str++)) return false;
    }
    return true;
}

bool create_map(char *name, int width, int height) {
    if (width > 255 || height > 255) {
        fprintf(stderr, "Width and height greater than 255 are not supported\n");
        return false;
    }
    int8_t *map[height];
    int8_t zeros[width];
    memset(zeros, 0, width * sizeof(int8_t));

    for (int i = 0; i < height; i++) {
        //get a pointer to the first element
        map[i] = &zeros[0];
    }
    g_map.matrix = map;
    g_map.width = width;
    g_map.height = height;
    return write_map_to_file(name);
}

bool resize(int dx, int dy) {
    int new_width = g_map.width + dx;
    int new_height = g_map.height + dy;
    if (new_height < g_map.height) {
        for (int i = new_height; i < g_map.height; i++) {
            free(g_map.matrix[i]);
        }
    }
    for (int i = 0; i < ((new_height < g_map.height) ? new_height: g_map.height); i++) {
        if ((g_map.matrix[i] = realloc(g_map.matrix[i], sizeof(int8_t) * new_width)) == NULL) {
            fprintf(stderr, "realloc failed\n");
            return false;
        };
        if (new_width > g_map.width) {
            memset(g_map.matrix[i] + g_map.width, 0, dx);
        }
    }
    if ((g_map.matrix = realloc(g_map.matrix, sizeof(int8_t *) * new_height)) == NULL) {
            fprintf(stderr, "realloc failed\n");
            return false;
    }
    if (new_height > g_map.height) {
        for (int i = g_map.height; i < new_height; i++) {
            if ((g_map.matrix[i] = calloc(new_width, sizeof(int8_t))) == NULL) {
                    fprintf(stderr, "malloc failed\n");
                    return false;
            }
        }
    }
    g_map.height = new_height;
    g_map.width = new_width;
    return true;
}

int main (int argc, char *argv[]) {

    if (argc == 1)
        usage();

    if (strcmp("edit", *++argv) == 0) {
        if (*++argv == NULL) {
            usage();
        }
        else {
            edit(*argv);
        }
    }
    else if (strcmp("info", *argv) == 0) {
        if (*++argv == NULL) {
            usage();
        }
        else {
            if (load_map(*argv)) {
                printf("%s: %dx%d\n", *argv, g_map.width, g_map.height);
                int *info = get_map_info();
                for (int i = 0; i < MAP_TOTAL; i++) {
                    if (info[i] > 0)
                        printf("%s: %d\n", tile_to_string(i), info[i]);
                }
                free(info);
            }
        }
    }
    else if (strcmp("create", *argv) == 0) {
        if (*++argv == NULL || argv[1] == NULL || argv[2] == NULL)
            usage();
        if (!isnumber(argv[1]) || !isnumber(argv[2])) {
            printf("Dimension provided is not a positive number\n");
            exit(1);
        }
        int width = atoi(argv[1]), height = atoi(argv[2]);
        if (width == 0) {
            printf("Width cannot be 0!\n");
            exit(1);
        }
        if (height == 0) {
            printf("Height cannot be 0!\n");
            exit(1);
        }
        create_map(*argv, width, height);
        printf("%dx%d map '%s' created successfully\n", width, height, *argv);
        
    }
    else if (strcmp("resize", *argv) == 0) {
        if (*++argv == NULL || argv[1] == NULL || argv[2] == NULL)
            usage();
        int dx = atoi(argv[1]), dy = atoi(argv[2]);
        if (!load_map(*argv)) {
            fprintf(stderr, "Could not load '%s'", *argv);
            exit(1);
        };
        if (!resize(dx, dy)) {
            fprintf(stderr, "Could not resize '%s'", *argv);
            exit(1);
        }
        if (!write_map_to_file(*argv)) {
            fprintf(stderr, "Could not write the map to file %s", *argv);
            exit(1);
        }
        printf("Map '%s' resized by %d %d\n", *argv, dx, dy);
    }
    else if (strcmp("translate", *argv) == 0) {
        if (*++argv == NULL || argv[1] == NULL || argv[2] == NULL)
            usage();
        int x = atoi(argv[1]), y = atoi(argv[2]);
        if (!load_map(*argv)) {
            fprintf(stderr, "Could not load %s", *argv);
            exit(1);
        };
        translate(x, y);
        if (!write_map_to_file(*argv)) {
            fprintf(stderr, "Could not write the map to file %s", *argv);
            exit(1);
        }
        printf("Map '%s' translated by %d and %d successfully\n", *argv, x, y);
    }
    else if(strcmp("help", *argv) == 0 || strcmp("-help", *argv) == 0 || strcmp("--help", *argv) == 0) {
        usage();
    }
    else edit(*argv);
    return 0;
}

