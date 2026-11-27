#pragma once

#include <stdbool.h>

#include "../../core.h"
#include "../../draw.h"
#include "../../../lib/clay/clay.h"

struct Module;
/* Declares the module's Clay elements for the frame being laid out.
 * A module with nothing to show declares nothing, which also removes
 * its gap from the group. */
typedef void (*ModLayoutFunc)(struct Module *mod, Monitor *m, bool updatecontent);
/* Handles a click on the module: child is the index of the clicked child,
 * or -1 for the module's own box around its children. button and state
 * are the X event's button and modifiers. */
typedef void (*ModClickFunc)(struct Module *mod, Monitor *m, int child, unsigned int button, unsigned int state);

/* Groups, modules and children are styled with a Style (see draw.h):
 * a Clay layout and a base state style, with state styles that go over
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
	ModClickFunc clickfunc; /* NULL if it takes no clicks */
	ModuleChildren children;
	double updateinterval;
	Style style;
} Module;

typedef struct {
	Module *modules;
	Style style;
} ModuleGroup;

enum ModuleGroupType {
	ModuleGroupLeft,
	ModuleGroupCenter,
	ModuleGroupRight,
	ModuleGroupLast,
};

void ui_bar_modules_setup(ModuleGroup *groups);
