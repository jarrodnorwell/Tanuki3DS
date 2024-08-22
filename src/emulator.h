#ifndef EMULATOR_H
#define EMULATOR_H

#include <SDL2/SDL.h>

#include "emulator_state.h"
#include "types.h"

int emulator_init(int argc, char** argv);
void emulator_quit();

void emulator_reset();

void read_args(int argc, char** argv);
void hotkey_press(SDL_KeyCode key);

#endif