/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

/* This file contains generic scanline rendering routines.  It is included
 * into various video module source files and makes use of macros defined in
 * those files (eg, LOCK_SURFACE and XSTEP) */

#include "machine.h"
#include "vdg_palette.h"

#ifndef FAST_VDG
# define RENDER_ARGS uint8_t *vram_ptr, int beam_to
#else
# define RENDER_ARGS uint8_t *vram_ptr
# define beam_to (640)
#endif

static void render_sg4(RENDER_ARGS);
static void render_sg6(RENDER_ARGS);
static void render_cg1(RENDER_ARGS);
static void render_rg1(RENDER_ARGS);
static void render_cg2(RENDER_ARGS);
static void render_rg6(RENDER_ARGS);
#ifndef FAST_VDG
static void render_rg6a(RENDER_ARGS);
# define RENDER_CROSS_COLOUR ((xroar_opt_ccr == CROSS_COLOUR_5BIT) ? render_rg6a : render_cg2)
//#define RENDER_CROSS_COLOUR render_cg2
#else
# define RENDER_CROSS_COLOUR (render_cg2)
#endif

#define IS_LEFT_BORDER (beam_pos < beam_to && beam_pos >= 0 && beam_pos < 32)
#define IS_ACTIVE_LINE (beam_pos < beam_to && beam_pos >= 32 && beam_pos < 288)
#define IS_RIGHT_BORDER (beam_pos < beam_to && beam_pos >= 288 && beam_pos < 320)

#ifdef NO_BORDER
# define RENDER_BORDER do { beam_pos += 8; } while (0)
#else
# define RENDER_BORDER do { \
		*(pixel) = border_colour; \
		pixel += XSTEP; \
		beam_pos++; \
	} while (0)
#endif

static unsigned int subline;
static Pixel *pixel;
static Pixel black, darkgreen, darkorange, brightorange;
static Pixel bg_colour;
static Pixel fg_colour;
static Pixel vdg_colour[16];
static Pixel artifact_colours[2][32];
static Pixel *cg_colours;
static Pixel border_colour;
static int beam_pos;

/* Map VDG palette entry */
static Pixel map_palette_entry(int i) {
	float R, G, B;
	int is_pal = (xroar_machine_config->tv_standard == TV_PAL);
	vdg_palette_RGB(xroar_vdg_palette, is_pal, i, &R, &G, &B);
	R *= 255.0;
	G *= 255.0;
	B *= 255.0;
	return MAPCOLOUR((int)R, (int)G, (int)B);
}

#ifdef GEKKO
void display_message(char *text, int delay);

unsigned int R_cfg[4],G_cfg[4],B_cfg[4];

static int check_hexa(char *value)
{
	if (value[strspn(value, "0123456789abcdefABCDEF")] != 0)
	{
		return 0;
	}
	return 1;
}

/*-----------------------------------------------------------------------*/
/**
 * Parse RGB values for the artifact palette
 * 
 * Check length : 4 colors in hexa (24 + 3 ':').
 * Check validity : only hexa character.
 * Split value : assign to 3 R,G,B variables.
 */
static int parse_colours()
{
	int i;
	char *tmp;
	char *token;
	char colour_cfg[4][7];
	int rgbvalue;
	int valid_data = 1;

	if (xroar_rgb )
	{
		tmp = strdup (xroar_rgb);

		if (strlen(xroar_rgb) < 27)
		{
			display_message("RGB colors invalid! Not enough values!", 80);
			valid_data = 0;
		}
		else if (strlen(xroar_rgb) > 27)
		{
			display_message("RGB colors invalid! Too many values!", 80);
			valid_data = 0;
		}

		for (i=0;i<4;i++)
		{
			token = strsep (&tmp, ":"); 
			valid_data = check_hexa(token);

			if (!valid_data)
			{
				display_message("RGB values are invalid! Must be hexa!", 80);
				running_config.cross_colour_phase=0;
				return valid_data;
				break;
			}

			if(token)
			{
					if(strlen(token) == 6)
					{
						strcpy(colour_cfg[i], token);
						rgbvalue = strtol(colour_cfg[i], NULL, 16);
						LOG_DEBUG(2, "colour_cfg[i]  :::::: %s. %d \n", token, i);

						R_cfg[i] = (rgbvalue>>16) & 0xff;
						G_cfg[i] = (rgbvalue>>8) & 0xff;
						B_cfg[i] = rgbvalue & 0xff;
						LOG_DEBUG(2, "VALUES  ::: %x %x %x. \n", R_cfg[i], G_cfg[i], B_cfg[i]);
					}
					else
					{
						strcpy(colour_cfg[i] ,"0");
					}
			}
		}
	}
	else
	{
		running_config.cross_colour_phase=0;
		return valid_data;
	}
	return valid_data;
}
#endif


/* Allocate colours */
static void alloc_colours(void) {
	int i;
#ifdef RESET_PALETTE
	RESET_PALETTE();
#endif
	for (i = 0; i < 8; i++) {
		vdg_colour[i] = map_palette_entry(i);
	}
	black = map_palette_entry(8);
	darkgreen = map_palette_entry(9);
	darkorange = map_palette_entry(10);
	brightorange = map_palette_entry(11);

	vdg_colour[8] = MAPCOLOUR(0x00, 0x00, 0x00);
	vdg_colour[9] = MAPCOLOUR(0x00, 0x80, 0xff);
	vdg_colour[10] = MAPCOLOUR(0xff, 0x80, 0x00);
	vdg_colour[11] = MAPCOLOUR(0xff, 0xff, 0xff);
	vdg_colour[12] = MAPCOLOUR(0x00, 0x00, 0x00);
	vdg_colour[13] = MAPCOLOUR(0xff, 0x80, 0x00);
	vdg_colour[14] = MAPCOLOUR(0x00, 0x80, 0xff);
	vdg_colour[15] = MAPCOLOUR(0xff, 0xff, 0xff);

#ifdef GEKKO
	if (running_config.cross_colour_phase > 2)
	{
		if (running_config.cross_colour_phase > 4 && parse_colours())
		{
			/* User defined palette, 4 new colours: Color#1 light, Color#1 dark, Color#2 light, Color#2 dark,   */
			artifact_colours[0][0x04] = MAPCOLOUR(R_cfg[0], G_cfg[0], B_cfg[0]);
			artifact_colours[0][0x05] = MAPCOLOUR(R_cfg[1], G_cfg[1], B_cfg[1]);
			artifact_colours[0][0x0a] = MAPCOLOUR(R_cfg[3], G_cfg[3], B_cfg[3]);
			artifact_colours[0][0x0b] = MAPCOLOUR(R_cfg[3], G_cfg[3], B_cfg[3]);
			artifact_colours[0][0x0c] = MAPCOLOUR(R_cfg[0], G_cfg[0], B_cfg[0]);
			artifact_colours[0][0x0e] = MAPCOLOUR(R_cfg[2], G_cfg[2], B_cfg[2]);
			artifact_colours[0][0x14] = MAPCOLOUR(R_cfg[0], G_cfg[0], B_cfg[0]);
			artifact_colours[0][0x15] = MAPCOLOUR(R_cfg[0], G_cfg[0], B_cfg[0]);
			artifact_colours[0][0x1a] = MAPCOLOUR(R_cfg[3], G_cfg[3], B_cfg[3]);
			artifact_colours[0][0x1b] = MAPCOLOUR(R_cfg[3], G_cfg[3], B_cfg[3]);
			artifact_colours[1][0x02] = MAPCOLOUR(R_cfg[1], G_cfg[1], B_cfg[1]);
			artifact_colours[1][0x04] = MAPCOLOUR(R_cfg[2], G_cfg[2], B_cfg[2]);
			artifact_colours[1][0x05] = MAPCOLOUR(R_cfg[2], G_cfg[2], B_cfg[2]);
			artifact_colours[1][0x06] = MAPCOLOUR(R_cfg[0], G_cfg[0], B_cfg[0]);
			artifact_colours[1][0x07] = MAPCOLOUR(R_cfg[2], G_cfg[2], B_cfg[2]);
			artifact_colours[1][0x08] = MAPCOLOUR(R_cfg[1], G_cfg[1], B_cfg[1]);
			artifact_colours[1][0x0a] = MAPCOLOUR(R_cfg[0], G_cfg[0], B_cfg[0]);
			artifact_colours[1][0x0b] = MAPCOLOUR(R_cfg[0], G_cfg[0], B_cfg[0]);
			artifact_colours[1][0x14] = MAPCOLOUR(R_cfg[3], G_cfg[3], B_cfg[3]);
			artifact_colours[1][0x15] = MAPCOLOUR(R_cfg[3], G_cfg[3], B_cfg[3]);
			artifact_colours[1][0x17] = MAPCOLOUR(R_cfg[2], G_cfg[2], B_cfg[2]);
			artifact_colours[1][0x1a] = MAPCOLOUR(R_cfg[0], G_cfg[0], B_cfg[0]);
			artifact_colours[1][0x1b] = MAPCOLOUR(R_cfg[0], G_cfg[0], B_cfg[0]);
			artifact_colours[1][0x1c] = MAPCOLOUR(R_cfg[2], G_cfg[2], B_cfg[2]);
			artifact_colours[1][0x1d] = MAPCOLOUR(R_cfg[2], G_cfg[2], B_cfg[2]);
		}
		else
		{
			/* Green/Blue palette for Pal-M brazilian CoCo */
			artifact_colours[0][0x04] = MAPCOLOUR(0x00, 0x8c, 0x64);
			artifact_colours[0][0x05] = MAPCOLOUR(0x00, 0x8c, 0x64); // green
			artifact_colours[0][0x0a] = MAPCOLOUR(0x00, 0x80, 0xff); // blue
			artifact_colours[0][0x0b] = MAPCOLOUR(0x00, 0x80, 0xff);
			artifact_colours[0][0x0c] = MAPCOLOUR(0xd2, 0xff, 0xd2); // lightgreen
			artifact_colours[0][0x0e] = MAPCOLOUR(0x64, 0xf0, 0xff);
			artifact_colours[0][0x14] = MAPCOLOUR(0x4a, 0xa2, 0x08); // green
			artifact_colours[0][0x15] = MAPCOLOUR(0x4a, 0xa2, 0x08); // green
			artifact_colours[0][0x1a] = MAPCOLOUR(0x00, 0x80, 0xff);
			artifact_colours[0][0x1b] = MAPCOLOUR(0x00, 0x80, 0xff);
			artifact_colours[1][0x02] = MAPCOLOUR(0x43, 0x96, 0x08); // Dark green
			artifact_colours[1][0x04] = MAPCOLOUR(0x46, 0xc8, 0xff);
			artifact_colours[1][0x05] = MAPCOLOUR(0x46, 0xc8, 0xff);
			artifact_colours[1][0x06] = MAPCOLOUR(0xd2, 0xff, 0xd2);
			artifact_colours[1][0x07] = MAPCOLOUR(0x64, 0xf0, 0xff);
			artifact_colours[1][0x08] = MAPCOLOUR(0x4a, 0xa2, 0x08); // Dark green
			artifact_colours[1][0x0a] = MAPCOLOUR(0x4a, 0xa2, 0x08); // green
			artifact_colours[1][0x0b] = MAPCOLOUR(0x4a, 0xa2, 0x08); // green
			artifact_colours[1][0x14] = MAPCOLOUR(0x00, 0x80, 0xff);
			artifact_colours[1][0x15] = MAPCOLOUR(0x00, 0x80, 0xff);
			artifact_colours[1][0x17] = MAPCOLOUR(0x64, 0xf0, 0xff);
			artifact_colours[1][0x1a] = MAPCOLOUR(0x4a, 0xa2, 0x08); // green
			artifact_colours[1][0x1b] = MAPCOLOUR(0x4a, 0xa2, 0x08); // green
			artifact_colours[1][0x1c] = MAPCOLOUR(0x64, 0xf0, 0xff);
			artifact_colours[1][0x1d] = MAPCOLOUR(0x64, 0xf0, 0xff);
		}
			/* Colors that are common to User defined and Pal-M */
			artifact_colours[0][0x00] = MAPCOLOUR(0x00, 0x00, 0x00);
			artifact_colours[0][0x01] = MAPCOLOUR(0x00, 0x00, 0x00);
			artifact_colours[0][0x02] = MAPCOLOUR(0x00, 0x32, 0x78); // Deep blue
			artifact_colours[0][0x03] = MAPCOLOUR(0x00, 0x28, 0x00); // Very dark green
			artifact_colours[0][0x06] = MAPCOLOUR(0xff, 0xd2, 0xff); // Pale pink
			artifact_colours[0][0x07] = MAPCOLOUR(0xff, 0xf0, 0xc8);
			artifact_colours[0][0x08] = MAPCOLOUR(0x00, 0x32, 0x78); // Deep blue
			artifact_colours[0][0x09] = MAPCOLOUR(0x00, 0x00, 0x3c);
			artifact_colours[0][0x0d] = MAPCOLOUR(0xff, 0xff, 0xff);
			artifact_colours[0][0x0f] = MAPCOLOUR(0xff, 0xff, 0xff);
			artifact_colours[0][0x10] = MAPCOLOUR(0x3c, 0x00, 0x00);
			artifact_colours[0][0x11] = MAPCOLOUR(0x3c, 0x00, 0x00);
			artifact_colours[0][0x12] = MAPCOLOUR(0x00, 0x00, 0x00);
			artifact_colours[0][0x13] = MAPCOLOUR(0x00, 0x28, 0x00); // Very dark green
			artifact_colours[0][0x16] = MAPCOLOUR(0xff, 0xff, 0xff);
			artifact_colours[0][0x17] = MAPCOLOUR(0xff, 0xf0, 0xc8);
			artifact_colours[0][0x18] = MAPCOLOUR(0x28, 0x00, 0x28); // Very Deep blue
			artifact_colours[0][0x19] = MAPCOLOUR(0x28, 0x00, 0x28); // Very Deep blue
			artifact_colours[0][0x1c] = MAPCOLOUR(0xff, 0xf0, 0xc8);
			artifact_colours[0][0x1d] = MAPCOLOUR(0xff, 0xf0, 0xc8);
			artifact_colours[0][0x1e] = MAPCOLOUR(0xff, 0xff, 0xff);
			artifact_colours[0][0x1f] = MAPCOLOUR(0xff, 0xff, 0xff);
			artifact_colours[1][0x00] = MAPCOLOUR(0x00, 0x00, 0x00);
			artifact_colours[1][0x01] = MAPCOLOUR(0x00, 0x00, 0x00);
			artifact_colours[1][0x03] = MAPCOLOUR(0x28, 0x00, 0x28); // Very Deep blue
			artifact_colours[1][0x09] = MAPCOLOUR(0x3c, 0x00, 0x00);
			artifact_colours[1][0x0c] = MAPCOLOUR(0xff, 0xd2, 0xff); // Pale pink
			artifact_colours[1][0x0d] = MAPCOLOUR(0xff, 0xff, 0xff);
			artifact_colours[1][0x0e] = MAPCOLOUR(0xff, 0xf0, 0xc8);
			artifact_colours[1][0x0f] = MAPCOLOUR(0xff, 0xff, 0xff);
			artifact_colours[1][0x10] = MAPCOLOUR(0x00, 0x00, 0x3c);
			artifact_colours[1][0x11] = MAPCOLOUR(0x00, 0x00, 0x3c);
			artifact_colours[1][0x12] = MAPCOLOUR(0x00, 0x00, 0x00);
			artifact_colours[1][0x13] = MAPCOLOUR(0x28, 0x00, 0x28); // Very Deep blue
			artifact_colours[1][0x16] = MAPCOLOUR(0xff, 0xff, 0xff);
			artifact_colours[1][0x18] = MAPCOLOUR(0x00, 0x28, 0x00); // Very dark green
			artifact_colours[1][0x19] = MAPCOLOUR(0x00, 0x28, 0x00); // Very dark green
			artifact_colours[1][0x1e] = MAPCOLOUR(0xff, 0xff, 0xff);
			artifact_colours[1][0x1f] = MAPCOLOUR(0xff, 0xff, 0xff);
	}
	else
	{
		/* Default Red/Blue palette */
		artifact_colours[0][0x00] = MAPCOLOUR(0x00, 0x00, 0x00);
		artifact_colours[0][0x01] = MAPCOLOUR(0x00, 0x00, 0x00);
		artifact_colours[0][0x02] = MAPCOLOUR(0x00, 0x32, 0x78);
		artifact_colours[0][0x03] = MAPCOLOUR(0x00, 0x28, 0x00);
		artifact_colours[0][0x04] = MAPCOLOUR(0xff, 0x8c, 0x64);
		artifact_colours[0][0x05] = MAPCOLOUR(0xff, 0x8c, 0x64);
		artifact_colours[0][0x06] = MAPCOLOUR(0xff, 0xd2, 0xff);
		artifact_colours[0][0x07] = MAPCOLOUR(0xff, 0xf0, 0xc8);
		artifact_colours[0][0x08] = MAPCOLOUR(0x00, 0x32, 0x78);
		artifact_colours[0][0x09] = MAPCOLOUR(0x00, 0x00, 0x3c);
		artifact_colours[0][0x0a] = MAPCOLOUR(0x00, 0x80, 0xff);
		artifact_colours[0][0x0b] = MAPCOLOUR(0x00, 0x80, 0xff);
		artifact_colours[0][0x0c] = MAPCOLOUR(0xd2, 0xff, 0xd2);
		artifact_colours[0][0x0d] = MAPCOLOUR(0xff, 0xff, 0xff);
		artifact_colours[0][0x0e] = MAPCOLOUR(0x64, 0xf0, 0xff);
		artifact_colours[0][0x0f] = MAPCOLOUR(0xff, 0xff, 0xff);
		artifact_colours[0][0x10] = MAPCOLOUR(0x3c, 0x00, 0x00);
		artifact_colours[0][0x11] = MAPCOLOUR(0x3c, 0x00, 0x00);
		artifact_colours[0][0x12] = MAPCOLOUR(0x00, 0x00, 0x00);
		artifact_colours[0][0x13] = MAPCOLOUR(0x00, 0x28, 0x00);
		artifact_colours[0][0x14] = MAPCOLOUR(0xff, 0x80, 0x00);
		artifact_colours[0][0x15] = MAPCOLOUR(0xff, 0x80, 0x00);
		artifact_colours[0][0x16] = MAPCOLOUR(0xff, 0xff, 0xff);
		artifact_colours[0][0x17] = MAPCOLOUR(0xff, 0xf0, 0xc8);
		artifact_colours[0][0x18] = MAPCOLOUR(0x28, 0x00, 0x28);
		artifact_colours[0][0x19] = MAPCOLOUR(0x28, 0x00, 0x28);
		artifact_colours[0][0x1a] = MAPCOLOUR(0x00, 0x80, 0xff);
		artifact_colours[0][0x1b] = MAPCOLOUR(0x00, 0x80, 0xff);
		artifact_colours[0][0x1c] = MAPCOLOUR(0xff, 0xf0, 0xc8);
		artifact_colours[0][0x1d] = MAPCOLOUR(0xff, 0xf0, 0xc8);
		artifact_colours[0][0x1e] = MAPCOLOUR(0xff, 0xff, 0xff);
		artifact_colours[0][0x1f] = MAPCOLOUR(0xff, 0xff, 0xff);
		artifact_colours[1][0x00] = MAPCOLOUR(0x00, 0x00, 0x00);
		artifact_colours[1][0x01] = MAPCOLOUR(0x00, 0x00, 0x00);
		artifact_colours[1][0x02] = MAPCOLOUR(0xb4, 0x3c, 0x1e);
		artifact_colours[1][0x03] = MAPCOLOUR(0x28, 0x00, 0x28);
		artifact_colours[1][0x04] = MAPCOLOUR(0x46, 0xc8, 0xff);
		artifact_colours[1][0x05] = MAPCOLOUR(0x46, 0xc8, 0xff);
		artifact_colours[1][0x06] = MAPCOLOUR(0xd2, 0xff, 0xd2);
		artifact_colours[1][0x07] = MAPCOLOUR(0x64, 0xf0, 0xff);
		artifact_colours[1][0x08] = MAPCOLOUR(0xb4, 0x3c, 0x1e);
		artifact_colours[1][0x09] = MAPCOLOUR(0x3c, 0x00, 0x00);
		artifact_colours[1][0x0a] = MAPCOLOUR(0xff, 0x80, 0x00);
		artifact_colours[1][0x0b] = MAPCOLOUR(0xff, 0x80, 0x00);
		artifact_colours[1][0x0c] = MAPCOLOUR(0xff, 0xd2, 0xff);
		artifact_colours[1][0x0d] = MAPCOLOUR(0xff, 0xff, 0xff);
		artifact_colours[1][0x0e] = MAPCOLOUR(0xff, 0xf0, 0xc8);
		artifact_colours[1][0x0f] = MAPCOLOUR(0xff, 0xff, 0xff);
		artifact_colours[1][0x10] = MAPCOLOUR(0x00, 0x00, 0x3c);
		artifact_colours[1][0x11] = MAPCOLOUR(0x00, 0x00, 0x3c);
		artifact_colours[1][0x12] = MAPCOLOUR(0x00, 0x00, 0x00);
		artifact_colours[1][0x13] = MAPCOLOUR(0x28, 0x00, 0x28);
		artifact_colours[1][0x14] = MAPCOLOUR(0x00, 0x80, 0xff);
		artifact_colours[1][0x15] = MAPCOLOUR(0x00, 0x80, 0xff);
		artifact_colours[1][0x16] = MAPCOLOUR(0xff, 0xff, 0xff);
		artifact_colours[1][0x17] = MAPCOLOUR(0x64, 0xf0, 0xff);
		artifact_colours[1][0x18] = MAPCOLOUR(0x00, 0x28, 0x00);
		artifact_colours[1][0x19] = MAPCOLOUR(0x00, 0x28, 0x00);
		artifact_colours[1][0x1a] = MAPCOLOUR(0xff, 0x80, 0x00);
		artifact_colours[1][0x1b] = MAPCOLOUR(0xff, 0x80, 0x00);
		artifact_colours[1][0x1c] = MAPCOLOUR(0x64, 0xf0, 0xff);
		artifact_colours[1][0x1d] = MAPCOLOUR(0x64, 0xf0, 0xff);
		artifact_colours[1][0x1e] = MAPCOLOUR(0xff, 0xff, 0xff);
		artifact_colours[1][0x1f] = MAPCOLOUR(0xff, 0xff, 0xff);
	}
#endif
}

/* Update graphics mode - change current select colour set */
static void set_mode(unsigned int mode) {
#ifdef GEKKO
int phase = 0;
#endif
	switch ((mode & 0xf0) >> 4) {
		case 0: case 2: case 4: case 6:
			VIDEO_MODULE_NAME.render_scanline = render_sg4;
			if (mode & 0x08) {
				fg_colour = brightorange;
				bg_colour = darkorange;
			} else {
				fg_colour = vdg_colour[0];
				bg_colour = darkgreen;
			}
			border_colour = black;
			break;
		case 1: case 3: case 5: case 7:
			VIDEO_MODULE_NAME.render_scanline = render_sg6;
			cg_colours = &vdg_colour[(mode & 0x08) >> 1];
			if (mode & 0x08) {
				fg_colour = brightorange;
				bg_colour = darkorange;
			} else {
				fg_colour = vdg_colour[0];
				bg_colour = darkgreen;
			}
			border_colour = black;
			break;
		case 8:
			VIDEO_MODULE_NAME.render_scanline = render_cg1;
			cg_colours = &vdg_colour[(mode & 0x08) >> 1];
			border_colour = cg_colours[0];
			break;
		case 9: case 11: case 13:
			VIDEO_MODULE_NAME.render_scanline = render_rg1;
			fg_colour = vdg_colour[(mode & 0x08) >> 1];
			bg_colour = black;
			border_colour = fg_colour;
			break;
		case 10: case 12: case 14:
			VIDEO_MODULE_NAME.render_scanline = render_cg2;
			cg_colours = &vdg_colour[(mode & 0x08) >> 1];
			border_colour = cg_colours[0];
			break;
		case 15: default:
#ifdef GEKKO
			if (running_config.cross_colour_phase > 2 && running_config.cross_colour_phase < 5)
			{
				phase = running_config.cross_colour_phase - 2;
			}
			else if (running_config.cross_colour_phase > 4 && running_config.cross_colour_phase < 7)
			{
				phase = running_config.cross_colour_phase - 4;
			}
			else
			{
				phase = running_config.cross_colour_phase;
			}
#endif
			if ((mode & 0x08) && running_config.cross_colour_phase) {
				VIDEO_MODULE_NAME.render_scanline = RENDER_CROSS_COLOUR;
#ifdef GEKKO
				cg_colours = &vdg_colour[4 + phase * 4];
#else
				cg_colours = &vdg_colour[4 + running_config.cross_colour_phase * 4];
#endif
				border_colour = vdg_colour[4];;
			} else {
				VIDEO_MODULE_NAME.render_scanline = render_rg6;
				fg_colour = vdg_colour[(mode & 0x08) >> 1];
				border_colour = fg_colour;
			}
			bg_colour = (mode & 0x08) ? black : darkgreen;
			break;
	}
}

/* Renders a line of alphanumeric/semigraphics 4 (mode is selected by data
 * line, so need to be handled together) */
static void render_sg4(RENDER_ARGS) {
	unsigned int octet;
	LOCK_SURFACE;
	while (IS_LEFT_BORDER) {
		RENDER_BORDER;
	}
	while (IS_ACTIVE_LINE) {
		Pixel tmp;
		octet = *(vram_ptr++);
		if (octet & 0x80) {
			tmp = vdg_colour[(octet & 0x70)>>4];
			if (subline < 6) {
				*pixel = *(pixel+1*XSTEP) = *(pixel+2*XSTEP) = *(pixel+3*XSTEP) = (octet & 0x08) ? tmp : black;
				*(pixel+4*XSTEP) = *(pixel+5*XSTEP) = *(pixel+6*XSTEP) = *(pixel+7*XSTEP) = (octet & 0x04) ? tmp : black;
			} else {
				*pixel = *(pixel+1*XSTEP) = *(pixel+2*XSTEP) = *(pixel+3*XSTEP) = (octet & 0x02) ? tmp : black;
				*(pixel+4*XSTEP) = *(pixel+5*XSTEP) = *(pixel+6*XSTEP) = *(pixel+7*XSTEP) = (octet & 0x01) ? tmp : black;
			}
		} else {
			tmp = vdg_alpha[(octet&0x3f)*12 + subline];
			if (octet & 0x40)
				tmp = ~tmp;
			*pixel = (tmp & 0x80) ? fg_colour : bg_colour;
			*(pixel+1*XSTEP) = (tmp & 0x40) ? fg_colour : bg_colour;
			*(pixel+2*XSTEP) = (tmp & 0x20) ? fg_colour : bg_colour;
			*(pixel+3*XSTEP) = (tmp & 0x10) ? fg_colour : bg_colour;
			*(pixel+4*XSTEP) = (tmp & 0x08) ? fg_colour : bg_colour;
			*(pixel+5*XSTEP) = (tmp & 0x04) ? fg_colour : bg_colour;
			*(pixel+6*XSTEP) = (tmp & 0x02) ? fg_colour : bg_colour;
			*(pixel+7*XSTEP) = (tmp & 0x01) ? fg_colour : bg_colour;
		}
		pixel += 8*XSTEP;
		beam_pos += 8;
	}
	while (IS_RIGHT_BORDER) {
		RENDER_BORDER;
	}
	UNLOCK_SURFACE;
}

/* Renders a line of external-alpha/semigraphics 6 (mode is selected by data
 * line, so need to be handled together) */
static void render_sg6(RENDER_ARGS) {
	unsigned int octet;
	LOCK_SURFACE;
	while (IS_LEFT_BORDER) {
		RENDER_BORDER;
	}
	while (IS_ACTIVE_LINE) {
		Pixel tmp;
		octet = *(vram_ptr++);
		if (octet & 0x80) {
			tmp = cg_colours[(octet & 0xc0)>>6];
			if (subline < 4) {
				*pixel = *(pixel+1*XSTEP) = *(pixel+2*XSTEP) = *(pixel+3*XSTEP) = (octet & 0x20) ? tmp : black;
				*(pixel+4*XSTEP) = *(pixel+5*XSTEP) = *(pixel+6*XSTEP) = *(pixel+7*XSTEP) = (octet & 0x10) ? tmp : black;
			} else if (subline < 8) {
				*pixel = *(pixel+1*XSTEP) = *(pixel+2*XSTEP) = *(pixel+3*XSTEP) = (octet & 0x08) ? tmp : black;
				*(pixel+4*XSTEP) = *(pixel+5*XSTEP) = *(pixel+6*XSTEP) = *(pixel+7*XSTEP) = (octet & 0x04) ? tmp : black;
			} else {
				*pixel = *(pixel+1*XSTEP) = *(pixel+2*XSTEP) = *(pixel+3*XSTEP) = (octet & 0x02) ? tmp : black;
				*(pixel+4*XSTEP) = *(pixel+5*XSTEP) = *(pixel+6*XSTEP) = *(pixel+7*XSTEP) = (octet & 0x01) ? tmp : black;
			}
		} else {
			tmp = octet;
			if (octet & 0x40)
				tmp = ~tmp;
			*pixel = (tmp & 0x80) ? fg_colour : bg_colour;
			//*(pixel+1*XSTEP) = (tmp & 0x40) ? fg_colour : bg_colour;
			*(pixel+1*XSTEP) = bg_colour;
			*(pixel+2*XSTEP) = (tmp & 0x20) ? fg_colour : bg_colour;
			*(pixel+3*XSTEP) = (tmp & 0x10) ? fg_colour : bg_colour;
			*(pixel+4*XSTEP) = (tmp & 0x08) ? fg_colour : bg_colour;
			*(pixel+5*XSTEP) = (tmp & 0x04) ? fg_colour : bg_colour;
			*(pixel+6*XSTEP) = (tmp & 0x02) ? fg_colour : bg_colour;
			*(pixel+7*XSTEP) = (tmp & 0x01) ? fg_colour : bg_colour;
		}
		pixel += 8*XSTEP;
		beam_pos += 8;
	}
	while (IS_RIGHT_BORDER) {
		RENDER_BORDER;
	}
	UNLOCK_SURFACE;
}

#define RENDER_BYTE_CG1(b) do { \
		*pixel = *(pixel+1*XSTEP) \
			= *(pixel+2*XSTEP) = *(pixel+3*XSTEP) \
			= cg_colours[(b & 0xc0) >> 6]; \
		*(pixel+4*XSTEP) = *(pixel+5*XSTEP) \
			= *(pixel+6*XSTEP) = *(pixel+7*XSTEP) \
			= cg_colours[(b & 0x30) >> 4]; \
		*(pixel+8*XSTEP) = *(pixel+9*XSTEP) \
			= *(pixel+10*XSTEP) = *(pixel+11*XSTEP) \
			= cg_colours[(b & 0x0c) >> 2]; \
		*(pixel+12*XSTEP) = *(pixel+13*XSTEP) \
			= *(pixel+14*XSTEP) = *(pixel+15*XSTEP) \
			= cg_colours[b & 0x03]; \
		pixel += 16 * XSTEP; \
		beam_pos += 16; \
	} while (0);

/* Render a 16-byte colour graphics line (CG1) */
static void render_cg1(RENDER_ARGS) {
	unsigned int octet;
	LOCK_SURFACE;
	while (IS_LEFT_BORDER) {
		RENDER_BORDER;
	}
	while (IS_ACTIVE_LINE) {
		octet = *(vram_ptr++);
		RENDER_BYTE_CG1(octet);
	}
	while (IS_RIGHT_BORDER) {
		RENDER_BORDER;
	}
	UNLOCK_SURFACE;
}

#define RENDER_BYTE_RG1(b) do { \
		*pixel = *(pixel+1*XSTEP) = (b & 0x80) ? fg_colour : bg_colour; \
		*(pixel+2*XSTEP) = *(pixel+3*XSTEP) = (b & 0x40) ? fg_colour : bg_colour; \
		*(pixel+4*XSTEP) = *(pixel+5*XSTEP) = (b & 0x20) ? fg_colour : bg_colour; \
		*(pixel+6*XSTEP) = *(pixel+7*XSTEP) = (b & 0x10) ? fg_colour : bg_colour; \
		*(pixel+8*XSTEP) = *(pixel+9*XSTEP) = (b & 0x08) ? fg_colour : bg_colour; \
		*(pixel+10*XSTEP) = *(pixel+11*XSTEP) = (b & 0x04) ? fg_colour : bg_colour; \
		*(pixel+12*XSTEP) = *(pixel+13*XSTEP) = (b & 0x02) ? fg_colour : bg_colour; \
		*(pixel+14*XSTEP) = *(pixel+15*XSTEP) = (b & 0x01) ? fg_colour : bg_colour; \
		pixel += 16 * XSTEP; \
		beam_pos += 16; \
	} while (0)

/* Render a 16-byte resolution graphics line (RG1,RG2,RG3) */
static void render_rg1(RENDER_ARGS) {
	unsigned int octet;
	LOCK_SURFACE;
	while (IS_LEFT_BORDER) {
		RENDER_BORDER;
	}
	while (IS_ACTIVE_LINE) {
		octet = *(vram_ptr++);
		RENDER_BYTE_RG1(octet);
	}
	while (IS_RIGHT_BORDER) {
		RENDER_BORDER;
	}
	UNLOCK_SURFACE;
}

#define RENDER_BYTE_CG2(o) do { \
		*pixel = *(pixel+1*XSTEP) = cg_colours[(o & 0xc0) >> 6]; \
		*(pixel+2*XSTEP) = *(pixel+3*XSTEP) = cg_colours[(o & 0x30) >> 4]; \
		*(pixel+4*XSTEP) = *(pixel+5*XSTEP) = cg_colours[(o & 0x0c) >> 2]; \
		*(pixel+6*XSTEP) = *(pixel+7*XSTEP) = cg_colours[o & 0x03]; \
		pixel += 8*XSTEP; \
		beam_pos += 8; \
	} while (0)

/* Render a 32-byte colour graphics line (CG2,CG3,CG6) */
static void render_cg2(RENDER_ARGS) {
	LOCK_SURFACE;
	while (IS_LEFT_BORDER) {
		RENDER_BORDER;
	}
	while (IS_ACTIVE_LINE) {
		unsigned int octet;
		octet = *(vram_ptr++);
		RENDER_BYTE_CG2(octet);
	}
	while (IS_RIGHT_BORDER) {
		RENDER_BORDER;
	}
	UNLOCK_SURFACE;
}

#define RENDER_BYTE_RG6(o) do { \
		*pixel = (o & 0x80) ? fg_colour : bg_colour; \
		*(pixel+1*XSTEP) = (o & 0x40) ? fg_colour : bg_colour; \
		*(pixel+2*XSTEP) = (o & 0x20) ? fg_colour : bg_colour; \
		*(pixel+3*XSTEP) = (o & 0x10) ? fg_colour : bg_colour; \
		*(pixel+4*XSTEP) = (o & 0x08) ? fg_colour : bg_colour; \
		*(pixel+5*XSTEP) = (o & 0x04) ? fg_colour : bg_colour; \
		*(pixel+6*XSTEP) = (o & 0x02) ? fg_colour : bg_colour; \
		*(pixel+7*XSTEP) = (o & 0x01) ? fg_colour : bg_colour; \
	} while (0)

/* Render a 32-byte resolution graphics line (RG6) */
static void render_rg6(RENDER_ARGS) {
	unsigned int octet;
	LOCK_SURFACE;
	while (IS_LEFT_BORDER) {
		RENDER_BORDER;
	}
	while (IS_ACTIVE_LINE) {
		octet = *(vram_ptr++);
		RENDER_BYTE_RG6(octet);
		pixel += 8*XSTEP;
		beam_pos += 8;
	}
	while (IS_RIGHT_BORDER) {
		RENDER_BORDER;
	}
	UNLOCK_SURFACE;
}

#ifndef FAST_VDG
/* Render artifacted colours */
static void render_rg6a(RENDER_ARGS) {
	static int aindex = 31;
	unsigned int octet;
	LOCK_SURFACE;
#ifdef GEKKO
	int phase = 0;

	if (running_config.cross_colour_phase > 2 && running_config.cross_colour_phase < 5)
	{
		phase = running_config.cross_colour_phase - 2;
	}
	else if (running_config.cross_colour_phase > 4 && running_config.cross_colour_phase < 7)
	{
		phase = running_config.cross_colour_phase - 4;
	}
	else
	{
		phase = running_config.cross_colour_phase;
	}
#endif
	while (beam_pos < 2 && beam_pos < beam_to) {
		pixel += XSTEP;
		beam_pos++;
	}
	while (beam_pos >= 2 && beam_pos < 32 && beam_pos < beam_to) {
		*(pixel-2*XSTEP) = border_colour;
		pixel += XSTEP;
		beam_pos++;
	}
	while (beam_pos >= 32 && beam_pos < 288 && beam_pos < beam_to) {
		int i;
		octet = *(vram_ptr++);
		for (i = 0; i < 8; i++) {
			aindex = ((aindex << 1) | (octet >> 7)) & 0x1f;
#ifdef GEKKO
			*(pixel-2*XSTEP) = artifact_colours[(i&1)^(phase-1)][aindex];
#else
			*(pixel-2*XSTEP) = artifact_colours[(i&1)^(running_config.cross_colour_phase-1)][aindex];
#endif
			octet = (octet << 1) & 0xff;
			pixel += XSTEP;
		}
		beam_pos += 8;
	}
	while (beam_pos >= 288 && beam_pos < 293 && beam_pos < beam_to) {
		aindex = ((aindex << 1) | 1) & 0x1f;
#ifdef GEKKO
		*(pixel-2*XSTEP) = artifact_colours[(beam_pos&1)^(phase-1)][aindex];
#else
		*(pixel-2*XSTEP) = artifact_colours[(beam_pos&1)^(running_config.cross_colour_phase-1)][aindex];
#endif
		pixel += XSTEP;
		beam_pos++;
	}
	while (beam_pos >= 293 && beam_pos < 320 && beam_pos < beam_to) {
		*(pixel-2*XSTEP) = border_colour;
		pixel += XSTEP;
		beam_pos++;
		if (beam_pos == 320) {
			*(pixel-2*XSTEP) = *(pixel-XSTEP) = border_colour;
		}
	}
	UNLOCK_SURFACE;
}
#endif

/* Render a line of border (top/bottom) */
static void render_border(void) {
#ifndef NO_BORDER
	unsigned int i;
	LOCK_SURFACE;
	for (i = 320; i; i--) {
		*pixel = border_colour;
		pixel += XSTEP;
	}
	UNLOCK_SURFACE;
	pixel += NEXTLINE;
#endif
}

static void hsync(void) {
	pixel += NEXTLINE;
	subline++;
	if (subline > 11)
		subline = 0;
	beam_pos = 0;
}
