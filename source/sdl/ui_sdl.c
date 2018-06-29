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

#include "types.h"
#include "keyboard.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "sam.h"
#include "vdg.h"
#include "xroar.h"
#ifdef GEKKO
#include <SDL_image.h> /* avoids assignment warnings */
#include <dirent.h>
#include <assert.h>
#include <wiiuse/wpad.h>
#include "cart.h"
#include "input.h"
#include "vdisk.h"
#include "images/cocokeyboard.h"
#include "images/dragonkeyboard.h"
#include "images/wiimote.xpm"
#include "menubg.h"
#include "keyboard_sdl_mappings.c"
#endif

void sdl_run(void);

extern VideoModule video_sdlgl_module;
extern VideoModule video_sdlyuv_module;
extern VideoModule video_sdl_module;
extern VideoModule video_null_module;
static VideoModule *sdl_video_module_list[] = {

/* Don't use YUV module at all */
#define PREFER_NOYUV
#ifdef HAVE_SDLGL
	&video_sdlgl_module,
#endif
#ifdef PREFER_NOYUV
	&video_sdl_module,
	//&video_sdlyuv_module,
#else
//	&video_sdlyuv_module,
	&video_sdl_module,
#endif
	&video_null_module,
	NULL
};

extern KeyboardModule keyboard_sdl_module;
static KeyboardModule *sdl_keyboard_module_list[] = {
	&keyboard_sdl_module,
	NULL
};

/* Note: prefer the default order for sound and joystick modules, which
 * will include the SDL options. */

UIModule ui_sdl_module = {
	.common = { .name = "sdl", .description = "SDL user-interface" },
	.video_module_list = sdl_video_module_list,
	.keyboard_module_list = sdl_keyboard_module_list,
	.run = sdl_run,
};

void sdl_keypress(SDL_keysym *keysym);
void sdl_keyrelease(SDL_keysym *keysym);

#ifdef GEKKO
#define VERSION "XRoar 0.4"

#define HELPTXT_Y 207
#define SETTINGTXT_Y 216

/* Analog sticks sensitivity */
#define ANALOG_SENSITIVITY 30

/* Delay before held keys triggering */
/* higher is the value, less responsive is the key update */
#define HELD_DELAY 10

/* Direction & selection update speed when a key is being held */
/* lower is the value, faster is the key update */
#define HELD_SPEED 4

#define WPAD_BUTTONS_HELD (WPAD_BUTTON_UP | WPAD_BUTTON_DOWN | WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT | \
		WPAD_CLASSIC_BUTTON_UP | WPAD_CLASSIC_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_RIGHT)

#define PAD_BUTTONS_HELD  (PAD_BUTTON_UP | PAD_BUTTON_DOWN | PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT)

extern SDL_Surface *screen, *openDialog;
extern TTF_Font *text_fontSmall, *text_fontLarge;
char currentgame[256], GamesDir[FILENAME_MAX];
static char statefile[256];
static int numberFiles = 34;
static unsigned int emulate_keyboard = 0, autorun = 1;
static unsigned int joymode = 1;
u16 MenuInput;
int Reset_Mode = 0;
int dir_speed[2][2] = {{127, 127}, {127, 127 }};
bool Dpad_Dir[2][4];
bool joy_sensitivity = false, dpad_delay = false;
int mousex, mousey;
/* Configurable buttons for the GameCube controller */
int gc_keymap[7] = {32, 13, 49, 89, 0, 0, 0 };
/* Configurable buttons for Wiimote and extensions */
int wiimote_keymap[17] = {32, 13, 49, 32, 0, 0, 0, 0, 0, 32, 13, 0, 89, 0, 0, 0, 0};
enum {Regular, Bold, Italic, Underline};
SDL_Color Red = {255,0,0,0};
SDL_Color Green = {0,255,0,0};
SDL_Color Blue = {0,0,255,0};
SDL_Color Yellow = {255,255,0,0};
SDL_Color DarkGreen = {0,100,0,0};
SDL_Color White = {255,255,255,0};
SDL_Color Black = {0,0,0,0};
SDL_Color DarkGrey = {128,128,128,0};

void map_keyboard(unsigned int *map);
int read_snapshot(const char *filename);
int write_snapshot(const char *filename);
void video_configure();
int menu();

typedef struct
{
  u8 flags;
  char filename[256];
}FILES;

FILES filelist[1000];

/*-----------------------------------------------------------------------*/
/**
 * Draw a text string using TTF.
 *
 * bool cut: define a max string length and cut if it exceeds.
 * int style: Regular, Bold, Italic, Underline.
 */
void DrawText(int x, int y, bool cut, char * text, TTF_Font *theFont, int style, SDL_Color Color,  SDL_Surface *screen)
{
	if (style == Regular)
		TTF_SetFontStyle(theFont, TTF_STYLE_NORMAL);
	else if (style == Bold)
		TTF_SetFontStyle(theFont, TTF_STYLE_BOLD);
	else if (style == Italic)
		TTF_SetFontStyle(theFont, TTF_STYLE_ITALIC);
	else if (style == Underline)
		TTF_SetFontStyle(theFont, TTF_STYLE_UNDERLINE);

	SDL_Surface *sText = TTF_RenderText_Solid( theFont, text, Color );
	SDL_Rect rcDest = {x,y,10,10};
	SDL_Rect srcDest = {0,0,235,20};

	if (cut)
		SDL_BlitSurface( sText,&srcDest, screen,&rcDest );
	else
		SDL_BlitSurface( sText,NULL, screen,&rcDest );

	SDL_FreeSurface( sText );
}

/*-----------------------------------------------------------------------*/
/**
 * Loads an image from memory and apply transparency.
 *
 */
SDL_Surface *Show_Background(const char *filerw, SDL_Surface** img, int size)
{
	SDL_Surface *TempBg, *BackgroundImg = NULL;
	SDL_RWops* Background_rwops = NULL;
	SDL_Rect menu_rect = {0,0,320,240};

	Background_rwops = SDL_RWFromMem( (char *)filerw, size);
	*img = IMG_Load_RW(Background_rwops, 1);
	BackgroundImg= SDL_CreateRGBSurface(0,320,240,32,0,0,0,0);

	if (SDL_SetAlpha(*img, SDL_SRCALPHA, 20) < 0)
		return NULL;

	return BackgroundImg;
}

/*-----------------------------------------------------------------------*/
/**
 * Display a text string in a dialog box.
 *
 */
void display_message(char *text, int delay)
{
	int i = 0;
	Uint32 bluebg  = SDL_MapRGB(openDialog->format,0,0,255);
	SDL_Rect rect;

	rect.w = 320;
	rect.h = 20;
	rect.x = 0;
	rect.y = openDialog->h - 40;

	SDL_FillRect(openDialog, &rect, bluebg);
	SDL_BlitSurface(openDialog, &rect, screen, &rect);
	SDL_UpdateRect(screen, 0, 0, rect.w, rect.h);

	while (i <= delay)
	{
		DrawText( rect.x+5, rect.y+5, false, text, text_fontSmall, Regular, DarkGrey, screen);
		i++;
		SDL_UpdateRect(screen, 0, 0, 320, 240);
		SDL_Flip(screen);
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Wii controllers sticks to Dragon/Coco sticks
 * 
 * First Wiimote and GameCube controller(0 & 4 SDL index) is the right
 * Dragon/Coco joystick(the default) with axis index 0 & 1.
 * Cf input.h
 * joynbr : the Dragon/Coco joystick axis(0,1 = Right joy; 2,3 = Left Joy).
 * 
 */
void joystick_move(int which, int axis, int value, bool sensitive)
{
	int joynbr;
	int axis_position;

	if (which == 0 || which == 4)
		joynbr = 0;
	else if  (which == 1 || which == 5)
		joynbr = 2;

	if (sensitive)
		axis_position = (value + 32768) >> 8;
	else
		axis_position = (value + 2000) >> 4;

	if(axis == 0) {
		/* Left-right movement */
		if (value < -3200)
		{
			input_control_press(joynbr, axis_position);
		}
		else if (value > 3200)
		{
			input_control_press(joynbr, axis_position);
		}
		else 
		{
			input_control_release(joynbr, 0);
			input_control_release(joynbr, 255);
		}
	} else if(axis == 1) {
		/* Up-Down movement */
		if (value < 3200)
			input_control_press(joynbr + 1, axis_position);
		else if (value > 3200)
			input_control_press(joynbr + 1, axis_position);
		else {
			input_control_release(joynbr + 1, 0);
			input_control_release(joynbr + 1, 255);
		}
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Wii controllers hats to Dragon/Coco sticks
 * 
 * First Wiimote and GameCube controller(0 & 4 SDL index) are the right
 * Dragon/Coco joystick(the default) with axis index 0 & 1.
 * 
 * which : joystick port = 0 or 1.
 * dir : the axis 0 = left/right, 1 = up/down
 * value : axis position between 0 & 255.
 * joynbr : the Dragon/Coco joystick axis(0,1 = Right joy; 2,3 = Left Joy).
 * Cf input.h
 */
void joyhat_move(int which, int dir, int value)
{
	int joynbr;

	if (which == 1)
		joynbr = 2;
	else
		joynbr = which;

	if (dir == 0)
		input_control_press(joynbr, value);
	else if (dir == 1)
		input_control_press(joynbr + 1, value);
}

static void Main_HandleWiiHat(int JoyId)
{
	if (JoyId == 0)
	{
		/* Go left */
		if (Dpad_Dir[JoyId][0])
		{
			dir_speed[JoyId][0]--;
			joyhat_move(JoyId, 0, dir_speed[JoyId][0]);
		}
		/* Go right */
		if (Dpad_Dir[JoyId][1])
		{
			dir_speed[JoyId][0]++;
			joyhat_move(JoyId, 0, dir_speed[JoyId][0]);
		}
		/* Go up */
		if (Dpad_Dir[JoyId][2])
		{
			dir_speed[JoyId][1]--;
			joyhat_move(JoyId, 1, dir_speed[JoyId][1]);
		}
		/* Go down */
		if (Dpad_Dir[JoyId][3])
		{
			dir_speed[JoyId][1]++;
			joyhat_move(JoyId, 1, dir_speed[JoyId][1]);
		}
	}

	if (JoyId == 1)
	{
		if (Dpad_Dir[JoyId][0])
		{
			dir_speed[JoyId][0]--;
			joyhat_move(JoyId, 0, dir_speed[JoyId][0]);
		}
		if (Dpad_Dir[JoyId][1])
		{
			dir_speed[JoyId][0]++;
			joyhat_move(JoyId, 0, dir_speed[JoyId][0]);
		}
		if (Dpad_Dir[JoyId][2])
		{
			dir_speed[JoyId][1]--;
			joyhat_move(JoyId, 1, dir_speed[JoyId][1]);
		}
		if (Dpad_Dir[JoyId][3])
		{
			dir_speed[JoyId][1]++;
			joyhat_move(JoyId, 1, dir_speed[JoyId][1]);
		}
	}
}

void show_joymode(int mode)
{
	if (mode == 0)
		display_message("Controller mapped to keyboard arrows", 80);
	else if (mode == 1)
		display_message("Controller mapped to Right Joystick port", 80);
	else if (mode == 2)
		display_message("Controller mapped to Left Joystick port", 80);
}

void swap_joysticks()
{
	if (joymode)
	{
		input_control_press(INPUT_SWAP_JOYSTICKS, 0);
		emulate_keyboard = 0;
	}
	joymode = (joymode + 1) % 3;

	if (joymode == 0)
		emulate_keyboard = 1;
	else
		emulate_keyboard = 0;

	show_joymode(joymode);
}

/*-----------------------------------------------------------------------*/
/**
 * Wii controllers buttons to Dragon/Coco key
 * 
 * Press Fire button in joystick mode.
 * Sends a key configured in the mapper.
 * Some buttons are used to call the menu or swap the joystick mode.
 */
static void Main_HandleWiiButton(int sdlport, int sdlbutton, Uint8 pushed)
{
	int key;
	bool show_menu = false;

	switch (sdlport)
	{
		/* Wiimote/Classic 1 */
		case 0:
			key = wiimote_keymap[sdlbutton];

			/* Release configured Dragon/Coco key if any */
			if (!pushed)
			{
				keyboard_unicode_release(key);

				/* Dragon/Coco release Fire button */
				if (joymode && (sdlbutton == 0 || sdlbutton == 3 || sdlbutton == 9))
					input_control_release(INPUT_JOY_RIGHT_FIRE, 0);
			}
			else
			{
				/* Dragon/Coco Fire button */
				if (joymode && (sdlbutton == 0 || sdlbutton == 3 || sdlbutton == 9))
					input_control_press(INPUT_JOY_RIGHT_FIRE, 0);

				/* Wiimote/Classic button + : Swap Dragon/Coco Joysticks mode */
				if (sdlbutton == 5 || sdlbutton == 18)
				{
					swap_joysticks();
				}
				/* Wiimote/Classic button Home or GC button Start : Main Menu */
				if (sdlbutton == 6 || sdlbutton == 19)
					menu();

				/* Press configured Dragon/Coco key if any */
				keyboard_unicode_press(key);
			}
			break;
		/* Wiimote/Classic 2 */
		case 1:
			key = wiimote_keymap[sdlbutton];

			/* Release configured Dragon/Coco key if any */
			if (!pushed)
			{
				keyboard_unicode_release(key);

				/* Dragon/Coco release Fire button */
				if (joymode && (sdlbutton == 0 || sdlbutton == 3 || sdlbutton == 9))
					input_control_release(INPUT_JOY_LEFT_FIRE, 0);
			}
			else
			{
				/* Dragon/Coco Fire button */
				if (joymode && (sdlbutton == 0 || sdlbutton == 3 || sdlbutton == 9))
					input_control_press(INPUT_JOY_LEFT_FIRE, 0);

				/* Press configured Dragon/Coco key if any */
				keyboard_unicode_press(key);
			}
			break;
		/* GC Pad 1 */
		case 4:
			key = gc_keymap[sdlbutton];

			/* Release configured Dragon/Coco key if any */
			if (!pushed)
			{
				keyboard_unicode_release(key);

				/* Dragon/Coco release Fire button */
				if (joymode && sdlbutton == 0)
					input_control_release(INPUT_JOY_RIGHT_FIRE, 0);
			}
			else
			{
				/* Dragon/Coco Fire button */
				if (joymode && sdlbutton == 0)
					input_control_press(INPUT_JOY_RIGHT_FIRE, 0);

				/* GC button Z : Swap Dragon/Coco Joysticks mode */
				if(sdlbutton == 4)
				{
					swap_joysticks();
				}
				/* GC button Start : Main Menu */
				if(sdlbutton == 7)
					menu();

				/* Press configured Dragon/Coco key if any */
				keyboard_unicode_press(key);
			}
			break;
		/* GC Pad 2 */
		case 5:
			key = gc_keymap[sdlbutton];

			/* Release configured Dragon/Coco key if any */
			if (!pushed)
			{
				keyboard_unicode_release(key);

				/* Dragon/Coco release Fire button */
				if (joymode && sdlbutton == 0)
					input_control_release(INPUT_JOY_LEFT_FIRE, 0);
			}
			else
			{
				/* Dragon/Coco Fire button */
				if (joymode && sdlbutton == 0)
					input_control_press(INPUT_JOY_LEFT_FIRE, 0);

				/* Press configured Dragon/Coco key if any */
				keyboard_unicode_press(key);
			}
			break;
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Nunchuck/Classic controller Left stick x
 */
static int wpad_StickX(WPADData *data, u8 right)
{
	struct joystick_t* js = NULL;

	switch (data->exp.type)
	{
		case WPAD_EXP_NUNCHUK:
			js = right ? NULL : &data->exp.nunchuk.js;
			break;

		case WPAD_EXP_CLASSIC:
			js = right ? &data->exp.classic.rjs : &data->exp.classic.ljs;
			break;

		default:
			break;
	}

	if (js)
	{
		/* raw X position */
		int pos = js->pos.x;

		/* X range calibration */
		int min = js->min.x;
		int max = js->max.x;
		int center = js->center.x;
 
		/* value returned could be above calibration limits */
		if (pos > max) return 127;
		if (pos < min) return -128;
		
		/* adjust against center position */
		pos -= center;

		/* return interpolated range [-128;127] */
		if (pos > 0)
		{
			return (int)(127.0 * ((float)pos / (float)(max - center)));
		}
		else
		{
			return (int)(128.0 * ((float)pos / (float)(center - min)));
		}
	}

	return 0;
}
/*-----------------------------------------------------------------------*/
/**
 * Nunchuck/Classic controller Left stick y
 */
static int wpad_StickY(WPADData *data, u8 right)
{
	struct joystick_t* js = NULL;

	switch (data->exp.type)
	{
		case WPAD_EXP_NUNCHUK:
			js = right ? NULL : &data->exp.nunchuk.js;
			break;

		case WPAD_EXP_CLASSIC:
			js = right ? &data->exp.classic.rjs : &data->exp.classic.ljs;
			break;

		default:
			break;
	}

	if (js)
	{
		/* raw Y position */
		int pos = js->pos.y;

		/* Y range calibration */
		int min = js->min.y;
		int max = js->max.y;
		int center = js->center.y;
 
		/* value returned could be above calibration limits */
		if (pos > max) return 127;
		if (pos < min) return -128;
		
		/* adjust against center position */
		pos -= center;

		/* return interpolated range [-128;127] */
		if (pos > 0)
		{
			return (int)(127.0 * ((float)pos / (float)(max - center)));
		}
		else
		{
			return (int)(128.0 * ((float)pos / (float)(center - min)));
		}
	}

	return 0;
}

/*-----------------------------------------------------------------------*/
/**
 * Menu inputs used in dialogs
 */
void Input_Update(void)
{
	uint32_t held; 
	static int held_cnt = 0;

	/* PAD status update */
	PAD_ScanPads();

	/* PAD pressed keys */
	s16 pp = PAD_ButtonsDown(0);

	/* PAD held keys (direction/selection) */
	s16 hp = PAD_ButtonsHeld(0) & PAD_BUTTONS_HELD;

	/* PAD analog sticks (handled as PAD held direction keys) */
	s8 x = PAD_StickX(0);
	s8 y = PAD_StickY(0);
	if (x > ANALOG_SENSITIVITY)       hp |= PAD_BUTTON_RIGHT;
	else if (x < -ANALOG_SENSITIVITY) hp |= PAD_BUTTON_LEFT;
	else if (y > ANALOG_SENSITIVITY)  hp |= PAD_BUTTON_UP;
	else if (y < -ANALOG_SENSITIVITY) hp |= PAD_BUTTON_DOWN;

	/* WPAD status update */
	WPAD_ScanPads();
	WPADData *data = WPAD_Data(0);

	/* WPAD pressed keys */
	u32 pw = data->btns_d;

	/* WPAD held keys (direction/selection) */
	u32 hw = data->btns_h & WPAD_BUTTONS_HELD;

	/* WPAD analog sticks (handled as PAD held direction keys) */
	x = wpad_StickX(data, 0);
	y = wpad_StickY(data, 0);

	if (x > ANALOG_SENSITIVITY)       hp |= PAD_BUTTON_RIGHT;
	else if (x < -ANALOG_SENSITIVITY) hp |= PAD_BUTTON_LEFT;
	else if (y > ANALOG_SENSITIVITY)  hp |= PAD_BUTTON_UP;
	else if (y < -ANALOG_SENSITIVITY) hp |= PAD_BUTTON_DOWN;

	/* check if any direction/selection key is being held or just being pressed/released */
	if (pp||pw) held_cnt = 0;
	else if (hp||hw) held_cnt++;
	else held_cnt = 0;
		
	/* initial delay (prevents triggering to start immediately) */
	if (held_cnt > HELD_DELAY)
	{
		/* key triggering */
		pp |= hp;
		pw |= hw;

		/* delay until next triggering (adjusts direction/selection update speed) */
		held_cnt -= HELD_SPEED;
	}

	/* Wiimote & Classic Controller direction keys */
	if(data->ir.valid)
	{
		/* Wiimote is handled vertically */
		if (pw & (WPAD_BUTTON_UP))          pp |= PAD_BUTTON_UP;
		else if (pw & (WPAD_BUTTON_DOWN))   pp |= PAD_BUTTON_DOWN;
		else if (pw & (WPAD_BUTTON_LEFT))   pp |= PAD_BUTTON_LEFT;
		else if (pw & (WPAD_BUTTON_RIGHT))  pp |= PAD_BUTTON_RIGHT;
	}
	else
	{
		/* Wiimote is handled horizontally */
		if (pw & (WPAD_BUTTON_UP))          pp |= PAD_BUTTON_LEFT;
		else if (pw & (WPAD_BUTTON_DOWN))   pp |= PAD_BUTTON_RIGHT;
		else if (pw & (WPAD_BUTTON_LEFT))   pp |= PAD_BUTTON_DOWN;
		else if (pw & (WPAD_BUTTON_RIGHT))  pp |= PAD_BUTTON_UP;
	}

	/* Classic Controller direction keys */
	if (pw & WPAD_CLASSIC_BUTTON_UP)          pp |= PAD_BUTTON_UP;
	else if (pw & WPAD_CLASSIC_BUTTON_DOWN)   pp |= PAD_BUTTON_DOWN;
	else if (pw & WPAD_CLASSIC_BUTTON_LEFT)   pp |= PAD_BUTTON_LEFT;
	else if (pw & WPAD_CLASSIC_BUTTON_RIGHT)  pp |= PAD_BUTTON_RIGHT;

	/* WPAD button keys */
	if (pw & (WPAD_BUTTON_2|WPAD_BUTTON_A|WPAD_CLASSIC_BUTTON_A))  pp |= PAD_BUTTON_A;
	if (pw & (WPAD_BUTTON_1|WPAD_BUTTON_B|WPAD_CLASSIC_BUTTON_B))  pp |= PAD_BUTTON_B;

	if (pw & (WPAD_BUTTON_HOME|WPAD_CLASSIC_BUTTON_HOME))        pp |= PAD_BUTTON_START;
	if (pw & (WPAD_BUTTON_MINUS|WPAD_CLASSIC_BUTTON_MINUS))        pp |= PAD_TRIGGER_Z;
	if (pw & (WPAD_BUTTON_PLUS|WPAD_CLASSIC_BUTTON_PLUS))        pp |= PAD_TRIGGER_L;

	/* Update menu inputs */
	MenuInput = pp;
}

bool File_DirExists(const char *path)
{
	struct stat buf;
	return (stat(path, &buf) == 0 && S_ISDIR(buf.st_mode));
}

void getdirectory(void) {
	if (IS_DRAGON)
	{
		snprintf(GamesDir, sizeof(GamesDir), "usb:/roms/dragon/");

		if (!File_DirExists(GamesDir))		
			snprintf(GamesDir, sizeof(GamesDir), "sd:/roms/dragon/");
		if (!File_DirExists(GamesDir))		
			strcpy(GamesDir, "/");
	}
	else
	{
		snprintf(GamesDir, sizeof(GamesDir), "usb:/roms/coco/");

		if (!File_DirExists(GamesDir))		
			snprintf(GamesDir, sizeof(GamesDir), "sd:/roms/coco/");
		if (!File_DirExists(GamesDir))		
			strcpy(GamesDir, "/");
	}
}

/*-----------------------------------------------------------------------*/
/**
 * File_ShrinkName & File_SplitPath from Hatari
 */
void File_ShrinkName(char *pDestFileName, const char *pSrcFileName, int maxlen)
{
	int srclen = strlen(pSrcFileName);
	if (srclen < maxlen)
		strcpy(pDestFileName, pSrcFileName);  /* It fits! */
	else
	{
		assert(maxlen > 6);
		strncpy(pDestFileName, pSrcFileName, maxlen/2);

		if (maxlen&1)  /* even or uneven? */
			pDestFileName[maxlen/2-1] = 0;
		else
			pDestFileName[maxlen/2-2] = 0;

		strcat(pDestFileName, "...");
		strcat(pDestFileName, &pSrcFileName[strlen(pSrcFileName)-maxlen/2+1]);
	}
}

void File_SplitPath(const char *pSrcFileName, char *pName, char *pExt)
{
	char *ptr1, *ptr2;
	char pDir[1024];

	/* Build pathname: */
	ptr1 = strrchr(pSrcFileName, '/');
	if (ptr1)
	{
		strcpy(pName, ptr1+1);
		memmove(pDir, pSrcFileName, ptr1-pSrcFileName);
		pDir[ptr1-pSrcFileName] = 0;
	}
	else
	{
 		strcpy(pName, pSrcFileName);
		sprintf(pDir, ".%c", '/');
	}

	/* Build the raw filename: */
	if (pExt != NULL)
	{
		ptr2 = strrchr(pName+1, '.');
		if (ptr2)
		{
			pName[ptr2-pName] = 0;
			/* Copy the file extension: */
			strcpy(pExt, ptr2+1);
		}
		else
			pExt[0] = 0;
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Save or load a snapshot.
 * 
 * A savestate is filename minus extension + .sna.
 * choice : Save = 0 ; Load = 1;
 */
void savestate(char *file, int choice)
{
	char statepath[1024];
	char tempname[1024];
	char file_txt[1024];
	static char text[256];
	struct stat testfile;   

	strncpy(statepath, file, 1024);
	statepath[strlen(statepath)-4] = 0;
	sprintf(statepath, "%s.sna", statepath);

	File_SplitPath(statepath, tempname, NULL);
	File_ShrinkName(file_txt, tempname, 33);

	if (choice == 0)
	{
		sprintf(text,"Saving \"%s\".", file_txt);
	}
	else if (choice == 1)
	{
		if (stat (statepath, &testfile) == 0)
			sprintf(text,"Loading \"%s\".", file_txt);
		else
			sprintf(text,"Can't load \"%s\"!", file_txt);
	}

	display_message(text, 80);

	if (choice == 0)
		write_snapshot(statepath);
	else if (choice == 1)
		read_snapshot(statepath);
}

static int FileSortCallback(const void *f1, const void *f2)
{
  /* Special case for implicit directories */
  if(((FILES *)f1)->filename[0] == '.' || ((FILES *)f2)->filename[0] == '.')
  {
    if(strcmp(((FILES *)f1)->filename, ".") == 0) { return -1; }
    if(strcmp(((FILES *)f2)->filename, ".") == 0) { return 1; }
    if(strcmp(((FILES *)f1)->filename, "..") == 0) { return -1; }
    if(strcmp(((FILES *)f2)->filename, "..") == 0) { return 1; }
  }
  
  /* If one is a file and one is a directory the directory is first. */
  if(((FILES *)f1)->flags && !((FILES *)f2)->flags) return -1;
  if(!((FILES *)f1)->flags  && ((FILES *)f2)->flags) return 1;
  
  return strcasecmp(((FILES *)f1)->filename, ((FILES *)f2)->filename);
}

int Parse_Directory(DIR *dir)
{
	int count =0;
	struct dirent *dp = NULL;

	/* list entries */
	while ((dp=readdir(dir)) != NULL  && (count < 1000))
	{
		if (dp->d_name[0] == '.' && strcmp(dp->d_name, "..") != 0 )
			continue;

		memset(&filelist[count], 0, sizeof (FILES));
		sprintf(filelist[count].filename,"%s",dp->d_name);

		if (dp->d_type == DT_DIR)
		{
			filelist[count].flags = 1;
    		}
		count++;
	}

	/* close directory */
	closedir(dir);

	/* Sort the file list */
	qsort(filelist, count, sizeof(FILES), FileSortCallback);

	return count;
}

void Draw_Border(int x, int y, int w, int h)
{
	SDL_Rect box;

	/* Draw upper border: */
	box.x = x;
	box.y = y;
	box.w = w;
	box.h = 2;
	SDL_FillRect(screen, &box, SDL_MapRGB(screen->format,0xff,0x00, 0x00));

	/* Draw left border: */
	box.x = x;
	box.y = y;
	box.w = 2;
	box.h = h;
	SDL_FillRect(screen, &box, SDL_MapRGB(screen->format,0xff,0x00, 0x00));

	/* Draw bottom border: */
	box.x = x;
	box.y = y + h;
	box.w = w;
	box.h = 2;
	SDL_FillRect(screen, &box, SDL_MapRGB(screen->format,0xff,0x00, 0x00));

	/* Draw right border: */
	box.x = x + w;
	box.y = y;
	box.w = 2;
	box.h = h + 2;
	SDL_FillRect(screen, &box, SDL_MapRGB(screen->format,0xff,0x00, 0x00));
}

void Browsefiles(char *dir_path, char * selectedFile)
{
	int i;
	int count = 0;
	int loop=0;
	int fileStart=0, fileEnd=16, displayFiles=16;
	int highlight = 0;
	int realSelected = 0;
	char fileNames[256];
	char lastdir[256];
	struct dirent *dp;

	/* Open directory */
	DIR *dir = opendir(dir_path);
	if (dir == NULL)
	{
		loop = 1;
	}

	/* Keep current path */
	sprintf(lastdir, dir_path);

	/* "Clear" filelist names */
	for (i=0;i<numberFiles;i++)
		filelist[i].filename[0] = '\0';

	numberFiles = Parse_Directory(dir);

	if ((fileEnd > numberFiles) & (displayFiles > numberFiles))
	{
		fileEnd = numberFiles;
		displayFiles = numberFiles;
	}

	SDL_Rect rect = {0,0,320,280};
	SDL_Rect box = {27, 32, 258, 164};
	SDL_FillRect(openDialog, &rect, SDL_MapRGB(openDialog->format,30,30,30)); // green screen
	SDL_FillRect(openDialog, &box, SDL_MapRGB(openDialog->format,0,0,0));
	SDL_BlitSurface(openDialog, &rect, screen, &rect);
	SDL_UpdateRect(screen, 0, 0, rect.w, rect.h);

	do
	{
		Input_Update();

		if (MenuInput & PAD_BUTTON_UP)
		{
			SDL_BlitSurface(openDialog, &rect, screen, &rect);
			highlight--;
			if (realSelected > 0)
				realSelected--;
			if (highlight < 0)
			{
				fileStart--;
				highlight = 0;
			}
			if (fileStart < 0)
				fileStart = 0;
			fileEnd = fileStart + displayFiles;
			SDL_Delay(20);
		}
		else if (MenuInput & PAD_BUTTON_DOWN)
		{
			SDL_BlitSurface(openDialog, &rect, screen, &rect);
			if (realSelected < numberFiles-1)
				realSelected++;
			if (highlight < displayFiles)
			{
				highlight++;
			}
			if (highlight > (displayFiles-1))
			{
				highlight = (displayFiles-1);
				fileStart++;
			}
			if (fileStart  > (numberFiles -displayFiles))
			{
				fileStart = numberFiles-displayFiles;
			}
				fileEnd = fileStart + displayFiles;
				SDL_Delay(20);
		}
		else if (MenuInput & PAD_BUTTON_LEFT)
		{
			SDL_BlitSurface(openDialog, &rect, screen, &rect);

			if (fileStart > 15)
			{
				fileStart = fileStart - 15;
			}
			else
			{
				fileStart = fileStart - 7;

				if (fileStart < 0)
					fileStart = 0;
			}

			realSelected = fileStart;
			highlight = 0;
			fileEnd = fileStart + displayFiles;
			SDL_Delay(20);
		}
		else if (MenuInput & PAD_BUTTON_RIGHT)
		{
			SDL_BlitSurface(openDialog, &rect, screen, &rect);
				fileStart = fileEnd;
				realSelected = fileEnd;
				highlight = 0;
				fileEnd = fileStart + displayFiles;

			if (fileStart  > (numberFiles -displayFiles))
			{
				fileStart = numberFiles-displayFiles;
				realSelected = fileStart;
				highlight = 0;
				fileEnd = fileStart + displayFiles;
			}
				SDL_Delay(20);
		}
		else if (MenuInput & PAD_BUTTON_A)
		{
			/* It's a directory */
			if (filelist[realSelected].flags == 1)
			{
				char currentdir[256];
				sprintf(currentdir,"%s/%s", lastdir, filelist[realSelected].filename);
				sprintf(lastdir, currentdir);

				/* Update the path used to load files */
				sprintf(dir_path,"%s/", currentdir);

				DIR *dir = opendir(currentdir);

				/* "Clear" filelist names */
				for (i=0;i<numberFiles;i++)
					filelist[i].filename[0] = '\0';

				numberFiles = Parse_Directory(dir);

				fileStart = 0;
				realSelected = 0;
				highlight = 0;
				fileEnd=16;
				displayFiles=16;

				if ((fileEnd > numberFiles) & (displayFiles > numberFiles))
				{
					fileEnd = numberFiles;
					displayFiles = numberFiles;
				}
			
				SDL_Rect box = {27, 32, 258, 164};
				SDL_FillRect(openDialog, &box, SDL_MapRGB(openDialog->format,0,0,0));
				SDL_BlitSurface(openDialog, &box, screen, &box);
				SDL_Rect selecter = {30,33+(0*10),252,11};
				SDL_FillRect(screen, &selecter, SDL_MapRGB(screen->format,0x00,0x64, 0x00));

				SDL_UpdateRect(screen, 0, 0, rect.w, rect.h);
				SDL_Delay(200);
			}
			/* It's a file */
			else
			{
				strncpy(selectedFile, filelist[realSelected].filename, sizeof(filelist[realSelected].filename));
				loop=1;
			}
		}
		else if (MenuInput & PAD_BUTTON_B)
		{
			selectedFile = NULL;
			loop=1;
		}
		else if (MenuInput & PAD_TRIGGER_L)
		{
			autorun = !autorun;
			SDL_BlitSurface(openDialog, &rect, screen, &rect);
		}
		else if(MenuInput & PAD_BUTTON_START)
		{
			return;
		}

		/* Display the files in the scrolling window */
		for (i=0;i<16;i++)
		{
			if ((fileStart + i) <= (fileStart + sizeof(filelist)))
			{
				if (i == highlight)
				{
					SDL_Rect selecter = {30,33+(i*10),252,11};
					SDL_FillRect(screen, &selecter, SDL_MapRGB(screen->format,0x00,0x64, 0x00));
				}
				File_ShrinkName(fileNames , filelist[fileStart + i].filename,39);
				DrawText(40,35+(i*10), true, fileNames, text_fontSmall, Regular, Green, screen);
			}
		}
		Draw_Border(25, 30, 260, 166);
		DrawText(25,19,false,"Select a file :", text_fontSmall, Regular, White, screen);
		DrawText(10,SETTINGTXT_Y-2,false,"Autorun:", text_fontSmall, Regular, White, screen);
		DrawText(60,SETTINGTXT_Y-2,false,autorun ? "On" : "Off", text_fontSmall, Regular, Red, screen);
		DrawText(10,HELPTXT_Y-2,false,"+/L to toggle autorun", text_fontSmall, Regular, DarkGrey, screen);

		SDL_UpdateRect(screen, 0, 0, 320, 280);
		SDL_Flip(screen);
	} while (loop == 0);

	/* remove any pending buttons */
	while (WPAD_ButtonsHeld(0))
	{
		WPAD_ScanPads();
		VIDEO_WaitVSync();
	}
}

char * LoadRom(char * path, char * file)
{
	Browsefiles(path, file);

	if (file[0] == '\0')
	{
		return file;
	}
	else
	{
		snprintf(path, FILENAME_MAX, "%s%s", path, file);
		snprintf(file, 256, "%s", path);
		//strncat(path, file, strlen(file));
		//strncpy(file, path, strlen(path));

	}
	return file;
}

void Show_Help()
{
	int i = 0;
	int done = 0;
	SDL_Event event;
	SDL_Rect rect;
	rect.w = 320;
	rect.h = 240;
	rect.x = 0;
	rect.y = 0;

	SDL_FillRect(openDialog, &rect, SDL_MapRGB(openDialog->format,0,0,0));
	SDL_BlitSurface(openDialog, &rect, screen, &rect);
	DrawText(6,10,false,"HELP", text_fontLarge, Underline, White, screen);
	DrawText(6,34,false,"The commands below explain how to launch a file", text_fontSmall, Regular, White, screen);
	DrawText(6,44,false,"without autorun in most cases.", text_fontSmall, Regular, White, screen);

	DrawText(6,64,false,"Disks (.dsk,.vdk)", text_fontSmall, Underline, White, screen);
	DrawText(6,76,false,"Type", text_fontSmall, Regular, White, screen);
	DrawText(33,76,false,"DIR[Enter]", text_fontSmall, Regular, Green, screen);
	DrawText(88,76,false," to list files on the disk.", text_fontSmall, Regular, White, screen);
	DrawText(6,86,false,"For Dragon type", text_fontSmall, Regular, White, screen);
	DrawText(100,86,false,"RUN\"FILE.BAS\"", text_fontSmall, Regular, Green, screen);
	DrawText(180,86,false,"or", text_fontSmall, Regular, White, screen);
	DrawText(194,86,false,"RUN\"FILE.BIN\"", text_fontSmall, Regular, Green, screen);
	DrawText(6,96,false,"For Coco type", text_fontSmall, Regular, White, screen);
	DrawText(90,96,false,"RUN\"FILE.BAS\"", text_fontSmall, Regular, Green, screen);
	DrawText(170,96,false,"or", text_fontSmall, Regular, White, screen);
	DrawText(184,96,false,"LOADM\"FILE.BIN\":EXEC", text_fontSmall, Regular, Green, screen);

	DrawText(6,116,false,"Tapes (.cas)", text_fontSmall, Underline, White, screen);
	DrawText(6,128,false,"Type", text_fontSmall, Regular, White, screen);
	DrawText(33,128,false,"CLOADM:EXEC", text_fontSmall, Regular, Green, screen);
	DrawText(100,128,false,"(machine code) or ", text_fontSmall, Regular, White, screen);
	DrawText(205,128,false,"CLOAD:RUN", text_fontSmall, Regular, Green, screen);
	DrawText(263,128,false,"(Basic).", text_fontSmall, Regular, White, screen);
	DrawText(6,138,false,"DOS emulation should be disabled. A hard reset is", text_fontSmall, Regular, White, screen);
	DrawText(6,148,false,"necessary when toggling this option, and when a new", text_fontSmall, Regular, White, screen);
	DrawText(6,158,false,"tape is inserted.", text_fontSmall, Regular, White, screen);

	DrawText(6,178,false,"A screen full of @ means the emulator cannot find", text_fontSmall, Regular, White, screen);
	DrawText(6,188,false,"the firmware ROM for the current machine.", text_fontSmall, Regular, White, screen);
	DrawText(6,198,false,"Refer to the manual/readme for more information.", text_fontSmall, Regular, White, screen);

	DrawText(6,216,false,"XRoar: Ciaran Anscomb. Wii Port: Wiimpathy", text_fontSmall, Regular, Yellow, screen);

	Draw_Border(2, 10, 316, 214);
	SDL_UpdateRect(screen, 0, 0, rect.w, rect.h);
	SDL_Flip(screen);

	/* remove any pending buttons */
	while (WPAD_ButtonsHeld(0))
	{
		WPAD_ScanPads();
		VIDEO_WaitVSync();
	}

	while((!done) && (SDL_WaitEvent(&event)))
	{
		switch(event.type)
		{  
			case SDL_JOYBUTTONDOWN:
				done = 1;
				break;
			default:
				break;
		}
	}
}

int Show_Menu(void)
{
	int i;
	int count = 0;
	int loop=0;
	int option = 0;
	int tab[] = {13,14,14,14,1};
	char *Help_txt[] = {
			"Load a program (tape, disk, or rom).",
			"Save a snapshot.",
			"Load a snapshot.",
			"Open the settings menu.",
			"Reset the machine (Press +/L to Toggle)",
			"Quit emulator."
	};

	SDL_Surface *TempBg, *BackgroundImg = NULL;
	SDL_RWops* Background_rwops = NULL;
	SDL_Rect dest, box;
	SDL_Rect menu_rect = {0,0,320,240};

	BackgroundImg = Show_Background(menubg, &TempBg, menubg_size);

	if (BackgroundImg == NULL)
		SDL_FillRect(BackgroundImg, &menu_rect, SDL_MapRGB(openDialog->format,0,0,0));
	else
		SDL_BlitSurface(TempBg, &menu_rect, BackgroundImg, &menu_rect);

	SDL_FreeSurface( TempBg );
	SDL_FillRect(openDialog, &menu_rect, SDL_MapRGB(openDialog->format,0,0,0));
	SDL_BlitSurface(openDialog, &menu_rect, screen, &menu_rect);
	SDL_UpdateRect(screen, 0, 0, menu_rect.w, menu_rect.h);

	do
	{
		Input_Update();

		if (MenuInput & PAD_BUTTON_UP)
		{
			option--;
			if (option < 0)
				option = 0;

			SDL_BlitSurface(openDialog, &menu_rect, screen, &menu_rect);
			SDL_Delay(100);
		}
		else if (MenuInput & PAD_BUTTON_DOWN)
		{
			option++;
			if (option > 5)
				option = 5;

			SDL_BlitSurface(openDialog, &menu_rect, screen, &menu_rect);
			SDL_Delay(100);
		}
		else if (MenuInput & PAD_BUTTON_LEFT)
		{
			option--;
			if (option < 0)
				option = 0;

			SDL_BlitSurface(openDialog, &menu_rect, screen, &menu_rect);
			SDL_Delay(100);
		}
		else if (MenuInput & PAD_BUTTON_RIGHT)
		{
			option++;
			if (option > 5)
				option = 5;

			SDL_BlitSurface(openDialog, &menu_rect, screen, &menu_rect);
			SDL_Delay(100);
		}
		else if (MenuInput & PAD_BUTTON_A)
		{
			loop=1;
		}		
		else if (MenuInput & PAD_BUTTON_B)
		{
			return -1;
		}
		else if ((MenuInput & PAD_TRIGGER_L)  && option == 4)
		{
			Reset_Mode = !Reset_Mode;
		}
		else if (MenuInput & PAD_BUTTON_START)
		{
			Show_Help();
		}

		SDL_FillRect(screen, &menu_rect, SDL_MapRGB(openDialog->format,0,0,0));
		SDL_BlitSurface(BackgroundImg, NULL, screen, &menu_rect);
		SDL_Rect selecter = {85, 56+(option*22), 150,20};
		SDL_FillRect(screen, &selecter, SDL_MapRGB(screen->format,0x00,0x64, 0x00));

		DrawText(25,14,false,"MAIN MENU", text_fontLarge, Underline, White, screen);
		DrawText(95,57,false,"Load File", text_fontLarge, Regular, Green, screen);
		DrawText(95,79,false,"Save State", text_fontLarge, Regular, Green, screen);
		DrawText(95,101,false,"Load State", text_fontLarge, Regular, Green, screen);
		DrawText(95,123,false,"Settings", text_fontLarge, Regular, Green, screen);
		DrawText(95,145,false,"Restart", text_fontLarge, Regular, Green, screen);
		DrawText(95,167,false,"Quit", text_fontLarge, Regular, Green, screen);
		DrawText(14,HELPTXT_Y,false,Help_txt[option], text_fontSmall, Regular, DarkGrey, screen);

		if (option == 4)
			DrawText(14,SETTINGTXT_Y,false, Reset_Mode ? "Mode: Hard Reset" : "Mode: Soft Reset", text_fontSmall, Regular, Red, screen);

		DrawText(260, 218, false, VERSION, text_fontSmall, Regular, White, screen);
		Draw_Border(65, 40, 190, 160);

		SDL_UpdateRect(screen, 0, 0, 320, 240);
		SDL_Flip(screen);
	} while (loop == 0);

	/* remove any pending buttons */
	while (WPAD_ButtonsHeld(0))
	{
		WPAD_ScanPads();
		VIDEO_WaitVSync();
	}

	SDL_FreeSurface( BackgroundImg );

	return option;
}

int Main_Menu()
{
	int option = -1;
	option = Show_Menu();

	if (option == 0)
	{
		int filetype;
		char *filename = malloc(256);
		memset(filename, 0x00, 256);
		getdirectory();
		filename = LoadRom(GamesDir, filename);

		if (filename[0] != '\0')
		{
			sprintf(currentgame, "%s", filename);
			if(autorun)
			{
				filetype = xroar_filetype_by_ext(filename);
				/* For Tapes hard reset the machine, for everything else except .bin 
					do a soft reset or autorun may not work */		 
				if (filetype == FILETYPE_CAS || filetype == FILETYPE_ASC)
				{
					/* Disable Dos cart */	
					xroar_set_cart(-1);
					machine_reset(RESET_HARD);
					SDL_Delay(200);
				}
				else if (filetype == FILETYPE_BIN || filetype == FILETYPE_HEX)
				{
					SDL_Delay(50);
				}
				else
				{
					machine_reset(RESET_SOFT);
				}
				xroar_load_file_by_type(filename,1); /* Run file */
			}
			else
			{
				xroar_load_file_by_type(filename,0); /* Load file */
			}
		}
		else
		{
			/* Return to Main Menu */
			return 0;
		}
			return -1;
	}
	else if (option == 1)
	{
		if(currentgame[0] != '\0')
		{
			savestate(currentgame, 0);
			return -1;
		}
		else
			display_message("Can't save file!", 200);
	}
	else if (option == 2)
	{
		if(currentgame[0] != '\0')
		{
			savestate(currentgame, 1);
			return -1;
		}
		else
			display_message("Can't load file!", 200);
	}
	else if (option == 3)
	{
		/* Settings Menu */
		return 1;
	}
	else if (option == 4)
	{
		machine_reset(Reset_Mode ? RESET_HARD : RESET_SOFT);
	}
	else if (option == 5)
	{
		exit(0);
	}
}

#define OPTION1_Y 45
#define OPTION2_Y (OPTION1_Y + 1 * 22)
#define OPTION3_Y (OPTION1_Y + 2 * 22)
#define OPTION4_Y (OPTION1_Y + 3 * 22)
#define OPTION5_Y (OPTION1_Y + 4 * 22)
#define OPTION6_Y (OPTION1_Y + 5 * 22)
#define OPTION7_Y (OPTION1_Y + 6 * 22)

int Show_Settings(void)
{
	int i;
	int loop = 0;
	int option = 0;
	char Keymap_txt[256];
	static unsigned int Keymap_Country = 0;
	static unsigned int Video_Option = 0;
	static unsigned int *selected_keymap;

	char *Help_txt[] = {
			"Cycle through emulated machines.",
			"Cycle through cross-colour video modes.",
			"Select various video settings.(+/L to Toggle)",
			"Toggle DOS emulation on/off.",
			"Choose keyboard type.",
			"Adjust joystick sensitivity.",
			"Continuous and more precise press (Off by default)"
	};

	char *Video_txt[] = {
			"Bilinear:",
			"TV filter:",
			"240p:"
	};

	char *Filter_txt[] = {
			"Off",
			"Light",
			"Medium",
			"Dark"
	};

	/* Get current keyboard type used (Azerty etc.) */
	for (i = 0; mappings[i].name; i++) 
	{
		if (xroar_opt_keymap && !strcmp(xroar_opt_keymap, mappings[i].name))
			Keymap_Country = i;
	}

	selected_keymap = mappings[Keymap_Country].raw;
	xroar_opt_keymap = (char *)mappings[Keymap_Country].name;

	SDL_Surface *TempBg, *BackgroundImg = NULL;
	SDL_RWops* Background_rwops = NULL;
	SDL_Rect dest, box;
	SDL_Rect menu_rect = {0,0,320,240};

	BackgroundImg = Show_Background(menubg, &TempBg, menubg_size);

	if (BackgroundImg == NULL)
		SDL_FillRect(BackgroundImg, &menu_rect, SDL_MapRGB(openDialog->format,0,0,0));
	else
		SDL_BlitSurface(TempBg, &menu_rect, BackgroundImg, &menu_rect);
	SDL_FreeSurface( TempBg );

	SDL_FillRect(openDialog, &menu_rect, SDL_MapRGB(openDialog->format,0,0,0));
	SDL_BlitSurface(openDialog, &menu_rect, screen, &menu_rect);
	SDL_UpdateRect(screen, 0, 0, menu_rect.w, menu_rect.h);

	do
	{
		Input_Update();

		if (MenuInput & PAD_BUTTON_UP)
		{
			option--;
			if (option < 0)
				option = 0;

			SDL_BlitSurface(openDialog, &menu_rect, screen, &menu_rect);
			SDL_Delay(100);
		}
		else if (MenuInput & PAD_BUTTON_DOWN)
		{
			option++;
			if (option > 6)
				option = 6;

			SDL_BlitSurface(openDialog, &menu_rect, screen, &menu_rect);
			SDL_Delay(100);
		}
		else if (MenuInput & PAD_BUTTON_LEFT)
		{
			option--;
			if (option < 0)
				option = 0;

			SDL_BlitSurface(openDialog, &menu_rect, screen, &menu_rect);
			SDL_Delay(100);
		}
		else if (MenuInput & PAD_BUTTON_RIGHT)
		{
			option++;
			if (option > 6)
				option = 6;

			SDL_BlitSurface(openDialog, &menu_rect, screen, &menu_rect);
			SDL_Delay(100);
		}
		else if (MenuInput & PAD_BUTTON_A)
		{
			if (option == 0)
			{
				xroar_set_machine(XROAR_CYCLE);
				machine_reset(RESET_HARD);
			}
			if (option == 1)
			{
				xroar_select_cross_colour(XROAR_CYCLE);

				/* Re-allocate the artifact colors */
				if (running_config.cross_colour_phase > 0)
				{
					xroar_fullscreen(XROAR_TOGGLE);
				}
			}
			if (option == 2) // Video
			{
				Video_Option = (Video_Option + 1) %3;
			}
			if (option == 3)
			{
				int insert_dos = -1;

				if (xroar_cart_config)
				{ 
					insert_dos = xroar_cart_config->type;
				}

				/* A generic romcart has been inserted. Re-insert a DOS cart now. */
				if(insert_dos == 0)
				{
					if (IS_DRAGON)
					{
						xroar_set_cart(0);
						xroar_set_cart(-1);
						machine_reset(RESET_SOFT);
					}
					else
					{
						xroar_set_cart(-1);
						xroar_set_cart(1);
						machine_reset(RESET_SOFT);
					}
				}
				else
				{
					xroar_set_cart(XROAR_TOGGLE);
				}
			}
			if (option == 4)
			{
				Keymap_Country = (Keymap_Country + 1) %4;
				selected_keymap = mappings[Keymap_Country].raw;
				xroar_opt_keymap = (char *)mappings[Keymap_Country].name;
				map_keyboard(selected_keymap);
			}
			if (option == 5)
			{
				joy_sensitivity = !joy_sensitivity;
			}
			if (option == 6)
			{
				dpad_delay = !dpad_delay;
			}
		}		
		else if (MenuInput & PAD_BUTTON_B)
		{
			/* Return to main menu */
			return 0;
		}
		else if ((MenuInput & PAD_TRIGGER_L)  && option == 2)
		{
			if (Video_Option == 0)
				xroar_bilinear = !xroar_bilinear;
			else if (Video_Option == 1)
				xroar_tvfilter = (xroar_tvfilter + 1) %4;
			else if (Video_Option == 2)
				xroar_240p = !xroar_240p;

			video_configure();
		}
		else if (MenuInput & PAD_BUTTON_START)
		{
			return -1;
		}

		SDL_FillRect(screen, &menu_rect, SDL_MapRGB(openDialog->format,0,0,0));
		SDL_BlitSurface(BackgroundImg, NULL, screen, &menu_rect);
		SDL_Rect selecter = {85, OPTION1_Y + (option*22), 150,20};
		SDL_FillRect(screen, &selecter, SDL_MapRGB(screen->format,0x00,0x64, 0x00));

		DrawText(25,14,false,"SETTINGS MENU", text_fontLarge, Underline, White, screen);
		DrawText(95,OPTION1_Y,false,"Switch Machine", text_fontLarge, Regular, Green, screen);
		DrawText(95,OPTION2_Y,false,"Color Mode", text_fontLarge, Regular, Green, screen);
		DrawText(95,OPTION3_Y,false,"Video Mode", text_fontLarge, Regular, Green, screen);
		DrawText(95,OPTION4_Y,false,"DOS emulation", text_fontLarge, Regular, Green, screen);
		DrawText(95,OPTION5_Y,false,"keyboard layout", text_fontLarge, Regular, Green, screen);
		DrawText(95,OPTION6_Y,false,"Joy sensitivity", text_fontLarge, Regular, Green, screen);
		DrawText(95,OPTION7_Y,false,"Dpad delay", text_fontLarge, Regular, Green, screen);
		DrawText(14,HELPTXT_Y,false,Help_txt[option], text_fontSmall, Regular, DarkGrey, screen);

		if (option == 0)
		{
			DrawText(14,SETTINGTXT_Y,false, "Machine:", text_fontSmall, Regular, White, screen);
			DrawText(63,SETTINGTXT_Y,false, xroar_machine_config->description, text_fontSmall, Regular, Red, screen);
		}

		if (option == 1)
		{
			DrawText(14,SETTINGTXT_Y,false, "Color:", text_fontSmall, Regular, White, screen);
			DrawText(51,SETTINGTXT_Y,false, (char *)xroar_cross_colour_list[running_config.cross_colour_phase].description, text_fontSmall, Regular, Red, screen);
		}

		if (option == 2)
		{
			DrawText(14,SETTINGTXT_Y,false, Video_txt[Video_Option], text_fontSmall, Regular, White, screen);

			if (Video_Option == 0)
				DrawText(70,SETTINGTXT_Y,false, xroar_bilinear ?  "ON" : "OFF", text_fontSmall, Regular, Red, screen);
			else if (Video_Option == 1)
				DrawText(75,SETTINGTXT_Y,false,  Filter_txt[xroar_tvfilter] , text_fontSmall, Regular, Red, screen);
			else if (Video_Option == 2)
				DrawText(45,SETTINGTXT_Y,false, xroar_240p ?  "ON" : "OFF", text_fontSmall, Regular, Red, screen);
		}

		if (option == 3)
		{
			DrawText(14,SETTINGTXT_Y,false, "Dos:", text_fontSmall, Regular, White, screen);
			DrawText(40,SETTINGTXT_Y,false, xroar_cart_config ? "ON" : "OFF", text_fontSmall, Regular, Red, screen);
		}

		if (option == 4)
		{
			DrawText(14,SETTINGTXT_Y,false, "Country:", text_fontSmall, Regular, White, screen);
			sprintf(Keymap_txt, xroar_opt_keymap);
			DrawText(64,SETTINGTXT_Y,false, Keymap_txt, text_fontSmall, Regular, Red, screen);
		}

		if (option == 5)
		{
			DrawText(14,SETTINGTXT_Y,false, "Mode:", text_fontSmall, Regular, White, screen);
			DrawText(45,SETTINGTXT_Y,false, joy_sensitivity ? "High" : "Normal", text_fontSmall, Regular, Red, screen);
		}

		if (option == 6)
		{
			DrawText(14,SETTINGTXT_Y,false, "Mode:", text_fontSmall, Regular, White, screen);
			DrawText(45,SETTINGTXT_Y,false, dpad_delay ? "ON" : "OFF", text_fontSmall, Regular, Red, screen);
		}

		DrawText(260, 218, false, VERSION, text_fontSmall, Regular, White, screen);
		Draw_Border(65, 40, 190, 160);

		SDL_UpdateRect(screen, 0, 0, 320, 240);
		SDL_Flip(screen);
	} while (loop == 0);

	/* remove any pending buttons */
	while (WPAD_ButtonsHeld(0))
	{
		WPAD_ScanPads();
		VIDEO_WaitVSync();
	}

	SDL_FreeSurface( BackgroundImg );
	return 1;
}

int menu()
{
	int loop = 0;
	int choice = 0;

	do
	{
		switch(choice)
		{
			case 0:
				choice = Main_Menu();
				break;
			case 1:
				choice = Show_Settings();
				break;
			default:
				choice = -1;
				break;
		}
	} while (choice != -1);
}

char *check_specialkey(int key)
{
	char *key_txt = "";

	switch (key)
	{
		case 2:
			sprintf(key_txt, "Shift");
			break;
		case 8:
			sprintf(key_txt, "Left");
			break;
		case 9:
			sprintf(key_txt, "Right");
			break;
		case 10:
			sprintf(key_txt, "Down");
			break;
		case 13:
			sprintf(key_txt, "Enter");
			break;
		case 27:
			sprintf(key_txt, "Clear");
			break;
		case ' ':
			sprintf(key_txt, "Space");
			break;
		case '`':
			sprintf(key_txt, "Break");
			break;
		case 94:
			sprintf(key_txt, "Up");
			break;
		default:
			sprintf(key_txt, "%c", key);
			break;
	}
	return key_txt;
}

void show_keyconfig(int key)
{
	int i;
	char GCbut_txt[7][20] = {"A", "B", "X", "Y", "", "R", ""};
	char wiibut_txt[6][20] = {"A", "B", "1", "2", "Z", "C"};
	char classic_txt[8][20] = {"A", "B", "X", "Y", "L", "R", "ZL", "ZR"};
	char key_txt[10];
	char info_txt[40];
	char *current_key = "";

	current_key = check_specialkey(key);
	sprintf(info_txt, "Press a button for key %s:", current_key);
	DrawText(20,100,false,info_txt, text_fontSmall, Regular, White, screen);
	DrawText(20,110,false,"Home/Start to go back, -/Z to clear.", text_fontSmall, Regular, White, screen);
	DrawText(32,130,false,"Wiimote", text_fontSmall, Underline, White, screen);
	DrawText(132,130,false,"Classic", text_fontSmall, Underline, White, screen);
	DrawText(232,130,false,"GameCube", text_fontSmall, Underline, White, screen);

	for (i = 0; i < 6; i++)
	{
		/* Nunchuck C/Z */
		if (i > 3)
			current_key = check_specialkey(wiimote_keymap[i+3]);
		else
			current_key = check_specialkey(wiimote_keymap[i]);

		sprintf(key_txt, ": %s", current_key);
		DrawText(35,146 + i*10,false,wiibut_txt[i], text_fontSmall, Regular, White, screen);
		DrawText(45,146 + i*10,false,key_txt, text_fontSmall, Regular, White, screen);
	}

	for (i = 0; i < 8; i++)
	{
		current_key = check_specialkey(wiimote_keymap[i+9]);
		sprintf(key_txt, " : %s", current_key);
		DrawText(135,146 + i*10,false,classic_txt[i], text_fontSmall, Regular, White, screen);
		DrawText(145,146 + i*10,false,key_txt, text_fontSmall, Regular, White, screen);
	}

	for (i = 0; i < 6; i++)
	{
		current_key = check_specialkey(gc_keymap[i]);
		sprintf(key_txt, ": %s", current_key);

		/* Skip Z button */
		if (i == 5)
		{
			DrawText(235,146 + 40,false,GCbut_txt[i], text_fontSmall, Regular, White, screen);
			DrawText(245,146 + 40,false,key_txt, text_fontSmall, Regular, White, screen);
		}
		else
		{
			DrawText(235,146 + i*10,false,GCbut_txt[i], text_fontSmall, Regular, White, screen);
			DrawText(245,146 + i*10,false,key_txt, text_fontSmall, Regular, White, screen);
		}
	}
}

void joy_config(int key)
{
	int i = 0;
	int done = 0;
	int button = -1;
	char txt[50];
	Uint32 bluebg  = SDL_MapRGB(openDialog->format,0,0,255);
	SDL_Event event;
	SDL_Rect rect;
	rect.w = 296;
	rect.h = 140;
	rect.x = 10;
	rect.y = 90;

	SDL_FillRect(openDialog, &rect, bluebg);
	SDL_BlitSurface(openDialog, &rect, screen, &rect);
	show_keyconfig(key);
	SDL_UpdateRect(screen, 0, 0, rect.w, rect.h);
	SDL_Flip(screen);

	/* remove any pending buttons */
	while (WPAD_ButtonsHeld(0))
	{
		WPAD_ScanPads();
		VIDEO_WaitVSync();
	}

	while (PAD_ButtonsHeld(0))
	{
		PAD_ScanPads();
		VIDEO_WaitVSync();
	}

	while((!done) && (SDL_WaitEvent(&event)))
	{
		switch(event.type)
		{  
			case SDL_JOYBUTTONDOWN:
				if (event.jbutton.which == 0)
				{
					if (event.jbutton.button == 4 |  event.jbutton.button == 17) /* Wiimote/Classic - */
					{
						/* Reset all key bindings */
						for (i = 0; i < 17; i++)
							wiimote_keymap[i] = 0;
						for (i = 0; i < 7; i++)
							gc_keymap[i] = 0;

						SDL_FillRect(openDialog, &rect, bluebg);
						SDL_BlitSurface(openDialog, &rect, screen, &rect);
						show_keyconfig(key);
						SDL_UpdateRect(screen, 0, 0, rect.w, rect.h);
						display_message("All bindings are erased!", 200);
					}
					else if (event.jbutton.button == 6 | event.jbutton.button == 19) /* Wiimote/Classic Home */
					{
						done = 1;
					}
					else if (event.jbutton.button == 5) /* Wiimote/Classic + */
					{
						while (i <= 150)
						{
							DrawText(20, 210, false, "Cannot be mapped!", text_fontSmall, Regular, Green, screen);
							i++;
							SDL_Flip(screen);
						}
						SDL_FillRect(openDialog, &rect, bluebg);
						SDL_BlitSurface(openDialog, &rect, screen, &rect);
						show_keyconfig(key);
						SDL_UpdateRect(screen, 0, 0, rect.w, rect.h);
						SDL_Flip(screen);
						i = 0;
						continue;
					}
					else
					{
						wiimote_keymap[event.jbutton.button] = key;

						SDL_FillRect(openDialog, &rect, bluebg);
						SDL_BlitSurface(openDialog, &rect, screen, &rect);
						show_keyconfig(key);
						SDL_UpdateRect(screen, 0, 0, rect.w, rect.h);

						if (event.jbutton.button < 9)
						{
							sprintf(txt, "Key %s mapped to Wiimote.", check_specialkey(key));
							display_message(txt, 200);
						}
						else
						{
							sprintf(txt, "Key %s mapped to Classic controller.", check_specialkey(key));
							display_message(txt, 200);
						}
						done = 1;
					}
				}
				if (event.jbutton.which == 4)
				{
					if (event.jbutton.button == 4) /* GC Pad Z */
					{
						/* Reset all key bindings */
						for (i = 0; i < 17; i++)
							wiimote_keymap[i] = 0;
						for (i = 0; i < 7; i++)
							gc_keymap[i] = 0;

						SDL_FillRect(openDialog, &rect, bluebg);
						SDL_BlitSurface(openDialog, &rect, screen, &rect);
						show_keyconfig(key);
						SDL_UpdateRect(screen, 0, 0, rect.w, rect.h);
						display_message("All bindings are erased!", 200);
					}
					else if (event.jbutton.button == 7) /* GC Pad Start */
					{
						done = 1;
					}
					else if (event.jbutton.button == 6) /* GC Pad L */
					{
						while (i <= 150)
						{
							DrawText(20, 210, false, "Cannot be mapped!", text_fontSmall, Regular, Green, screen);
							i++;
							SDL_Flip(screen);
						}
						SDL_FillRect(openDialog, &rect, bluebg);
						SDL_BlitSurface(openDialog, &rect, screen, &rect);
						show_keyconfig(key);
						SDL_UpdateRect(screen, 0, 0, rect.w, rect.h);
						SDL_Flip(screen);
						i = 0;
						continue;
					}
					else
					{
						gc_keymap[event.jbutton.button] = key;

						SDL_FillRect(openDialog, &rect, bluebg);
						SDL_BlitSurface(openDialog, &rect, screen, &rect);
						show_keyconfig(key);
						SDL_UpdateRect(screen, 0, 0, rect.w, rect.h);
						sprintf(txt, "Key %s mapped to GameCube controller.", check_specialkey(key));
						display_message(txt, 200);
						done = 1;
					}
				}
				break;
			default:
				break;
		}
	}
}


static char keycode[6][14] = {{'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', ':', '-', '`', 0},
                              {94, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '@', 8, 9},
                              {10, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\r', '\r' , 27},
                              {2,'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', 2, 2, 0},
                              {' ', 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                              {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

static char keycode_shift[6][14] = {{'!', '"', '#', '$', '%', '&', '\'', '(', ')', '0', '*', '=', '`', 0},
                                    {94, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '@', 8, 9},
                                    {10, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '+', '\r', '\r' , 27},
                                    {2,'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 2, 2, 0},
                                    {' ', 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

char *key_names[5][14] = {{" 1 ", " 2 ", " 3 ", " 4 ", " 5 ", " 6 ", " 7 ", " 8 ", " 9 ", " 0 ", " : ", " - ", "BRK", " "},
                          {"UP", " Q ", " W ", " E ", " R ", " T ", " Y ", " U ", " I ", " O ", " P ", " @ ", "LFT", "RGT"},
                          {"DWN", " A ", " S ", " D ", " F ", " G ", " H ", " J ", " K ", " L ", " ; ", "ENTER", " ", "CLR"},
                          {"SHIFT"," ", " Z ", " X ", " C ", " V ", " B ", " N ", " M ", " , ", " . ", " / ", "SHIFT", " "},
                          {" ", " ", " ", " ", " ", " ", " ", " ", " ", " ", "   ","   ","   ","   "}};

char *key_names_shift[5][14] = {{" ! ", " \" ", " # ", " $ ", " % ", " & ", " ' ", " ( ", " ) ", " 0 ", " * ", " = ", "BRK", " "},
                                {"UP", " Q ", " W ", " E ", " R ", " T ", " Y ", " U ", " I ", " O ", " P ", " @ ", "LFT", "RGT"},
                                {"DWN", " A ", " S ", " D ", " F ", " G ", " H ", " J ", " K ", " L ", " + ", "ENTER", " ", "CLR"},
                                {"SHIFT"," ", " Z ", " X ", " C ", " V ", " B ", " N ", " M ", " < ", " > ", " ? ", "SHIFT", " "},
                                {" ", " ", " ", " ", " ", " ", " ", " ", " ", " ", "   ","   ","   ","   "}};

void vkeyboard(char **keys)
{
	int loop=0;
	char buffer[47] = "";
	int add = 0;
	int key_index = 0;
	int row = 0;
	int row_txt = 0;
	int col_txt = 0;
	int shift = 0;
	int keypressed = 0;
	bool add_key = false;
	int MaxKeysRow[] = {12,13,13,11,1};
	int key_offset[5] = {29,19,19,39,69};
	bool show_box = false;
	static int now = 0;
	struct ir_t ir;

	SDL_Surface *wii_cursor, *KeyboardImg;
	SDL_RWops *Keyboard_rwops = NULL;
	SDL_Rect dest;
	SDL_Rect select_rect, offset;
	SDL_Rect keyboard_rect = {0,70,320,240};

	/* Load keyboard image from source */
	if (IS_DRAGON)
		Keyboard_rwops = SDL_RWFromMem( (char *)dragonkeyboard, dragonkeyboard_size);
	else
		Keyboard_rwops = SDL_RWFromMem( (char *)cocokeyboard, cocokeyboard_size);

	KeyboardImg = IMG_Load_RW(Keyboard_rwops, 1);

	wii_cursor = IMG_ReadXPMFromArray(wiimote_xpm);

	SDL_FillRect(openDialog, &keyboard_rect, SDL_MapRGB(openDialog->format,0,0,0));
	SDL_BlitSurface(openDialog, &keyboard_rect, screen, &keyboard_rect);
	SDL_UpdateRect(screen, 0, 0, keyboard_rect.w, keyboard_rect.h);

	WPAD_SetVRes(WPAD_CHAN_ALL,640,480);
	WPAD_SetDataFormat(WPAD_CHAN_0,WPAD_FMT_BTNS_ACC_IR);

	do
	{
		/* Scan inputs and IR pointer */
		Input_Update();
		WPAD_IR(WPAD_CHAN_0,&ir);

		if (MenuInput & PAD_BUTTON_UP)
		{
			row--;
			if (row < 0)
				row = 0;

			key_index = 0;

			SDL_BlitSurface(openDialog, &keyboard_rect, screen, &keyboard_rect);
			SDL_Delay(50);
		}
		else if (MenuInput & PAD_BUTTON_DOWN)
		{
			row++;
			if (row > 4)
				row = 4;

			key_index = 0;

			SDL_BlitSurface(openDialog, &keyboard_rect, screen, &keyboard_rect);
			SDL_Delay(50);
		}
		else if (MenuInput & PAD_BUTTON_LEFT)
		{
			key_index--;
			if (key_index < 0)
			 key_index = 0; 
			/* Key Enter, 2 squares skip 1 */
			if (row == 2 && key_index == 12)
				key_index = 11;

			SDL_BlitSurface(openDialog, &keyboard_rect, screen, &keyboard_rect);
			SDL_Delay(50);
		}
		else if (MenuInput & PAD_BUTTON_RIGHT)
		{
			key_index++;
			if (key_index > MaxKeysRow[row])
			 key_index = key_index - 1; 
			/* Key Enter, 2 squares skip 1 */
			if (row == 2 && key_index == 12)
				key_index = 13;
			/* Key Shift, 2 squares skip 1 */
			if (row == 3 && key_index == 13)
				key_index = 12;

			SDL_BlitSurface(openDialog, &keyboard_rect, screen, &keyboard_rect);
			SDL_Delay(50);
		}

		if (add_key)
		{
			if(ir.valid)
			{
				if (ir.y > 80 )
				{
					row = (ir.y - 120) / 20;

					if (row == 4)
						key_index =  ( ((int)ir.x - key_offset[row]) / 180);
					else
						key_index =  ( ((int)ir.x - key_offset[row]) / 20);
				}
			}

			if ( ( (key_index) == 0  || (key_index) == 11 || (key_index) == 12 ) && row == 3)
			{
				shift = !shift;
				add_key=false;

				if (key_index == 12)
					key_index = 11;
			}

			keypressed = (shift ? keycode_shift[row][key_index] : keycode[row][key_index]);

			/* Ignore out of bound key */
			if (key_index > 13 || key_index < 0)
				keypressed = 0;

			if (keypressed == '\r')
				key_index = 11;

			/* 'Done' button pressed, don't add key value */
			if (keypressed == 1)
				add_key = false;

			if (shift)
			{
				if (keypressed == 0 || keypressed == 2)
				{
					add_key=false;
				}
				else
				{
					/* Only add when under buffer limit */
					if (strlen(buffer) < 46)
					{
						buffer[add] = keycode_shift[row][key_index];
						add++;
					}
				}
			}
			else
			{
				if (keypressed == 0 || keypressed == 2)
				{
					add_key=false;
				}
				else
				{
					/* Only add when under buffer limit */
					if (strlen(buffer) < 46)
					{
						buffer[add] = keycode[row][key_index];
						add++;
					}
				}
			}

			add_key=false;
		}

		if (MenuInput & PAD_BUTTON_A)
		{
			add_key = true;
		}

		/* Erase last key */
		if ( (MenuInput & PAD_TRIGGER_Z) && strlen(buffer) > 1 )
		{
			buffer[add] = 0;
			add--;
			buffer[add] = 0;
		}			

		if (MenuInput & PAD_BUTTON_B)
		{
			buffer[strlen(buffer)+1] = '\0';
			*keys = NULL;
			loop=1;
		}

		SDL_FillRect(screen, &keyboard_rect, SDL_MapRGB(screen->format,0x00,0x00, 0x00));

		dest.x = 0;
		dest.y = 68;
		SDL_BlitSurface(KeyboardImg, NULL, screen, &dest);

		/* Line 1-0... */
		for (col_txt = 1; col_txt < 15; col_txt++)
			DrawText( ((col_txt+1)*20) - 10 ,  122+(0*20),false,shift ? key_names_shift[0][col_txt-1] : key_names[0][col_txt-1], text_fontSmall, Regular, Black, screen);

		for (row_txt = 1; row_txt < 5; row_txt++)
		{
			for (col_txt = 0; col_txt < 14; col_txt++)
				DrawText( (col_txt+1)*20,122+(row_txt*20),false,shift ? key_names_shift[row_txt][col_txt] : key_names[row_txt][col_txt], text_fontSmall, Regular, Black, screen);
		}

		now = SDL_GetTicks();

		/* Blinking (epileptic?) underscore */
		if (now % 10 == 0)
				DrawText(20+(add*6),100,false,"_", text_fontSmall, Regular, DarkGreen, screen);


		/* Display keystrokes */
		DrawText(20,100,false,buffer, text_fontSmall, Regular, DarkGreen, screen);

		if (IS_COCO)
			DrawText(152,76,false,"COLOR COMPUTER", text_fontSmall, Regular, White, screen);

		DrawText(272,204,false,"DONE", text_fontSmall, Regular, Black, screen);
		DrawText(14,208,false,"(+) Bind", text_fontSmall, Regular, Blue, screen);

		/* 'Done' button pressed, send the key(s) */
		if (keypressed == 1)
		{
			buffer[strlen(buffer)+1] = '\0';
			*keys=buffer;
			/* Don't miss the key ! */
			SDL_Delay(200);
			loop=1;
		}

		/* Wii cursor */
		offset.x = ir.x;
		offset.y = ir.y;
		offset.w = 14;
		offset.h = 20;

		int select_x = 0;
		int select_y = 115;
		int key_width = 0;
		int key_height = 20;
		int keyselect = 0;

		keyselect = shift ? keycode_shift[row][key_index] : keycode[row][key_index];

		if (MenuInput & PAD_TRIGGER_L)
		{
			joy_config(keyselect);
		}

		if (keyselect == '\r' || keyselect == 2)
			key_width = 40;
		else if (keyselect == ' ')
			key_width = 180;
		else if (row == 4 && key_index == 1)
		{
			key_width = 30;
			key_height = 15;
		}
		else
			key_width = 20;

		if (row == 3 && key_index == 0)
		{
			select_x = 20;
		}
		else if (row == 4 && key_index == 1)
		{
			select_x = 8*key_width + key_offset[0];
			select_y = 119;
		}
		else
		{
			select_x = key_index*20 + key_offset[row];
		}

		if (key_index > MaxKeysRow[row])
			show_box = false;
		else if (key_index < 0)
			show_box = false;
		else if (row == 5)
			show_box = false;
		else
			show_box = true;

		/* Display a rectangle around the selected key */
		if (show_box)
		{
			/* Draw upper border: */
			select_rect.x = select_x;
			select_rect.y = select_y+(row*20);
			select_rect.w = key_width;
			select_rect.h = 1;
			SDL_FillRect(screen, &select_rect, SDL_MapRGB(screen->format,0x00,0x00, 0x00));

			/* Draw left border: */
			select_rect.x = select_x;
			select_rect.y = select_y+(row*20);
			select_rect.w = 1;
			select_rect.h = key_height;
			SDL_FillRect(screen, &select_rect, SDL_MapRGB(screen->format,0x00,0x00, 0x00));

			/* Draw bottom border: */
			select_rect.x = select_x;
			select_rect.y = select_y+(row*20) + key_height;
			select_rect.w = key_width;
			select_rect.h = 1;
			SDL_FillRect(screen, &select_rect, SDL_MapRGB(screen->format,0x00,0x00, 0x00));

			/* Draw right border: */
			select_rect.x = select_x + key_width;
			select_rect.y = select_y+(row*20);
			select_rect.w = 1;
			select_rect.h = key_height;
			SDL_FillRect(screen, &select_rect, SDL_MapRGB(screen->format,0x00,0x00, 0x00));
		}

		if ( ir.y > 70 )
			SDL_BlitSurface(wii_cursor, NULL, screen, &offset);

		SDL_UpdateRect(screen, 0, 0, 320, 240);
		SDL_Flip(screen);
	} while (loop == 0);
}

/*-----------------------------------------------------------------------*/
/**
* Emulate the Dragon/Coco sticks with the Wiimote Pointer
* 
* It can be used in mouse based softwares like Cocomax or Arkanoid for ex.
*/
static void update_mouse_state(int irx, int iry) {
	int offset;

	if (irx > 160)
	{
		offset =  ((irx - 160) / 10) * 2000;
		mousex = 2000+offset;
	}
	else 
	{
		offset =  ( abs(irx - 160) / 10) * 2000;
		mousex = -2000-offset;
	}

	if (iry > 120)
	{
		offset =  ((iry - 120) / 10) * 2400;
		mousey = 2000+offset;
	}
	else 
	{
		offset =  ( abs(iry - 120) / 10) * 2400;
		mousey = -2000-offset;
	}
}

int dpad_ready()
{
	static int start = 0;
	static int now = 0;

	start = SDL_GetTicks();

	if (start - now > 3)
	{
		now = start;
		return 1;
	}

	return 0;
}
#endif

void sdl_run(void) {
#ifdef GEKKO
	static int last_speedx = 0;
	static int last_speedy = 0;
	static int start = 0;
	int port = -1;
	int i;
	struct ir_t ir;

	/* Reset Wii hats */
	for (i=0; i<4; i++)
	{
		Dpad_Dir[0][i] = false;
		Dpad_Dir[1][i] = false;
	}
#endif
	while (1) {
		SDL_Event event;
		sam_run(VDG_LINE_DURATION * 8);

		WPAD_IR(WPAD_CHAN_0,&ir);

		if (ir.valid)
		{
			update_mouse_state(ir.x, ir.y);
			joystick_move(0, 0, mousex, joy_sensitivity);
			joystick_move(0, 1, mousey, joy_sensitivity);
		}
		while (SDL_PollEvent(&event) == 1) {
			switch(event.type) {
			case SDL_VIDEORESIZE:
				if (video_module->resize) {
					video_module->resize(event.resize.w, event.resize.h);
				}
				break;
			case SDL_QUIT:
				exit(0); break;
			case SDL_KEYDOWN:
				sdl_keypress(&event.key.keysym);
				keyboard_column_update();
				keyboard_row_update();
				break;
			case SDL_KEYUP:
				sdl_keyrelease(&event.key.keysym);
				keyboard_column_update();
				keyboard_row_update();
				break;
#ifdef GEKKO
			case SDL_JOYAXISMOTION:
				if (emulate_keyboard) {        
					switch(event.jaxis.axis) {
					case 0: /* Left/Right */
						if (event.jaxis.value < -16384) {			// Left            
							event.key.keysym.sym=SDLK_LEFT;			
							sdl_keypress(&event.key.keysym);
						} else if ( event.jaxis.value > 16384 ) {	// Right
							event.key.keysym.sym=SDLK_RIGHT;
							sdl_keypress(&event.key.keysym);
						} else {
							event.key.keysym.sym=SDLK_LEFT;
							sdl_keyrelease(&event.key.keysym);
							event.key.keysym.sym=SDLK_RIGHT;
							sdl_keyrelease(&event.key.keysym);
						}
						break;
					case 1: /* Left/Right */
						if( event.jaxis.value < -16384 ) {	// Up
							event.key.keysym.sym=SDLK_UP;			
							sdl_keypress(&event.key.keysym);
						} else if ( event.jaxis.value > 16384 ) {	// Down
							event.key.keysym.sym=SDLK_DOWN;
							sdl_keypress(&event.key.keysym);
						} else {
							event.key.keysym.sym=SDLK_UP;
							sdl_keyrelease(&event.key.keysym);
							event.key.keysym.sym=SDLK_DOWN;
							sdl_keyrelease(&event.key.keysym);
						}
						break;
					}
				} else {
					joystick_move(event.jaxis.which, event.jaxis.axis, event.jaxis.value, joy_sensitivity);
				}
				break;
			case SDL_JOYHATMOTION:
				if (emulate_keyboard) {
					if(event.jhat.value == SDL_HAT_LEFT) {
						event.key.keysym.sym=SDLK_LEFT;			
						sdl_keypress(&event.key.keysym);
					} else if (event.jhat.value == SDL_HAT_RIGHT) {
						event.key.keysym.sym=SDLK_RIGHT;
						sdl_keypress(&event.key.keysym);
					} else if (event.jhat.value == SDL_HAT_UP) {
					  event.key.keysym.sym=SDLK_UP;
					  sdl_keypress(&event.key.keysym);
					} else if (event.jhat.value == SDL_HAT_DOWN) {
						event.key.keysym.sym=SDLK_DOWN;
						sdl_keypress(&event.key.keysym);
					} else if (event.jhat.value == SDL_HAT_CENTERED) {
						event.key.keysym.sym=SDLK_LEFT;
						sdl_keyrelease(&event.key.keysym);
						event.key.keysym.sym=SDLK_RIGHT;
						sdl_keyrelease(&event.key.keysym);
						event.key.keysym.sym=SDLK_UP;
						sdl_keyrelease(&event.key.keysym);
						event.key.keysym.sym=SDLK_DOWN;
						sdl_keyrelease(&event.key.keysym);
					}
				} else {
					/* Reset Wii hats */
					for (i=0; i<4; i++)
					{
						Dpad_Dir[0][i] = false;
						Dpad_Dir[1][i] = false;
					}

					/* port is player id here 0 or 1. SDL reports 1st wiimote/gc pad as 0/4 */
					/* and 2d wiimote/gc pad as 1/5. */
					port = event.jhat.which & 1;

					/* Put all the Dragon/Coco sticks in neutral position */
					if (!dpad_delay)
					{
						if (port == 0)
						{
							for (i=0; i<2; i++)
							{
								dir_speed[0][i] = 127;
								input_control_release(port + i, 0);
								input_control_release(port + i, 255);
							}
						}
						else if (port == 1)
						{
							for (i=0; i<2; i++)
							{
								dir_speed[1][i] = 127;
								input_control_release(port + i, 0);
								input_control_release(port + i, 255);
							}
						}
					}

					if (event.jhat.value & SDL_HAT_LEFT)  Dpad_Dir[port][0] = true;
					if (event.jhat.value & SDL_HAT_RIGHT)    Dpad_Dir[port][1] = true;
					if (event.jhat.value & SDL_HAT_UP) Dpad_Dir[port][2] = true;
					if (event.jhat.value & SDL_HAT_DOWN)  Dpad_Dir[port][3] = true;

					if (event.jhat.value == SDL_HAT_CENTERED)
					{
						if (dpad_delay)
						{
							input_control_press(port, last_speedx);
							input_control_press(port+1, last_speedy);
							dir_speed[port][0] = last_speedx;
							dir_speed[port][1] = last_speedy;
						}
					}
				}			
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				/* Send Coco/Dragon Fire button or keys configured in the virtual keyboard */
				Main_HandleWiiButton(event.jbutton.which, event.jbutton.button, event.type==SDL_JOYBUTTONDOWN);
				break;
#endif
			}
		}
		/* XXX will this ever be needed? */
		while (EVENT_PENDING(UI_EVENT_LIST))
			DISPATCH_NEXT_EVENT(UI_EVENT_LIST);
#ifdef GEKKO
		if (dpad_delay)
		{
			start = dpad_ready();
			if (start)
			{
				Main_HandleWiiHat(port);
				last_speedx = dir_speed[port][0];
				last_speedy = dir_speed[port][1];
			}
		}
		else
		{
			Main_HandleWiiHat(port);
		}
	}
#endif
}
