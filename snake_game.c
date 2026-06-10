#include "sgl/sgl.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define HOR_RES 800
#define VER_RES 600

// 游戏区域参数
#define CELL_SIZE 20
#define GRID_W 30
#define GRID_H 25
#define OFFSET_X ((HOR_RES - GRID_W * CELL_SIZE) / 2)
#define OFFSET_Y ((VER_RES - GRID_H * CELL_SIZE) / 2 + 20)

// 蛇身最大长度
#define MAX_SNAKE_LEN (GRID_W * GRID_H)

// 方向枚举
#define DIR_UP 0
#define DIR_RIGHT 1
#define DIR_DOWN 2
#define DIR_LEFT 3

// 游戏状态
#define STATE_READY 0
#define STATE_PLAYING 1
#define STATE_OVER 2

typedef struct {
    int x;
    int y;
} Point;

// 全局变量
SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;
sgl_screen_t scr;
uint16_t buffer[HOR_RES * VER_RES];

// 游戏状态变量
Point snake[MAX_SNAKE_LEN];
int snake_len;
int direction;
int next_direction;
Point food;
int score;
int high_score;
int game_state;
int move_timer;       // 移动计时器
int move_interval;    // 移动间隔(ms)
uint32_t last_time;

// 颜色定义 (RGB565)
#define COLOR_BG       sgl_color_to_rgb565(SGL_COLOR_RGB(30, 30, 30))
#define COLOR_GRID     sgl_color_to_rgb565(SGL_COLOR_RGB(40, 40, 40))
#define COLOR_SNAKE_H  sgl_color_to_rgb565(SGL_COLOR_RGB(0, 220, 0))
#define COLOR_SNAKE_B  sgl_color_to_rgb565(SGL_COLOR_RGB(0, 180, 0))
#define COLOR_FOOD     sgl_color_to_rgb565(SGL_COLOR_RGB(220, 50, 50))
#define COLOR_BORDER   sgl_color_to_rgb565(SGL_COLOR_RGB(100, 100, 100))
#define COLOR_TEXT     sgl_color_to_rgb565(SGL_COLOR_RGB(255, 255, 255))
#define COLOR_SCORE    sgl_color_to_rgb565(SGL_COLOR_RGB(255, 220, 50))

void sgl_flush(void *buf, sgl_rect_t *visible);
void sgl_paint(sgl_screen_t *scr);

// 生成食物
void spawn_food(void) {
    int occupied;
    do {
        food.x = rand() % GRID_W;
        food.y = rand() % GRID_H;
        occupied = 0;
        for (int i = 0; i < snake_len; i++) {
            if (snake[i].x == food.x && snake[i].y == food.y) {
                occupied = 1;
                break;
            }
        }
    } while (occupied);
}

// 初始化游戏
void init_game(void) {
    snake_len = 3;
    snake[0].x = GRID_W / 2;
    snake[0].y = GRID_H / 2;
    snake[1].x = GRID_W / 2 - 1;
    snake[1].y = GRID_H / 2;
    snake[2].x = GRID_W / 2 - 2;
    snake[2].y = GRID_H / 2;
    direction = DIR_RIGHT;
    next_direction = DIR_RIGHT;
    score = 0;
    move_timer = 0;
    move_interval = 120;
    spawn_food();
}

// 移动蛇
int move_snake(void) {
    direction = next_direction;

    // 计算新头部位置
    Point new_head = snake[0];
    switch (direction) {
    case DIR_UP:    new_head.y--; break;
    case DIR_DOWN:  new_head.y++; break;
    case DIR_LEFT:  new_head.x--; break;
    case DIR_RIGHT: new_head.x++; break;
    }

    // 检测墙壁碰撞
    if (new_head.x < 0 || new_head.x >= GRID_W ||
        new_head.y < 0 || new_head.y >= GRID_H) {
        return 0; // 游戏结束
    }

    // 检测自身碰撞
    for (int i = 0; i < snake_len; i++) {
        if (snake[i].x == new_head.x && snake[i].y == new_head.y) {
            return 0; // 游戏结束
        }
    }

    // 检测是否吃到食物
    int ate = (new_head.x == food.x && new_head.y == food.y);

    if (!ate) {
        // 没吃到食物，移动蛇身（尾部去掉）
        for (int i = snake_len - 1; i > 0; i--) {
            snake[i] = snake[i - 1];
        }
    } else {
        // 吃到食物，蛇身增长
        for (int i = snake_len; i > 0; i--) {
            snake[i] = snake[i - 1];
        }
        snake_len++;
        score += 10;
        // 加速
        if (move_interval > 50) {
            move_interval -= 2;
        }
        spawn_food();
    }
    snake[0] = new_head;

    return 1; // 继续游戏
}

// 绘制游戏
void game_paint(sgl_screen_t *scr) {
    // 清屏
    sgl_clear_screen(scr, 0);

    // 绘制游戏区域背景
    sgl_draw_rect(scr, OFFSET_X - 2, OFFSET_Y - 2,
                  GRID_W * CELL_SIZE + 4, GRID_H * CELL_SIZE + 4,
                  1, COLOR_BORDER);
    sgl_draw_rect(scr, OFFSET_X, OFFSET_Y,
                  GRID_W * CELL_SIZE, GRID_H * CELL_SIZE,
                  1, COLOR_BG);

    // 绘制网格线（淡色）
    for (int i = 0; i <= GRID_W; i++) {
        int x = OFFSET_X + i * CELL_SIZE;
        for (int j = 0; j < GRID_H * CELL_SIZE; j++) {
            sgl_draw_pixel_rgb565(scr, x, OFFSET_Y + j, COLOR_GRID);
        }
    }
    for (int j = 0; j <= GRID_H; j++) {
        int y = OFFSET_Y + j * CELL_SIZE;
        for (int i = 0; i < GRID_W * CELL_SIZE; i++) {
            sgl_draw_pixel_rgb565(scr, OFFSET_X + i, y, COLOR_GRID);
        }
    }

    // 绘制食物
    int fx = OFFSET_X + food.x * CELL_SIZE + 2;
    int fy = OFFSET_Y + food.y * CELL_SIZE + 2;
    sgl_draw_rect(scr, fx, fy, CELL_SIZE - 4, CELL_SIZE - 4, 1, COLOR_FOOD);

    // 绘制蛇身
    for (int i = 0; i < snake_len; i++) {
        int sx = OFFSET_X + snake[i].x * CELL_SIZE + 1;
        int sy = OFFSET_Y + snake[i].y * CELL_SIZE + 1;
        uint32_t color = (i == 0) ? COLOR_SNAKE_H : COLOR_SNAKE_B;
        sgl_draw_rect(scr, sx, sy, CELL_SIZE - 2, CELL_SIZE - 2, 1, color);
    }

    // 绘制分数
    sgl_show_format(scr, OFFSET_X, OFFSET_Y - 18, SGL_ALIGN_UP_LEFT,
                    SGL_DIR_DEFAULT, COLOR_SCORE, "Score: %d", score);
    sgl_show_format(scr, OFFSET_X + GRID_W * CELL_SIZE, OFFSET_Y - 18,
                    SGL_ALIGN_UP_RIGHT, SGL_DIR_DEFAULT, COLOR_TEXT,
                    "High: %d", high_score);

    // 状态提示
    if (game_state == STATE_READY) {
        sgl_show_format(scr, HOR_RES / 2, VER_RES / 2, SGL_ALIGN_CENTER,
                        SGL_DIR_DEFAULT, COLOR_TEXT,
                        "SNAKE GAME");
        sgl_show_format(scr, HOR_RES / 2, VER_RES / 2 + 30, SGL_ALIGN_CENTER,
                        SGL_DIR_DEFAULT, COLOR_TEXT,
                        "Press SPACE to Start");
        sgl_show_format(scr, HOR_RES / 2, VER_RES / 2 + 60, SGL_ALIGN_CENTER,
                        SGL_DIR_DEFAULT, COLOR_TEXT,
                        "Arrow Keys to Move");
    } else if (game_state == STATE_OVER) {
        sgl_show_format(scr, HOR_RES / 2, VER_RES / 2, SGL_ALIGN_CENTER,
                        SGL_DIR_DEFAULT, COLOR_FOOD,
                        "GAME OVER!");
        sgl_show_format(scr, HOR_RES / 2, VER_RES / 2 + 30, SGL_ALIGN_CENTER,
                        SGL_DIR_DEFAULT, COLOR_TEXT,
                        "Score: %d", score);
        sgl_show_format(scr, HOR_RES / 2, VER_RES / 2 + 60, SGL_ALIGN_CENTER,
                        SGL_DIR_DEFAULT, COLOR_TEXT,
                        "Press SPACE to Restart");
    }
}

int main(int argc, char *argv[]) {
    srand((unsigned int)time(NULL));

    // 初始化 SDL
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Snake Game - SGL", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, HOR_RES, VER_RES, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
                                SDL_TEXTUREACCESS_STREAMING, HOR_RES, VER_RES);

    // 初始化 SGL
    sgl_init(&scr, buffer, HOR_RES * VER_RES * 2, HOR_RES, VER_RES);
    sgl_set_draw_pixel(&scr, sgl_draw_pixel_rgb565);
    sgl_set_flush(&scr, sgl_flush);
    sgl_set_paint(&scr, sgl_paint);
    sgl_set_screen_rotation(&scr, SGL_ROTATE_0);

    // 初始化游戏
    high_score = 0;
    game_state = STATE_READY;
    init_game();
    last_time = SDL_GetTicks();

    // 主循环
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
                case SDLK_UP:
                    if (direction != DIR_DOWN)
                        next_direction = DIR_UP;
                    break;
                case SDLK_DOWN:
                    if (direction != DIR_UP)
                        next_direction = DIR_DOWN;
                    break;
                case SDLK_LEFT:
                    if (direction != DIR_RIGHT)
                        next_direction = DIR_LEFT;
                    break;
                case SDLK_RIGHT:
                    if (direction != DIR_LEFT)
                        next_direction = DIR_RIGHT;
                    break;
                case SDLK_SPACE:
                    if (game_state == STATE_READY || game_state == STATE_OVER) {
                        init_game();
                        game_state = STATE_PLAYING;
                        last_time = SDL_GetTicks();
                    }
                    break;
                case SDLK_ESCAPE:
                    running = 0;
                    break;
                }
                break;
            }
        }

        // 游戏逻辑更新
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

    // 清理
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
