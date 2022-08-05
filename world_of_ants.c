#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdbool.h>

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

#define emod(a, b) (((a) % (b)) + (b)) % (b)

//Screen dimension constants
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;
const Uint32 ANT_ANIM_MS = 100;
const Uint32 ANT_MS_TO_MOVE = 10;
const int ANT_VEL_MAX = 2;
const int ANT_TURN_DEGREES = 1;
const int ANT_STEP_LEN = 50;


enum ANT_STATES {ANT_STATE_PREPARE, ANT_STATE_TURN, ANT_STATE_STEP};
//Texture - an SDL_Texture with additional information
typedef struct {
    SDL_Texture *texture_proper;
    int width;
    int height;
} Texture;

//Ant struct hold information needed to draw any ant
//Player and Npc strcut are used to calculate motion. There is a sort of 'inheritance' from Ant

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
} Npc;

typedef struct {
    Ant *ant;
    int vel;
    int turn_vel;
    int width;
    int height;
} Player;

//////////////// GLOBALS ////////////////////////////////////////////////////////

//The window we'll be rendering to
SDL_Window* g_window = NULL;
//The window renderer
SDL_Renderer* g_renderer = NULL;
//Ant texture
Texture g_ant_texture;
//Ant frames rects
#define ANT_FRAMES_NUM 4
SDL_Rect g_antframes[ANT_FRAMES_NUM];

#define ANT_STACK_SIZE 100
int g_ant_sp;
Ant *g_ant_stack[ANT_STACK_SIZE];
//////////////// FUNCTIONS //////////////////////////////////////////////////////

//push an ant onto ant stack
void push_ant(Ant * ant);
//get random int between from and to
int randint(int from, int to);

void init() {

	//Initialize SDL
	scc(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER), "Could not initialize SDL");
    if( !SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "1" ) ) {
        printf( "Warning: Linear texture filtering not enabled!" );
    }

    //Create window
    scp((g_window = SDL_CreateWindow("Can'ts", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN)),
            "Could not create window");
    //Create renderer for window
    scp((g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED)),
            "Could not create renderer");
    //Initialize renderer color
    SDL_SetRenderDrawColor(g_renderer, 0x00, 0x90, 0x00, 0xFF);

    //Initialize PNG loading
    int imgFlags = IMG_INIT_PNG;
    if(!( IMG_Init(imgFlags) & imgFlags)) {
        fprintf(stderr, "SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
        exit(1);
    }
}

//return Texture struct
Texture loadTexture(char *path)
{
	//The final texture
	SDL_Texture *new_texture = NULL;
    SDL_Surface *loaded_surface = NULL;
    Texture texture_struct = {0};

	//Load image at specified path
	scp((loaded_surface = IMG_Load(path)), "Could not load image");
    //Create texture from surface pixels
    scp((new_texture = SDL_CreateTextureFromSurface(g_renderer, loaded_surface)), "Could not create texture from surface");

    texture_struct.texture_proper = new_texture;
    texture_struct.width = loaded_surface->w;
    texture_struct.height = loaded_surface->h;

    //Get rid of old loaded surface
    SDL_FreeSurface(loaded_surface);

	return texture_struct;
}


void loadMedia() {
	//Load PNG texture
    g_ant_texture = loadTexture("images/antspritesheet.png");
    for (int i = 0; i < ANT_FRAMES_NUM; i++) {
        g_antframes[i].x = g_ant_texture.width * i / ANT_FRAMES_NUM;
        g_antframes[i].y = 0;
        g_antframes[i].h = g_ant_texture.height;
        g_antframes[i].w = g_ant_texture.width / ANT_FRAMES_NUM;
    }
}

void closesdl()
{
	//Free loaded image
	SDL_DestroyTexture(g_ant_texture.texture_proper);
    g_ant_texture.texture_proper = NULL;

	//Destroy window	
	SDL_DestroyRenderer(g_renderer);
	SDL_DestroyWindow(g_window);
	g_window = NULL;
	g_renderer = NULL;

	//Quit SDL subsystems
	IMG_Quit();
	SDL_Quit();
}

Ant *create_ant(void) {
    Ant *ant = malloc(sizeof(Ant));
    memset((void *) ant, 0, sizeof(Ant));
    ant->anim_time = SDL_GetTicks();
    ant->x = randint(0, SCREEN_WIDTH - g_ant_texture.width);
    ant->y = randint(0, SCREEN_HEIGHT - g_ant_texture.height);
    ant->scale = (double) rand() / RAND_MAX + 0.75; 
    printf("Ant #%d created at x %d y %d\n", g_ant_sp, (int) ant->x, (int) ant->y);
    return ant;
}

Npc *create_npc(void) {
    Npc *npc = malloc(sizeof(Npc));
    npc->ant = create_ant();
    push_ant(npc->ant);
    npc->state = ANT_STATE_PREPARE;

    return npc;
}

void render_ant_anim(Ant *ant) {
    if (SDL_GetTicks() - ant->anim_time > ANT_ANIM_MS) {
        ant->anim_time = SDL_GetTicks();
        ant->frame = (ant->frame + 1) % ANT_FRAMES_NUM;
    }
    SDL_Rect render_rect;
    render_rect.x = ant->x;
    render_rect.y = ant->y;
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

Uint32 move_player(Uint32 interval, void *player_void) {
    //TODO: figure out whether the compiler optimizes out multiplication by 0
    Player *player = (Player *) player_void;
    float dx=0, dy=0;

    player->ant->angle += player->turn_vel;

    if (player->vel != 0) {
        dx = cosf((player->ant->angle + 90) * M_PI / 180.0);
        dy = sinf((player->ant->angle + 90) * M_PI / 180.0);
        printf("%d degrees\tdx: %lf\tdy: %lf\n", player->ant->angle, dx, dy);
        player->ant->x += player->vel * dx;
        player->ant->y += player->vel * dy;
        if (player->ant->x < 0 || player->ant->x + player->width > SCREEN_WIDTH) {
            player->ant->x -= player->vel;
        }
        if (player->ant->y + player->height> SCREEN_HEIGHT || player->ant->y < 0) {
            player->ant->y -= player->vel;
        }
    }

    return interval;
}

Uint32 move_npc(Uint32 interval, void *npc_void) {

    int target_rotation;
    Npc *npc = (Npc *) npc_void;
    switch (npc->state) {
        case ANT_STATE_PREPARE:
            target_rotation = randint(0, 4) * 45;
            npc->cw = randint(0, 1) * 2 - 1;
            npc->target_angle = emod(npc->ant->angle + target_rotation * npc->cw, 360);
            npc->steps_done = 0;
            npc->state = ANT_STATE_TURN;
            printf("chosen angle: %d + %d * %d\n", npc->ant->angle, target_rotation, npc->cw);
            printf("chosen angle: %d\tcw: %d\n", npc->target_angle, npc->cw);
            break;
        case ANT_STATE_TURN:
            
            if (emod(npc->ant->angle, 360) != npc->target_angle) {
                npc->ant->angle = emod(npc->ant->angle + 5 * npc->cw, 360);
            }
            else
                npc->state = ANT_STATE_STEP;
            printf("cur angle: %d\ttarget: %d\n", npc->ant->angle, npc->target_angle);
            break;
        case ANT_STATE_STEP:
            if (npc->steps_done < ANT_STEP_LEN) {
                npc->steps_done++;
                float rad = (npc->ant->angle + 90) * M_PI / 180.0;
                npc->ant->x += cosf(rad);
                npc->ant->y += sinf(rad);
            }
            else
                npc->state = ANT_STATE_PREPARE;
            printf("cur steps: %d/%d\n", npc->steps_done, ANT_STEP_LEN);
            break;
    }

    return interval;
}

//////////////// MAIN ///////////////////////////////////////////////////////////

int main(void)
{
	//Start up SDL and create window
    init();
    //load needed media
    loadMedia();
    //Main loop flag
    bool quit = false;

    //Event handler
    SDL_Event event;

    Player player = {0};
    player.ant = create_ant();
    push_ant(player.ant);
    player.ant->x = 300;
    player.ant->y = 300;
    player.width = g_ant_texture.width / ANT_FRAMES_NUM;
    player.height = g_ant_texture.height;

    //call move_player each ANT_MS_TO_MOVE sec
    SDL_AddTimer(ANT_MS_TO_MOVE, move_player, (void *) &player);

    //While application is running
    while(!quit) {
        //Handle events on queue
        while(SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LCTRL) {
                Npc *npc = create_npc();
                SDL_AddTimer(ANT_MS_TO_MOVE, move_npc, (void *) npc);
            }
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                switch (event.key.keysym.sym) {
                    case SDLK_w: 
                        player.vel -= ANT_VEL_MAX;
                        break;
                    case SDLK_s: 
                        player.vel += ANT_VEL_MAX;
                        break;
                    case SDLK_a: 
                        player.turn_vel -= ANT_TURN_DEGREES;
                        break;
                    case SDLK_d: 
                        player.turn_vel += ANT_TURN_DEGREES;
                        break;
                }
            }
            if (event.type == SDL_KEYUP && event.key.repeat == 0) {
                switch (event.key.keysym.sym) {
                    case SDLK_w: 
                        player.vel += ANT_VEL_MAX;
                        break;
                    case SDLK_s: 
                        player.vel -= ANT_VEL_MAX;
                        break;
                    case SDLK_a: 
                        player.turn_vel += ANT_TURN_DEGREES;
                        break;
                    case SDLK_d: 
                        player.turn_vel -= ANT_TURN_DEGREES;
                        break;
                }
            }
            //User requests quit
            if(event.type == SDL_QUIT) {
                quit = true;
            }
        }

        //Clear screen
        SDL_RenderClear(g_renderer);
        for (int i = 0; i < g_ant_sp; i++) {
            render_ant_anim(g_ant_stack[i]);
        }
        //Update screen
        SDL_RenderPresent(g_renderer);
    }

	//Free resources and close SDL
	closesdl();

	return 0;
}

//TODO:make this stack dynamically growing
//Until then it leaks memory by creating pointers outside of the stack
void push_ant(Ant *ant) {
    if (g_ant_sp < ANT_STACK_SIZE) {
        g_ant_stack[g_ant_sp++] = ant;
    }
}

int randint(int from, int to) {
    return from + rand() % (to - from + 1);
}

