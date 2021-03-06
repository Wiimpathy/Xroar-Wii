/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#ifdef GEKKO
#include <SDL_ttf.h>
#endif
#include "types.h"
#include "logging.h"
#include "input.h"
#include "keyboard.h"
#include "machine.h"
#include "module.h"
#include "printer.h"
#include "xroar.h"

#ifdef GEKKO
#include "vdisk.h"
#include <dirent.h>
#include <assert.h>
#include <wiiuse/wpad.h>
#endif

static int init(void);
static void update_kbd_translate(void);

KeyboardModule keyboard_sdl_module = {
	.common = { .name = "sdl", .description = "SDL keyboard driver",
	            .init = init },
	.update_kbd_translate = update_kbd_translate,
};

#ifndef GEKKO
struct keymap {
	const char *name;
	unsigned int *raw;
};
#endif

#include "keyboard_sdl_mappings.c"

static unsigned int control = 0, shift = 0;
static unsigned int emulate_joystick = 0;
static int old_frameskip = 0;

static uint_least16_t sdl_to_keymap[768];

#ifdef GEKKO
extern char GamesDir[FILENAME_MAX];
extern char currentgame[256];

void display_message(char *text, int delay);
void getdirectory(void);
char * LoadRom(char * path, char * file);
void savestate(char *file, int choice);
int vdrive_insert_disk(int drive, struct vdisk *disk);
int vdrive_eject_disk(int drive);
#endif

/* Track which unicode value was last generated by key presses (SDL only
 * guarantees to fill in the unicode field on key presses, not releases). */
static unsigned int unicode_last_keysym[SDLK_LAST];

static const char *keymap_option;
static unsigned int *selected_keymap;

#ifdef GEKKO
void map_keyboard(unsigned int *map) {
#else
static void map_keyboard(unsigned int *map) {
#endif
	int i;
	for (i = 0; i < 256; i++)
		sdl_to_keymap[i] = i & 0x7f;
	for (i = 0; i < SDLK_LAST; i++)
		unicode_last_keysym[i] = 0;
	if (map == NULL)
		return;
	while (*map) {
		unsigned int sdlkey = *(map++);
		unsigned int dgnkey = *(map++);
		sdl_to_keymap[sdlkey & 0xff] = dgnkey & 0x7f;
	}
}

static int init(void) {
	int i;
	keymap_option = xroar_opt_keymap;
	selected_keymap = NULL;
	for (i = 0; mappings[i].name; i++) {
		if (selected_keymap == NULL
				&& !strcmp("uk", mappings[i].name)) {
			selected_keymap = mappings[i].raw;
		}
		if (keymap_option && !strcmp(keymap_option, mappings[i].name)) {
			selected_keymap = mappings[i].raw;
			LOG_DEBUG(2,"\tSelecting '%s' keymap\n",keymap_option);
		}
	}
	map_keyboard(selected_keymap);
	SDL_EnableUNICODE(xroar_kbd_translate);
	return 0;
}

static void update_kbd_translate(void) {
	SDL_EnableUNICODE(xroar_kbd_translate);
}

static void emulator_command(SDLKey sym) {
	switch (sym) {
	case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4:
#ifdef GEKKO
		if (shift) {
			display_message("Not implemented !!!", 800);
		} else {
			char *filename = malloc(256);
			memset(filename, 0x00, 256);

			getdirectory();
			filename=(char *)LoadRom(GamesDir, filename);
			sprintf(currentgame, "%s", filename);

			if (filename) {
				struct vdisk *disk = vdisk_load(filename);
				vdrive_eject_disk(sym - SDLK_1);
				vdrive_insert_disk(sym - SDLK_1, disk);
			}
		}
#else
		if (shift) {
			xroar_new_disk(sym - SDLK_1);
		} else {
			xroar_insert_disk(sym - SDLK_1);
		}
#endif
		break;
	case SDLK_5: case SDLK_6: case SDLK_7: case SDLK_8:
		if (shift) {
			xroar_set_write_back(sym - SDLK_5, XROAR_TOGGLE);
		} else {
			xroar_set_write_enable(sym - SDLK_5, XROAR_TOGGLE);
		}
		break;
	case SDLK_a:
		xroar_select_cross_colour(XROAR_CYCLE);
		break;
	case SDLK_c:
	case SDLK_q:
		xroar_quit();
		break;
	case SDLK_e:
		xroar_set_cart(XROAR_TOGGLE);
		break;
	case SDLK_f:
		xroar_fullscreen(XROAR_TOGGLE);
		break;
	case SDLK_i:
#ifdef GEKKO
		{
			/* insert a cartridge and run */
			char *filename = malloc(256);
			memset(filename, 0x00, 256);
			getdirectory();
			filename=LoadRom(GamesDir, filename);
			sprintf(currentgame, "%s", filename);
			xroar_load_file_by_type(filename,1);
		}
#else
		if (shift) {
			xroar_load_file(xroar_cart_exts);
		} else {
			xroar_run_file(xroar_cart_exts);
		}
#endif
		break;
	case SDLK_j:
		if (shift) {
			input_control_press(INPUT_SWAP_JOYSTICKS, 0);
		} else {
			if (emulate_joystick) {
				input_control_press(INPUT_SWAP_JOYSTICKS, 0);
			}
			emulate_joystick = (emulate_joystick + 1) % 3;
		}
		break;
	case SDLK_k:
		xroar_set_keymap(XROAR_CYCLE);
		break;
	case SDLK_b:
	case SDLK_h:
	case SDLK_t:
	case SDLK_l:
#ifdef GEKKO
		{
			char *filename = malloc(256);
			memset(filename, 0x00, 256);
			getdirectory();
			filename=LoadRom(GamesDir, filename);

			if (filename[0] != '\0')
			{
				sprintf(currentgame, "%s", filename);
				if (shift) {
					machine_reset(RESET_HARD);
					xroar_load_file_by_type(filename,1); /* Run file */
					} else {
						xroar_load_file_by_type(filename,0); /* Load file */
					}
			}
		}
#else
		if (shift) {
			xroar_run_file(NULL);
		} else {
			xroar_load_file(NULL);
		}
#endif
		break;
	case SDLK_m:
		xroar_set_machine(XROAR_CYCLE);
		break;
	case SDLK_p:
		if (shift)
			printer_flush();
		break;
	case SDLK_r:
		machine_reset(shift ? RESET_HARD : RESET_SOFT);
		break;
	case SDLK_s:
#ifdef GEKKO
		if(currentgame[0] != '\0')
			savestate(currentgame, 0);
		else
			display_message("Can't save file!", 200);
#else
		xroar_save_snapshot();
#endif
		break;
	case SDLK_w:
		xroar_select_tape_output();
		break;
#ifdef TRACE
	case SDLK_v:
		xroar_set_trace(XROAR_TOGGLE);
		break;
#endif
	case SDLK_z: // running out of letters...
		xroar_set_kbd_translate(XROAR_TOGGLE);
		break;
	default:
		break;
	}
	return;
}

void sdl_keypress(SDL_keysym *keysym) {
	SDLKey sym = keysym->sym;
	if (emulate_joystick) {
		if (sym == SDLK_UP) { input_control_press(INPUT_JOY_RIGHT_Y, 0); return; }
		if (sym == SDLK_DOWN) { input_control_press(INPUT_JOY_RIGHT_Y, 255); return; }
		if (sym == SDLK_LEFT) { input_control_press(INPUT_JOY_RIGHT_X, 0); return; }
		if (sym == SDLK_RIGHT) { input_control_press(INPUT_JOY_RIGHT_X, 255); return; }
		if (sym == SDLK_LALT) { input_control_press(INPUT_JOY_RIGHT_FIRE, 0); return; }
	}
	if (sym == SDLK_LSHIFT || sym == SDLK_RSHIFT) {
		shift = 1;
		KEYBOARD_PRESS(0);
		return;
	}
	if (sym == SDLK_LCTRL || sym == SDLK_RCTRL) { control = 1; return; }
	if (sym == SDLK_F11) {
		xroar_fullscreen(XROAR_TOGGLE);
		return;
	}
	if (sym == SDLK_F12) {
		xroar_noratelimit = 1;
		old_frameskip = xroar_frameskip;
		xroar_frameskip = 10;
	}
	if (control) {
		emulator_command(sym);
		return;
	}
	if (sym == SDLK_UP) { KEYBOARD_PRESS(94); return; }
	if (sym == SDLK_DOWN) { KEYBOARD_PRESS(10); return; }
	if (sym == SDLK_LEFT) { KEYBOARD_PRESS(8); return; }
	if (sym == SDLK_RIGHT) { KEYBOARD_PRESS(9); return; }
	if (sym == SDLK_HOME) { KEYBOARD_PRESS(12); return; }
	if (xroar_kbd_translate) {
		unsigned int unicode;
		if (sym >= SDLK_LAST)
			return;
		unicode = keysym->unicode;
		/* Map shift + backspace/delete to ^U */
		if (shift && (unicode == 8 || unicode == 127)) {
			unicode = 0x15;
		}
		unicode_last_keysym[sym] = unicode;
		keyboard_unicode_press(unicode);
		return;
	}
	if (sym < 256) {
		unsigned int mapped = sdl_to_keymap[sym];
		KEYBOARD_PRESS(mapped);
	}
}

#define JOY_UNLOW(j) if (j < 127) j = 127;
#define JOY_UNHIGH(j) if (j > 128) j = 128;

void sdl_keyrelease(SDL_keysym *keysym) {
	SDLKey sym = keysym->sym;
	if (emulate_joystick) {
		if (sym == SDLK_UP) { input_control_release(INPUT_JOY_RIGHT_Y, 0); return; }
		if (sym == SDLK_DOWN) { input_control_release(INPUT_JOY_RIGHT_Y, 255); return; }
		if (sym == SDLK_LEFT) { input_control_release(INPUT_JOY_RIGHT_X, 0); return; }
		if (sym == SDLK_RIGHT) { input_control_release(INPUT_JOY_RIGHT_X, 255); return; }
		if (sym == SDLK_LALT) { input_control_release(INPUT_JOY_RIGHT_FIRE, 0); return; }
	}
	if (sym == SDLK_LSHIFT || sym == SDLK_RSHIFT) {
		shift = 0;
		KEYBOARD_RELEASE(0);
		return;
	}
	if (sym == SDLK_LCTRL || sym == SDLK_RCTRL) { control = 0; return; }
	if (sym == SDLK_F12) {
		xroar_noratelimit = 0;
		xroar_frameskip = old_frameskip;
	}
	if (sym == SDLK_UP) { KEYBOARD_RELEASE(94); return; }
	if (sym == SDLK_DOWN) { KEYBOARD_RELEASE(10); return; }
	if (sym == SDLK_LEFT) { KEYBOARD_RELEASE(8); return; }
	if (sym == SDLK_RIGHT) { KEYBOARD_RELEASE(9); return; }
	if (sym == SDLK_HOME) { KEYBOARD_RELEASE(12); return; }
	if (xroar_kbd_translate) {
		unsigned int unicode;
		if (sym >= SDLK_LAST)
			return;
		unicode = unicode_last_keysym[sym];
		/* Map shift + backspace/delete to ^U */
		if (shift && (unicode == 8 || unicode == 127)) {
			unicode = 0x15;
		}
		keyboard_unicode_release(unicode);
		/* Might have unpressed shift prematurely */
		if (shift)
			KEYBOARD_PRESS(0);
		return;
	}
	if (sym < 256) {
		unsigned int mapped = sdl_to_keymap[sym];
		KEYBOARD_RELEASE(mapped);
	}
}
