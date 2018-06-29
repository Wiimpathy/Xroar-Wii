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

#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#ifdef GEKKO
#include <gccore.h>
#include <ogcsys.h>
#include <SDL/SDL_ttf.h>
#include "gctypes.h"
#include "dejavusans_ttf.h"
#include "another_ttf.h"
#endif
#include <SDL/SDL_syswm.h>
#include "types.h"
#include "logging.h"
#include "module.h"
#include "vdg_bitmaps.h"
#include "xroar.h"
#ifdef WINDOWS32
#include "windows32/common_windows32.h"
#endif

static int init(void);
static void shutdown(void);
static void alloc_colours(void);
static void reset(void);
static void vsync(void);
static void hsync(void);
static void set_mode(unsigned int mode);
static void render_border(void);
static int set_fullscreen(int fullscreen);

VideoModule video_sdl_module = {
	.common = { .name = "sdl", .description = "Standard SDL surface",
	            .init = init, .shutdown = shutdown },
	.update_palette = alloc_colours,
	.reset = reset, .vsync = vsync, .hsync = hsync, .set_mode = set_mode,
	.render_border = render_border, .set_fullscreen = set_fullscreen,
};

typedef Uint32 Pixel;
#define RESET_PALETTE() reset_palette()
#define MAPCOLOUR(r,g,b) alloc_and_map(r, g, b)
#define VIDEO_SCREENBASE ((Pixel *)backend->pixels)
#define XSTEP 1
#define NEXTLINE 0
#define VIDEO_TOPLEFT (VIDEO_SCREENBASE)
#define VIDEO_VIEWPORT_YOFFSET (0)
#define LOCK_SURFACE SDL_LockSurface(backend)
#define UNLOCK_SURFACE SDL_UnlockSurface(backend)
#define VIDEO_MODULE_NAME video_sdl_module

#ifdef GEKKO
extern void WII_ChangeSquare(int xscale, int yscale, int xshift, int yshift);
extern void WII_SetDoubleStrikeVideoMode( int xscale, int yscale, int width );
extern void WII_SetDefaultVideoMode();
extern void WII_SetWidescreen(int wide);
extern void WII_SetFilter( BOOL b );

TTF_Font *text_fontSmall, *text_fontLarge;
SDL_Surface  *openDialog, *backend, *screen;
static int filter = 0;
#endif

static int palette_index = 0;

static void reset_palette(void) {
	palette_index = 0;
}

static Pixel alloc_and_map(int r, int g, int b) {
	SDL_Color c;
	c.r = r;
	c.g = g;
	c.b = b;
	SDL_SetPalette(screen, SDL_LOGPAL|SDL_PHYSPAL, &c, palette_index, 1);
	palette_index++;
	palette_index %= 256;
	return SDL_MapRGB(screen->format, r, g, b);
}

#include "vo_generic_ops.h"

/* From 'The Ur-Quan Masters' */
static void ScanLines(SDL_Surface *dst, SDL_Rect *r)
{
	const int rw = r->w * 2;
	const int rh = r->h * 2;
	SDL_PixelFormat *fmt = dst->format;
	const int pitch = dst->pitch;
	const int len = pitch / fmt->BytesPerPixel;
	int ddst;
	Uint32 *p = (Uint32 *) dst->pixels;
	int x, y;

	p += len * (r->y * 2) + (r->x * 2);
	ddst = len + len - rw;

	for (y = rh; y; y -= 2, p += ddst)
	{
		for (x = rw; x; --x, ++p)
		{
			// we ignore the lower bits as the difference
			// of 1 in 255 is negligible
			if (xroar_tvfilter == 1)
				*p = ((*p >> 1) & 0x7f7f7f7f) + ((*p >> 2) & 0x3f3f3f3f);
			else if  (xroar_tvfilter == 2)
				*p =  (*p >> 1) & 0x7f7f7f;
			else if  (xroar_tvfilter == 3)
				*p =  (*p >> 1) & 0x5f5f5f;
		}
	}
}

void video_configure()
{
	if (xroar_240p && VIDEO_GetCurrentTvMode() != VI_PAL)
	{
		WII_SetDoubleStrikeVideoMode(640, 240, 640);
		WII_ChangeSquare(640, 240, 320, 120);
	}
	else
	{
		WII_SetDefaultVideoMode();
		WII_ChangeSquare(640, 480, 320, 240);
	}

	if (xroar_bilinear)
	{
		WII_SetFilter(true);
	}
	else
	{
		WII_SetFilter(false);
	}

	if (xroar_tvfilter)
		filter = 1;
	else
		filter = 0;

	if (xroar_wide)
		WII_SetWidescreen(1);
}

static int init(void) {
	LOG_DEBUG(2,"Initialising SDL video driver\n");
#ifdef WINDOWS32
	if (!getenv("SDL_VIDEODRIVER"))
		putenv("SDL_VIDEODRIVER=windib");
#endif

#ifdef GEKKO
	/* init SDL TTF */
	TTF_Init();

	SDL_RWops* fontsmallRW = SDL_RWFromMem((char *)another_ttf, another_ttf_size);
	text_fontSmall = TTF_OpenFontRW(fontsmallRW, 1, 12); 
	SDL_RWops* fontlargeRW = SDL_RWFromMem((char *)dejavusans_ttf, dejavusans_ttf_size);
	text_fontLarge = TTF_OpenFontRW(fontlargeRW, 1, 13); 
#endif
	if (!SDL_WasInit(SDL_INIT_NOPARACHUTE)) {
		if (SDL_Init(SDL_INIT_NOPARACHUTE) < 0) {
			LOG_ERROR("Failed to initialise SDL: %s\n", SDL_GetError());
			return 1;
		}
	}
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		LOG_ERROR("Failed to initialise SDL video driver: %s\n", SDL_GetError());
		return 1;
	}

	if (xroar_240p)
	{
		WII_SetDoubleStrikeVideoMode(640, 240, 640);
		WII_ChangeSquare(640, 240, 320, 120);
	}
	else
	{
		WII_SetDefaultVideoMode();
		WII_ChangeSquare(640, 480, 320, 240);
	}

	if (xroar_bilinear)
	{
		WII_SetFilter(true);
	}
	else
	{
		WII_SetFilter(false);
	}

	if (xroar_tvfilter)
		filter = 1;
	else
		filter = 0;

	if (xroar_wide)
		WII_SetWidescreen(1);

	screen = SDL_SetVideoMode(640, 480, 32, SDL_HWSURFACE|SDL_FULLSCREEN);
	backend = SDL_CreateRGBSurface(SDL_HWSURFACE,320, 240,32, 0, 0, 0, 0);
	openDialog = SDL_CreateRGBSurface(0, 320, 240, 32, 0, 0, 0, 0);

	if (set_fullscreen(xroar_opt_fullscreen))
		return 1;
#ifdef WINDOWS32
	{
		SDL_version sdlver;
		SDL_SysWMinfo sdlinfo;
		SDL_VERSION(&sdlver);
		sdlinfo.version = sdlver;
		SDL_GetWMInfo(&sdlinfo);
		windows32_main_hwnd = sdlinfo.window;
	}
#endif
	reset();
	set_mode(0);

	return 0;
}

static void shutdown(void) {
	LOG_DEBUG(2,"Shutting down SDL video driver\n");
	set_fullscreen(0);
	/* Should not be freed by caller: SDL_FreeSurface(screen); */
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static int set_fullscreen(int fullscreen) {
	if (screen == NULL) {
		LOG_ERROR("Failed to allocate SDL surface for display\n");
		return 1;
	}
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	alloc_colours();

	if (fullscreen)
		SDL_ShowCursor(SDL_DISABLE);
	else
		SDL_ShowCursor(SDL_ENABLE);
	video_sdl_module.is_fullscreen = fullscreen;
#ifdef GEKKO
	SDL_ShowCursor(SDL_DISABLE);
#endif
	return 0;
}

static void reset(void) {
	pixel = VIDEO_TOPLEFT + VIDEO_VIEWPORT_YOFFSET;
	subline = 0;
	beam_pos = 0;
}
static void vsync(void) {
#ifdef GEKKO
	SDL_Rect filter_rect = {0, 0, 320, 240};

	SDL_BlitSurface(backend, NULL, screen, NULL);

	if (filter)
	{
		SDL_LockSurface(screen);
		SDL_LockSurface(backend);
		ScanLines(screen, &filter_rect);	
		SDL_UnlockSurface(backend);
		SDL_UnlockSurface(screen);
	}
#endif
	SDL_UpdateRect(screen, 0, 0, 320, 240);
	reset();
}
