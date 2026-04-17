/*
 * Touch input for Android. Right thumb drags to steer the ship (absolute
 * target, ship tracks toward it); left-side floating buttons drive action
 * keys. SDL's own touch-to-mouse handles menu tapping.
 *
 * On non-Android platforms this module compiles to empty stubs.
 */
#include "android_input.h"

#ifdef __ANDROID__

#include "video.h"

#include <SDL.h>
#include <string.h>

#define SHIP_FINGER_NONE   ((SDL_FingerID)-1)
#define GAMEPLAY_TIMEOUT_MS 250

typedef struct {
    int x, y, w, h;   // virtual 1000x1000 coords
    Uint32 color;     // 0xRRGGBB
    const char *label;
} ButtonLayout;

static const ButtonLayout buttons[ANDROID_BTN_COUNT] = {
    [ANDROID_BTN_FIRE]           = { 40,  650, 200, 200, 0xE04040, "FIRE" },
    [ANDROID_BTN_CHANGE_WEAPON]  = { 40,  430, 200, 200, 0x4080E0, "MODE" },
    [ANDROID_BTN_LEFT_SIDEKICK]  = { 40,   40, 130, 130, 0x40C060, "L"    },
    [ANDROID_BTN_RIGHT_SIDEKICK] = { 190,  40, 130, 130, 0xE0C040, "R"    },
    [ANDROID_BTN_MENU]           = { 40,  220, 130, 130, 0x808080, "MENU" },
};

static bool btn_down[ANDROID_BTN_COUNT];
static SDL_FingerID btn_finger[ANDROID_BTN_COUNT];

static SDL_FingerID ship_finger = SHIP_FINGER_NONE;
static int ship_target_win_x = 0;
static int ship_target_win_y = 0;

static bool initialized;
static Uint32 last_gameplay_tick;

/* 5x7 bitmap font, one byte per column (LSB = top row). Only the glyphs
 * used by button labels are populated. */
static const struct { char c; Uint8 col[5]; } glyphs[] = {
    {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}},
    {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
    {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
    {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
    {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
    {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
    {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}},
    {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}},
    {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
    {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
    {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
};
#define GLYPH_COLS 5
#define GLYPH_ROWS 7

static const Uint8 *find_glyph(char c)
{
    for (size_t i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); ++i) {
        if (glyphs[i].c == c) return glyphs[i].col;
    }
    return glyphs[sizeof(glyphs) / sizeof(glyphs[0]) - 1].col; // space
}

static void reset_button_state(void)
{
    for (int i = 0; i < ANDROID_BTN_COUNT; ++i) {
        btn_down[i] = false;
        btn_finger[i] = SHIP_FINGER_NONE;
    }
    ship_finger = SHIP_FINGER_NONE;
}

void android_input_init(void)
{
    if (initialized) return;
    reset_button_state();
    initialized = true;
}

void android_input_note_gameplay_frame(void)
{
    last_gameplay_tick = SDL_GetTicks();
}

static bool gameplay_active(void)
{
    if (last_gameplay_tick == 0) return false;
    return (SDL_GetTicks() - last_gameplay_tick) < GAMEPLAY_TIMEOUT_MS;
}

static void button_rect_to_window(const ButtonLayout *b, int win_w, int win_h, SDL_Rect *out)
{
    int canvas_px = win_h;
    if (canvas_px > win_w * 3 / 4) canvas_px = win_w * 3 / 4;
    out->x = (b->x * canvas_px + 500) / 1000;
    out->y = (b->y * canvas_px + 500) / 1000;
    out->w = (b->w * canvas_px + 500) / 1000;
    out->h = (b->h * canvas_px + 500) / 1000;
}

static bool point_in_rect(int x, int y, const SDL_Rect *r)
{
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

static int hit_test_button(int win_x, int win_y, int win_w, int win_h)
{
    if (!gameplay_active()) return -1; // buttons only hot during gameplay
    for (int i = 0; i < ANDROID_BTN_COUNT; ++i) {
        SDL_Rect r;
        button_rect_to_window(&buttons[i], win_w, win_h, &r);
        if (point_in_rect(win_x, win_y, &r))
            return i;
    }
    return -1;
}

static void get_window_size(int *win_w, int *win_h)
{
    extern SDL_Window *main_window;
    if (main_window != NULL) SDL_GetWindowSize(main_window, win_w, win_h);
    else { *win_w = 1; *win_h = 1; }
}

void android_input_handle_event(const SDL_Event *ev)
{
    if (!initialized) android_input_init();

    int win_w, win_h;
    get_window_size(&win_w, &win_h);

    switch (ev->type) {
        case SDL_FINGERDOWN: {
            int fx = (int)(ev->tfinger.x * win_w);
            int fy = (int)(ev->tfinger.y * win_h);
            int btn = hit_test_button(fx, fy, win_w, win_h);
            if (btn >= 0) {
                btn_down[btn] = true;
                btn_finger[btn] = ev->tfinger.fingerId;
            } else if (gameplay_active()) {
                ship_finger = ev->tfinger.fingerId;
                ship_target_win_x = fx;
                ship_target_win_y = fy;
            }
            break;
        }
        case SDL_FINGERMOTION: {
            if (ev->tfinger.fingerId == ship_finger) {
                ship_target_win_x = (int)(ev->tfinger.x * win_w);
                ship_target_win_y = (int)(ev->tfinger.y * win_h);
            }
            break;
        }
        case SDL_FINGERUP: {
            if (ev->tfinger.fingerId == ship_finger) {
                ship_finger = SHIP_FINGER_NONE;
            }
            for (int i = 0; i < ANDROID_BTN_COUNT; ++i) {
                if (btn_finger[i] == ev->tfinger.fingerId) {
                    btn_down[i] = false;
                    btn_finger[i] = SHIP_FINGER_NONE;
                }
            }
            break;
        }
        default:
            break;
    }
}

bool android_input_button_down(AndroidButton b)
{
    if (b < 0 || b >= ANDROID_BTN_COUNT) return false;
    return btn_down[b];
}

bool android_input_get_ship_target(int *out_x, int *out_y)
{
    if (ship_finger == SHIP_FINGER_NONE) return false;
    Sint32 sx = ship_target_win_x;
    Sint32 sy = ship_target_win_y;
    mapWindowPointToScreen(&sx, &sy);
    *out_x = sx;
    *out_y = sy;
    return true;
}

static void set_color(SDL_Renderer *r, Uint32 rgb, Uint8 a)
{
    SDL_SetRenderDrawColor(r, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, a);
}

static void draw_char(SDL_Renderer *r, char c, int x, int y, int px)
{
    const Uint8 *g = find_glyph(c);
    for (int col = 0; col < GLYPH_COLS; ++col) {
        Uint8 bits = g[col];
        for (int row = 0; row < GLYPH_ROWS; ++row) {
            if (bits & (1u << row)) {
                SDL_Rect cell = { x + col * px, y + row * px, px, px };
                SDL_RenderFillRect(r, &cell);
            }
        }
    }
}

static void draw_text_centered(SDL_Renderer *r, const char *text, int cx, int cy, int px)
{
    int len = (int)strlen(text);
    int glyph_w = GLYPH_COLS * px;
    int glyph_gap = px;
    int total_w = len * glyph_w + (len - 1) * glyph_gap;
    int total_h = GLYPH_ROWS * px;
    int start_x = cx - total_w / 2;
    int start_y = cy - total_h / 2;
    for (int i = 0; i < len; ++i) {
        draw_char(r, text[i], start_x + i * (glyph_w + glyph_gap), start_y, px);
    }
}

void android_input_render_overlay(SDL_Renderer *renderer, int win_w, int win_h)
{
    if (!initialized) return;
    if (!gameplay_active()) return;

    SDL_BlendMode old;
    SDL_GetRenderDrawBlendMode(renderer, &old);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int px_unit = win_h / 1000;
    if (px_unit < 1) px_unit = 1;

    for (int i = 0; i < ANDROID_BTN_COUNT; ++i) {
        SDL_Rect r;
        button_rect_to_window(&buttons[i], win_w, win_h, &r);

        Uint8 fill_alpha = btn_down[i] ? 200 : 110;
        set_color(renderer, buttons[i].color, fill_alpha);
        SDL_RenderFillRect(renderer, &r);

        set_color(renderer, 0xFFFFFF, btn_down[i] ? 255 : 180);
        SDL_RenderDrawRect(renderer, &r);
        SDL_Rect inner = { r.x + 1, r.y + 1, r.w - 2, r.h - 2 };
        SDL_RenderDrawRect(renderer, &inner);

        // Label text, scaled to button size.
        int label_px = r.h / 14;
        if (label_px < 2) label_px = 2;
        set_color(renderer, 0xFFFFFF, 240);
        draw_text_centered(renderer, buttons[i].label, r.x + r.w / 2, r.y + r.h / 2, label_px);
    }

    if (ship_finger != SHIP_FINGER_NONE) {
        int cx = ship_target_win_x;
        int cy = ship_target_win_y;
        int size = win_h / 40;
        if (size < 8) size = 8;
        set_color(renderer, 0xFFFFFF, 180);
        SDL_Rect h = { cx - size, cy - 1, 2 * size, 3 };
        SDL_Rect v = { cx - 1, cy - size, 3, 2 * size };
        SDL_RenderFillRect(renderer, &h);
        SDL_RenderFillRect(renderer, &v);
    }

    SDL_SetRenderDrawBlendMode(renderer, old);
    (void)px_unit;
}

#else  // !__ANDROID__

void android_input_init(void) {}
void android_input_note_gameplay_frame(void) {}
void android_input_handle_event(const SDL_Event *ev) { (void)ev; }
bool android_input_button_down(AndroidButton b) { (void)b; return false; }
bool android_input_get_ship_target(int *x, int *y) { (void)x; (void)y; return false; }
void android_input_render_overlay(SDL_Renderer *r, int w, int h) { (void)r; (void)w; (void)h; }

#endif
