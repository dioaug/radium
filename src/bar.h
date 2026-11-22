#pragma once

#include <stdbool.h>

#include "core.h"
#include "draw.h"
#include "../lib/clay/clay.h"

typedef struct {
	int ModuleGroupIndex;
	int ModuleIndex;
} ModuleId;

struct Module;
/* Declares the module's Clay elements for the frame being laid out.
 * A module with nothing to show declares nothing, which also removes
 * its gap from the group. */
typedef void (*ModLayoutFunc)(struct Module *mod, Monitor *m, bool updatecontent);

/* Groups, modules and children are styled with a Style (see draw.h):
 * a Clay element declaration as the base, with state styles that go over
 * it. Hover follows the pointer but skips selected elements, and modules
 * say when subdued and selected apply, like the tag module does for Tags
 * without windows and the viewed Tags. Clay draws borders over the
 * padding instead of around it, so padding has to include the border
 * width. */

typedef struct {
	// char **elements;
	// size_t num_elements;
	Style style;
	char *content;
} ModuleChildren;

typedef struct Module {
	ModLayoutFunc layoutfunc;
	ModuleChildren children;
	double updateinterval;
	Style style;
} Module;

typedef struct {
	Module *modules;
	Style style;
} ModuleGroup;

typedef struct {
	Dimensions dimensions;
	/* drawbar() sets the sizing: the monitor's width, and the content's height */
	Clay_ElementDeclaration style;
} Bar;

enum ModuleGroupType {
	ModuleGroupLeft,
	ModuleGroupCenter,
	ModuleGroupRight,
};

extern ModuleGroup modulegroups[3];
extern Bar bar;
extern pthread_t bar_intervalthreads;
extern bool bar_updating;

void drawbar(Monitor *m, ModuleId* updatespecificmodcontent);
void drawbars(void);
void setupbarmodules();
void bar_initupdateintervals();

void modulelayout_tags(struct Module *mod, Monitor *m, bool updatecontent);
void modulelayout_rect(struct Module *mod, Monitor *m, bool updatecontent);
void modulelayout_wintitle(struct Module *mod, Monitor *m, bool updatecontent);
void modulelayout_time(struct Module *mod, Monitor *m, bool updatecontent);
void modulelayout_systray(struct Module *mod, Monitor *m, bool updatecontent);