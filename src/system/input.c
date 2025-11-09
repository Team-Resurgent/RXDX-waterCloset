/*
Copyright (C) 2019 Parallel Realities

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "input.h"

 /* --- D-Pad mapped to synthetic "button" slots. Adjust if needed. --- */
#define DPAD_UP_BTN     20
#define DPAD_DOWN_BTN   21
#define DPAD_LEFT_BTN   22
#define DPAD_RIGHT_BTN  23

/* If your codebase defines JOYPAD_BUTTON_MAX, great. Otherwise pick a safe size. */
#ifndef JOYPAD_BUTTON_MAX
#define JOYPAD_BUTTON_MAX 64
#endif

/* Rising-edge tracking for joystick buttons */
static unsigned char s_prevJoypadButton[JOYPAD_BUTTON_MAX];
static unsigned char s_edgeJoypadButton[JOYPAD_BUTTON_MAX];

/* Call this at the start of each frame (we do it inside doInput) */
static inline void beginInputFrame(void)
{
    /* clear one-shot edges each frame */
    for (int i = 0; i < JOYPAD_BUTTON_MAX; i++) {
        s_edgeJoypadButton[i] = 0;
    }
}

/* Public-style helper you can call from gameplay code */
static inline int getJoypadButtonPressedOnce(int button)
{
    if (button < 0 || button >= JOYPAD_BUTTON_MAX) return 0;
    return s_edgeJoypadButton[button] ? 1 : 0;
}

/* --- existing functions --- */

void doKeyUp(SDL_KeyboardEvent* event)
{
    if (event->repeat == 0 && event->keysym.scancode < MAX_KEYBOARD_KEYS)
    {
        app.keyboard[event->keysym.scancode] = 0;
    }
}

void doKeyDown(SDL_KeyboardEvent* event)
{
    if (event->repeat == 0 && event->keysym.scancode < MAX_KEYBOARD_KEYS)
    {
        app.keyboard[event->keysym.scancode] = 1;
        app.lastKeyPressed = event->keysym.scancode;
    }
}

static void doMouseDown(SDL_MouseButtonEvent* event)
{
    if (event->button >= 0 && event->button < MAX_MOUSE_BUTTONS)
    {
        app.mouse.buttons[event->button] = 1;
    }
}

static void doMouseUp(SDL_MouseButtonEvent* event)
{
    if (event->button >= 0 && event->button < MAX_MOUSE_BUTTONS)
    {
        app.mouse.buttons[event->button] = 0;
    }
}

/*
 * Note: the following assumes that SDL_BUTTON_X1 and SDL_BUTTON_X2 are 4 and 5, respectively. They usually are.
 */
static void doMouseWheel(SDL_MouseWheelEvent* event)
{
    if (event->y == -1) { app.mouse.buttons[SDL_BUTTON_X1] = 1; }
    if (event->y == 1) { app.mouse.buttons[SDL_BUTTON_X2] = 1; }
}

/* --- Joystick button handling with edge detection --- */
static void doButtonDown(SDL_JoyButtonEvent* event)
{
    int b = (int)event->button;
    if (b >= 0 && b < JOYPAD_BUTTON_MAX && event->state == SDL_PRESSED)
    {
        /* rising edge? (prev == 0, now going to 1) */
        if (!s_prevJoypadButton[b]) {
            s_edgeJoypadButton[b] = 1;   /* one-shot this frame */
        }
        s_prevJoypadButton[b] = 1;       /* remember pressed */
        app.joypadButton[b] = 1;         /* held state */
        app.lastButtonPressed = b;
    }
}

static void doButtonUp(SDL_JoyButtonEvent* event)
{
    int b = (int)event->button;
    if (b >= 0 && b < JOYPAD_BUTTON_MAX && event->state == SDL_RELEASED)
    {
        s_prevJoypadButton[b] = 0;
        app.joypadButton[b] = 0;
    }
}

/* Invert Y axes so UP is negative, DOWN is positive */
static void doJoyAxis(SDL_JoyAxisEvent* event)
{
    if (event->axis < JOYPAD_AXIS_MAX)
    {
        int value = event->value;
        if (event->axis == 1 || event->axis == 3) {
            value = -value; /* axis 1 = left Y, axis 3 = right Y */
        }
        app.joypadAxis[event->axis] = value;
    }
}

/* Hat/D-pad handling: maps to virtual buttons and also drives left-stick axes */
static void doJoyHat(SDL_JoyHatEvent* event)
{
    /* Map to synthetic buttons 20..23 */
    app.joypadButton[DPAD_UP_BTN] = (event->value & SDL_HAT_UP) ? 1 : 0;
    app.joypadButton[DPAD_DOWN_BTN] = (event->value & SDL_HAT_DOWN) ? 1 : 0;
    app.joypadButton[DPAD_LEFT_BTN] = (event->value & SDL_HAT_LEFT) ? 1 : 0;
    app.joypadButton[DPAD_RIGHT_BTN] = (event->value & SDL_HAT_RIGHT) ? 1 : 0;

    /* Also drive the left-stick axes so movement works even if code only reads axes */
    const int16_t AXIS_MIN = -32768;  /* left / up  */
    const int16_t AXIS_MAX = 32767;  /* right / down */
    const int     DEAD = 10000;  /* if the real stick is moving, don't override it */

    int x = app.joypadAxis[0];
    int y = app.joypadAxis[1];

    if (abs(x) < DEAD) {
        int16_t hx = 0;
        if (event->value & SDL_HAT_LEFT)  hx = AXIS_MIN;
        if (event->value & SDL_HAT_RIGHT) hx = AXIS_MAX;
        app.joypadAxis[0] = hx;
    }

    if (abs(y) < DEAD) {
        int16_t hy = 0;
        /* UP should be negative on Y; DOWN positive */
        if (event->value & SDL_HAT_UP)    hy = AXIS_MIN;
        if (event->value & SDL_HAT_DOWN)  hy = AXIS_MAX;
        app.joypadAxis[1] = hy;
    }
}

void doInput(void)
{
    SDL_Event event;

    beginInputFrame(); /* reset one-shot edges */

    while (SDL_PollEvent(&event))
    {
        /* --- optional debug ---
        if (event.type == SDL_JOYBUTTONDOWN) SDL_Log("JOY BTN %d", (int)event.jbutton.button);
        if (event.type == SDL_JOYAXISMOTION && abs(event.jaxis.value) > 10000)
            SDL_Log("JOY AXIS %d = %d", (int)event.jaxis.axis, (int)event.jaxis.value);
        if (event.type == SDL_JOYHATMOTION) SDL_Log("JOY HAT value=0x%02X", (unsigned)event.jhat.value);
        --- end debug --- */

        switch (event.type) {
        case SDL_MOUSEWHEEL:       doMouseWheel(&event.wheel); break;
        case SDL_MOUSEBUTTONDOWN:  doMouseDown(&event.button); break;
        case SDL_MOUSEBUTTONUP:    doMouseUp(&event.button); break;
        case SDL_KEYDOWN:          doKeyDown(&event.key); break;
        case SDL_KEYUP:            doKeyUp(&event.key); break;

        case SDL_JOYBUTTONDOWN:    doButtonDown(&event.jbutton); break;
        case SDL_JOYBUTTONUP:      doButtonUp(&event.jbutton); break;
        case SDL_JOYAXISMOTION:    doJoyAxis(&event.jaxis); break;
        case SDL_JOYHATMOTION:     doJoyHat(&event.jhat); break;

        case SDL_QUIT:             exit(0); break;
        default: break;
        }
    }

    SDL_GetMouseState(&app.mouse.x, &app.mouse.y);

    /* Example usage (remove after wiring into your real control code):
       if (getJoypadButtonPressedOnce(0)) SDL_Log("A was pressed this frame (one-shot)");
    */
}
