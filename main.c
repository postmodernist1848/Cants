#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include "map.h"
#include "cants_config.h"

#define scp(pointer, message) {                                               \
    if (pointer == NULL) {                                                    \
        SDL_Log("Error: %s! SDL_Error: %s", message, SDL_GetError()); \
        exit(1);                                                              \
    }                                                                         \
}

#define scc(code, message) {                                                  \
    if (code < 0) {                                                           \
        SDL_Log("Error: %s! SDL_Error: %s", message, SDL_GetError()); \
        exit(1);                                                              \
    }                                                                         \
}

#define imgcp(pointer, message) { if (pointer == NULL) {SDL_Log("Error: %s! IMG_Error: %s", message, IMG_GetError()); exit(1);}}
#define imgcc(code, message) { if (code < 0) {SDL_Log("Error: %s! IMG_Error: %s", message, IMG_GetError()); exit(1);}}
#define ttfcp(pointer, message) { if (pointer == NULL) {SDL_Log("Error: %s! TTF_Error: %s", message, TTF_GetError()); exit(1);}}
#define ttfcc(code, message) { if (code < 0) {SDL_Log("Error: %s! TTF_Error: %s", message, TTF_GetError()); exit(1);}}

#define emod(a, b) (((a) % (b)) + (b)) % (b)

int screen_width = 1920;
int screen_height = 1080;
const int LEVEL_WIDTH = 3000;
const int LEVEL_HEIGHT = 3000;
const Uint32 ANT_ANIM_MS = 100;
const Uint32 ANT_MS_TO_MOVE = 10;
const int ANT_VEL_MAX = 2;
const int ANT_TURN_DEGREES = 1;
const int CELL_SIZE = 50;
const int ANT_STEP_LEN = CELL_SIZE;
const int UNIVERSAL_FOOD_COUNT = 40;

enum ANT_STATES {ANT_STATE_PREPARE, ANT_STATE_TURN, ANT_STATE_STEP};

//Texture - an SDL_Texture with additional information
typedef struct {
    SDL_Texture *texture_proper;
    int width;
    int height;
} Texture;

//Ant structs hold information needed to draw any ant
//Player and Npc structs are used to calculate motion. There is a sort of 'inheritance' from Ant

typedef struct {
    int8_t frame;
    Uint32 anim_time;
    float x;
    float y;
    int angle;
    float scale;
} Ant;

typedef struct {
    Ant *ant;
    enum ANT_STATES state;
    int target_angle;
    int8_t cw;
    int steps_done;
    int gm_x; //game coordinates
    int gm_y;
} Npc;

typedef struct {
    Ant *ant;
    int vel;
    int turn_vel;
    int width;
    int height;
    int food_count;
    uint8_t in_anthill : 1;
    uint8_t won : 1;
} Player;


typedef struct {
    int x;
    int y;
    int gm_x;
    int gm_y;
    int level;
} Anthill;

//////////////// GLOBALS ////////////////////////////////////////////////////////

SDL_Window* g_window;

SDL_Renderer* g_renderer;

Uint32 g_eventstart;

Texture g_leaf_texture;
Texture g_background_texture;
Texture g_ant_texture;
Texture g_food_count_texture;
Texture g_anthill_texture;
Texture g_anthill_icon_texture;
Texture g_upgrade_prompt_texture;
Texture g_anthill_level_texture;
TTF_Font *g_font;

int g_world_food_count;

//Ant frames clip rects
#define ANT_FRAMES_NUM 4
SDL_Rect g_antframes[ANT_FRAMES_NUM];

//remember the inverted y axis
Point g_ant_move_table[8] = {
    {0, -1}, //0
    {1, -1}, //45
    {1,  0}, //90
    {1,  1}, //135
    {0,  1}, //180
    {-1, 1}, //225
    {-1, 0}, //270
    {-1,-1}  //315
};

#define MAX_LEVEL 10
int g_levels_table[MAX_LEVEL + 1] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 100};

SDL_Rect g_camera = {
    0,
    0,
    0,
    0
};

size_t g_ant_sp;
Ant **g_ant_stack = NULL;

//////////////// FUNCTIONS //////////////////////////////////////////////////////

//push an ant onto ant stack
bool push_ant(Ant * ant);

//create a dynamically allocated stack which holds ants and is used for rendering them all
Ant **init_ant_stack(void);

//check collision of two axis aligned rectangles
bool check_collision(SDL_Rect x, SDL_Rect y);

void init(void) {

	scc(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER), "Could not initialize SDL");
    if( !SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "1" ) ) {
        SDL_Log("Warning: Linear texture filtering not enabled!");
    }
    ttfcc(TTF_Init(), "Could not initialize SDL_ttf");
#if ANDROID_BUILD
    scp((g_window = SDL_CreateWindow("Can'ts", 0, 0, screen_width, screen_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN)),
            "Could not create window");
#else
    scp((g_window = SDL_CreateWindow("Can'ts", 0, 0, screen_width, screen_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE)),
            "Could not create window");
#endif
    //TODO:some functions like this may fail
    SDL_GetWindowSize(g_window, &screen_width, &screen_height);
    g_camera.w = screen_width;
    g_camera.h = screen_height;
    //Create renderer for window
    scp((g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)),
            "Could not create renderer");

    int imgFlags = IMG_INIT_PNG;
    if(!( IMG_Init(imgFlags) & imgFlags)) {
        SDL_Log("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
        exit(1);
    }
    //init stack with ant pointers
    if (init_ant_stack() == NULL) {
        SDL_Log("Error: Could not initialize ant stack!");
        exit(1);
    }
}

//return Texture struct
Texture load_texture(const char *path)
{
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

//return Texture struct
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

void load_media() {
    g_ant_texture = load_texture(ASSETS_PREFIX"antspritesheet.png");
    for (int i = 0; i < ANT_FRAMES_NUM; i++) {
        g_antframes[i].x = g_ant_texture.width * i / ANT_FRAMES_NUM;
        g_antframes[i].y = 0;
        g_antframes[i].h = g_ant_texture.height;
        g_antframes[i].w = g_ant_texture.width / ANT_FRAMES_NUM;
    }
    g_background_texture = load_texture(ASSETS_PREFIX"grass500x500.png");
    //<a href="https://www.freepik.com/vectors/cartoon-grass">Cartoon grass vector created by babysofja - www.freepik.com</a>
    g_leaf_texture = load_texture(ASSETS_PREFIX"leaf.png");
    g_font = TTF_OpenFont(ASSETS_PREFIX"OpenSans-Regular.ttf", 50);
    g_food_count_texture = load_text_texture("0/10");
    g_anthill_texture = load_texture(ASSETS_PREFIX"anthill.png");
    g_anthill_icon_texture = load_texture(ASSETS_PREFIX"anthill_icon.png");
    g_upgrade_prompt_texture = load_text_texture("Press Space to upgrade");
    g_anthill_level_texture = load_text_texture("1/10");
}

void closesdl()
{
	//Free loaded image
	SDL_DestroyTexture(g_ant_texture.texture_proper);
    g_ant_texture.texture_proper = NULL;
    SDL_DestroyTexture(g_background_texture.texture_proper);
    g_background_texture.texture_proper = NULL;
    SDL_DestroyTexture(g_leaf_texture.texture_proper);
    g_background_texture.texture_proper = NULL;
    SDL_DestroyTexture(g_food_count_texture.texture_proper);
    g_background_texture.texture_proper = NULL;
    SDL_DestroyTexture(g_anthill_texture.texture_proper);
    g_background_texture.texture_proper = NULL;
    SDL_DestroyTexture(g_anthill_icon_texture.texture_proper);


	//Destroy window
	SDL_DestroyRenderer(g_renderer);
	SDL_DestroyWindow(g_window);
	g_window = NULL;
	g_renderer = NULL;

    TTF_CloseFont(g_font);
	//Quit SDL subsystems
	IMG_Quit();
    TTF_Quit();
	SDL_Quit();
}

Ant *create_ant(int x, int y) {
    Ant *ant = malloc(sizeof(Ant));
    if (ant == NULL) {
        return NULL;
    }
    memset((void *) ant, 0, sizeof(Ant));
    ant->anim_time = SDL_GetTicks();
    ant->x = x;
    ant->y = y;

    ant->scale = (double) rand() / RAND_MAX + 0.75;
#if DEBUGMODE
    SDL_Log("Ant #%ld created at x %d y %d\n", g_ant_sp, (int) ant->x, (int) ant->y);
#endif
    return ant;
}

void render_player_anim(Player *player) {
    if (SDL_GetTicks() - player->ant->anim_time > ANT_ANIM_MS && (player->vel != 0 || player->turn_vel != 0)) {
        player->ant->anim_time = SDL_GetTicks();
        player->ant->frame = (player->ant->frame + 1) % ANT_FRAMES_NUM;
    }
    SDL_Rect render_rect;
    render_rect.x = player->ant->x - g_camera.x - (float) g_ant_texture.width / ANT_FRAMES_NUM / 2;
    render_rect.y = player->ant->y - g_camera.y - (float) g_ant_texture.height / 2;
    render_rect.h = g_antframes[0].h * player->ant->scale;
    render_rect.w = g_antframes[0].w * player->ant->scale;
    SDL_RenderCopyEx(g_renderer, g_ant_texture.texture_proper, &g_antframes[player->ant->frame], &render_rect, player->ant->angle, NULL, SDL_FLIP_NONE);
}

void render_ant_anim(Ant *ant) {
    if (SDL_GetTicks() - ant->anim_time > ANT_ANIM_MS) {
        ant->anim_time = SDL_GetTicks();
        ant->frame = (ant->frame + 1) % ANT_FRAMES_NUM;
    }
    SDL_Rect render_rect;
    render_rect.x = ant->x - g_camera.x - (float) g_ant_texture.width / ANT_FRAMES_NUM / 2;
    render_rect.y = ant->y - g_camera.y - (float) g_ant_texture.height / 2;
    render_rect.h = g_antframes[0].h * ant->scale;
    render_rect.w = g_antframes[0].w * ant->scale;
    SDL_RenderCopyEx(g_renderer, g_ant_texture.texture_proper, &g_antframes[ant->frame], &render_rect, ant->angle, NULL, SDL_FLIP_NONE);
}

void render_texture(Texture texture, int x, int y) {
    SDL_Rect render_rect;
    render_rect.x = x;
    render_rect.y = y;
    render_rect.h = texture.height;
    render_rect.w = texture.width;
    SDL_RenderCopy(g_renderer, texture.texture_proper, NULL, &render_rect);
}

void set_camera(Player *player) {
    //Center the camera over the player
    g_camera.x = ((int) player->ant->x + g_ant_texture.width / (2 * ANT_FRAMES_NUM)) - screen_width / 2;
    g_camera.y = ((int) player->ant->y + g_ant_texture.height / 2) - screen_height / 2;

    //Keep the camera in bounds
    if(g_camera.x < 0) {
        g_camera.x = 0;
    }
    if(g_camera.y < 0) {
        g_camera.y = 0;
    }
    if(g_camera.x > LEVEL_WIDTH - g_camera.w) {
        g_camera.x = LEVEL_WIDTH - g_camera.w;
    }
    if(g_camera.y > LEVEL_HEIGHT - g_camera.h) {
        g_camera.y = LEVEL_HEIGHT - g_camera.h;
    }
}

void remove_food(int8_t *cell) {
    *cell = MAP_FREE;
    SDL_Event event;
    SDL_UserEvent userevent;
    event.type = SDL_USEREVENT;
    userevent.type = g_eventstart;
    event.user = userevent;
    SDL_PushEvent(&event);
}

Uint32 move_player(Uint32 interval, void *player_void) {
    //TODO: figure out whether the compiler optimizes out multiplication by 0
    Player *player = (Player *) player_void;
    float dx=0, dy=0;

    if (player->vel >= 0)
        player->ant->angle += player->turn_vel;
    else
        player->ant->angle -= player->turn_vel;

    if (player->vel != 0) {
        dx = cosf((player->ant->angle - 90) * M_PI / 180.0);
        dy = sinf((player->ant->angle - 90) * M_PI / 180.0);
        player->ant->x += player->vel * dx;
        player->ant->y += player->vel * dy;
        //collision checks
        //TODO: accessing the map with the player outside of the map may segfault
        //Circular collision might be worth it
        int8_t *cell = &g_map.matrix[(int) player->ant->y / CELL_SIZE][(int) player->ant->x / CELL_SIZE];
        switch (*cell) {
            case MAP_FREE:
                player->in_anthill = false;
                break;
            case MAP_ANTHILL:
                player->in_anthill = true;
                /* FALLTHRU */
            case MAP_WALL:
                player->ant->x -= player->vel * dx;
                player->ant->y -= player->vel * dy;
                break;
            case MAP_FOOD:
                remove_food(cell);
                break;
        }
    }

    return interval;
}

void update_food_count_texture(int food_count, int next_level) {
    char str[22];
    sprintf(str, "%d/%d", food_count, next_level);
    SDL_DestroyTexture(g_food_count_texture.texture_proper);
    g_food_count_texture = load_text_texture(str);
}
void update_anthill_level_texture(int level) {
    char str[22];
    sprintf(str, "%d/%d", level, MAX_LEVEL);
    SDL_DestroyTexture(g_anthill_level_texture.texture_proper);
    g_anthill_level_texture = load_text_texture(str);
}

Uint32 move_npc(Uint32 interval, void *npc_void) {

    int target_rotation;
    Npc *npc = (Npc *) npc_void;
    switch (npc->state) {
        case ANT_STATE_PREPARE:
            target_rotation = rand() % 5 * 45;
            npc->cw = rand() % 2 * 2 - 1;
            npc->target_angle = emod(npc->ant->angle + target_rotation * npc->cw, 360);
            npc->steps_done = 0;

            //next game cell
            int gm_x_next = npc->gm_x + g_ant_move_table[npc->target_angle / 45].x;
            int gm_y_next = npc->gm_y + g_ant_move_table[npc->target_angle / 45].y;

            //TODO: may segfault on boundary of the map
            if (g_map.matrix[gm_y_next][gm_x_next] != MAP_WALL && g_map.matrix[gm_y_next][gm_x_next] != MAP_ANTHILL) {
                npc->gm_x = gm_x_next;
                npc->gm_y = gm_y_next;
                npc->state = ANT_STATE_TURN;
            }
            break;

        case ANT_STATE_TURN:

            if (emod(npc->ant->angle, 360) != npc->target_angle) {
                npc->ant->angle = emod(npc->ant->angle + 5 * npc->cw, 360);
            }
            else
                npc->state = ANT_STATE_STEP;
            break;
        case ANT_STATE_STEP:
            if (npc->steps_done < ANT_STEP_LEN) {
                npc->steps_done++;
                float rad = (npc->ant->angle - 90) * M_PI / 180.0;
                if (npc->ant->angle % 90 == 0) {
                    npc->ant->x += cosf(rad);
                    npc->ant->y += sinf(rad);
                }
                else {
                    npc->ant->x += cosf(rad) * M_SQRT2;
                    npc->ant->y += sinf(rad) * M_SQRT2;
                }
            }
            else {
                //correction

                npc->ant->x = npc->gm_x * CELL_SIZE + (float) CELL_SIZE / 2;
                npc->ant->y = npc->gm_y * CELL_SIZE + (float) CELL_SIZE / 2;
                int8_t *cell = &g_map.matrix[(int) npc->ant->y / CELL_SIZE][(int) npc->ant->x / CELL_SIZE];
                if (*cell == MAP_FOOD) {
                    remove_food(cell);
                }
                npc->state = ANT_STATE_PREPARE;
            }
            break;
    }
    return interval;
}

Npc *create_npc(int gm_x, int gm_y) {
    Npc *npc = malloc(sizeof(Npc));
    if (npc == NULL) return NULL;
    npc->ant = create_ant((gm_x + 0.5) * CELL_SIZE, (gm_y + 0.5) * CELL_SIZE);
    if (npc->ant == NULL) {
        free(npc);
        SDL_Log("Warning: Could not allocate memory for an npc ant");
        return NULL;
    }
    if (!push_ant(npc->ant)) {
        SDL_Log("Warning: could not push npc ant\n");
        free(npc->ant);
        free(npc);
        return NULL;
    }
    npc->gm_x = gm_x;
    npc->gm_y = gm_y;
    npc->state = ANT_STATE_PREPARE;
    SDL_AddTimer(ANT_MS_TO_MOVE, move_npc, (void *) npc);
    return npc;
}

//TODO:create food outside the camera only
void create_food(void) {
    Point point = find_random_free_spot_on_a_map();
    g_map.matrix[point.y][point.x] = MAP_FOOD;
    g_world_food_count++;
}

//coordinates of the entrance (where the ants spawn)
void init_anthill(Anthill *anthill) {
    anthill->x = LEVEL_WIDTH / 2 - CELL_SIZE * 1;
    anthill->y = LEVEL_HEIGHT / 2;

    int gm_x = anthill->gm_x = anthill->x / CELL_SIZE + 1;
    int gm_y = anthill->gm_y = anthill->y / CELL_SIZE;

    g_map.matrix[gm_y][gm_x - 1] = MAP_ANTHILL;
    g_map.matrix[gm_y][gm_x] = MAP_ANTHILL;
    g_map.matrix[gm_y][gm_x + 1] = MAP_ANTHILL;

    g_map.matrix[gm_y + 1][gm_x - 1] = MAP_ANTHILL;
    g_map.matrix[gm_y + 1][gm_x] = MAP_ANTHILL;
    g_map.matrix[gm_y + 1][gm_x + 1] = MAP_ANTHILL;

    g_map.matrix[gm_y + 2][gm_x - 1] = MAP_ANTHILL;
    g_map.matrix[gm_y + 2][gm_x] = MAP_ANTHILL;
    g_map.matrix[gm_y + 2][gm_x + 1] = MAP_ANTHILL;
}


Texture win(void) {
Texture win_texture = load_text_texture("Congratulations! You won!");
return win_texture;
}

void render_game_objects(Player *player, Anthill *anthill) {
        SDL_SetRenderDrawColor(g_renderer, 0x00, 0x90, 0x00, 0xFF);
        SDL_RenderClear(g_renderer);

        //render background texture tiles (only those that are on the screen)
        for (int y = 0; y < LEVEL_HEIGHT; y += g_background_texture.height) {
            for (int x = 0; x < LEVEL_WIDTH; x += g_background_texture.width) {
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

        render_player_anim(player);
        set_camera(player);

        //render ants which are on the screen
        for (size_t i = 0; i < g_ant_sp; i++) {
            SDL_Rect coords = {
                g_ant_stack[i]->x,
                g_ant_stack[i]->y,
                g_ant_texture.width / ANT_FRAMES_NUM,
                g_ant_texture.height
            };
            if (check_collision(coords, g_camera)) {
                render_ant_anim(g_ant_stack[i]);
            }
        }


        for (int i = g_camera.y / CELL_SIZE; i < (g_camera.y + g_camera.h + CELL_SIZE) / CELL_SIZE && i < g_map.height; i++) {
            for (int j = g_camera.x / CELL_SIZE; j < (g_camera.x + g_camera.w + CELL_SIZE) / CELL_SIZE && j < g_map.width; j++) {
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
            }
        }
        //render anthill
        render_texture(g_anthill_texture, anthill->x - g_camera.x, anthill->y - g_camera.y);

        //draw HUD
        //maybe draw a single picture instead of many rect (also would be more pretty)
        SDL_SetRenderDrawColor(g_renderer, 0x50, 0x50, 0x50, 0xFF);
        SDL_Rect hud = {0, screen_height * 14 / 15, screen_width, screen_height / 15};
        SDL_RenderFillRect(g_renderer, &hud);
        SDL_SetRenderDrawColor(g_renderer, 0x90, 0xCC, 0x90, 0xFF);
        SDL_Rect space_for_hud1 = {screen_width / 20, screen_height * 44 / 45 - g_food_count_texture.height / 2,
            screen_width * 19/ 20, g_leaf_texture.height};
        SDL_RenderFillRect(g_renderer, &space_for_hud1);
        render_texture(g_leaf_texture, screen_width / 20, screen_height * 34 / 35 - g_leaf_texture.height / 2);
        render_texture(g_food_count_texture, screen_width / 10, screen_height * 34 / 35 - g_food_count_texture.height / 2 - 5);

        render_texture(g_anthill_icon_texture, screen_width * 4 / 5, screen_height * 34 / 35 - g_anthill_icon_texture.height / 2);
        render_texture(g_anthill_level_texture, screen_width * 4 / 5 + g_anthill_icon_texture.width, screen_height * 34 / 35 - g_food_count_texture.height / 2 - 5);

}

void toggle_fullscreen(void) {
    Uint32 FullscreenFlag = SDL_WINDOW_FULLSCREEN;
    bool IsFullscreen = SDL_GetWindowFlags(g_window) & FullscreenFlag;
    SDL_SetWindowFullscreen(g_window, IsFullscreen ? 0 : FullscreenFlag);
    SDL_ShowCursor(IsFullscreen);
}

//////////////// MAIN ///////////////////////////////////////////////////////////

//!TODO: use SDL file reading to open the map file on Android
//TODO: tutorial
//TODO: better player anchor when determing collision
//TODO: make npcs prioretise MAP_FOOD tiles when choosing direction
//TODO: create food on the screen only
//TODO: fix rendering flickering issue
//TODO: show entire map after win-state achieved

int main(int argc, char *argv[]) {
#if ANDROID_BUILD
    (void) argc;
    (void) argv;
    if (!load_map()) {
        SDL_Log("Could not load map\n");
        exit(1);
    }
    else
        SDL_Log("Map %dx%d loaded successfully!\n", g_map.width, g_map.height);
#else
    char *map_path;
    if (argc > 1)
        map_path = argv[1];
    else
        map_path = "assets/map.txt";
    SDL_Log("Loading map...\n");
    if (!load_map(map_path)) {
        SDL_Log("Could not load map\n");
        exit(1);
    }
    else
        SDL_Log("Map %dx%d loaded successfully!\n", g_map.width, g_map.height);
#endif

    Anthill anthill = {0};
    init_anthill(&anthill);

    init();
    load_media();

    bool quit = false;

    g_eventstart = SDL_RegisterEvents(1);

    SDL_Event event;

    Player player = {0};
    player.ant = create_ant(LEVEL_WIDTH / 2, LEVEL_HEIGHT / 2);
    if (player.ant == NULL) {
        SDL_Log("Error: could not allocate memory for player ant\n");
        exit(1);
    }

    player.ant->scale=1.59;
    player.width = g_ant_texture.width / ANT_FRAMES_NUM;
    player.height = g_ant_texture.height;
    set_camera(&player);

    while(g_world_food_count < UNIVERSAL_FOOD_COUNT) {
        create_food();
    }

    //call move_player each ANT_MS_TO_MOVE sec
    SDL_AddTimer(ANT_MS_TO_MOVE, move_player, (void *) &player);

    //While application is running
    while(!quit) {
        //Handle events on queue
        while(SDL_PollEvent(&event) != 0) {
            switch (event.type) {
#if ANDROID_BUILD
                case SDL_FINGERDOWN:;
                    int x = event.tfinger.x * screen_width, y = event.tfinger.y * screen_height;
                    if (anthill.x <= x + g_camera.x && x + g_camera.x <= anthill.x + g_anthill_texture.width &&
                        anthill.y * CELL_SIZE <= y + g_camera.y && y + g_camera.y <= anthill.y + g_anthill_texture.height) {
                        //tapped on the anthill
                        if (player.in_anthill && player.food_count >= g_levels_table[anthill.level] && anthill.level < MAX_LEVEL) {
                            player.food_count -= g_levels_table[anthill.level];
                            update_food_count_texture(player.food_count, g_levels_table[anthill.level + 1]);
                            for (int i = 0; i < g_levels_table[anthill.level] / 2; i++)
                                if (create_npc(anthill.gm_x, anthill.gm_y) == NULL)
                                    SDL_Log("Warning: could not create NPC ant\n");
                            update_anthill_level_texture(++anthill.level);
                            if (anthill.level == MAX_LEVEL) {
                                goto win;
                            }
                        }
                    }
                    else if (event.tfinger.x <= 1.0 / 3) {
                        player.turn_vel = -ANT_TURN_DEGREES;
                    }

                    else if (event.tfinger.x >= 2.0 / 3) {
                        player.turn_vel = ANT_TURN_DEGREES;
                    }
                    else if (event.tfinger.y <= 0.5)
                        if (player.vel < 0)
                            player.vel = 0;
                        else
                            player.vel = ANT_VEL_MAX;
                    else
                        if (player.vel > 0)
                            player.vel = 0;
                        else
                            player.vel = -ANT_VEL_MAX / 2;

                    break;
                case SDL_FINGERUP:
                    if (event.tfinger.x <= 1.0 / 3)
                        player.turn_vel = 0;
                        //left
                    else if (event.tfinger.x >= 2.0 / 3)
                        player.turn_vel = 0;
                        //right
                    break;
#else
                case SDL_KEYDOWN:
                switch (event.key.keysym.scancode) {
#if DEBUGMODE
                    //cheats for developers
                    case SDL_SCANCODE_LCTRL:
                        if (create_npc(anthill.gm_x, anthill.gm_y) == NULL)
                            SDL_Log("Warning: Could not allocate memory for an npc ant");
                        break;
                    case SDL_SCANCODE_RCTRL:
                        player.food_count++;
                        update_food_count_texture(player.food_count, g_levels_table[anthill.level]);
                        break;
#endif
                    case SDL_SCANCODE_SPACE:
                        //upgrade
                        if (player.in_anthill && player.food_count >= g_levels_table[anthill.level] && anthill.level < MAX_LEVEL) {
                            player.food_count -= g_levels_table[anthill.level];
                            update_food_count_texture(player.food_count, g_levels_table[anthill.level + 1]);
                            for (int i = 0; i < g_levels_table[anthill.level] / 2; i++)
                                if (create_npc(anthill.gm_x, anthill.gm_y) == NULL)
                                    SDL_Log("Warning: could not create NPC ant\n");
                            update_anthill_level_texture(++anthill.level);
                            if (anthill.level == MAX_LEVEL) {
                                goto win;
                            }
                        }
                        break;
                    case SDL_SCANCODE_F11:
                        toggle_fullscreen();
                        break;
                    }
                    if (event.key.repeat == 0)
                        switch (event.key.keysym.scancode) {
                            case SDL_SCANCODE_W:
                                player.vel += ANT_VEL_MAX;
                                break;
                            case SDL_SCANCODE_S:
                                player.vel -= ANT_VEL_MAX / 2;
                                break;
                            case SDL_SCANCODE_A:
                                player.turn_vel -= ANT_TURN_DEGREES;
                                break;
                            case SDL_SCANCODE_D:
                                player.turn_vel += ANT_TURN_DEGREES;
                                break;
                    }
                    break;
                case SDL_KEYUP:
                    switch (event.key.keysym.scancode) {
                        case SDL_SCANCODE_W:
                            player.vel -= ANT_VEL_MAX;
                            break;
                        case SDL_SCANCODE_S:
                            player.vel += ANT_VEL_MAX / 2;
                            break;
                        case SDL_SCANCODE_A:
                            player.turn_vel += ANT_TURN_DEGREES;
                            break;
                        case SDL_SCANCODE_D:
                            player.turn_vel -= ANT_TURN_DEGREES;
                            break;
                    }
                    break;
#endif
                case SDL_WINDOWEVENT:
                      if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        g_camera.w = screen_width = event.window.data1;
                        g_camera.h = screen_height = event.window.data2;
#if ANDROID_BUILD
                        SDL_SetWindowFullscreen(g_window, SDL_WINDOW_FULLSCREEN);
#endif
                      }
                    break;
                case SDL_QUIT:
                    quit = true;
                    break;
                case SDL_USEREVENT:
                    //only friendly ants currently
                    player.food_count++;
                    update_food_count_texture(player.food_count, g_levels_table[anthill.level]);
                    create_food();
                    break;
            }
        }
        render_game_objects(&player, &anthill);
        SDL_RenderPresent(g_renderer);
    }

	//Free resources and close SDL
	closesdl();
	return 0;

win:;
    Texture win_texture = win();
    while(!quit) {
            while(SDL_PollEvent(&event) != 0) {
                switch (event.type) {
                    case SDL_QUIT:
                        quit = true;
                        break;
                //partially copypasted from main event loop which is a problem, will find a fix later
                case SDL_WINDOWEVENT:
                      if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        g_camera.w = screen_width = event.window.data1;
                        g_camera.h = screen_height = event.window.data2;
#if ANDROID_BUILD
                        SDL_SetWindowFullscreen(g_window, SDL_WINDOW_FULLSCREEN);
#endif
                      }
                    break;
                }
            }

        render_game_objects(&player, &anthill);
        render_texture(win_texture, screen_width / 2 - win_texture.width / 2, screen_height / 2 - win_texture.height / 2);
        SDL_RenderPresent(g_renderer);
    }
    closesdl();
    return 0;
}

#define ANT_STACK_INIT_SIZE 10
Ant ** init_ant_stack(void) {
    return g_ant_stack = malloc(ANT_STACK_INIT_SIZE * sizeof(Ant *));
}

bool push_ant(Ant *ant) {
    static size_t stack_size = ANT_STACK_INIT_SIZE;
    if (g_ant_sp < stack_size) {
        g_ant_stack[g_ant_sp++] = ant;
    }
    else {
        if ((g_ant_stack = realloc((void *) g_ant_stack, stack_size * 2 * sizeof(Ant *))) == NULL) {
            return false;
        }
        stack_size += 2;
        g_ant_stack[g_ant_sp++] = ant;

    }
    return true;
}

bool check_collision(SDL_Rect a, SDL_Rect b) {
    if(a.y + a.h <= b.y  ||
        a.y >= b.y + b.h ||
        a.x + a.w <= b.x ||
        a.x >= b.x + b.w)
        return false;
    return true;
}

