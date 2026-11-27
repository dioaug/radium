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

typedef struct {
	Clay_Color backgroundColor;
	Clay_BorderElementConfig border;
	Clay_Color textColor;
} StateStyle;

typedef struct {
	Clay_ElementDeclaration base;
	const char *font;
	uint16_t fontsize;
	uint16_t fontweight;
	uint16_t fontstyle;
	StateStyle subdued;
	StateStyle selected;
	StateStyle hover;
} Style;

enum { ClaySubdued = 1 << 0, ClaySelected = 1 << 1 }; /* states for clay_style(), hover follows the pointer */
enum { WeightThin = 1, WeightExtraLight, WeightLight, WeightRegular, WeightMedium,
	WeightSemiBold, WeightBold, WeightExtraBold, WeightBlack };
enum { StyleNormal = 1, StyleOblique, StyleItalic };

typedef struct {
	uint32_t id;
	Clay_BoundingBox box;
	bool selected;
	bool hovered;
} ClayHoverable;

typedef struct {
	uint32_t id;
	Clay_BoundingBox box;
	void *data;
	int index;
} ClayClickable;

typedef struct {
	bool in; // whether the pointer is in the window
	Clay_Vector2 pos;
	int n; // hoverable elements in the last layout, only 64 of them are kept
	ClayHoverable hoverables[64];
	int nclickables; // clickable elements in the last layout, only 64 of them are kept
	ClayClickable clickables[64];
} ClayPointer;

/* Drawable abstraction */
Drw *drw_create(Display *dpy, int screen, Window win, unsigned int w, unsigned int h, Visual *visual, unsigned int depth, Colormap cmap);
void drw_resize(Drw *drw, unsigned int w, unsigned int h);
void drw_free(Drw *drw);

/* Cursor abstraction */
Cur *drw_cur_create(Drw *drw, int shape);
void drw_cur_free(Drw *drw, Cur *cursor);

void drw_roundrect(Drw *drw, Clay_BoundingBox box, Clay_CornerRadius r, Clay_BorderWidth bw, Clay_Color bg_color, Clay_Color bd_color);
bool drw_font_add(const char *path);
void drw_text(Drw *drw, int x, int y, const char *text, int len, uint16_t font, uint16_t size, Clay_Color color);
Clay_Dimensions drw_textextents(Drw *drw, const char *text, int len, uint16_t font, uint16_t size);

/* Map functions */
void drw_map(Drw *drw, Window win, int x, int y, unsigned int w, unsigned int h);
void drw_detach(Drw *drw, Window win);
void drw_clear(Drw *drw);

/* Clay functions */
void clay_layout_init(Drw *drw);
void clay_layout_render(Drw *drw, Clay_RenderCommandArray cmds); // Draws one frame's render commands.
Clay_String clay_string(const char *s); // CLAY_STRING() for strings that aren't literals.
void clay_apply_state(const StateStyle *s, Clay_ElementDeclaration *box, Clay_TextElementConfig *text); // box or text can be NULL
void clay_layout_begin(ClayPointer *p, Clay_Dimensions size);
Clay_RenderCommandArray clay_layout_end(void);
Clay_ElementDeclaration clay_style(const Style *s, int states); // pass it as the declaration in CLAY(), so the element is open for hover
Clay_TextElementConfig clay_text(Clay_TextElementConfig text);
Clay_TextElementConfig clay_text_style(const Style *s, Clay_TextElementConfig text, int states); // text with the text colors of the element's states
bool clay_pointer_move(ClayPointer *p, int x, int y);
bool clay_pointer_leave(ClayPointer *p);
void clay_clickable(void *data, int index); // call first thing inside an element, so clicks on it give data and index
bool clay_pointer_click(const ClayPointer *p, int x, int y, void **data, int *index); // the innermost clickable element at x, y
