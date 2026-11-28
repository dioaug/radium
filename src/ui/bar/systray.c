#include <stdio.h>
#include <stdlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "systray.h"
#include "bar.h"
#include "../../util.h"

#define _NET_SYSTEM_TRAY_ORIENTATION_HORZ 0

#define SYSTEM_TRAY_REQUEST_DOCK    0
/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY      0
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_FOCUS_IN             4
#define XEMBED_MODALITY_ON         10
#define XEMBED_MAPPED              (1 << 0)
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_WINDOW_DEACTIVATE    2
#define VERSION_MAJOR               0
#define VERSION_MINOR               0
#define XEMBED_EMBEDDED_VERSION (VERSION_MAJOR << 16) | VERSION_MINOR

static const int showsystray        = 1;        /* 0 means no systray */

typedef struct Systray   Systray;
struct Systray {
	Window win;
	Client *icons;
};

static Systray *systray = NULL;
static unsigned long systrayorientation = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;
Atom xatom[XLast];
Vec2 systraypos;
unsigned int systrayiconsize;
unsigned int systrayspacing;
int systraymonitormode;

static Monitor *
systraytomon(void)
{
	Monitor *m = ui_bar_modulemon(systraymonitormode);

	return m ? m : selmon;
}

static Client *
wintosystrayicon(Window w) {
	Client *i = NULL;

	if (!showsystray || !w)
		return i;
	for (i = systray->icons; i && i->win != w; i = i->next) ;
	return i;
}

static void
removesystrayicon(Client *i)
{
	Client **ii;

	if (!showsystray || !i)
		return;
	for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next);
	if (ii)
		*ii = i->next;
	free(i);
}

static void
updatesystrayicongeom(Client *i, int w, int h)
{
	if (!i)
		return;
	applysizehints(i, &(i->x), &(i->y), &(i->w), &(i->h), &(i->bw), False);
	i->w = systrayiconsize;
	i->h = systrayiconsize;
}

static void
updatesystrayiconstate(Client *i, XPropertyEvent *ev)
{
	long flags;
	int code = 0;

	if (!showsystray || !i || ev->atom != xatom[XembedInfo] ||
			!(flags = getatomprop(i, xatom[XembedInfo])))
		return;

	if (flags & XEMBED_MAPPED && !i->tags) {
		i->tags = 1;
		code = XEMBED_WINDOW_ACTIVATE;
		XMapRaised(dpy, i->win);
		setclientstate(i, NormalState);
	}
	else if (!(flags & XEMBED_MAPPED) && i->tags) {
		i->tags = 0;
		code = XEMBED_WINDOW_DEACTIVATE;
		XUnmapWindow(dpy, i->win);
		setclientstate(i, WithdrawnState);
	}
	else
		return;
	sendevent(i->win, xatom[Xembed], StructureNotifyMask, CurrentTime, code, 0,
			systray->win, XEMBED_EMBEDDED_VERSION);
}

unsigned int
getsystraywidth()
{
	unsigned int w = 0;
	Client *i;
	if(showsystray)
		for(i = systray->icons; i; w += i->w + systrayspacing, i = i->next) ;
	return w ? w - systrayspacing : 1;
}

void
updatesystray(int updatebar)
{
	XWindowChanges wc;
	Client *i;
	Monitor *m = systraytomon();
	unsigned int x = systraypos.x, y = systraypos.y;
	unsigned int w = 1;
	XSetWindowAttributes wa = {
		.background_pixel = 0,
		.border_pixel = 0,
		.colormap = cmap,
		.event_mask = ButtonPressMask | ExposureMask,
		.override_redirect = True,
	};

	if (!showsystray)
		return;

	if (!systray) {
		/* init systray */
		if (!(systray = (Systray *)calloc(1, sizeof(Systray))))
			die("fatal: could not malloc() %u bytes\n", sizeof(Systray));

	wa.override_redirect = True;
	wa.event_mask = ButtonPressMask|ExposureMask;
	wa.background_pixel = 0;
	wa.border_pixel = 0;
	wa.colormap = cmap;
	systray->win = XCreateWindow(dpy, DefaultRootWindow(dpy), x, y, w, systrayiconsize, 0, depth,
					InputOutput, visual,
					CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &wa);
	XSelectInput(dpy, systray->win, SubstructureNotifyMask);
	XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation], XA_CARDINAL, 32,
			PropModeReplace, (unsigned char *)&systrayorientation, 1);
	// XChangeProperty(dpy, systray->win, netatom[NetSystemTrayVisual], XA_VISUALID, 32,
	// 		PropModeReplace, (unsigned char *)&visual->visualid, 1);
	XChangeProperty(dpy, systray->win, netatom[NetWMWindowType], XA_ATOM, 32,
			PropModeReplace, (unsigned char *)&netatom[NetWMWindowTypeToolbar], 1);
	XMapRaised(dpy, systray->win);
	XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
		if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
			sendevent(DefaultRootWindow(dpy), xatom[Manager], StructureNotifyMask, CurrentTime, netatom[NetSystemTray], systray->win, 0, 0);
			XSync(dpy, False);
		}
		else {
			fprintf(stderr, "radium: unable to obtain system tray.\n");
			free(systray);
			systray = NULL;
			return;
		}
	}
	for (w = 0, i = systray->icons; i; i = i->next) {
		/* make sure the background color stays the same */
		wa.background_pixel  = 0;
		wa.border_pixel = 0;
		wa.colormap = cmap;
		// XChangeWindowAttributes(dpy, i->win, CWBackPixel, &wa);
		XMapRaised(dpy, i->win);
		i->x = w;
		i->y = 0;
        XMoveResizeWindow(dpy, i->win, i->x, i->y, i->w, i->h);
        w += i->w + systrayspacing;
		if (i->mon != m)
			i->mon = m;
	}
	w = w ? w - systrayspacing : 1;
	if (updatebar)
		ui_bar_draw(m);
	x = systraypos.x;
	y = systraypos.y;
	if (getsystraywidth() == 1 || !m->showbar)
		x = -w;
	XMoveResizeWindow(dpy, systray->win, x, y, w, systrayiconsize);
	wc.x = x; wc.y = y; wc.width = w; wc.height = systrayiconsize;
	wc.stack_mode = Above; wc.sibling = m->barwin;
	XConfigureWindow(dpy, systray->win, CWX|CWY|CWWidth|CWHeight|CWSibling|CWStackMode, &wc);
	XMapWindow(dpy, systray->win);
	XMapSubwindows(dpy, systray->win);
	XSync(dpy, False);
}

void
setupsystray(void)
{
	xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
	xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
	xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
	updatesystray(0);
}

void
cleanupsystray(void)
{
	if (!showsystray)
		return;
	XUnmapWindow(dpy, systray->win);
	XDestroyWindow(dpy, systray->win);
	free(systray);
}

void
raisesystray(Monitor *m)
{
	if (showsystray && m == systraytomon())
		XMapRaised(dpy, systray->win);
}

int
systrayclientmessage(XEvent *e)
{
	XWindowAttributes wa;
	XSetWindowAttributes swa;
	XClientMessageEvent *cme = &e->xclient;
	Client *c;

	if (!showsystray || cme->window != systray->win || cme->message_type != netatom[NetSystemTrayOP])
		return 0;
	/* add systray icons */
	if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
		if (!(c = (Client *)calloc(1, sizeof(Client))))
			die("fatal: could not malloc() %u bytes\n", sizeof(Client));
		if (!(c->win = cme->data.l[2])) {
			free(c);
			return 1;
		}
		c->mon = selmon;
		c->next = systray->icons;
		systray->icons = c;
		if (!XGetWindowAttributes(dpy, c->win, &wa)) {
			/* use sane defaults */
			wa.width = systrayiconsize;
			wa.height = systrayiconsize;
			wa.border_width = 0;
		}
		c->x = c->oldx = c->y = c->oldy = 0;
		c->w = c->oldw = wa.width;
		c->h = c->oldh = wa.height;
		c->oldbw = wa.border_width;
		c->bw = 0;
		c->isfloating = True;
		/* reuse tags field as mapped status */
		c->tags = 1;
		updatesizehints(c);
		updatesystrayicongeom(c, wa.width, wa.height);
		XAddToSaveSet(dpy, c->win);
		XSelectInput(dpy, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
		XReparentWindow(dpy, c->win, systray->win, 0, 0);
		/* use parents background color */
		swa.background_pixel  = 0;
		// XChangeWindowAttributes(dpy, c->win, CWBackPixmap, &swa);
		sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_EMBEDDED_NOTIFY, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
		/* FIXME not sure if I have to send these events, too */
		sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_FOCUS_IN, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
		sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
		sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_MODALITY_ON, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
		XSync(dpy, False);
		// resizebarwin(selmon);
		updatesystray(1);
		setclientstate(c, NormalState);
	}
	return 1;
}

int
systraydestroynotify(XEvent *e)
{
	XDestroyWindowEvent *ev = &e->xdestroywindow;
	Client *c;

	if (!(c = wintosystrayicon(ev->window)))
		return 0;
	removesystrayicon(c);
	// resizebarwin(selmon);
	updatesystray(1);
	return 1;
}

int
systraymaprequest(XEvent *e)
{
	XMapRequestEvent *ev = &e->xmaprequest;
	Client *i;

	if (!(i = wintosystrayicon(ev->window)))
		return 0;
	sendevent(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
	// resizebarwin(selmon);
	updatesystray(1);
	return 1;
}

int
systraypropertynotify(XEvent *e)
{
	XPropertyEvent *ev = &e->xproperty;
	Client *c;

	if (!(c = wintosystrayicon(ev->window)))
		return 0;
	if (ev->atom == XA_WM_NORMAL_HINTS) {
		updatesizehints(c);
		updatesystrayicongeom(c, c->w, c->h);
	}
	else
		updatesystrayiconstate(c, ev);
	// resizebarwin(selmon);
	updatesystray(1);
	return 1;
}

int
systrayresizerequest(XEvent *e)
{
	XResizeRequestEvent *ev = &e->xresizerequest;
	Client *i;

	if (!(i = wintosystrayicon(ev->window)))
		return 0;
	updatesystrayicongeom(i, ev->width, ev->height);
	// resizebarwin(selmon);
	updatesystray(1);
	return 1;
}

int
systrayunmapnotify(XEvent *e)
{
	XUnmapEvent *ev = &e->xunmap;
	Client *c;

	if (!(c = wintosystrayicon(ev->window)))
		return 0;
	/* KLUDGE! sometimes icons occasionally unmap their windows, but do
	* _not_ destroy them. We map those windows back */
	XMapRaised(dpy, c->win);
	updatesystray(1);
	return 1;
}
