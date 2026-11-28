#pragma once

#include <X11/Xlib.h>

#include "../../core.h"
#include "../../util.h"

enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */

extern Atom xatom[XLast];
extern Vec2 systraypos;
extern unsigned int systrayiconsize;
extern unsigned int systrayspacing;
extern int systraymonitormode;

void setupsystray(void);
void cleanupsystray(void);
unsigned int getsystraywidth();
void raisesystray(Monitor *m);
void updatesystray(int updatebar);
int systrayclientmessage(XEvent *e);
int systraydestroynotify(XEvent *e);
int systraymaprequest(XEvent *e);
int systraypropertynotify(XEvent *e);
int systrayresizerequest(XEvent *e);
int systrayunmapnotify(XEvent *e);
