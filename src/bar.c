#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "bar.h"
#include "config.h"
#include "util.h"
#include "draw.h"
#include "errors.h"
#include "../lib/dynarray/dynarray.h"
#include "../lib/clay/clay.h"

static const Clay_TextElementConfig bartext = {
	.textColor = {1.0, 1.0, 1.0, 1.0},
	.fontId = 0,
	.wrapMode = CLAY_TEXT_WRAP_NONE,
};
static const float boxradius = 6;
static const Clay_Color boxbg = {0.1, 0.1, 0.1, 0.4};
static const Clay_Color boxbd = {0.2, 0.2, 0.2, 1.0};

static pthread_t *intervalthreads;
bool bar_updating;

ModuleGroup modulegroups[3];
Bar bar = {
	.dimensions = {0,1},
	.style = {
		.layout = {
			.padding = {2, 2, 2, 2},
			.childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
		},
		.backgroundColor = {0.1, 0.1, 0.1, 1.0},
	},
};

/* makes the open element take clicks for mod, if mod handles any, with
 * child as the clicked child's index (-1 for the module's own box) */
static void
clickable(Module *mod, int child)
{
	if (mod->clickfunc)
		clay_clickable(mod, child);
}

static void
layoutbar(Monitor *m, ModuleId *update)
{
	static const Clay_LayoutAlignmentX groupalign[] = {
		[ModuleGroupLeft] = CLAY_ALIGN_X_LEFT,
		[ModuleGroupCenter] = CLAY_ALIGN_X_CENTER,
		[ModuleGroupRight] = CLAY_ALIGN_X_RIGHT,
	};
	Clay_ElementDeclaration root = bar.style;

	root.layout.sizing = (Clay_Sizing){
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(bar.dimensions.height),
	};

	CLAY(CLAY_ID("bar"), root) {
		for (int g = 0; g < LENGTH(modulegroups); g++) {
			ModuleGroup *group = &modulegroups[g];

			CLAY(CLAY_IDI("column", g), {
				.layout = {
					.sizing = { .width = CLAY_SIZING_GROW(0) },
					.childAlignment = { .x = groupalign[g] },
				},
			}) {
				if (dynarray_length(group->modules) > 0) {
					CLAY(CLAY_IDI("group", g), clay_style(&group->style, 0)) {
						for (int i = 0; i < dynarray_length(group->modules); i++) {
							bool updatecontent = update != NULL
								&& update->ModuleGroupIndex == g
								&& update->ModuleIndex == i;

							group->modules[i].layoutfunc(&group->modules[i], m, updatecontent);
						}
					}
				}
			}
		}
	}
}

static int
barcontentheight(void)
{
	float h = 0;

	for (int g = 0; g < LENGTH(modulegroups); g++)
		h = MAX(h, Clay_GetElementData(CLAY_IDI("column", g)).boundingBox.height);
	return ceilf(h + bar.style.layout.padding.top + bar.style.layout.padding.bottom);
}

void
drawbar(Monitor *m, ModuleId* updatespecificmodcontent)
{
	Clay_RenderCommandArray cmds;
	Clay_ElementData el;
	Monitor *mon;
	int height = 0, resized;

	if (!m->showbar || (bar_updating && updatespecificmodcontent == NULL))
		return;

	bar_updating = true;

	/* the bar sizes its own height; the layout height only bounds culling */
	clay_layout_begin(&m->barpointer, (Clay_Dimensions){ m->ww, m->mh });
	layoutbar(m, updatespecificmodcontent);
	cmds = clay_layout_end();

	m->barh = barcontentheight();
	for (mon = mons; mon; mon = mon->next)
		if (mon->showbar)
			height = MAX(height, mon->barh);
	if ((resized = height != bar.dimensions.height)) {
		bar.dimensions.height = height;
		clay_layout_begin(&m->barpointer, (Clay_Dimensions){ m->ww, m->mh });
		layoutbar(m, NULL);
		cmds = clay_layout_end();
		for (mon = mons; mon; mon = mon->next)
			updatebarpos(mon);
		arrange(NULL);
	}
	resizebarwin(m);

	el = Clay_GetElementData(CLAY_ID("systray"));
	if (el.found)
		systraypos = (Vec2){ roundf(el.boundingBox.x), roundf(el.boundingBox.y) };

	drw_clear(drw);
	clay_layout_render(drw, cmds);
	drw_map(drw, m->barwin, 0, 0, m->ww, bar.dimensions.height);

	bar_updating = false;

	if (resized) {
		for (mon = mons; mon; mon = mon->next)
			if (mon != m)
				drawbar(mon, NULL);
		updatesystray(0);
	}
}

void
drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m, NULL);
}

bool
bar_click(Monitor *m, int x, int y, unsigned int button, unsigned int state)
{
	void *mod;
	int child;

	if (!clay_pointer_click(&m->barpointer, x, y, &mod, &child))
		return false;
	((Module *)mod)->clickfunc(mod, m, child, button, state);
	return true;
}

void
setupbarmodules()
{
	for (int i = 0; i < LENGTH(modulegroups); i++) {
		modulegroups[i] = (ModuleGroup){
			.modules = dynarray_create(Module),
			.style = {
				.layout = {
					.padding = {0, 0, 0, 0},
					.childGap = 4,
					.childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
				},
				.base = {
					.backgroundColor = boxbg,
					.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
					.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
				},
			},
		};
	}

	Module module_tags = (Module){
		.layoutfunc = modulelayout_tags,
		.clickfunc = moduleclick_tags,
		.updateinterval = 0,
		.children = {
			// .elements = boxtest,
			// .num_elements = LENGTH(boxtest),
			.style = {
				.layout = {
					/* 6px and 1px of space inside the 2px border */
					.padding = { .left = 8, .right = 8, .top = 3, .bottom = 3 },
					.childGap = 0,
					.childAlignment = { .y = CLAY_ALIGN_Y_TOP },
				},
				.base = {
					.textColor = {1.0, 0.0, 0.0, 1.0},
					.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
				},
				.subdued = {
					.backgroundColor = {0.1, 0.1, 0.1, 0.0},
					.border = { .color = {0.2, 0.2, 0.2, 0.0}, .width = CLAY_BORDER_OUTSIDE(2) },
					.textColor = {0.3686, 0.3686, 0.3686, 1.0},
				},
				.selected = {
					.backgroundColor = {0.2, 0.2, 0.2, 1.0},
					.border = { .color = {0.2, 0.2, 0.2, 0.0}, .width = CLAY_BORDER_OUTSIDE(2) },
					.textColor = {1.0, 1.0, 1.0, 1.0},
				},
				.hover = {
					.border = { .color = {0.2, 0.2, 0.2, 1.0}, .width = CLAY_BORDER_OUTSIDE(2) },
					.textColor = {1.0, 1.0, 1.0, 1.0},
				},
			},
		},
		.style = {
			.layout = {
				.padding = {0, 0, 0, 0},
				.childGap = 2,
				.childAlignment = { .y = CLAY_ALIGN_Y_TOP },
			},
			.base = {
				.backgroundColor = boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
				.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
			},
		}
	};

	Module module_rect = (Module){
		.layoutfunc = modulelayout_rect,
		.updateinterval = 0,
		.children = {
			// .elements = boxtest,
			// .num_elements = LENGTH(boxtest),
			.style = {
				.layout = { .sizing = { CLAY_SIZING_FIXED(16), CLAY_SIZING_FIXED(16) } },
				.base = {
					.backgroundColor = boxbg,
					.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
					.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(1) },
				},
			},
		},
		.style = {
			.layout = {
				.padding = {0, 0, 0, 0},
				.childGap = 2,
				.childAlignment = { .y = CLAY_ALIGN_Y_TOP },
			},
			.base = {
				.backgroundColor = boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
				.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
			},
		}
	};

	Module module_wintitle = (Module){
		.layoutfunc = modulelayout_wintitle,
		.updateinterval = 0,
		.children = {
			// .elements = boxtest,
			// .num_elements = LENGTH(boxtest),
			.style = {
				.layout = {
					.padding = { .left = 8, .right = 8, .top = 3, .bottom = 3 },
					.childGap = 0,
					.childAlignment = { .y = CLAY_ALIGN_Y_TOP },
				},
				.base = {
					.backgroundColor = boxbg,
					.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
					.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(2) },
				},
			},
		},
		.style = {
			.layout = {
				.padding = {0, 0, 0, 0},
				.childGap = 2,
				.childAlignment = { .y = CLAY_ALIGN_Y_TOP },
			},
			.base = {
				.backgroundColor = boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
				.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
			},
		}
	};

	Module module_time = (Module){
		.layoutfunc = modulelayout_time,
		.updateinterval = 1,
		.children = {
			.content = (char *)malloc(sizeof(char) * 100),
			// .elements = boxtest,
			// .num_elements = LENGTH(boxtest),
			.style = {
				.layout = {
					.padding = { .left = 8, .right = 8, .top = 3, .bottom = 3 },
					.childGap = 0,
					.childAlignment = { .y = CLAY_ALIGN_Y_TOP },
				},
				.base = {
					.backgroundColor = boxbg,
					.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
					.border = { .color = {0.2, 0.2, 0.2, 0.0}, .width = CLAY_BORDER_OUTSIDE(2) },
				},
				.hover = {
					.border = { .color = {0.2, 0.2, 0.2, 1.0}, .width = CLAY_BORDER_OUTSIDE(2) },
					.textColor = {1.0, 1.0, 1.0, 1.0},
				},
			},
		},
		.style = {
			.layout = {
				.padding = {0, 0, 0, 0},
				.childGap = 2,
				.childAlignment = { .y = CLAY_ALIGN_Y_TOP },
			},
			.base = {
				.backgroundColor = boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
				.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
			},
		}
	};
	snprintf(module_time.children.content, 100, "0");

	Module module_errors = (Module){
		.layoutfunc = modulelayout_errors,
		.clickfunc = moduleclick_errors,
		.updateinterval = 0,
		.children = {
			.content = (char *)malloc(sizeof(char) * 100),
			.style = {
				.layout = {
					.padding = { .left = 8, .right = 8, .top = 3, .bottom = 3 },
					.childGap = 0,
					.childAlignment = { .y = CLAY_ALIGN_Y_TOP },
				},
				.base = {
					.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
					.backgroundColor = {1.0, 0.251, 0.439, 0.1},
					.textColor = {1.0, 0.251, 0.439, 1.0},
				},
				.selected = {
					.backgroundColor = {1.0, 0.251, 0.439, 1.0},
					.textColor = {0.0, 0.0, 0.0, 0.4},
				},
				.hover = {
					.backgroundColor = {1.0, 0.251, 0.439, 1.0},
					.textColor = {0.0, 0.0, 0.0, 0.4},
				},
			},
		},
		.style = {
			.layout = {
				.padding = {0, 0, 0, 0},
				.childGap = 2,
				.childAlignment = { .y = CLAY_ALIGN_Y_TOP },
			},
			.base = {
				.backgroundColor = boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
				.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
			},
		}
	};

	Module module_systray = (Module){
		.layoutfunc = modulelayout_systray,
		.updateinterval = 0,
		.children = {
			// .elements = boxtest,
			// .num_elements = LENGTH(boxtest),
			.style = {
				/* the width follows the tray's icons, see modulelayout_systray() */
				.layout = { .sizing = { .height = CLAY_SIZING_FIXED(16) } },
				.base = {
					.backgroundColor = boxbg,
					.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
					.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(1) },
				},
			},
		},
		.style = {
			.layout = {
				.padding = {0, 0, 0, 0},
				.childGap = 2,
				.childAlignment = { .y = CLAY_ALIGN_Y_TOP },
			},
			.base = {
				.backgroundColor = boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
				.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
			},
		}
	};

	dynarray_push(modulegroups[ModuleGroupLeft].modules, module_tags);
	dynarray_push(modulegroups[ModuleGroupCenter].modules, module_wintitle);
	dynarray_push(modulegroups[ModuleGroupRight].modules, module_errors);
	dynarray_push(modulegroups[ModuleGroupRight].modules, module_systray);
	dynarray_push(modulegroups[ModuleGroupRight].modules, module_time);
}

void* bar_moduleroutine(void* arg) {
	ModuleId mID = *(ModuleId*)arg;
	free(arg);

	while (1) {
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		while (bar_updating)
			usleep(10000);
		drawbar(selmon, &mID);
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

		usleep(modulegroups[mID.ModuleGroupIndex].modules[mID.ModuleIndex].updateinterval * 1000000);
	}

	return NULL;
}

void
bar_initupdateintervals()
{
	ModuleId *validintervals = dynarray_create(ModuleId);

	for (int modulegroup = 0; modulegroup < LENGTH(modulegroups); modulegroup++) {
		for (int i = 0; i < dynarray_length(modulegroups[modulegroup].modules); i++) {
			if (modulegroups[modulegroup].modules[i].updateinterval > 0) {
				ModuleId mId = (ModuleId){.ModuleGroupIndex = modulegroup, .ModuleIndex = i};
				dynarray_push(validintervals, mId);
			}
		}
	}

	intervalthreads = dynarray_create(pthread_t);
	for (size_t i = 0; i < dynarray_length(validintervals); ++i) {
		ModuleId* modinfo = malloc(sizeof(ModuleId));
		pthread_t thread;

		*modinfo = validintervals[i];
		if (pthread_create(&thread, NULL, bar_moduleroutine, modinfo) != 0) {
			perror("Failed to create module interval thread");
			free(modinfo);
		} else
			dynarray_push(intervalthreads, thread);
	}

	dynarray_destroy(validintervals);
}

void
bar_stopupdateintervals(void)
{
	if (!intervalthreads)
		return;
	for (size_t i = 0; i < dynarray_length(intervalthreads); i++) {
		pthread_cancel(intervalthreads[i]);
		pthread_join(intervalthreads[i], NULL);
	}
	dynarray_destroy(intervalthreads);
	intervalthreads = NULL;
}

void
modulelayout_rect(struct Module *mod, Monitor *m, bool updatecontent)
{
	CLAY_AUTO_ID(clay_style(&mod->style, 0)) {
		CLAY_AUTO_ID(clay_style(&mod->children.style, 0)) {}
	}
}

void
modulelayout_tags(struct Module *mod, Monitor *m, bool updatecontent)
{
	unsigned int occ = 0;
	Client *c;

	for (c = m->clients; c; c = c->next)
		occ |= c->tags;

	CLAY_AUTO_ID(clay_style(&mod->style, 0)) {
		clickable(mod, -1);
		for (int i = 0; i < LENGTH(tags); i++) {
			int states = 0;

			if (!(occ & 1 << i))
				states |= ClaySubdued;
			if (m->tagset[m->seltags] & 1 << i)
				states |= ClaySelected;

			CLAY(CLAY_IDI("tag", i), clay_style(&mod->children.style, states)) {
				clickable(mod, i);
				CLAY_TEXT(clay_string(tags[i]), clay_text_style(&mod->children.style, bartext, states));
			}
		}
	}
}

void
modulelayout_time(struct Module *mod, Monitor *m, bool updatecontent)
{
	if (updatecontent) {
		time_t raw_time;
		struct tm * time_info;

		time(&raw_time);
		time_info = localtime(&raw_time);

		char buffer[6];
		strftime(buffer, sizeof(buffer), "%H:%M", time_info);

		snprintf(mod->children.content, 100, "%s", buffer);
	}

	CLAY_AUTO_ID(clay_style(&mod->style, 0)) {
		CLAY_AUTO_ID(clay_style(&mod->children.style, 0)) {
			CLAY_TEXT(clay_string(mod->children.content), clay_text_style(&mod->children.style, bartext, 0));
		}
	}
}

void
modulelayout_wintitle(struct Module *mod, Monitor *m, bool updatecontent)
{
	if (m->sel == NULL || m->sel->name[0] == '\0')
		return;

	CLAY_AUTO_ID(clay_style(&mod->style, 0)) {
		CLAY_AUTO_ID(clay_style(&mod->children.style, 0)) {
			CLAY_TEXT(clay_string(m->sel->name), clay_text_style(&mod->children.style, bartext, 0));
		}
	}
}

void
modulelayout_systray(struct Module *mod, Monitor *m, bool updatecontent)
{
	Style tray = mod->children.style;
	unsigned int width = getsystraywidth();

	/* getsystraywidth() returns 1 when the tray has no icons */
	if (width <= 1)
		return;

	tray.layout.sizing.width = CLAY_SIZING_FIXED(width);
	CLAY_AUTO_ID(clay_style(&mod->style, 0)) {
		/* drawbar() reads this element's position into systraypos */
		CLAY(CLAY_ID("systray"), clay_style(&tray, 0)) {}
	}
}

void
modulelayout_errors(struct Module *mod, Monitor *m, bool updatecontent)
{
	int n = ui_errors_count();
	int states = ui_errors_visible() ? ClaySelected : 0;

	if (n == 0)
		return;
	
	snprintf(mod->children.content, 100, "%d error%s", n, n == 1 ? "" : "s");
	CLAY_AUTO_ID(clay_style(&mod->style, 0)) {
		clickable(mod, -1);
		CLAY_AUTO_ID(clay_style(&mod->children.style, states)) {
			CLAY_TEXT(clay_string(mod->children.content), clay_text_style(&mod->children.style, bartext, states));
		}
	}
}

void
moduleclick_tags(struct Module *mod, Monitor *m, int child, unsigned int button, unsigned int state)
{
	if (child >= 0)
		runbuttons(ClkTagBar, &(Arg){ .ui = 1 << child }, button, state);
}

void
moduleclick_errors(struct Module *mod, Monitor *m, int child, unsigned int button, unsigned int state)
{
	if (button == Button1)
		ui_errors_toggle(m);
}
