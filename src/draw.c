/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <pango/pangocairo.h>

#include "draw.h"
#include "util.h"

Drw *
drw_create(Display *dpy, int screen, Window root, unsigned int w, unsigned int h, Visual *visual, unsigned int depth, Colormap cmap)
{
	Drw *drw = ecalloc(1, sizeof(Drw));

	if(!drw)
		return NULL;
	drw->dpy = dpy;
	drw->screen = screen;
	drw->w = w;
	drw->h = h;

	drw->sur = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	drw->ctx = cairo_create(drw->sur);
	cairo_set_line_width(drw->ctx, 1);

	drw->sur_b = cairo_xlib_surface_create(dpy, root, DefaultVisual(dpy, screen), w, h);
	drw->ctx_b = cairo_create(drw->sur_b);
	cairo_surface_destroy(drw->sur_b);

	return drw;
}

void
drw_resize(Drw *drw, unsigned int w, unsigned int h)
{
	if(!drw)
		return;
	drw->w = w;
	drw->h = h;
}

void
drw_free(Drw *drw)
{
	cairo_surface_finish(drw->sur);
	cairo_destroy(drw->ctx);
	cairo_destroy(drw->ctx_b);
	free(drw);
}

static void
roundrectpath(cairo_t *ctx, Clay_BoundingBox b, Clay_CornerRadius r)
{
	/* shrink radii that don't fit so opposite corners never overlap */
	float max = MIN(b.width, b.height) / 2;

	r.topLeft = MIN(r.topLeft, max);
	r.topRight = MIN(r.topRight, max);
	r.bottomLeft = MIN(r.bottomLeft, max);
	r.bottomRight = MIN(r.bottomRight, max);

	/* a zero radius arc is just a line to the corner */
	cairo_new_sub_path(ctx);
	cairo_arc(ctx, b.x + b.width - r.topRight, b.y + r.topRight, r.topRight, -M_PI/2, 0);
	cairo_arc(ctx, b.x + b.width - r.bottomRight, b.y + b.height - r.bottomRight, r.bottomRight, 0, M_PI/2);
	cairo_arc(ctx, b.x + r.bottomLeft, b.y + b.height - r.bottomLeft, r.bottomLeft, M_PI/2, M_PI);
	cairo_arc(ctx, b.x + r.topLeft, b.y + r.topLeft, r.topLeft, M_PI, 3*M_PI/2);
	cairo_close_path(ctx);
}

/* fills the box with bg_color, then draws a border of widths bw in
 * bd_color inside it, following the rounded corners */
void
drw_roundrect(Drw *drw, Clay_BoundingBox box, Clay_CornerRadius r, Clay_BorderWidth bw, Clay_Color bg_color, Clay_Color bd_color)
{
	Clay_BoundingBox inner = {
		.x = box.x + bw.left,
		.y = box.y + bw.top,
		.width = box.width - bw.left - bw.right,
		.height = box.height - bw.top - bw.bottom,
	};
	/* the inner edge runs parallel to the outer one, so its corners are
	 * tighter by the border width */
	Clay_CornerRadius ir = {
		.topLeft = MAX(0, r.topLeft - MAX(bw.top, bw.left)),
		.topRight = MAX(0, r.topRight - MAX(bw.top, bw.right)),
		.bottomLeft = MAX(0, r.bottomLeft - MAX(bw.bottom, bw.left)),
		.bottomRight = MAX(0, r.bottomRight - MAX(bw.bottom, bw.right)),
	};

	if (box.width <= 0 || box.height <= 0)
		return;

	cairo_new_path(drw->ctx);
	roundrectpath(drw->ctx, box, r);

	if (bg_color.a > 0) {
		cairo_set_source_rgba(drw->ctx, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
		cairo_fill_preserve(drw->ctx);
	}

	if (bd_color.a > 0 && (bw.left || bw.right || bw.top || bw.bottom)) {
		/* the outer outline is still the current path; adding the inner
		 * one and filling even-odd paints only the ring between them */
		if (inner.width > 0 && inner.height > 0)
			roundrectpath(drw->ctx, inner, ir);
		cairo_save(drw->ctx);
		cairo_set_fill_rule(drw->ctx, CAIRO_FILL_RULE_EVEN_ODD);
		cairo_set_source_rgba(drw->ctx, bd_color.r, bd_color.g, bd_color.b, bd_color.a);
		cairo_fill(drw->ctx);
		cairo_restore(drw->ctx);
	}

	cairo_new_path(drw->ctx);
}

void
drw_text(Drw *drw, int x, int y, const char *text, int len, const char *font_desc, Clay_Color color) {
	if (text == NULL || len == 0 || *text == '\0')
        return;

	PangoLayout *layout = pango_cairo_create_layout(drw->ctx);

    cairo_set_source_rgb(drw->ctx, color.r, color.g, color.b);
	pango_layout_set_text(layout, text, len);
	PangoFontDescription *desc = pango_font_description_from_string(font_desc);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	cairo_move_to(drw->ctx, x, y);

	pango_cairo_show_layout(drw->ctx, layout);

	g_object_unref(layout);
}

void
drw_clear(Drw *drw) {
	if(!drw)
		return;
	cairo_save(drw->ctx);
	cairo_set_operator(drw->ctx, CAIRO_OPERATOR_CLEAR);
	cairo_paint(drw->ctx);
	cairo_restore(drw->ctx);
}

void
drw_map(Drw *drw, Window win, int x, int y, unsigned int w, unsigned int h) {
	if(!drw)
		return;
	cairo_xlib_surface_set_drawable(drw->sur_b, win, w, h);
	cairo_set_source_surface(drw->ctx_b, drw->sur, x, y);
	cairo_paint(drw->ctx_b);
	XSync(drw->dpy, False);
}

/* measures text in fractional pixels; callers decide how to round */
Clay_Dimensions
drw_textextents(Drw *drw, const char *text, int len, const char *font_desc) {
	if (text == NULL || len == 0 || *text == '\0')
        return (Clay_Dimensions){0, 0};

	PangoLayout *layout = pango_cairo_create_layout(drw->ctx);

    pango_layout_set_text(layout, text, len);
    PangoFontDescription *desc = pango_font_description_from_string(font_desc);
    pango_layout_set_font_description(layout, desc);

    int width, height;
    pango_layout_get_size(layout, &width, &height);  // in Pango units (1 Pango unit = 1/1024 of a point)

    pango_font_description_free(desc);
    g_object_unref(layout);

	return (Clay_Dimensions){ (float)width / PANGO_SCALE, (float)height / PANGO_SCALE };
}

Cur *
drw_cur_create(Drw *drw, int shape)
{
	Cur *cur;

	if (!drw || !(cur = ecalloc(1, sizeof(Cur))))
		return NULL;

	cur->cursor = XCreateFontCursor(drw->dpy, shape);

	return cur;
}

void
drw_cur_free(Drw *drw, Cur *cursor)
{
	if (!cursor)
		return;

	XFreeCursor(drw->dpy, cursor->cursor);
	free(cursor);
}

static const char *fontdescs[] = { "sans-serif bold 8" };
static const Clay_Color transparent = {0, 0, 0, 0};
static const Clay_BorderWidth noborder = {0};

static const char *
fontdesc(uint16_t id)
{
	return fontdescs[id < LENGTH(fontdescs) ? id : 0];
}

static Clay_Dimensions
claymeasuretextfunc(Clay_StringSlice text, Clay_TextElementConfig *config, void *userdata)
{
	return drw_textextents(userdata, text.chars, text.length, fontdesc(config->fontId));
}

static void
clayerror(Clay_ErrorData err)
{
	fprintf(stderr, "radium: layout: %.*s\n", (int)err.errorText.length, err.errorText.chars);
}

void
clay_layout_init(Drw *drw)
{
	uint32_t size;

	Clay_SetMaxElementCount(512);
	size = Clay_MinMemorySize();
	Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(size, ecalloc(1, size)),
		(Clay_Dimensions){ 0, 0 }, (Clay_ErrorHandler){ clayerror, NULL });
	Clay_SetMeasureTextFunction(claymeasuretextfunc, drw);
}

void
clay_layout_render(Drw *drw, Clay_RenderCommandArray cmds)
{
	for (int i = 0; i < cmds.length; i++) {
		Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&cmds, i);
		Clay_BoundingBox b = cmd->boundingBox;
		/* snapped to whole pixels so edges stay sharp */
		Clay_BoundingBox px = {
			.x = roundf(b.x),
			.y = roundf(b.y),
			.width = roundf(b.x + b.width) - roundf(b.x),
			.height = roundf(b.y + b.height) - roundf(b.y),
		};

		switch (cmd->commandType) {
		/* Clay sends a box's fill and border as separate commands, the
		 * border after the box's children so it's drawn on top of them */
		case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
			drw_roundrect(drw, px, cmd->renderData.rectangle.cornerRadius, noborder,
				cmd->renderData.rectangle.backgroundColor, transparent);
			break;
		case CLAY_RENDER_COMMAND_TYPE_BORDER:
			drw_roundrect(drw, px, cmd->renderData.border.cornerRadius, cmd->renderData.border.width,
				transparent, cmd->renderData.border.color);
			break;
		case CLAY_RENDER_COMMAND_TYPE_TEXT: {
			Clay_TextRenderData *t = &cmd->renderData.text;

			drw_text(drw, px.x, px.y, t->stringContents.chars, t->stringContents.length,
				fontdesc(t->fontId), t->textColor);
			break;
		}
		case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
			cairo_save(drw->ctx);
			cairo_rectangle(drw->ctx, px.x, px.y, px.width, px.height);
			cairo_clip(drw->ctx);
			break;
		case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
			cairo_restore(drw->ctx);
			break;
		default:
			break;
		}
	}
}

Clay_String
clay_string(const char *s)
{
	return (Clay_String){ .isStaticallyAllocated = false, .length = strlen(s), .chars = s };
}

/* the pointer of the window being laid out, and the hoverable elements
 * declared so far, which become its hoverables at clay_layout_end() */
static ClayPointer *claylayoutpointer;
static ClayHoverable claynext[LENGTH(((ClayPointer *)0)->hoverables)];
static int claynnext;

static bool
claystateisset(const StateStyle *s)
{
	return s->backgroundColor.a > 0 || s->border.color.a > 0 || s->textColor.a > 0;
}

static bool
claypointerin(const ClayPointer *p, Clay_BoundingBox b)
{
	return p->in && p->pos.x >= b.x && p->pos.x < b.x + b.width
		&& p->pos.y >= b.y && p->pos.y < b.y + b.height;
}

/* whether drawing the window again would hover other elements */
static bool
clayhoverchanged(const ClayPointer *p)
{
	if (p->n > LENGTH(p->hoverables))
		return true;
	for (int i = 0; i < p->n; i++)
		if (!p->hoverables[i].selected
		&& claypointerin(p, p->hoverables[i].box) != p->hoverables[i].hovered)
			return true;
	return false;
}

/* whether the open element gets its hover style: it isn't selected and
 * the pointer is over it, going by where the last layout put it (Clay's
 * own Clay_Hovered() goes by the last layout of any window). Notes the
 * element for the next pointer event, selected or not, so it's found in
 * that layout once it isn't. */
static bool
clayhovered(const StateStyle *hover, bool selected)
{
	ClayPointer *p = claylayoutpointer;
	uint32_t id;
	bool hovered = false;
	int i;

	if (p == NULL || !claystateisset(hover))
		return false;
	id = Clay_GetOpenElementId();
	for (i = 0; i < MIN(p->n, (int)LENGTH(p->hoverables)); i++)
		if (p->hoverables[i].id == id) {
			hovered = !selected && claypointerin(p, p->hoverables[i].box);
			break;
		}
	/* noted once, though both the element and its text can ask */
	for (i = 0; i < MIN(claynnext, (int)LENGTH(claynext)); i++)
		if (claynext[i].id == id)
			return hovered;
	if (claynnext < LENGTH(claynext))
		claynext[claynnext] = (ClayHoverable){ .id = id, .selected = selected, .hovered = hovered };
	claynnext++;
	return hovered;
}

void
clay_apply_state(const StateStyle *s, Clay_ElementDeclaration *box, Clay_TextElementConfig *text)
{
	if (box && s->backgroundColor.a > 0)
		box->backgroundColor = s->backgroundColor;
	if (box && s->border.color.a > 0)
		box->border = s->border;
	if (text && s->textColor.a > 0)
		text->textColor = s->textColor;
}

void
clay_layout_begin(ClayPointer *p, Clay_Dimensions size)
{
	claylayoutpointer = p;
	claynnext = 0;
	Clay_SetLayoutDimensions(size);
	Clay_BeginLayout();
}

Clay_RenderCommandArray
clay_layout_end(void)
{
	Clay_RenderCommandArray cmds = Clay_EndLayout(0);
	ClayPointer *p = claylayoutpointer;

	if (p) {
		p->n = claynnext;
		for (int i = 0; i < MIN(claynnext, (int)LENGTH(claynext)); i++) {
			p->hoverables[i] = claynext[i];
			p->hoverables[i].box = Clay_GetElementData((Clay_ElementId){ .id = claynext[i].id }).boundingBox;
		}
	}
	claylayoutpointer = NULL;
	return cmds;
}

/* puts the state styles of s that apply over box and text */
static void
clayapplystates(const Style *s, int states, Clay_ElementDeclaration *box, Clay_TextElementConfig *text)
{
	if (states & ClaySubdued)
		clay_apply_state(&s->subdued, box, text);
	if (states & ClaySelected)
		clay_apply_state(&s->selected, box, text);
	if (clayhovered(&s->hover, states & ClaySelected))
		clay_apply_state(&s->hover, box, text);
}

Clay_ElementDeclaration
clay_style(const Style *s, int states)
{
	Clay_ElementDeclaration d = s->base;

	clayapplystates(s, states, &d, NULL);
	return d;
}

Clay_TextElementConfig
clay_text_style(const Style *s, Clay_TextElementConfig text, int states)
{
	clayapplystates(s, states, NULL, &text);
	return text;
}

bool
clay_pointer_move(ClayPointer *p, int x, int y)
{
	p->in = true;
	/* the pixel's center, so it's inside the boxes that cover the pixel
	 * once they're snapped to whole pixels */
	p->pos = (Clay_Vector2){ x + 0.5f, y + 0.5f };
	return clayhoverchanged(p);
}

bool
clay_pointer_leave(ClayPointer *p)
{
	p->in = false;
	return clayhoverchanged(p);
}
