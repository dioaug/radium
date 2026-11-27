#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "bar.h"
#include "../../util.h"
#include "../../draw.h"
#include "../../../lib/dynarray/dynarray.h"
#include "../../../lib/clay/clay.h"

static pthread_t *ui_bar_intervalthreads;
static ModuleGroup ui_bar_modulegroups[ModuleGroupLast];
static struct {
	Window win;
	void *mod;
	int child;
	unsigned int button, state;
} ui_bar_press;
bool ui_bar_updating;

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

static void
ui_bar_layout(Monitor *m, ModuleId *update)
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
ui_bar_contentheight(void)
{
	float h = 0;

	for (int g = 0; g < LENGTH(ui_bar_modulegroups); g++)
		h = MAX(h, Clay_GetElementData(CLAY_IDI("column", g)).boundingBox.height);
	return ceilf(h + ui_bar.style.layout.padding.top + ui_bar.style.layout.padding.bottom);
}

void
ui_bar_draw(Monitor *m, ModuleId* updatespecificmodcontent)
{
	Clay_RenderCommandArray cmds;
	Clay_ElementData el;
	Monitor *mon;
	int height = 0, resized;

	if (!m->showbar || (ui_bar_updating && updatespecificmodcontent == NULL))
		return;

	ui_bar_updating = true;

	/* the bar sizes its own height; the layout height only bounds culling */
	clay_layout_begin(&m->barpointer, (Clay_Dimensions){ m->ww, m->mh });
	ui_bar_layout(m, updatespecificmodcontent);
	cmds = clay_layout_end();

	m->barh = ui_bar_contentheight();
	for (mon = mons; mon; mon = mon->next)
		if (mon->showbar)
			height = MAX(height, mon->barh);
	if ((resized = height != ui_bar.dimensions.height)) {
		ui_bar.dimensions.height = height;
		clay_layout_begin(&m->barpointer, (Clay_Dimensions){ m->ww, m->mh });
		ui_bar_layout(m, NULL);
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
	drw_map(drw, m->barwin, 0, 0, m->ww, ui_bar.dimensions.height);

	ui_bar_updating = false;

	if (resized) {
		for (mon = mons; mon; mon = mon->next)
			if (mon != m)
				ui_bar_draw(mon, NULL);
		updatesystray(0);
	}
}

void
ui_bar_drawall(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		ui_bar_draw(m, NULL);
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

static void* ui_bar_moduleroutine(void* arg) {
	ModuleId mID = *(ModuleId*)arg;
	free(arg);

	while (1) {
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		while (ui_bar_updating)
			usleep(10000);
		ui_bar_draw(selmon, &mID);
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

		usleep(ui_bar_modulegroups[mID.ModuleGroupIndex].modules[mID.ModuleIndex].updateinterval * 1000000);
	}

	return NULL;
}

void
ui_bar_initupdateintervals(void)
{
	ModuleId *validintervals = dynarray_create(ModuleId);

	for (int modulegroup = 0; modulegroup < LENGTH(ui_bar_modulegroups); modulegroup++) {
		for (int i = 0; i < dynarray_length(ui_bar_modulegroups[modulegroup].modules); i++) {
			if (ui_bar_modulegroups[modulegroup].modules[i].updateinterval > 0) {
				ModuleId mId = (ModuleId){.ModuleGroupIndex = modulegroup, .ModuleIndex = i};
				dynarray_push(validintervals, mId);
			}
		}
	}

	ui_bar_intervalthreads = dynarray_create(pthread_t);
	for (size_t i = 0; i < dynarray_length(validintervals); ++i) {
		ModuleId* modinfo = malloc(sizeof(ModuleId));
		pthread_t thread;

		*modinfo = validintervals[i];
		if (pthread_create(&thread, NULL, ui_bar_moduleroutine, modinfo) != 0) {
			perror("Failed to create module interval thread");
			free(modinfo);
		} else
			dynarray_push(ui_bar_intervalthreads, thread);
	}

	dynarray_destroy(validintervals);
}

void
ui_bar_stopupdateintervals(void)
{
	if (!ui_bar_intervalthreads)
		return;
	for (size_t i = 0; i < dynarray_length(ui_bar_intervalthreads); i++) {
		pthread_cancel(ui_bar_intervalthreads[i]);
		pthread_join(ui_bar_intervalthreads[i], NULL);
	}
	dynarray_destroy(ui_bar_intervalthreads);
	ui_bar_intervalthreads = NULL;
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
		ui_bar_draw(m, NULL);
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
		ui_bar_draw(m, NULL);
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
		ui_bar_draw(m, NULL);
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
		ui_bar_draw(m, NULL);
		if (m == selmon)
			updatesystray(0);
	}
	return 1;
}
