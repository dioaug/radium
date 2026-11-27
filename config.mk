# radium version
VERSION = 0.1

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man
DATAPREFIX = ${PREFIX}/share/radium

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# Xinerama, comment if you don't want it
XINERAMALIBS  = -lXinerama
XINERAMAFLAGS = -DXINERAMA

# freetype
FREETYPELIBS = -lfontconfig -lXft
FREETYPEINC = /usr/include/freetype2

# Lua, the pkg-config name of the version to use
LUA = lua5.4

PKG_CONFIG_CFLAGS = $(shell pkg-config --cflags pangocairo pangoft2 $(LUA))
PKG_CONFIG_LIBS   = $(shell pkg-config --libs pangocairo pangoft2 $(LUA))

# includes and libs
INCS = -I${X11INC} -I${FREETYPEINC} ${PKG_CONFIG_CFLAGS}
LIBS = -L${X11LIB} -lX11 -lXext ${XINERAMALIBS} ${FREETYPELIBS} -lXrender -lcairo -lm ${PKG_CONFIG_LIBS}

# flags
CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700L -DVERSION=\"${VERSION}\" -DLUALIBDIR=\"${DATAPREFIX}/lua\" ${XINERAMAFLAGS}
#CFLAGS   = -g -std=c99 -pedantic -Wall -O0 ${INCS} ${CPPFLAGS}
CFLAGS   = -std=c99 -pedantic -Wall -Wno-deprecated-declarations -Os ${INCS} ${CPPFLAGS}
LDFLAGS  = ${LIBS}

# compiler and linker
CC = gcc
