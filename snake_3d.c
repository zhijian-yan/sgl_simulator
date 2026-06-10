#include "sgl/sgl.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define HOR_RES 1024
#define VER_RES 768

#define GRID_SIZE 8
#define CELL_SIZE 30
#define MAX_SNAKE_LEN (GRID_SIZE * GRID_SIZE * GRID_SIZE)

#define DIR_RIGHT 0
#define DIR_LEFT  1
#define DIR_UP    2
#define DIR_DOWN  3
#define DIR_FRONT 4
#define DIR_BACK  5

#define STATE_READY 0
#define STATE_PLAYING 1
#define STATE_OVER 2

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    int x, y, z;
} Cell3D;

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;
sgl_screen_t scr;
uint16_t buffer[HOR_RES * VER_RES];

Cell3D snake[MAX_SNAKE_LEN];
int snake_len;
int direction;
int next_direction;
Cell3D food;
int score;
int high_score;
int game_state;
int move_interval;
uint32_t last_time;
uint32_t move_timer;

float cam_rot_x = 0.5f;
float cam_rot_y = 0.7f;
float cam_dist = 400.0f;

#define FOV 500.0f
#define CENTER_X (HOR_RES / 2)
#define CENTER_Y (VER_RES / 2)

void sgl_flush(void *buf, sgl_rect_t *visible);
void sgl_paint(sgl_screen_t *scr);

Vec3 rotate_x(Vec3 v, float angle) {
    float c = cosf(angle), s = sinf(angle);
    Vec3 r;
    r.x = v.x;
    r.y = v.y * c - v.z * s;
    r.z = v.y * s + v.z * c;
    return r;
}

Vec3 rotate_y(Vec3 v, float angle) {
    float c = cosf(angle), s = sinf(angle);
    Vec3 r;
    r.x = v.x * c + v.z * s;
    r.y = v.y;
    r.z = -v.x * s + v.z * c;
    return r;
}

int project(Vec3 v, int *sx, int *sy, float *depth) {
    v.z += cam_dist;
    v = rotate_x(v, cam_rot_x);
    v = rotate_y(v, cam_rot_y);
    
    if (v.z <= 10) return 0;
    
    *depth = v.z;
    *sx = (int)(CENTER_X + v.x * FOV / v.z);
    *sy = (int)(CENTER_Y - v.y * FOV / v.z);
    return 1;
}

Vec3 cell_to_world(int x, int y, int z) {
    Vec3 v;
    float offset = (GRID_SIZE - 1) * CELL_SIZE / 2.0f;
    v.x = x * CELL_SIZE - offset;
    v.y = y * CELL_SIZE - offset;
    v.z = z * CELL_SIZE - offset;
    return v;
}

void spawn_food(void) {
    int occupied;
    do {
        food.x = rand() % GRID_SIZE;
        food.y = rand() % GRID_SIZE;
        food.z = rand() % GRID_SIZE;
        occupied = 0;
        for (int i = 0; i < snake_len; i++) {
            if (snake[i].x == food.x && snake[i].y == food.y && snake[i].z == food.z) {
                occupied = 1;
                break;
            }
        }
    } while (occupied);
}

void init_game(void) {
    snake_len = 3;
    int center = GRID_SIZE / 2;
    snake[0].x = center; snake[0].y = center; snake[0].z = center;
    snake[1].x = center - 1; snake[1].y = center; snake[1].z = center;
    snake[2].x = center - 2; snake[2].y = center; snake[2].z = center;
    direction = DIR_RIGHT;
    next_direction = DIR_RIGHT;
    score = 0;
    move_timer = 0;
    move_interval = 200;
    spawn_food();
}

int move_snake(void) {
    direction = next_direction;
    
    Cell3D new_head = snake[0];
    switch (direction) {
    case DIR_RIGHT: new_head.x++; break;
    case DIR_LEFT:  new_head.x--; break;
    case DIR_UP:    new_head.y++; break;
    case DIR_DOWN:  new_head.y--; break;
    case DIR_FRONT: new_head.z++; break;
    case DIR_BACK:  new_head.z--; break;
    }
    
    if (new_head.x < 0 || new_head.x >= GRID_SIZE ||
        new_head.y < 0 || new_head.y >= GRID_SIZE ||
        new_head.z < 0 || new_head.z >= GRID_SIZE) {
        return 0;
    }
    
    for (int i = 0; i < snake_len; i++) {
        if (snake[i].x == new_head.x && snake[i].y == new_head.y && snake[i].z == new_head.z) {
            return 0;
        }
    }
    
    int ate = (new_head.x == food.x && new_head.y == food.y && new_head.z == food.z);
    
    if (!ate) {
        for (int i = snake_len - 1; i > 0; i--) {
            snake[i] = snake[i - 1];
        }
    } else {
        for (int i = snake_len; i > 0; i--) {
            snake[i] = snake[i - 1];
        }
        snake_len++;
        score += 10;
        if (move_interval > 80) {
            move_interval -= 3;
        }
        spawn_food();
    }
    snake[0] = new_head;
    
    return 1;
}

void draw_wire_cube(sgl_screen_t *scr, int cx, int cy, int cz, int size, uint32_t color) {
    float half = size / 2.0f;
    Vec3 corners[8];
    int sx[8], sy[8];
    float depth[8];
    int visible[8];
    
    corners[0] = (Vec3){cx - half, cy - half, cz - half};
    corners[1] = (Vec3){cx + half, cy - half, cz - half};
    corners[2] = (Vec3){cx + half, cy + half, cz - half};
    corners[3] = (Vec3){cx - half, cy + half, cz - half};
    corners[4] = (Vec3){cx - half, cy - half, cz + half};
    corners[5] = (Vec3){cx + half, cy - half, cz + half};
    corners[6] = (Vec3){cx + half, cy + half, cz + half};
    corners[7] = (Vec3){cx - half, cy + half, cz + half};
    
    for (int i = 0; i < 8; i++) {
        visible[i] = project(corners[i], &sx[i], &sy[i], &depth[i]);
    }
    
    int edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0},
        {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    };
    
    for (int i = 0; i < 12; i++) {
        int a = edges[i][0], b = edges[i][1];
        if (visible[a] && visible[b]) {
            sgl_draw_line(scr, sx[a], sy[a], sx[b], sy[b], color);
        }
    }
}

void draw_filled_cube(sgl_screen_t *scr, int cx, int cy, int cz, int size, uint32_t color) {
    float half = size / 2.0f;
    
    Vec3 front[4] = {
        {cx - half, cy - half, cz - half},
        {cx + half, cy - half, cz - half},
        {cx + half, cy + half, cz - half},
        {cx - half, cy + half, cz - half}
    };
    
    int sx[4], sy[4];
    float depth[4];
    int vis = 1;
    
    for (int i = 0; i < 4; i++) {
        if (!project(front[i], &sx[i], &sy[i], &depth[i])) {
            vis = 0;
            break;
        }
    }
    
    if (vis) {
        int min_y = sy[0], max_y = sy[0];
        for (int i = 1; i < 4; i++) {
            if (sy[i] < min_y) min_y = sy[i];
            if (sy[i] > max_y) max_y = sy[i];
        }
        
        for (int y = min_y; y <= max_y; y++) {
            int min_x = HOR_RES, max_x = 0;
            for (int i = 0; i < 4; i++) {
                int j = (i + 1) % 4;
                if ((sy[i] <= y && sy[j] >= y) || (sy[j] <= y && sy[i] >= y)) {
                    if (sy[i] != sy[j]) {
                        int x = sx[i] + (sx[j] - sx[i]) * (y - sy[i]) / (sy[j] - sy[i]);
                        if (x < min_x) min_x = x;
                        if (x > max_x) max_x = x;
                    }
                }
            }
            if (min_x < max_x) {
                sgl_draw_hline(scr, min_x, y, max_x - min_x, color);
            }
        }
    }
}

typedef struct {
    int index;
    float depth;
    int is_food;
} DrawItem;

int compare_depth(const void *a, const void *b) {
    return ((DrawItem*)b)->depth - ((DrawItem*)a)->depth > 0 ? 1 : -1;
}

void game_paint(sgl_screen_t *scr) {
    sgl_clear_screen(scr, 0);
    
    draw_wire_cube(scr, 0, 0, 0, GRID_SIZE * CELL_SIZE, 
                   sgl_color_to_rgb565(SGL_COLOR_RGB(80, 80, 80)));
    
    DrawItem items[MAX_SNAKE_LEN + 1];
    int item_count = 0;
    
    for (int i = 0; i < snake_len; i++) {
        Vec3 pos = cell_to_world(snake[i].x, snake[i].y, snake[i].z);
        pos.z += cam_dist;
        pos = rotate_x(pos, cam_rot_x);
        pos = rotate_y(pos, cam_rot_y);
        items[item_count].index = i;
        items[item_count].depth = pos.z;
        items[item_count].is_food = 0;
        item_count++;
    }
    
    {
        Vec3 pos = cell_to_world(food.x, food.y, food.z);
        pos.z += cam_dist;
        pos = rotate_x(pos, cam_rot_x);
        pos = rotate_y(pos, cam_rot_y);
        items[item_count].index = 0;
        items[item_count].depth = pos.z;
        items[item_count].is_food = 1;
        item_count++;
    }
    
    qsort(items, item_count, sizeof(DrawItem), compare_depth);
    
    for (int i = 0; i < item_count; i++) {
        if (items[i].is_food) {
            Vec3 pos = cell_to_world(food.x, food.y, food.z);
            uint32_t color = sgl_color_to_rgb565(SGL_COLOR_RGB(255, 50, 50));
            draw_filled_cube(scr, (int)pos.x, (int)pos.y, (int)pos.z, CELL_SIZE - 4, color);
            draw_wire_cube(scr, (int)pos.x, (int)pos.y, (int)pos.z, CELL_SIZE - 2,
                          sgl_color_to_rgb565(SGL_COLOR_RGB(255, 150, 150)));
        } else {
            int idx = items[i].index;
            Vec3 pos = cell_to_world(snake[idx].x, snake[idx].y, snake[idx].z);
            uint32_t color;
            if (idx == 0) {
                color = sgl_color_to_rgb565(SGL_COLOR_RGB(50, 255, 50));
            } else {
                int shade = 150 + (snake_len - idx) * 100 / snake_len;
                if (shade > 255) shade = 255;
                color = sgl_color_to_rgb565(SGL_COLOR_RGB(0, shade, 0));
            }
            draw_filled_cube(scr, (int)pos.x, (int)pos.y, (int)pos.z, CELL_SIZE - 4, color);
            draw_wire_cube(scr, (int)pos.x, (int)pos.y, (int)pos.z, CELL_SIZE - 2,
                          sgl_color_to_rgb565(SGL_COLOR_RGB(100, 255, 100)));
        }
    }
    
    uint32_t color_text = sgl_color_to_rgb565(SGL_COLOR_RGB(255, 255, 255));
    uint32_t color_score = sgl_color_to_rgb565(SGL_COLOR_RGB(255, 220, 50));
    
    sgl_show_format(scr, 20, 20, SGL_ALIGN_UP_LEFT, SGL_DIR_DEFAULT, 
                    color_score, "Score: %d", score);
    sgl_show_format(scr, HOR_RES - 20, 20, SGL_ALIGN_UP_RIGHT, SGL_DIR_DEFAULT,
                    color_text, "High: %d", high_score);
    sgl_show_format(scr, 20, 50, SGL_ALIGN_UP_LEFT, SGL_DIR_DEFAULT,
                    color_text, "Length: %d", snake_len);
    
    sgl_show_format(scr, 20, VER_RES - 80, SGL_ALIGN_UP_LEFT, SGL_DIR_DEFAULT,
                    color_text, "WASD: X/Z plane  Q/E: Y axis");
    sgl_show_format(scr, 20, VER_RES - 60, SGL_ALIGN_UP_LEFT, SGL_DIR_DEFAULT,
                    color_text, "Arrow: Rotate view  SPACE: Start/Restart");
    
    if (game_state == STATE_READY) {
        sgl_show_format(scr, HOR_RES / 2, VER_RES / 2 - 40, SGL_ALIGN_CENTER,
                        SGL_DIR_DEFAULT, color_text, "3D SNAKE");
        sgl_show_format(scr, HOR_RES / 2, VER_RES / 2, SGL_ALIGN_CENTER,
                        SGL_DIR_DEFAULT, color_text, "Press SPACE to Start");
    } else if (game_state == STATE_OVER) {
        uint32_t color_over = sgl_color_to_rgb565(SGL_COLOR_RGB(255, 80, 80));
        sgl_show_format(scr, HOR_RES / 2, VER_RES / 2 - 40, SGL_ALIGN_CENTER,
                        SGL_DIR_DEFAULT, color_over, "GAME OVER!");
        sgl_show_format(scr, HOR_RES / 2, VER_RES / 2, SGL_ALIGN_CENTER,
                        SGL_DIR_DEFAULT, color_text, "Score: %d", score);
        sgl_show_format(scr, HOR_RES / 2, VER_RES / 2 + 30, SGL_ALIGN_CENTER,
                        SGL_DIR_DEFAULT, color_text, "Press SPACE to Restart");
    }
}

int main(int argc, char *argv[]) {
    srand((unsigned int)time(NULL));
    
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("3D Snake Game", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, HOR_RES, VER_RES, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
                                SDL_TEXTUREACCESS_STREAMING, HOR_RES, VER_RES);
    
    sgl_init(&scr, buffer, HOR_RES * VER_RES * 2, HOR_RES, VER_RES);
    sgl_set_draw_pixel(&scr, sgl_draw_pixel_rgb565);
    sgl_set_flush(&scr, sgl_flush);
    sgl_set_paint(&scr, sgl_paint);
    sgl_set_screen_rotation(&scr, SGL_ROTATE_0);
    
    high_score = 0;
    game_state = STATE_READY;
    init_game();
    last_time = SDL_GetTicks();
    
    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = 0;
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                case SDLK_w:
                    if (direction != DIR_BACK) next_direction = DIR_FRONT;
                    break;
                case SDLK_s:
                    if (direction != DIR_FRONT) next_direction = DIR_BACK;
                    break;
                case SDLK_a:
                    if (direction != DIR_RIGHT) next_direction = DIR_LEFT;
                    break;
                case SDLK_d:
                    if (direction != DIR_LEFT) next_direction = DIR_RIGHT;
                    break;
                case SDLK_q:
                    if (direction != DIR_DOWN) next_direction = DIR_UP;
                    break;
                case SDLK_e:
                    if (direction != DIR_UP) next_direction = DIR_DOWN;
                    break;
                case SDLK_UP:
                    cam_rot_x -= 0.1f;
                    break;
                case SDLK_DOWN:
                    cam_rot_x += 0.1f;
                    break;
                case SDLK_LEFT:
                    cam_rot_y -= 0.1f;
                    break;
                case SDLK_RIGHT:
                    cam_rot_y += 0.1f;
                    break;
                case SDLK_SPACE:
                    if (game_state == STATE_READY || game_state == STATE_OVER) {
                        init_game();
                        game_state = STATE_PLAYING;
                        last_time = SDL_GetTicks();
                        move_timer = 0;
                    }
                    break;
                case SDLK_ESCAPE:
                    running = 0;
                    break;
                }
                break;
            }
        }
        
        if (game_state == STATE_PLAYING) {
            uint32_t current_time = SDL_GetTicks();
            move_timer += current_time - last_time;
            last_time = current_time;

            if (move_timer >= move_interval) {
                move_timer = 0;
                if (!move_snake()) {
                    game_state = STATE_OVER;
                    if (score > high_score) {
                        high_score = score;
                    }
                }
            }
        } else {
            last_time = SDL_GetTicks();
        }

        sgl_handler(&scr);
        SDL_Delay(10);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

void sgl_flush(void *buf, sgl_rect_t *visible) {
    (void)visible;
    SDL_UpdateTexture(texture, NULL, buf, HOR_RES * sizeof(uint16_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void sgl_paint(sgl_screen_t *scr) {
    game_paint(scr);
}
