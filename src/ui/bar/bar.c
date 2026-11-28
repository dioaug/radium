#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "bar.h"
#include "systray.h"
#include "../../util.h"
#include "../../draw.h"
#include "../../../lib/dynarray/dynarray.h"
#include "../../../lib/clay/clay.h"

static ModuleGroup ui_bar_modulegroups[ModuleGroupLast];
static struct {
	Window win;
	void *mod;
	int child;
	unsigned int button, state;
} ui_bar_press;
static bool ui_bar_updating;

Bar ui_bar = {
	.dimensions = {0,1},
	.style = {
		.layout = {
			.padding = {2, 2, 2, 2},
			.childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
		},
		.backgroundColor = {0.1, 0.1, 0.1, 1.0},
	},
};

static Monitor *
ui_bar_wintomon(Window w)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		if (w == m->barwin)
			return m;
	return NULL;
}

Monitor *
ui_bar_modulemon(int monitormode)
{
	switch (monitormode) {
	case ModuleMonitorModeSelected:
		return selmon;
	case ModuleMonitorModeMain:
		return mons;
	}
	return NULL;
}

static void
ui_bar_layout(Monitor *m)
{
	static const Clay_LayoutAlignmentX groupalign[] = {
		[ModuleGroupLeft] = CLAY_ALIGN_X_LEFT,
		[ModuleGroupCenter] = CLAY_ALIGN_X_CENTER,
		[ModuleGroupRight] = CLAY_ALIGN_X_RIGHT,
	};
	Clay_ElementDeclaration root = ui_bar.style;

	root.layout.sizing = (Clay_Sizing){
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(ui_bar.dimensions.height),
	};

	CLAY(CLAY_ID("bar"), root) {
		for (int g = 0; g < LENGTH(ui_bar_modulegroups); g++) {
			ModuleGroup *group = &ui_bar_modulegroups[g];

			CLAY(CLAY_IDI("column", g), {
				.layout = {
					.sizing = { .width = CLAY_SIZING_GROW(0) },
					.childAlignment = { .x = groupalign[g] },
				},
			}) {
				if (dynarray_length(group->modules) > 0) {
					CLAY(CLAY_IDI("group", g), clay_style(&group->style, 0)) {
						for (int i = 0; i < dynarray_length(group->modules); i++) {
							Module *mod = &group->modules[i];
							Monitor *on = ui_bar_modulemon(mod->monitormode);

							if (on && on != m)
								continue;
							mod->layoutfunc(mod, m, mod->updatecontent);
							mod->updatecontent = false;
						}
					}
				}
			}
		}
	}
}

static int
ui_bar_contentheight(void)
{
	float h = 0;

	for (int g = 0; g < LENGTH(ui_bar_modulegroups); g++)
		h = MAX(h, Clay_GetElementData(CLAY_IDI("column", g)).boundingBox.height);
	return ceilf(h + ui_bar.style.layout.padding.top + ui_bar.style.layout.padding.bottom);
}

void
ui_bar_draw(Monitor *m)
{
	Clay_RenderCommandArray cmds;
	Clay_ElementData el;
	Monitor *mon;
	Vec2 pos;
	int height = 0, resized, moved = 0;

	if (!m->showbar || ui_bar_updating)
		return;

	ui_bar_updating = true;

	/* the bar sizes its own height; the layout height only bounds culling */
	clay_layout_begin(&m->barpointer, (Clay_Dimensions){ m->ww, m->mh });
	ui_bar_layout(m);
	cmds = clay_layout_end();

	m->barh = ui_bar_contentheight();
	for (mon = mons; mon; mon = mon->next)
		if (mon->showbar)
			height = MAX(height, mon->barh);
	if ((resized = height != ui_bar.dimensions.height)) {
		ui_bar.dimensions.height = height;
		clay_layout_begin(&m->barpointer, (Clay_Dimensions){ m->ww, m->mh });
		ui_bar_layout(m);
		cmds = clay_layout_end();
		for (mon = mons; mon; mon = mon->next)
			updatebarpos(mon);
		arrange(NULL);
	}
	resizebarwin(m);

	el = Clay_GetElementData(CLAY_ID("systray"));
	if (el.found) {
		pos = (Vec2){ m->wx + roundf(el.boundingBox.x), m->by + roundf(el.boundingBox.y) };
		moved = pos.x != systraypos.x || pos.y != systraypos.y;
		systraypos = pos;
	}

	drw_clear(drw);
	clay_layout_render(drw, cmds);
	drw_map(drw, m->barwin, 0, 0, m->ww, ui_bar.dimensions.height);

	ui_bar_updating = false;

	if (resized)
		for (mon = mons; mon; mon = mon->next)
			if (mon != m)
				ui_bar_draw(mon);
	if (resized || moved)
		updatesystray(0);
}

void
ui_bar_drawall(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		ui_bar_draw(m);
}

void
ui_bar_setup(void)
{
	ui_bar_modules_setup(ui_bar_modulegroups);
}

void
ui_bar_cleanup(void)
{
	for (int i = 0; i < LENGTH(ui_bar_modulegroups); i++) {
		for (int z = 0; z < dynarray_length(ui_bar_modulegroups[i].modules); z++)
			free(ui_bar_modulegroups[i].modules[z].children.content);

		dynarray_destroy(ui_bar_modulegroups[i].modules);
	}
}

static double
ui_bar_now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

int
ui_bar_runupdateintervals(void)
{
	double now = ui_bar_now(), next = -1;
	int all = 0, onsel = 0, onmain = 0;
	Monitor *on;
	Module *mod;

	for (int g = 0; g < LENGTH(ui_bar_modulegroups); g++) {
		for (int i = 0; i < dynarray_length(ui_bar_modulegroups[g].modules); i++) {
			mod = &ui_bar_modulegroups[g].modules[i];
			if (mod->updateinterval <= 0)
				continue;
			if (mod->nextupdate <= now) {
				mod->updatecontent = true;
				mod->nextupdate = now + mod->updateinterval;
				on = ui_bar_modulemon(mod->monitormode);
				all |= !on;
				onsel |= on == selmon;
				onmain |= on == mons;
			}
			if (next < 0 || mod->nextupdate < next)
				next = mod->nextupdate;
		}
	}
	if (all)
		ui_bar_drawall();
	else {
		if (onsel)
			ui_bar_draw(selmon);
		if (onmain && mons != selmon)
			ui_bar_draw(mons);
	}
	return next < 0 ? -1 : ceil((next - now) * 1000);
}

int
ui_bar_buttonpress(XEvent *e)
{
	XButtonPressedEvent *ev = &e->xbutton;

	if (ev->window != selmon->barwin
	|| !clay_pointer_click(&selmon->barpointer, ev->x, ev->y, &ui_bar_press.mod, &ui_bar_press.child))
		return 0;
	ui_bar_press.win = ev->window;
	ui_bar_press.button = ev->button;
	ui_bar_press.state = ev->state;
	return 1;
}

int
ui_bar_buttonrelease(XEvent *e)
{
	XButtonReleasedEvent *ev = &e->xbutton;
	Window win = ui_bar_press.win;
	Monitor *m;
	void *mod;
	int child;

	if (!win || ev->button != ui_bar_press.button)
		return 0;
	ui_bar_press.win = None;
	if (ev->window == win && (m = ui_bar_wintomon(win))
	&& clay_pointer_click(&m->barpointer, ev->x, ev->y, &mod, &child)
	&& mod == ui_bar_press.mod && child == ui_bar_press.child)
		((Module *)mod)->clickfunc(mod, m, child, ui_bar_press.button, ui_bar_press.state);
	return 1;
}

int
ui_bar_motionnotify(XEvent *e)
{
	XMotionEvent *ev = &e->xmotion;
	Monitor *m;

	if (!(m = ui_bar_wintomon(ev->window)))
		return 0;
	if (clay_pointer_move(&m->barpointer, ev->x, ev->y))
		ui_bar_draw(m);
	return 1;
}

int
ui_bar_enternotify(XEvent *e)
{
	XCrossingEvent *ev = &e->xcrossing;
	Monitor *m;

	if (!(m = ui_bar_wintomon(ev->window)))
		return 0;
	if (clay_pointer_move(&m->barpointer, ev->x, ev->y))
		ui_bar_draw(m);
	return 1;
}

int
ui_bar_leavenotify(XEvent *e)
{
	XCrossingEvent *ev = &e->xcrossing;
	Monitor *m;

	if (!(m = ui_bar_wintomon(ev->window)))
		return 0;
	if (clay_pointer_leave(&m->barpointer))
		ui_bar_draw(m);
	return 1;
}

int
ui_bar_expose(XEvent *e)
{
	XExposeEvent *ev = &e->xexpose;
	Monitor *m;

	if (!(m = ui_bar_wintomon(ev->window)))
		return 0;
	if (ev->count == 0) {
		ui_bar_draw(m);
		if (m == selmon)
			updatesystray(0);
	}
	return 1;
}
