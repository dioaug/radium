/* See LICENSE file for copyright and license details. */
 
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <stdlib.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include "core.h"
#include "lua.h"

int
main(int argc, char *argv[])
{
	const char *config = NULL;

	if (argc == 2 && !strcmp("-v", argv[1]))
		die("radium-"VERSION);
	else if (argc == 3 && !strcmp("-c", argv[1]))
		config = argv[2];
	else if (argc != 1)
		die("usage: radium [-v] [-c config]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("radium: cannot open display");
	checkotherwm();
	rlua_init(config);
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan();
	rlua_startup();
	run();
	cleanup();
	rlua_cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}