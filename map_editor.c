#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include "map.h"

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

const int CELL_SIZE = 25;

SDL_Window *g_window;
SDL_Renderer *g_renderer;
Texture g_background_texture;
Texture g_leaf_texture;
TTF_Font *g_font;
Texture g_mode_texture;

SDL_Rect g_camera = {
    0,
    0,
    0,
    0
};

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

void setmode(int mode) {
    if (mode == cur_mode) return;
    cur_mode = mode;
    switch (cur_mode) {
        case MAP_WALL:
            g_mode_texture = load_text_texture("Wall");
            break;
        case MAP_FREE:
            g_mode_texture = load_text_texture("Free");
            break;
        case MAP_ENCLOSED:
            g_mode_texture = load_text_texture("Enclosed");
            break;
        case MAP_FOOD:
            g_mode_texture = load_text_texture("Food");
            break;
        default:
            g_mode_texture = load_text_texture("Unknown");
            break;
    }
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
    scp((g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED)),
            "Could not create renderer");
    g_background_texture = load_texture("assets/grass500x500.png");
    g_leaf_texture = load_texture("assets/leaf.png");
    g_font = TTF_OpenFont("assets/OpenSans-Regular.ttf", 50);
    setmode(MAP_WALL);
}

void render_texture(Texture texture, int x, int y) {
    SDL_Rect render_rect;
    render_rect.x = x;
    render_rect.y = y;
    render_rect.h = texture.height;
    render_rect.w = texture.width;
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


int main (int argc, char *argv[]) {

    char *map_path;
    if (argc > 1)
        map_path = argv[1];
    else
        map_path = "assets/map.txt";
    printf("Loading map...\n");
    if (!load_map(map_path)) {
        fprintf(stderr, "Could not load map\n");
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

    SDL_Event event;
    while (!quit) {
        while(SDL_PollEvent(&event) != 0) {
            switch (event.type) {
                case SDL_QUIT:
                    quit = true;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_MIDDLE) mmb_pressed = true;
                    else if (event.button.button == SDL_BUTTON_LEFT) {
                        lmb_pressed = true;
                        int x = event.button.x + g_camera.x, y = event.button.y + g_camera.y; 
                        if (x > 0 && x < level_width && y > 0 && y < level_height)
                            g_map.matrix[y / CELL_SIZE][x / CELL_SIZE] = cur_mode;
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
                        if ((g_camera.x += event.motion.xrel) < 0 || g_camera.x + g_camera.w > level_width) g_camera.x -= event.motion.xrel;
                        if ((g_camera.y += event.motion.yrel) < 0 || g_camera.y + g_camera.h >= level_height) g_camera.y -= event.motion.yrel;
                    }
                    else if (lmb_pressed) {
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
                    render_texture(g_background_texture, x - g_camera.x, y - g_camera.y);
                }
            }
        }

        SDL_SetRenderDrawColor(g_renderer, 0x00, 0x90, 0x00, 0xFF);
        for (int i = 0; i < (g_camera.y + g_camera.h + CELL_SIZE) / CELL_SIZE; i++) {
            for (int j = 0; j < (g_camera.x + g_camera.w + CELL_SIZE) / CELL_SIZE; j++) {
                if (g_map.matrix[i][j] == MAP_WALL) {
                    SDL_Rect coords = {
                        j * CELL_SIZE - g_camera.x,
                        i * CELL_SIZE - g_camera.y,
                        CELL_SIZE,
                        CELL_SIZE
                    };
                    //TODO: compare SDL_RenderFillRect and SDL_FillRect speed
                    SDL_RenderFillRect(g_renderer, &coords);
                }
                else if (g_map.matrix[i][j] == MAP_FOOD) {
                    render_texture(g_leaf_texture, j * CELL_SIZE - g_camera.x, i * CELL_SIZE - g_camera.y);
                }
                else if (g_map.matrix[i][j] == MAP_ENCLOSED) {
                    SDL_Rect coords = {
                        j * CELL_SIZE - g_camera.x,
                        i * CELL_SIZE - g_camera.y,
                        CELL_SIZE,
                        CELL_SIZE
                    };
                    //TODO: compare SDL_RenderFillRect and SDL_FillRect speed
                    SDL_SetRenderDrawColor(g_renderer, 0x00, 0x90, 0x90, 0xFF);
                    SDL_RenderFillRect(g_renderer, &coords);
                    SDL_SetRenderDrawColor(g_renderer, 0x00, 0x90, 0x00, 0xFF);
                }
            }
        }

        //hud
        render_texture(g_mode_texture, 0, 0);

        SDL_RenderPresent(g_renderer);
    }


    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
