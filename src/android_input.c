/*
 * Touch input for Android. Right thumb drags to steer the ship (acts as
 * an absolute-position mouse follower); left thumb taps floating action
 * buttons. Both fingers operate independently.
 *
 * On non-Android platforms this module compiles to empty stubs.
 */
#include "android_input.h"

#ifdef __ANDROID__

#include "video.h"

#include <SDL.h>
#include <string.h>

#define MAX_BUTTON_FINGERS ANDROID_BTN_COUNT
#define SHIP_FINGER_NONE   ((SDL_FingerID)-1)

typedef struct {
    int x, y, w, h;   // in logical "overlay units" — see layout below
    Uint32 color;     // 0xRRGGBB
    const char *label;
} ButtonLayout;

// Overlay is laid out in a virtual 1000x1000 canvas then mapped to window.
// The entire button column lives on the left edge of the screen so the
// right thumb stays free for ship control.
static const ButtonLayout buttons[ANDROID_BTN_COUNT] = {
    // Primary Fire: large, bottom of left column, easiest thumb reach.
    [ANDROID_BTN_FIRE]           = { 40,  650, 200, 200, 0xE04040, "FIRE" },
    // Change weapon mode: above fire.
    [ANDROID_BTN_CHANGE_WEAPON]  = { 40,  430, 200, 200, 0x4080E0, "MODE" },
    // Left sidekick: small, top-left.
    [ANDROID_BTN_LEFT_SIDEKICK]  = { 40,   40, 130, 130, 0x40C060, "L" },
    // Right sidekick: small, next to left.
    [ANDROID_BTN_RIGHT_SIDEKICK] = { 190,  40, 130, 130, 0xE0C040, "R" },
    // Menu: tiny corner button.
    [ANDROID_BTN_MENU]           = { 40,  220, 130, 130, 0x808080, "MENU" },
};

static bool btn_down[ANDROID_BTN_COUNT];
static SDL_FingerID btn_finger[ANDROID_BTN_COUNT];

static SDL_FingerID ship_finger = SHIP_FINGER_NONE;
static int ship_target_win_x = 0;
static int ship_target_win_y = 0;

static bool initialized;

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
    // We handle touch directly — don't let SDL synthesize mouse events
    // from touches or vice versa.
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    initialized = true;
}

// Convert a virtual button rect (1000x1000 canvas) to window pixel rect.
// Canvas height maps to the shorter window dimension; X is anchored to left.
static void button_rect_to_window(const ButtonLayout *b, int win_w, int win_h, SDL_Rect *out)
{
    (void)win_w;
    // Use window height as the canvas denominator so buttons stay reachable
    // even on very wide screens. Cap the canvas pixel size so buttons don't
    // bloat absurdly on tablets.
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
    if (main_window != NULL)
        SDL_GetWindowSize(main_window, win_w, win_h);
    else {
        *win_w = 1;
        *win_h = 1;
    }
}

static void synth_mouse_motion(int win_x, int win_y)
{
    SDL_Event e = {0};
    e.type = SDL_MOUSEMOTION;
    e.motion.which = SDL_TOUCH_MOUSEID;
    e.motion.x = win_x;
    e.motion.y = win_y;
    SDL_PushEvent(&e);
}

static void synth_mouse_button(int win_x, int win_y, bool down)
{
    SDL_Event e = {0};
    e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    e.button.which = SDL_TOUCH_MOUSEID;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.state = down ? SDL_PRESSED : SDL_RELEASED;
    e.button.clicks = 1;
    e.button.x = win_x;
    e.button.y = win_y;
    SDL_PushEvent(&e);
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
            } else {
                // Ship-control finger: drive ship target and also synthesize
                // a mouse click so OpenTyrian menus respond to taps.
                ship_finger = ev->tfinger.fingerId;
                ship_target_win_x = fx;
                ship_target_win_y = fy;
                synth_mouse_motion(fx, fy);
                synth_mouse_button(fx, fy, true);
            }
            break;
        }
        case SDL_FINGERMOTION: {
            int fx = (int)(ev->tfinger.x * win_w);
            int fy = (int)(ev->tfinger.y * win_h);
            if (ev->tfinger.fingerId == ship_finger) {
                ship_target_win_x = fx;
                ship_target_win_y = fy;
                synth_mouse_motion(fx, fy);
            }
            break;
        }
        case SDL_FINGERUP: {
            int fx = (int)(ev->tfinger.x * win_w);
            int fy = (int)(ev->tfinger.y * win_h);
            if (ev->tfinger.fingerId == ship_finger) {
                ship_finger = SHIP_FINGER_NONE;
                synth_mouse_button(fx, fy, false);
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

void android_input_render_overlay(SDL_Renderer *renderer, int win_w, int win_h)
{
    if (!initialized) return;
    SDL_BlendMode old;
    SDL_GetRenderDrawBlendMode(renderer, &old);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < ANDROID_BTN_COUNT; ++i) {
        SDL_Rect r;
        button_rect_to_window(&buttons[i], win_w, win_h, &r);

        Uint8 fill_alpha = btn_down[i] ? 180 : 90;
        set_color(renderer, buttons[i].color, fill_alpha);
        SDL_RenderFillRect(renderer, &r);

        set_color(renderer, 0xFFFFFF, btn_down[i] ? 240 : 160);
        SDL_RenderDrawRect(renderer, &r);
        // Double border for visibility against any background.
        SDL_Rect inner = { r.x + 1, r.y + 1, r.w - 2, r.h - 2 };
        SDL_RenderDrawRect(renderer, &inner);
    }

    // If ship-control finger is active, draw a small crosshair where it points.
    if (ship_finger != SHIP_FINGER_NONE) {
        int cx = ship_target_win_x;
        int cy = ship_target_win_y;
        int size = win_h / 40;
        if (size < 8) size = 8;
        set_color(renderer, 0xFFFFFF, 160);
        SDL_Rect h = { cx - size, cy - 1, 2 * size, 3 };
        SDL_Rect v = { cx - 1, cy - size, 3, 2 * size };
        SDL_RenderFillRect(renderer, &h);
        SDL_RenderFillRect(renderer, &v);
    }

    SDL_SetRenderDrawBlendMode(renderer, old);
}

#else  // !__ANDROID__

void android_input_init(void) {}
void android_input_handle_event(const SDL_Event *ev) { (void)ev; }
bool android_input_button_down(AndroidButton b) { (void)b; return false; }
bool android_input_get_ship_target(int *x, int *y) { (void)x; (void)y; return false; }
void android_input_render_overlay(SDL_Renderer *r, int w, int h) { (void)r; (void)w; (void)h; }

#endif
