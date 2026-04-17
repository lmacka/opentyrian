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

#define SHIP_FINGER_NONE    ((SDL_FingerID)-1)
#define GAMEPLAY_TIMEOUT_MS 250

typedef struct {
    int x, y, w, h;  // virtual 1000x1000 coords
    bool is_primary; // gets the accent colour
    const char *label;
} ButtonLayout;

/* Layout anchored to the left edge of the visible canvas.
 *
 * MENU: tiny top-left, far from the action zone — no accidental pauses.
 * L / R: mid-height pair, small (sidekicks are secondary actions).
 * MODE: medium, above FIRE with a real gap so a slipping thumb doesn't
 *       trigger weapon-mode change mid-dodge.
 * FIRE: bottom-left, large, the only accent colour. Anchored 170 virtual
 *       units above the canvas bottom so it stays clear of the Android
 *       gesture navigation zone on modern devices.
 */
static const ButtonLayout buttons[ANDROID_BTN_COUNT] = {
    [ANDROID_BTN_MENU]           = {  60,  50,  80,  60, false, "MENU" },
    [ANDROID_BTN_LEFT_SIDEKICK]  = {  60, 300, 120, 120, false, "L"    },
    [ANDROID_BTN_RIGHT_SIDEKICK] = { 200, 300, 120, 120, false, "R"    },
    [ANDROID_BTN_CHANGE_WEAPON]  = {  60, 450, 180, 140, false, "MODE" },
    [ANDROID_BTN_FIRE]           = {  60, 620, 220, 210, true,  "FIRE" },
};

/* Palette. One accent (warm orange) for the primary action; everything else
 * sits in muted slate so a colourblind player can still tell the primary
 * from the rest, and the overall overlay stops competing with the game. */
/* HUD palette. Primary (FIRE) uses warm orange; secondaries use cyan.
 * Fill is a dark navy common to all, letting the outline + corner
 * brackets do the talking. */
#define COL_PRIMARY_R   0xF0
#define COL_PRIMARY_G   0x60
#define COL_PRIMARY_B   0x30
#define COL_SECONDARY_R 0x40
#define COL_SECONDARY_G 0xB0
#define COL_SECONDARY_B 0xD0
#define COL_FILL_R      0x0C
#define COL_FILL_G      0x12
#define COL_FILL_B      0x20
#define COL_FILL_PR_R   0x1E
#define COL_FILL_PR_G   0x28
#define COL_FILL_PR_B   0x40

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
    // Android exposes the accelerometer as a joystick by default; its
    // permanently-deflected axis then feeds push_joysticks_as_keyboard()
    // which spams SDL_SCANCODE_DOWN at the menus. Disable here, before
    // SDL_InitSubSystem(SDL_INIT_JOYSTICK) runs.
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
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
    if (!gameplay_active()) return -1;
    for (int i = 0; i < ANDROID_BTN_COUNT; ++i) {
        SDL_Rect r;
        button_rect_to_window(&buttons[i], win_w, win_h, &r);
        if (point_in_rect(win_x, win_y, &r)) return i;
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
            if (ev->tfinger.fingerId == ship_finger)
                ship_finger = SHIP_FINGER_NONE;
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
    for (int i = 0; i < len; ++i)
        draw_char(r, text[i], start_x + i * (glyph_w + glyph_gap), start_y, px);
}

static void fill_rect_rgba(SDL_Renderer *r, int x, int y, int w, int h,
                            Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca)
{
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_Rect rct = { x, y, w, h };
    SDL_RenderFillRect(r, &rct);
}

static void draw_hud_button(SDL_Renderer *renderer, SDL_Rect r, bool primary,
                            bool pressed, const char *label)
{
    Uint8 ar = primary ? COL_PRIMARY_R : COL_SECONDARY_R;
    Uint8 ag = primary ? COL_PRIMARY_G : COL_SECONDARY_G;
    Uint8 ab = primary ? COL_PRIMARY_B : COL_SECONDARY_B;

    // Drop shadow — sells separation from busy backgrounds.
    fill_rect_rgba(renderer, r.x + 4, r.y + 6, r.w, r.h, 0, 0, 0, 110);

    // Main fill.
    if (pressed) {
        fill_rect_rgba(renderer, r.x, r.y, r.w, r.h,
                       COL_FILL_PR_R, COL_FILL_PR_G, COL_FILL_PR_B, 225);
        // Faked inner glow: concentric alpha rects in the accent colour.
        for (int i = 1; i <= 4; ++i) {
            int inset = i * (r.h / 30 + 1);
            if (inset * 2 >= r.w || inset * 2 >= r.h) break;
            Uint8 alpha = (Uint8)(80 - i * 15);
            fill_rect_rgba(renderer, r.x + inset, r.y + inset,
                           r.w - 2 * inset, r.h - 2 * inset,
                           ar, ag, ab, alpha);
        }
    } else {
        fill_rect_rgba(renderer, r.x, r.y, r.w, r.h,
                       COL_FILL_R, COL_FILL_G, COL_FILL_B, 185);
    }

    // Outline in the accent colour.
    int ow = pressed ? 3 : 2;
    SDL_SetRenderDrawColor(renderer, ar, ag, ab, pressed ? 255 : 215);
    for (int t = 0; t < ow; ++t) {
        SDL_Rect e = { r.x + t, r.y + t, r.w - 2 * t, r.h - 2 * t };
        SDL_RenderDrawRect(renderer, &e);
    }

    // Corner brackets — the HUD touch. Each corner gets two small
    // filled rects forming an L.
    int bshort = (r.h < r.w ? r.h : r.w);
    int bl = bshort / (pressed ? 4 : 5);
    int bt = pressed ? 4 : 3;
    SDL_Rect brks[8] = {
        // Top-left
        { r.x,                r.y,                bl, bt },
        { r.x,                r.y,                bt, bl },
        // Top-right
        { r.x + r.w - bl,     r.y,                bl, bt },
        { r.x + r.w - bt,     r.y,                bt, bl },
        // Bottom-left
        { r.x,                r.y + r.h - bt,     bl, bt },
        { r.x,                r.y + r.h - bl,     bt, bl },
        // Bottom-right
        { r.x + r.w - bl,     r.y + r.h - bt,     bl, bt },
        { r.x + r.w - bt,     r.y + r.h - bl,     bt, bl },
    };
    SDL_SetRenderDrawColor(renderer, ar, ag, ab, 255);
    for (int i = 0; i < 8; ++i) SDL_RenderFillRect(renderer, &brks[i]);

    // Label.
    int label_px = r.h / 14;
    if (r.h < 80) label_px = r.h / 18;
    if (label_px < 1) label_px = 1;
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 248);
    draw_text_centered(renderer, label, r.x + r.w / 2, r.y + r.h / 2, label_px);
}

static void draw_hud_crosshair(SDL_Renderer *r, int cx, int cy, int size)
{
    int gap = size / 4;
    if (gap < 4) gap = 4;
    int arm = size - gap;
    if (arm < 3) arm = 3;

    // Dark halo around every stroke so the orange reads on bright enemies
    // and starfields alike.
    fill_rect_rgba(r, cx - size - 1, cy - 3, arm + 2, 6, 0, 0, 0, 200);
    fill_rect_rgba(r, cx + gap - 1,  cy - 3, arm + 2, 6, 0, 0, 0, 200);
    fill_rect_rgba(r, cx - 3, cy - size - 1, 6, arm + 2, 0, 0, 0, 200);
    fill_rect_rgba(r, cx - 3, cy + gap - 1,  6, arm + 2, 0, 0, 0, 200);

    // Orange strokes with centre gap.
    Uint8 ar = COL_PRIMARY_R, ag = COL_PRIMARY_G, ab = COL_PRIMARY_B;
    fill_rect_rgba(r, cx - size, cy - 1, arm, 3, ar, ag, ab, 240);
    fill_rect_rgba(r, cx + gap,  cy - 1, arm, 3, ar, ag, ab, 240);
    fill_rect_rgba(r, cx - 1, cy - size, 3, arm, ar, ag, ab, 240);
    fill_rect_rgba(r, cx - 1, cy + gap,  3, arm, ar, ag, ab, 240);

    // Centre dot with halo so the ship isn't hidden when touched close.
    fill_rect_rgba(r, cx - 3, cy - 3, 6, 6, 0, 0, 0, 200);
    fill_rect_rgba(r, cx - 2, cy - 2, 4, 4, ar, ag, ab, 240);
}

void android_input_render_overlay(SDL_Renderer *renderer, int win_w, int win_h)
{
    if (!initialized) return;
    if (!gameplay_active()) return;

    SDL_BlendMode old;
    SDL_GetRenderDrawBlendMode(renderer, &old);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < ANDROID_BTN_COUNT; ++i) {
        SDL_Rect r;
        button_rect_to_window(&buttons[i], win_w, win_h, &r);
        draw_hud_button(renderer, r, buttons[i].is_primary, btn_down[i],
                        buttons[i].label);
    }

    if (ship_finger != SHIP_FINGER_NONE) {
        int size = win_h / 28;
        if (size < 14) size = 14;
        draw_hud_crosshair(renderer, ship_target_win_x, ship_target_win_y, size);
    }

    SDL_SetRenderDrawBlendMode(renderer, old);
}

#else  // !__ANDROID__

void android_input_init(void) {}
void android_input_note_gameplay_frame(void) {}
void android_input_handle_event(const SDL_Event *ev) { (void)ev; }
bool android_input_button_down(AndroidButton b) { (void)b; return false; }
bool android_input_get_ship_target(int *x, int *y) { (void)x; (void)y; return false; }
void android_input_render_overlay(SDL_Renderer *r, int w, int h) { (void)r; (void)w; (void)h; }

#endif
