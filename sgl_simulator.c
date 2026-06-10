#define SDL_MAIN_HANDLED
#include "sgl/sgl.h"
#include <SDL2/SDL.h>
#include <stdio.h>

#define HOR_RES 1280
#define VER_RES 720

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;
SDL_TimerID timer;
sgl_screen_t scr;
uint16_t buffer[HOR_RES * VER_RES];
void sgl_flush(void *buffer, sgl_rect_t *visible);
void sgl_paint(sgl_screen_t *scr);
int fps;

Uint32 timer_callback(Uint32 interval, void *param) {
    fps = sgl_get_fcount(&scr);
    sgl_reset_fcount(&scr);
    return interval;
}

int main(int argc, char *argv[]) {
    // init sdl
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("sgl simulator", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, HOR_RES, VER_RES, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
                                SDL_TEXTUREACCESS_STREAMING, HOR_RES, VER_RES);
    timer = SDL_AddTimer(1000, timer_callback, NULL);
    // init sgl
    sgl_init(&scr, buffer, HOR_RES * VER_RES * 2, HOR_RES, VER_RES);
    sgl_set_draw_pixel(&scr, sgl_draw_pixel_rgb565);
    sgl_set_flush(&scr, sgl_flush);
    sgl_set_paint(&scr, sgl_paint);
    sgl_set_screen_rotation(&scr, SGL_ROTATE_0);
    // loop
    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = 0;
                break;
            }
        }
        uint64_t start = SDL_GetPerformanceCounter();
        sgl_handler(&scr);
        uint64_t end = SDL_GetPerformanceCounter();
        SDL_Delay(10);
    }
    // destroy
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

void sgl_flush(void *buffer, sgl_rect_t *visible) {
    (void)visible;
    SDL_UpdateTexture(texture, NULL, buffer, HOR_RES * sizeof(uint16_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void sgl_paint(sgl_screen_t *scr) {
    sgl_clear_screen(scr, 0);
    static int test_x = 0;
    static int a = 0;
    static int b = 4;
    a += b;
    if (a == 100)
        b = -1;
    else if (a == 0)
        b = 4;
    test_x++;
    test_x %= 360;
    sgl_draw_circle_center(
        scr, HOR_RES / 2, VER_RES / 2, a * a / 10, 1,
        sgl_color_to_rgb565(sgl_color_to_rgb((test_x + 180) % 360, 255, 255)));
    sgl_draw_round_rect(
        scr, 100, 100, a * a / 10, 40, 20, 1,
        sgl_color_to_rgb565(sgl_color_to_rgb((test_x) % 360, 255, 255)));
    sgl_show_format(scr, HOR_RES - 1, 0, SGL_ALIGN_UP_RIGHT, SGL_DIR_DEFAULT,
                    0x07f0, "%dfps", fps);
}
