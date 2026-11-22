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

pthread_t bar_intervalthreads;
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
					CLAY(CLAY_IDI("group", g), group->style) {
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

void
drawbar(Monitor *m, ModuleId* updatespecificmodcontent)
{
	Clay_RenderCommandArray cmds;
	Clay_ElementData el;
	int height;

	if (!m->showbar || (bar_updating && updatespecificmodcontent == NULL))
		return;

	bar_updating = true;

	/* the bar sizes its own height; the layout height only bounds culling */
	Clay_SetLayoutDimensions((Clay_Dimensions){ m->ww, m->mh });
	Clay_BeginLayout();
	layoutbar(m, updatespecificmodcontent);
	cmds = Clay_EndLayout(0);

	/* Grow the bar to fit its content. It never shrinks: the height is
	 * shared by all monitors, and shrinking would let monitors that need
	 * different heights keep resizing each other. */
	el = Clay_GetElementData(CLAY_ID("bar"));
	height = ceilf(el.boundingBox.height);
	if (height > bar.dimensions.height) {
		bar.dimensions.height = height;
		updatebarpos(m);
		arrange(m);
	}
	resizebarwin(m);

	el = Clay_GetElementData(CLAY_ID("systray"));
	if (el.found)
		systraypos = (Vec2){ roundf(el.boundingBox.x), roundf(el.boundingBox.y) };

	drw_clear(drw);
	clay_layout_render(drw, cmds);
	drw_map(drw, m->barwin, 0, 0, m->ww, bar.dimensions.height);

	bar_updating = false;
}

void
drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m, NULL);
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
				.backgroundColor = boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
				.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
			},
		};
	}

	Module module_tags = (Module){
		.layoutfunc = modulelayout_tags,
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
				.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
			},
			.states = {
				[TagEmpty] = {
					.backgroundColor = {0.1, 0.1, 0.1, 0.0},
					.border = { .color = {0.2, 0.2, 0.2, 0.0}, .width = CLAY_BORDER_OUTSIDE(2) },
					.textColor = {0.3686, 0.3686, 0.3686, 1.0},
				},
				[TagOccupied] = {
					.backgroundColor = {0.1, 0.1, 0.1, 0.0},
					.border = { .color = {0.2, 0.2, 0.2, 0.0}, .width = CLAY_BORDER_OUTSIDE(2) },
					.textColor = {1.0, 1.0, 1.0, 1.0},
				},
				[TagSelected] = {
					.backgroundColor = {0.2, 0.2, 0.2, 1.0},
					.border = { .color = {0.2, 0.2, 0.2, 0.0}, .width = CLAY_BORDER_OUTSIDE(2) },
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
			.backgroundColor = boxbg,
			.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
			.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
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
				.backgroundColor = boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
				.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(1) },
			},
		},
		.style = {
			.layout = {
				.padding = {0, 0, 0, 0},
				.childGap = 2,
				.childAlignment = { .y = CLAY_ALIGN_Y_TOP },
			},
			.backgroundColor = boxbg,
			.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
			.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
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
				.backgroundColor = boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
				.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(2) },
			},
		},
		.style = {
			.layout = {
				.padding = {0, 0, 0, 0},
				.childGap = 2,
				.childAlignment = { .y = CLAY_ALIGN_Y_TOP },
			},
			.backgroundColor = boxbg,
			.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
			.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
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
				.backgroundColor = boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
				.border = { .color = {0.2, 0.2, 0.2, 0.0}, .width = CLAY_BORDER_OUTSIDE(2) },
			},
		},
		.style = {
			.layout = {
				.padding = {0, 0, 0, 0},
				.childGap = 2,
				.childAlignment = { .y = CLAY_ALIGN_Y_TOP },
			},
			.backgroundColor = boxbg,
			.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
			.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
		}
	};
	snprintf(module_time.children.content, 100, "0");

	Module module_systray = (Module){
		.layoutfunc = modulelayout_systray,
		.updateinterval = 0,
		.children = {
			// .elements = boxtest,
			// .num_elements = LENGTH(boxtest),
			.style = {
				/* the width follows the tray's icons, see modulelayout_systray() */
				.layout = { .sizing = { .height = CLAY_SIZING_FIXED(16) } },
				.backgroundColor = boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
				.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(1) },
			},
		},
		.style = {
			.layout = {
				.padding = {0, 0, 0, 0},
				.childGap = 2,
				.childAlignment = { .y = CLAY_ALIGN_Y_TOP },
			},
			.backgroundColor = boxbg,
			.cornerRadius = CLAY_CORNER_RADIUS(boxradius),
			.border = { .color = boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
		}
	};

	dynarray_push(modulegroups[ModuleGroupLeft].modules, module_tags);
	dynarray_push(modulegroups[ModuleGroupCenter].modules, module_wintitle);
	dynarray_push(modulegroups[ModuleGroupRight].modules, module_systray);
	dynarray_push(modulegroups[ModuleGroupRight].modules, module_time);
}

void* bar_moduleroutine(void* arg) {
	ModuleId mID = *(ModuleId*)arg;
	free(arg);

	while (1) {
		while (bar_updating)
			usleep(10000);
		drawbar(selmon, &mID);

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

	for (size_t i = 0; i < dynarray_length(validintervals); ++i) {
		ModuleId* modinfo = malloc(sizeof(ModuleId));

		*modinfo = validintervals[i];
		if (pthread_create(&bar_intervalthreads, NULL, bar_moduleroutine, modinfo) != 0) {
			perror("Failed to create module interval thread");
			free(modinfo);
		}
	}

	dynarray_destroy(validintervals);
}

void
modulelayout_rect(struct Module *mod, Monitor *m, bool updatecontent)
{
	CLAY_AUTO_ID(mod->style) {
		CLAY_AUTO_ID(mod->children.style) {}
	}
}

void
modulelayout_tags(struct Module *mod, Monitor *m, bool updatecontent)
{
	unsigned int occ = 0;
	Client *c;

	for (c = m->clients; c; c = c->next)
		occ |= c->tags;

	CLAY_AUTO_ID(mod->style) {
		for (int i = 0; i < LENGTH(tags); i++) {
			const StateStyle *state = &mod->children.states[
				m->tagset[m->seltags] & 1 << i ? TagSelected
				: occ & 1 << i ? TagOccupied
				: TagEmpty];
			Clay_ElementDeclaration tagbox = mod->children.style;
			Clay_TextElementConfig label = bartext;

			tagbox.backgroundColor = state->backgroundColor;
			tagbox.border = state->border;
			label.textColor = state->textColor;

			/* named so a bar click can be matched to its tag */
			CLAY(CLAY_IDI("tag", i), tagbox) {
				CLAY_TEXT(clay_string(tags[i]), label);
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

	CLAY_AUTO_ID(mod->style) {
		CLAY_AUTO_ID(mod->children.style) {
			CLAY_TEXT(clay_string(mod->children.content), bartext);
		}
	}
}

void
modulelayout_wintitle(struct Module *mod, Monitor *m, bool updatecontent)
{
	if (m->sel == NULL || m->sel->name[0] == '\0')
		return;

	CLAY_AUTO_ID(mod->style) {
		CLAY_AUTO_ID(mod->children.style) {
			CLAY_TEXT(clay_string(m->sel->name), bartext);
		}
	}
}

void
modulelayout_systray(struct Module *mod, Monitor *m, bool updatecontent)
{
	Clay_ElementDeclaration tray = mod->children.style;
	unsigned int width = getsystraywidth();

	/* getsystraywidth() returns 1 when the tray has no icons */
	if (width <= 1)
		return;

	tray.layout.sizing.width = CLAY_SIZING_FIXED(width);
	CLAY_AUTO_ID(mod->style) {
		/* drawbar() reads this element's position into systraypos */
		CLAY(CLAY_ID("systray"), tray) {}
	}
}
