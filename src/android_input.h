/*
 * Touch input for Android: one finger tracks the ship like a mouse cursor,
 * a set of floating buttons on the left drives the action keys.
 */
#ifndef ANDROID_INPUT_H
#define ANDROID_INPUT_H

#include <SDL.h>
#include <stdbool.h>

typedef enum {
    ANDROID_BTN_FIRE = 0,
    ANDROID_BTN_CHANGE_WEAPON,
    ANDROID_BTN_LEFT_SIDEKICK,
    ANDROID_BTN_RIGHT_SIDEKICK,
    ANDROID_BTN_MENU,
    ANDROID_BTN_COUNT
} AndroidButton;

void android_input_init(void);
void android_input_handle_event(const SDL_Event *ev);
bool android_input_button_down(AndroidButton b);

/* When a ship-tracking finger is active, writes the target ship position
 * in game-screen coordinates and returns true. Returns false otherwise. */
bool android_input_get_ship_target(int *out_x, int *out_y);

/* Render the floating-button overlay on the main window renderer.
 * Called from video.c between RenderCopy and RenderPresent. */
void android_input_render_overlay(SDL_Renderer *renderer, int win_w, int win_h);

#endif
