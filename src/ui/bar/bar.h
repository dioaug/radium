#pragma once

#include <stdbool.h>
#include <X11/Xlib.h>

#include "../../core.h"
#include "../../draw.h"
#include "../../../lib/clay/clay.h"
#include "modules.h"

typedef struct {
	int ModuleGroupIndex;
	int ModuleIndex;
} ModuleId;

typedef struct {
	Dimensions dimensions;
	/* ui_bar_draw() sets the sizing: the monitor's width, and the content's height */
	Clay_ElementDeclaration style;
} Bar;

extern Bar ui_bar;
extern bool ui_bar_updating;

void ui_bar_setup(void);
void ui_bar_cleanup(void);
void ui_bar_initupdateintervals(void);
void ui_bar_stopupdateintervals(void);
void ui_bar_draw(Monitor *m, ModuleId* updatespecificmodcontent);
void ui_bar_drawall(void);
int ui_bar_buttonpress(XEvent *e);
int ui_bar_buttonrelease(XEvent *e);
int ui_bar_motionnotify(XEvent *e);
int ui_bar_enternotify(XEvent *e);
int ui_bar_leavenotify(XEvent *e);
int ui_bar_expose(XEvent *e);
