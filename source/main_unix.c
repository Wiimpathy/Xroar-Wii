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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_SDL
# include <SDL.h>
#endif
#include <stdio.h>

#include "xroar.h"
#include "logging.h"

#ifdef GEKKO
#include <unistd.h>
#include <gccore.h>
#include <ogcsys.h>
#include <fat.h>

extern void __exception_setreload(int t);
#endif

int main(int argc, char **argv) {
#ifdef GEKKO
	__exception_setreload(8);
	L2Enhance();

	if(IOS_GetVersion() != 58)
		IOS_ReloadIOS(58);

	/* initialize FAT devices */
	int retry = 0;
	int fatMounted = 0;

	/* try to mount FAT devices during 3 seconds */
	while (!fatMounted && (retry < 12))
	{
		fatMounted = fatInitDefault();
		usleep(250000);
		retry++;
	}
#endif
	atexit(xroar_shutdown);
	if (xroar_init(argc, argv) != 0)
		exit(1);
	xroar_mainloop();
	return 0;
}
