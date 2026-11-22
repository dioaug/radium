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

/* Styles are Clay element declarations, used as-is for the element they
 * style. Clay draws borders over the padding instead of around it, so
 * padding has to include the border width. */

/* The part of a child's style that follows its state, like a tag being
 * selected. Fields are named after the Clay ones they set. */
typedef struct {
	Clay_Color backgroundColor;
	Clay_BorderElementConfig border;
	Clay_Color textColor;
} StateStyle;

/* the states a tag can be in, indexing the tag module's children.states */
enum { TagEmpty, TagOccupied, TagSelected, TagStateLast };

typedef struct {
	// char **elements;
	// size_t num_elements;
	Clay_ElementDeclaration style;
	/* for children with states, like the tags: style's colors and
	 * border in each state */
	StateStyle states[TagStateLast];
	char *content;
} ModuleChildren;

typedef struct Module {
	ModLayoutFunc layoutfunc;
	ModuleChildren children;
	double updateinterval;
	Clay_ElementDeclaration style;
} Module;

typedef struct {
	Module *modules;
	Clay_ElementDeclaration style;
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