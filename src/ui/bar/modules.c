#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "modules.h"
#include "../../config.h"
#include "../../util.h"
#include "../../draw.h"
#include "../errors.h"
#include "../../../lib/dynarray/dynarray.h"
#include "../../../lib/clay/clay.h"

static const Clay_TextElementConfig ui_bar_modules_text = {
	.textColor = {1.0, 1.0, 1.0, 1.0},
	.fontId = 0,
	.wrapMode = CLAY_TEXT_WRAP_NONE,
};
static const float ui_bar_modules_boxradius = 6;
static const Clay_Color ui_bar_modules_boxbg = {0.1, 0.1, 0.1, 0.4};
static const Clay_Color ui_bar_modules_boxbd = {0.2, 0.2, 0.2, 1.0};

/* makes the open element take clicks for mod, if mod handles any, with
 * child as the clicked child's index (-1 for the module's own box) */
static void
ui_bar_modules_clickable(Module *mod, int child)
{
	if (mod->clickfunc)
		clay_clickable(mod, child);
}

static void
ui_bar_modules_layoutrect(struct Module *mod, Monitor *m, bool updatecontent)
{
	CLAY_AUTO_ID(clay_style(&mod->style, 0)) {
		CLAY_AUTO_ID(clay_style(&mod->children.style, 0)) {}
	}
}

static void
ui_bar_modules_layouttags(struct Module *mod, Monitor *m, bool updatecontent)
{
	unsigned int occ = 0;
	Client *c;

	for (c = m->clients; c; c = c->next)
		occ |= c->tags;

	CLAY_AUTO_ID(clay_style(&mod->style, 0)) {
		ui_bar_modules_clickable(mod, -1);
		for (int i = 0; i < LENGTH(tags); i++) {
			int states = 0;

			if (!(occ & 1 << i))
				states |= ClaySubdued;
			if (m->tagset[m->seltags] & 1 << i)
				states |= ClaySelected;

			CLAY(CLAY_IDI("tag", i), clay_style(&mod->children.style, states)) {
				ui_bar_modules_clickable(mod, i);
				CLAY_TEXT(clay_string(tags[i]), clay_text_style(&mod->children.style, ui_bar_modules_text, states));
			}
		}
	}
}

static void
ui_bar_modules_layouttime(struct Module *mod, Monitor *m, bool updatecontent)
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
			CLAY_TEXT(clay_string(mod->children.content), clay_text_style(&mod->children.style, ui_bar_modules_text, 0));
		}
	}
}

static void
ui_bar_modules_layoutwintitle(struct Module *mod, Monitor *m, bool updatecontent)
{
	if (m->sel == NULL || m->sel->name[0] == '\0')
		return;

	CLAY_AUTO_ID(clay_style(&mod->style, 0)) {
		CLAY_AUTO_ID(clay_style(&mod->children.style, 0)) {
			CLAY_TEXT(clay_string(m->sel->name), clay_text_style(&mod->children.style, ui_bar_modules_text, 0));
		}
	}
}

static void
ui_bar_modules_layoutsystray(struct Module *mod, Monitor *m, bool updatecontent)
{
	Style tray = mod->children.style;
	unsigned int width = getsystraywidth();

	/* getsystraywidth() returns 1 when the tray has no icons */
	if (width <= 1)
		return;

	tray.layout.sizing.width = CLAY_SIZING_FIXED(width);
	CLAY_AUTO_ID(clay_style(&mod->style, 0)) {
		/* ui_bar_draw() reads this element's position into systraypos */
		CLAY(CLAY_ID("systray"), clay_style(&tray, 0)) {}
	}
}

static void
ui_bar_modules_layouterrors(struct Module *mod, Monitor *m, bool updatecontent)
{
	int n = ui_errors_count();
	int states = ui_errors_visible() ? ClaySelected : 0;

	if (n == 0)
		return;

	snprintf(mod->children.content, 100, "%d error%s", n, n == 1 ? "" : "s");
	CLAY_AUTO_ID(clay_style(&mod->style, 0)) {
		ui_bar_modules_clickable(mod, -1);
		CLAY_AUTO_ID(clay_style(&mod->children.style, states)) {
			CLAY_TEXT(clay_string(mod->children.content), clay_text_style(&mod->children.style, ui_bar_modules_text, states));
		}
	}
}

static void
ui_bar_modules_clicktags(struct Module *mod, Monitor *m, int child, unsigned int button, unsigned int state)
{
	if (child >= 0)
		runbuttons(ClkTagBar, &(Arg){ .ui = 1 << child }, button, state);
}

static void
ui_bar_modules_clickerrors(struct Module *mod, Monitor *m, int child, unsigned int button, unsigned int state)
{
	if (button == Button1)
		ui_errors_toggle(m);
}

void
ui_bar_modules_setup(ModuleGroup *groups)
{
	for (int i = 0; i < ModuleGroupLast; i++) {
		groups[i] = (ModuleGroup){
			.modules = dynarray_create(Module),
			.style = {
				.layout = {
					.padding = {0, 0, 0, 0},
					.childGap = 4,
					.childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
				},
				.base = {
					.backgroundColor = ui_bar_modules_boxbg,
					.cornerRadius = CLAY_CORNER_RADIUS(ui_bar_modules_boxradius),
					.border = { .color = ui_bar_modules_boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
				},
			},
		};
	}

	Module module_tags = (Module){
		.layoutfunc = ui_bar_modules_layouttags,
		.clickfunc = ui_bar_modules_clicktags,
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
					.cornerRadius = CLAY_CORNER_RADIUS(ui_bar_modules_boxradius),
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
				.backgroundColor = ui_bar_modules_boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(ui_bar_modules_boxradius),
				.border = { .color = ui_bar_modules_boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
			},
		}
	};

	Module module_rect = (Module){
		.layoutfunc = ui_bar_modules_layoutrect,
		.updateinterval = 0,
		.children = {
			// .elements = boxtest,
			// .num_elements = LENGTH(boxtest),
			.style = {
				.layout = { .sizing = { CLAY_SIZING_FIXED(16), CLAY_SIZING_FIXED(16) } },
				.base = {
					.backgroundColor = ui_bar_modules_boxbg,
					.cornerRadius = CLAY_CORNER_RADIUS(ui_bar_modules_boxradius),
					.border = { .color = ui_bar_modules_boxbd, .width = CLAY_BORDER_OUTSIDE(1) },
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
				.backgroundColor = ui_bar_modules_boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(ui_bar_modules_boxradius),
				.border = { .color = ui_bar_modules_boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
			},
		}
	};

	Module module_wintitle = (Module){
		.layoutfunc = ui_bar_modules_layoutwintitle,
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
					.backgroundColor = ui_bar_modules_boxbg,
					.cornerRadius = CLAY_CORNER_RADIUS(ui_bar_modules_boxradius),
					.border = { .color = ui_bar_modules_boxbd, .width = CLAY_BORDER_OUTSIDE(2) },
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
				.backgroundColor = ui_bar_modules_boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(ui_bar_modules_boxradius),
				.border = { .color = ui_bar_modules_boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
			},
		}
	};

	Module module_time = (Module){
		.layoutfunc = ui_bar_modules_layouttime,
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
					.backgroundColor = ui_bar_modules_boxbg,
					.cornerRadius = CLAY_CORNER_RADIUS(ui_bar_modules_boxradius),
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
				.backgroundColor = ui_bar_modules_boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(ui_bar_modules_boxradius),
				.border = { .color = ui_bar_modules_boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
			},
		}
	};
	snprintf(module_time.children.content, 100, "0");

	Module module_errors = (Module){
		.layoutfunc = ui_bar_modules_layouterrors,
		.clickfunc = ui_bar_modules_clickerrors,
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
					.cornerRadius = CLAY_CORNER_RADIUS(ui_bar_modules_boxradius),
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
				.backgroundColor = ui_bar_modules_boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(ui_bar_modules_boxradius),
				.border = { .color = ui_bar_modules_boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
			},
		}
	};

	Module module_systray = (Module){
		.layoutfunc = ui_bar_modules_layoutsystray,
		.updateinterval = 0,
		.children = {
			// .elements = boxtest,
			// .num_elements = LENGTH(boxtest),
			.style = {
				/* the width follows the tray's icons, see ui_bar_modules_layoutsystray() */
				.layout = { .sizing = { .height = CLAY_SIZING_FIXED(16) } },
				.base = {
					.backgroundColor = ui_bar_modules_boxbg,
					.cornerRadius = CLAY_CORNER_RADIUS(ui_bar_modules_boxradius),
					.border = { .color = ui_bar_modules_boxbd, .width = CLAY_BORDER_OUTSIDE(1) },
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
				.backgroundColor = ui_bar_modules_boxbg,
				.cornerRadius = CLAY_CORNER_RADIUS(ui_bar_modules_boxradius),
				.border = { .color = ui_bar_modules_boxbd, .width = CLAY_BORDER_OUTSIDE(0) },
			},
		}
	};

	dynarray_push(groups[ModuleGroupLeft].modules, module_tags);
	dynarray_push(groups[ModuleGroupCenter].modules, module_wintitle);
	dynarray_push(groups[ModuleGroupRight].modules, module_errors);
	dynarray_push(groups[ModuleGroupRight].modules, module_systray);
	dynarray_push(groups[ModuleGroupRight].modules, module_time);
}
