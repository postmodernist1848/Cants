#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

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

#define imgcp(pointer, message) { if (pointer == NULL) {fprintf(stderr, "Error: %s! TTF_Error: %s", message, TTF_GetError()); exit(1);}}
#define imgcc(code, message) { if (code < 0) {fprintf(stderr, "Error: %s! TTF_Error: %s", message, TTF_GetError()); exit(1);}}
#define ttfcp(pointer, message) { if (pointer == NULL) {fprintf(stderr, "Error: %s! TTF_Error: %s", message, TTF_GetError()); exit(1);}}
#define ttfcc(code, message) { if (code < 0) {fprintf(stderr, "Error: %s! TTF_Error: %s", message, TTF_GetError()); exit(1);}}

#define emod(a, b) (((a) % (b)) + (b)) % (b)

const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;
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
enum MAP {MAP_FREE, MAP_WALL, MAP_FOOD};
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
} Player;

typedef struct {
    int8_t **matrix;
    int width;
    int height;
} Map;

typedef struct {
    short x;
    short y;
} Point;

//////////////// GLOBALS ////////////////////////////////////////////////////////

//The window we'll be rendering to
SDL_Window* g_window;
//The window renderer
SDL_Renderer* g_renderer;

Uint32 g_eventstart;

Texture g_leaf_texture;
Texture g_background_texture;
Texture g_ant_texture;
Texture g_food_count_texture;
Texture g_anthill_texture;
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

//Camera rect
SDL_Rect g_camera = {
    LEVEL_WIDTH / 2,
    LEVEL_HEIGHT / 2,
    SCREEN_WIDTH,
    SCREEN_HEIGHT
};

Map g_map = {0};

size_t g_ant_sp;
Ant **g_ant_stack = NULL;
//////////////// FUNCTIONS //////////////////////////////////////////////////////

//push an ant onto ant stack
bool push_ant(Ant * ant);
//get random int between from and to
int randint(int from, int to);
Ant **init_ant_stack(void);
bool check_collision(SDL_Rect x, SDL_Rect y);
char *slurp_file(const char *, size_t *size);

void init() {

	//Initialize SDL
	scc(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_RENDERER_PRESENTVSYNC), "Could not initialize SDL");
    if( !SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "1" ) ) {
        fprintf(stderr, "Warning: Linear texture filtering not enabled!");
    }
    ttfcc(TTF_Init() < 0, "Could not initialize SDL_ttf");

    //Create window
    scp((g_window = SDL_CreateWindow("Can'ts", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN)),
            "Could not create window");
    //Create renderer for window
    scp((g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED)),
            "Could not create renderer");

    //Initialize PNG loading
    int imgFlags = IMG_INIT_PNG;
    if(!( IMG_Init(imgFlags) & imgFlags)) {
        fprintf(stderr, "SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
        exit(1);
    }
    //init stack with ant pointers
    if (init_ant_stack() == NULL) {
        fprintf(stderr, "Error: Could not initialize ant stack!");
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
Texture load_text_texture(const char *text)
{
	//The final texture
	SDL_Texture *new_texture = NULL;
    SDL_Surface *text_surface = NULL;
    Texture texture_struct = {0};

	//Load image at specified path
    SDL_Color color = {0xFF, 0xFF, 0xFF, 0xFF};
	ttfcp((text_surface = TTF_RenderText_Solid(g_font, text, color)), "Could not load text surface");
    //Create texture from surface pixels
    scp((new_texture = SDL_CreateTextureFromSurface(g_renderer, text_surface)), "Could not create texture from surface");

    texture_struct.texture_proper = new_texture;
    texture_struct.width = text_surface->w;
    texture_struct.height = text_surface->h;

    //Get rid of old loaded surface
    SDL_FreeSurface(text_surface);

	return texture_struct;
}

void load_media() {
	//Load PNG texture
    g_ant_texture = load_texture("assets/antspritesheet.png");
    for (int i = 0; i < ANT_FRAMES_NUM; i++) {
        g_antframes[i].x = g_ant_texture.width * i / ANT_FRAMES_NUM;
        g_antframes[i].y = 0;
        g_antframes[i].h = g_ant_texture.height;
        g_antframes[i].w = g_ant_texture.width / ANT_FRAMES_NUM;
    }
    g_background_texture = load_texture("assets/grass500x500.png");
    //<a href="https://www.freepik.com/vectors/cartoon-grass">Cartoon grass vector created by babysofja - www.freepik.com</a>
    g_leaf_texture = load_texture("assets/leaf.png");
    g_font = TTF_OpenFont("assets/OpenSans-Regular.ttf", 50);
    g_food_count_texture = load_text_texture("0");
    g_anthill_texture = load_texture("assets/anthill.png");
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
    ant->x = x; //randint(g_camera.x, g_camera.x + SCREEN_WIDTH - g_ant_texture.width / ANT_FRAMES_NUM);
    ant->y = y; //randint(g_camera.y, g_camera.y + SCREEN_HEIGHT - g_ant_texture.height);
    ant->scale = (double) rand() / RAND_MAX + 0.75; 
    printf("Ant #%ld created at x %d y %d\n", g_ant_sp, (int) ant->x, (int) ant->y);
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

void set_camera(Player *player)
{
    //Center the camera over the player
    g_camera.x = ((int) player->ant->x + g_ant_texture.width / (2 * ANT_FRAMES_NUM)) - SCREEN_WIDTH / 2;
    g_camera.y = ((int) player->ant->y + g_ant_texture.height / 2) - SCREEN_HEIGHT / 2;

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
    //currently, there is only one user even, so no data here
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

    if (player->vel <= 0)
        player->ant->angle += player->turn_vel;
    else
        player->ant->angle -= player->turn_vel;

    if (player->vel != 0) {
        dx = cosf((player->ant->angle + 90) * M_PI / 180.0);
        dy = sinf((player->ant->angle + 90) * M_PI / 180.0);
        player->ant->x += player->vel * dx;
        player->ant->y += player->vel * dy;
        //collision checks
        //TODO: accessing the map with the player outside of the map may segfault
        //Circular collision might be worth it
        int8_t *cell = &g_map.matrix[(int) player->ant->y / CELL_SIZE][(int) player->ant->x / CELL_SIZE];
        switch (*cell) {
            case MAP_WALL:
                player->ant->x -= player->vel * dx;
                player->ant->y -= player->vel * dy;
                break;
            case MAP_FOOD:
            player->food_count++;
            remove_food(cell);
                break;
        }
    }
    set_camera(player);

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
            //next game cell
            int gm_x_next = npc->gm_x + g_ant_move_table[npc->target_angle / 45].x;
            int gm_y_next = npc->gm_y + g_ant_move_table[npc->target_angle / 45].y; 
            //TODO: fix - may segfault on boundary of the map
            if (g_map.matrix[gm_y_next][gm_x_next] != MAP_WALL) {
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
        fprintf(stderr, "Warning: Could not allocate memory for an npc ant");
    }
    if (!push_ant(npc->ant)) {
        fprintf(stderr, "Warning: could not push npc ant\n");
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

bool load_map(char *path) {
    char *map_str;
    size_t size;

    map_str = slurp_file(path, &size);
    if (map_str == NULL) {
        fprintf(stderr, "Could not load map file\n");
        return false;
    }
    //calculate line number
    size_t line_length;
    for (line_length = 0; line_length < size && map_str[line_length] != '\n'; line_length++);
    size_t num_lines = (size + 1) / (line_length + 1);
    g_map.matrix = malloc(num_lines * sizeof(int8_t *));
    if (g_map.matrix == NULL) {
        fprintf(stderr, "Could not allocate memory for the map!\n");
        return false;
    }

    //from this point on, line length is measured in tokens (numbers)
    size_t cur_line = 0;
    size_t line_len_in_tokens = 0;
    for (size_t i = 0; i < size; i += line_length + 1) {
        size_t num_count = 0;
        size_t j;
        bool in = false;
        //check number count
        for (j = 0; map_str[i + j] != '\n' && i + j < size; j++) {
            if (in && !isdigit(map_str[i + j])) {
                in = false;
            }
            else if (!in && isdigit(map_str[i + j])) {
                in = true;
                num_count++;
            }
        }
        if (i + j < size) map_str[i + j] = '\0';
        assert(num_count > 0 && "map should have at least one column");
        if (line_len_in_tokens == 0) line_len_in_tokens = num_count;
        if (num_count != line_len_in_tokens) {
            fprintf(stderr, "Wrong map format at line %zu:%zu numbers (lines don't have the same length)!\n", cur_line, num_count);
            return false;
        }
        if (cur_line == num_lines) {
            fprintf(stderr, "Too many lines when parsing map!\n");
            return false;
        }
        g_map.matrix[cur_line] = malloc(line_len_in_tokens * sizeof(int8_t));
        if (g_map.matrix[cur_line] == NULL) {
            fprintf(stderr, "Could not allocate memory for the map!\n");
            return false;
        }
        char *num;
        j = 0;
        num = strtok(&map_str[i], " ");
        g_map.matrix[cur_line][j++] = atoi(num);
        while ((num = strtok(NULL, " ")) != NULL) {
            g_map.matrix[cur_line][j++] = atoi(num);
        }
        cur_line++;
    }
    assert(cur_line == num_lines);
    g_map.width = line_len_in_tokens;
    g_map.height = num_lines;
    
    free(map_str);
    return true;
}

Point find_random_free_spot_on_a_map() {
    short x, y;
    while (g_map.matrix[y = rand() % g_map.height][x = rand() % g_map.width] != MAP_FREE);
    Point point = {x, y};
    return point;
}

//not the most efficient way, but allows to check if there are free points at all
//TODO: index all free spaces and make this function extra fast and safe
Point find_random_free_spot_on_a_map_safe() {
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

//TODO:create food outside the camera only
void create_food(void) {
    Point point = find_random_free_spot_on_a_map();
    g_map.matrix[point.y][point.x] = MAP_FOOD;
    g_world_food_count++;
}

void update_food_count_texture(int food_count) {
    char str[11];
    sprintf(str, "%d", food_count);
    SDL_DestroyTexture(g_food_count_texture.texture_proper);
    g_food_count_texture = load_text_texture(str);
}

//TODO: maybe add another type MAP_ANTHILL
//coordinates of the entrance (where the ants spawn)
void place_anthill(int gm_x, int gm_y) {
    g_map.matrix[gm_y][gm_x - 1] = MAP_WALL;
    g_map.matrix[gm_y][gm_x] = MAP_WALL;
    g_map.matrix[gm_y][gm_x + 1] = MAP_WALL;

    g_map.matrix[gm_y + 1][gm_x - 1] = MAP_WALL;
    g_map.matrix[gm_y + 1][gm_x] = MAP_WALL;
    g_map.matrix[gm_y + 1][gm_x + 1] = MAP_WALL;

    g_map.matrix[gm_y + 2][gm_x - 1] = MAP_WALL;
    g_map.matrix[gm_y + 2][gm_x] = MAP_WALL;
    g_map.matrix[gm_y + 2][gm_x + 1] = MAP_WALL;
}

//////////////// MAIN ///////////////////////////////////////////////////////////

//TODO: tutorial
int main(int argc, char **argv) {

    char *map_path;
    if (argc > 1)
        map_path = argv[1];
    else
        map_path = "map.txt";
    printf("Loading map...\n");
    if (!load_map(map_path)) {
        fprintf(stderr, "Could not load map\n");
    }
    else
        printf("Map %dx%d loaded successfully!\n", g_map.width, g_map.height);

    int anthill_x = LEVEL_WIDTH / 2 - CELL_SIZE * 1, anthill_y = LEVEL_HEIGHT / 2;
    //pos of the entrance
    int anthill_gm_x = anthill_x / CELL_SIZE + 1, anthill_gm_y = anthill_y / CELL_SIZE;
    place_anthill(anthill_gm_x, anthill_gm_y);
	//Start up SDL and create window
    init();
    //load needed media
    load_media();
    //Main loop flag
    bool quit = false;

    g_eventstart = SDL_RegisterEvents(1);

    //Event handler
    SDL_Event event;

    Player player = {0};
    player.ant = create_ant(LEVEL_WIDTH / 2, LEVEL_HEIGHT / 2);
    if (player.ant == NULL) {
        fprintf(stderr, "Error: could not allocate memory for player ant\n");
        exit(1);
    }
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
                case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_LCTRL:
                        if (create_npc(anthill_gm_x, anthill_gm_y) == NULL) {
                            fprintf(stderr, "Warning: could not allocate memory for an npc");
                            continue;
                        }
                        break;
                    }
                    if (event.key.repeat == 0)
                        switch (event.key.keysym.sym) {
                            case SDLK_w: 
                                player.vel -= ANT_VEL_MAX;
                                break;
                            case SDLK_s: 
                                player.vel += ANT_VEL_MAX / 2;
                                break;
                            case SDLK_a: 
                                player.turn_vel -= ANT_TURN_DEGREES;
                                break;
                            case SDLK_d: 
                                player.turn_vel += ANT_TURN_DEGREES;
                                break;

                    }
                    break;
                case SDL_KEYUP:
                    switch (event.key.keysym.sym) {
                        case SDLK_w: 
                            player.vel += ANT_VEL_MAX;
                            break;
                        case SDLK_s: 
                            player.vel -= ANT_VEL_MAX / 2;
                            break;
                        case SDLK_a: 
                            player.turn_vel += ANT_TURN_DEGREES;
                            break;
                        case SDLK_d: 
                            player.turn_vel -= ANT_TURN_DEGREES;
                            break;
                    }
                    break;
                case SDL_QUIT:
                    quit = true;
                    break;
                case SDL_USEREVENT:
                    //food_update
                    update_food_count_texture(player.food_count);
                    create_food();
                    break;
            }
        }
        //Clear screen
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

        render_player_anim(&player);

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

        //only iterate over walls that are on the screen
        for (int i = g_camera.y / CELL_SIZE; i < (g_camera.y + g_camera.h + CELL_SIZE) / CELL_SIZE && i < g_map.height; i++) {
            for (int j = g_camera.x / CELL_SIZE; j < (g_camera.x + g_camera.w + CELL_SIZE) / CELL_SIZE && i < g_map.width; j++) {
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
        render_texture(g_anthill_texture, anthill_x - g_camera.x, anthill_y - g_camera.y);

        //draw HUD
        SDL_SetRenderDrawColor(g_renderer, 0x50, 0x50, 0x50, 0xFF);
        SDL_Rect hud = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT / 10};
        SDL_RenderFillRect(g_renderer, &hud);
        render_texture(g_leaf_texture, SCREEN_WIDTH / 20, SCREEN_HEIGHT / 40);
        render_texture(g_food_count_texture, SCREEN_WIDTH / 10, SCREEN_HEIGHT / 100);

        //Update screen
        SDL_RenderPresent(g_renderer);
    }

	//Free resources and close SDL
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

int randint(int from, int to) {
    return from + rand() % (to - from + 1);
}

bool check_collision(SDL_Rect a, SDL_Rect b) {
    if(a.y + a.h <= b.y  ||
        a.y >= b.y + b.h ||
        a.x + a.w <= b.x ||
        a.x >= b.x + b.w)
        return false;
    return true;
}

char *slurp_file(const char *file_path, size_t *size)
{
    char *buffer = NULL;

    FILE *f = fopen(file_path, "r");
    if (f == NULL) {
        goto error;
    }

    if (fseek(f, 0, SEEK_END) < 0) {
        goto error;
    }

    long m = ftell(f);
    if (m < 0) {
        goto error;
    }

    buffer = malloc(sizeof(char) * m);
    if (buffer == NULL) {
        goto error;
    }

    if (fseek(f, 0, SEEK_SET) < 0) {
        goto error;
    }

    size_t n = fread(buffer, 1, m, f);
    if (n != (size_t) m) {
        goto error;
    }

    if (ferror(f)) {
        goto error;
    }

    *size = n;

    fclose(f);

    return buffer;

error:
    if (f) {
        fclose(f);
    }

    if (buffer) {
        free(buffer);
    }

    return NULL;
}

