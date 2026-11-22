#pragma once

/* See LICENSE file for copyright and license details. */
#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include "../lib/clay/clay.h"

typedef struct {
	Cursor cursor;
} Cur;

typedef struct {
	int width;
	int height;
} Dimensions;

enum { ColFg, ColBg, ColBorder }; /* Clr scheme index */
// typedef XftColor Clr;

typedef struct {
	unsigned int w, h;
	Display *dpy;
	int screen;
	Window root;
	Visual *visual;
	unsigned int depth;
	Colormap cmap;
	
	cairo_t *ctx;
	cairo_surface_t *sur;
	cairo_t *ctx_b;
	cairo_surface_t *sur_b;
	
} Drw;

/* Drawable abstraction */
Drw *drw_create(Display *dpy, int screen, Window win, unsigned int w, unsigned int h, Visual *visual, unsigned int depth, Colormap cmap);
void drw_resize(Drw *drw, unsigned int w, unsigned int h);
void drw_free(Drw *drw);

/* Cursor abstraction */
Cur *drw_cur_create(Drw *drw, int shape);
void drw_cur_free(Drw *drw, Cur *cursor);

void drw_roundrect(Drw *drw, Clay_BoundingBox box, Clay_CornerRadius r, Clay_BorderWidth bw, Clay_Color bg_color, Clay_Color bd_color);
void drw_text(Drw *drw, int x, int y, const char *text, int len, const char *font_desc, Clay_Color color);
Clay_Dimensions drw_textextents(Drw *drw, const char *text, int len, const char *font_desc);

/* Map functions */
void drw_map(Drw *drw, Window win, int x, int y, unsigned int w, unsigned int h);
void drw_clear(Drw *drw);

/* Clay functions */
void clay_layout_init(Drw *drw);
void clay_layout_render(Drw *drw, Clay_RenderCommandArray cmds); // Draws one frame's render commands.
Clay_String clay_string(const char *s); // CLAY_STRING() for strings that aren't literals.