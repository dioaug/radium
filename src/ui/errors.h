#pragma once

#include <X11/Xlib.h>

#include "../core.h"

void ui_errors_setup(void);
void ui_errors_cleanup(void);
void ui_errors_add(const char *where, const char *fmt, ...);
int ui_errors_count(void);
int ui_errors_visible(void);
void ui_errors_toggle(Monitor *m);
int ui_errors_buttonpress(XEvent *e);
int ui_errors_buttonrelease(XEvent *e);
int ui_errors_motionnotify(XEvent *e);
int ui_errors_expose(XEvent *e);
