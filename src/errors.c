#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "bar.h"
#include "draw.h"
#include "errors.h"
#include "util.h"
#include "../lib/clay/clay.h"
#include "config.h"

static const int ui_errors_winw = 550;
static const int ui_errors_winh = 350;

static const unsigned long ui_errors_bgpixel = 0xff1a1a1a;
static const unsigned long ui_errors_borderpixel = 0xffff4070;

static const Clay_Color ui_errors_bgcolor = {0.1020, 0.1020, 0.1020, 1.0};
static const Clay_Color ui_errors_scrollhandlecolor = {0.2, 0.2, 0.2, 1.0};
static const Clay_Color ui_errors_scrollareacolor = {0.080, 0.080, 0.080, 1.0};
static const Clay_Color ui_errors_separatorcolor = {0.140, 0.140, 0.140, 1.0};
static const Clay_Color ui_errors_accentcolor = {1.0, 1.0, 1.0, 1.0};

static const float ui_errors_scrollbarw = 8;
static const float ui_errors_scrollminhandleh = 24;
static const float ui_errors_scrollstep = 48;

static const Clay_TextElementConfig ui_errors_titletext = {
	.textColor = {1.0, 1.0, 1.0, 1.0},
	.fontId = 0,
	.fontSize = 12,
	.wrapMode = CLAY_TEXT_WRAP_NONE,
};
static const Clay_TextElementConfig ui_errors_errortext = {
	.textColor = {0.6, 0.6, 0.6, 1.0},
	.fontId = 0,
	.wrapMode = CLAY_TEXT_WRAP_WORDS,
};
static const Clay_TextElementConfig ui_errors_wheretext = {
	.textColor = {0.3, 0.3, 0.3, 1.0},
	.fontId = 0,
	.wrapMode = CLAY_TEXT_WRAP_NONE,
};

typedef struct {
	char *where;
	char *text;
	bool *accent;
} Error;

static Error *ui_errors_errors;
static int ui_errors_nerrors;
static Window ui_errors_win;
static ClayPointer ui_errors_pointer;
static int ui_errors_isvisible, ui_errors_dragging;
static float ui_errors_scroll, ui_errors_dragy, ui_errors_dragscroll;
static float ui_errors_viewh, ui_errors_contenth, ui_errors_handleh;
static Clay_BoundingBox ui_errors_track;

static int
ui_errors_setscroll(float s)
{
	float old = ui_errors_scroll;

	ui_errors_scroll = roundf(MAX(0, MIN(s, ui_errors_contenth - ui_errors_viewh)));
	return ui_errors_scroll != old;
}

static float
ui_errors_handletoscroll(float dy)
{
	float range = ui_errors_track.height - ui_errors_handleh;

	return range > 0 ? dy * (ui_errors_contenth - ui_errors_viewh) / range : 0;
}

static Clay_RenderCommandArray
ui_errors_layout(void)
{
	float handley = ui_errors_contenth > ui_errors_viewh
		? floorf(ui_errors_scroll * (ui_errors_track.height - ui_errors_handleh) / (ui_errors_contenth - ui_errors_viewh))
		: 0;

	clay_layout_begin(&ui_errors_pointer, (Clay_Dimensions){ ui_errors_winw - 2 * borderpx, ui_errors_winh - 2 * borderpx });
	CLAY(CLAY_ID("errors"), {
		.layout = {
			.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.padding = CLAY_PADDING_ALL(24),
			.childGap = 16,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = ui_errors_bgcolor,
	}) {
		CLAY_TEXT(CLAY_STRING("Errors"), ui_errors_titletext);
		CLAY(CLAY_ID("errorsbody"), {
			.layout = {
				.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
				.childGap = 16,
			},
		}) {
			CLAY(CLAY_ID("errorslist"), {
				.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } },
				.clip = { .horizontal = true, .vertical = true, .childOffset = { 0, -ui_errors_scroll } },
			}) {
				CLAY(CLAY_ID("errorsrows"), {
					.layout = {
						.sizing = { .width = CLAY_SIZING_PERCENT(1) },
						.layoutDirection = CLAY_TOP_TO_BOTTOM,
					},
					.border = { .color = ui_errors_separatorcolor, .width = { .betweenChildren = 1 } },
				}) {
					for (int i = 0; i < ui_errors_nerrors; i++) {
						Clay_TextElementConfig msg = ui_errors_errortext;

						msg.userData = &ui_errors_errors[i];
						CLAY(CLAY_IDI("error", i), {
							.layout = {
								.sizing = { .width = CLAY_SIZING_GROW(0) },
								.padding = { .top = 8, .bottom = 8 },
								.childGap = 4,
								.layoutDirection = CLAY_TOP_TO_BOTTOM,
							},
						}) {
							CLAY_TEXT(clay_string(ui_errors_errors[i].text), msg);
							if (ui_errors_errors[i].where)
								CLAY_TEXT(clay_string(ui_errors_errors[i].where), ui_errors_wheretext);
						}
					}
				}
			}
			if (ui_errors_contenth > ui_errors_viewh)
				CLAY(CLAY_ID("errorstrack"), {
					.layout = {
						.sizing = { CLAY_SIZING_FIXED(ui_errors_scrollbarw), CLAY_SIZING_GROW(0) },
						.padding = { .top = handley },
					},
					.backgroundColor = ui_errors_scrollareacolor,
					.cornerRadius = CLAY_CORNER_RADIUS(ui_errors_scrollbarw / 2),
				}) {
					clay_clickable(NULL, 0);
					CLAY(CLAY_ID("errorshandle"), {
						.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(ui_errors_handleh) } },
						.backgroundColor = ui_errors_scrollhandlecolor,
						.cornerRadius = CLAY_CORNER_RADIUS(ui_errors_scrollbarw / 2),
					}) {
						clay_clickable(NULL, 1);
					}
				}
		}
	}
	return clay_layout_end();
}

static int
ui_errors_measure(void)
{
	Clay_BoundingBox t = Clay_GetElementData(CLAY_ID("errorstrack")).boundingBox;
	float v = Clay_GetElementData(CLAY_ID("errorslist")).boundingBox.height;
	float c = Clay_GetElementData(CLAY_ID("errorsrows")).boundingBox.height;

	if (v == ui_errors_viewh && c == ui_errors_contenth && t.y == ui_errors_track.y && t.height == ui_errors_track.height)
		return 0;
	ui_errors_viewh = v;
	ui_errors_contenth = c;
	ui_errors_track = t;
	ui_errors_handleh = c > v ? MIN(t.height, MAX(ui_errors_scrollminhandleh, roundf(t.height * v / c))) : t.height;
	ui_errors_setscroll(ui_errors_scroll);
	return 1;
}

static void
ui_errors_render(Clay_RenderCommandArray cmds)
{
	Clay_RenderCommand *cmd;
	Clay_TextRenderData *t;
	const Error *e;
	const bool *accent;
	int i, start, end;
	float x;

	for (i = 0; i < cmds.length; i++) {
		cmd = Clay_RenderCommandArray_Get(&cmds, i);
		e = cmd->userData;
		if (cmd->commandType != CLAY_RENDER_COMMAND_TYPE_TEXT || !e) {
			clay_layout_render(drw, (Clay_RenderCommandArray){ .capacity = 1, .length = 1, .internalArray = cmd });
			continue;
		}
		t = &cmd->renderData.text;
		accent = e->accent + (t->stringContents.chars - e->text);
		for (start = 0; start < t->stringContents.length; start = end) {
			for (end = start + 1; end < t->stringContents.length && accent[end] == accent[start]; end++)
				;
			x = cmd->boundingBox.x + drw_textextents(drw, t->stringContents.chars, start, t->fontId, t->fontSize).width;
			drw_text(drw, roundf(x), roundf(cmd->boundingBox.y), t->stringContents.chars + start, end - start,
				t->fontId, t->fontSize, accent[start] ? ui_errors_accentcolor : t->textColor);
		}
	}
}

static void
ui_errors_draw(void)
{
	Clay_RenderCommandArray cmds;

	while (bar_updating)
		usleep(1000);
	bar_updating = true;
	cmds = ui_errors_layout();
	for (int i = 0; i < 4 && ui_errors_measure(); i++)
		cmds = ui_errors_layout();
	drw_clear(drw);
	ui_errors_render(cmds);
	drw_map(drw, ui_errors_win, 0, 0, ui_errors_winw - 2 * borderpx, ui_errors_winh - 2 * borderpx);
	bar_updating = false;
}

void
ui_errors_setup(void)
{
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixel = ui_errors_bgpixel,
		.border_pixel = ui_errors_borderpixel,
		.colormap = cmap,
		.event_mask = ButtonPressMask|ButtonReleaseMask|Button1MotionMask|ExposureMask,
	};
	XClassHint ch = {"errors", "radium"};

	ui_errors_win = XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0,
		ui_errors_winw - 2 * borderpx, ui_errors_winh - 2 * borderpx, borderpx, depth, InputOutput, visual, CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &wa);
	XSetClassHint(dpy, ui_errors_win, &ch);
}

void
ui_errors_cleanup(void)
{
	drw_detach(drw, ui_errors_win);
	XDestroyWindow(dpy, ui_errors_win);
	for (int i = 0; i < ui_errors_nerrors; i++) {
		free(ui_errors_errors[i].where);
		free(ui_errors_errors[i].text);
		free(ui_errors_errors[i].accent);
	}
	free(ui_errors_errors);
}

static char *
ui_errors_copy(const char *s)
{
	char *c = NULL;

	if (s && !(c = strdup(s)))
		die("strdup:");
	return c;
}

static char *
ui_errors_format(const char *fmt, char marker, va_list ap)
{
	char *f = ecalloc(strlen(fmt) + 1, 1), *s;
	va_list cp;
	int i, j, n;

	for (i = j = 0; fmt[i]; i++) {
		if (fmt[i] == '*' && fmt[i + 1] == '*') {
			f[j++] = marker;
			i++;
		} else
			f[j++] = fmt[i];
	}
	va_copy(cp, ap);
	n = vsnprintf(NULL, 0, f, cp);
	va_end(cp);
	s = ecalloc(n + 1, 1);
	vsnprintf(s, n + 1, f, ap);
	free(f);
	return s;
}

void
ui_errors_add(const char *where, const char *fmt, ...)
{
	Error e = { .where = ui_errors_copy(where) };
	bool accent = false;
	va_list ap;
	char *alt;
	int i, j;

	va_start(ap, fmt);
	e.text = ui_errors_format(fmt, '\1', ap);
	va_end(ap);
	va_start(ap, fmt);
	alt = ui_errors_format(fmt, '\2', ap);
	va_end(ap);
	e.accent = ecalloc(strlen(e.text) + 1, sizeof(bool));
	for (i = j = 0; e.text[i]; i++) {
		if (e.text[i] != alt[i])
			accent = !accent;
		else {
			e.accent[j] = accent;
			e.text[j++] = e.text[i];
		}
	}
	e.text[j] = '\0';
	free(alt);
	if (!(ui_errors_errors = realloc(ui_errors_errors, (ui_errors_nerrors + 1) * sizeof(*ui_errors_errors))))
		die("realloc:");
	ui_errors_errors[ui_errors_nerrors++] = e;
	fprintf(stderr, "radium: %s%s%s\n", where ? where : "", where ? ": " : "", e.text);
	if (ui_errors_isvisible)
		ui_errors_draw();
	drawbars();
}

int
ui_errors_count(void)
{
	return ui_errors_nerrors;
}

int
ui_errors_visible(void)
{
	return ui_errors_isvisible;
}

void
ui_errors_toggle(Monitor *m)
{
	if (!ui_errors_isvisible && ui_errors_nerrors == 0)
		return;
	if (ui_errors_isvisible)
		XUnmapWindow(dpy, ui_errors_win);
	else {
		XMoveWindow(dpy, ui_errors_win, m->mx + (m->mw - ui_errors_winw) / 2, m->my + (m->mh - ui_errors_winh) / 2);
		XMapRaised(dpy, ui_errors_win);
	}
	ui_errors_isvisible = !ui_errors_isvisible;
	drawbars();
}

int
ui_errors_buttonpress(XEvent *e)
{
	XButtonPressedEvent *ev = &e->xbutton;
	void *data;
	int part;

	if (ev->window != ui_errors_win)
		return 0;
	if (ev->button == Button4 || ev->button == Button5) {
		if (ui_errors_setscroll(ui_errors_scroll + (ev->button == Button4 ? -ui_errors_scrollstep : ui_errors_scrollstep)))
			ui_errors_draw();
	} else if (ev->button == Button1) {
		ui_errors_dragging = clay_pointer_click(&ui_errors_pointer, ev->x, ev->y, &data, &part);
		if (ui_errors_dragging && part == 0 && ui_errors_setscroll(ui_errors_handletoscroll(ev->y - ui_errors_track.y - ui_errors_handleh / 2)))
			ui_errors_draw();
		ui_errors_dragy = ev->y;
		ui_errors_dragscroll = ui_errors_scroll;
	}
	return 1;
}

int
ui_errors_buttonrelease(XEvent *e)
{
	if (e->xbutton.window != ui_errors_win)
		return 0;
	if (e->xbutton.button == Button1)
		ui_errors_dragging = 0;
	return 1;
}

int
ui_errors_motionnotify(XEvent *e)
{
	XEvent next;

	if (e->xmotion.window != ui_errors_win)
		return 0;
	while (XCheckTypedWindowEvent(dpy, ui_errors_win, MotionNotify, &next))
		e = &next;
	if (ui_errors_dragging && ui_errors_setscroll(ui_errors_dragscroll + ui_errors_handletoscroll(e->xmotion.y - ui_errors_dragy)))
		ui_errors_draw();
	return 1;
}

int
ui_errors_expose(XEvent *e)
{
	if (e->xexpose.window != ui_errors_win)
		return 0;
	if (e->xexpose.count == 0)
		ui_errors_draw();
	return 1;
}
