include config.mk

SRCDIR = src
LIBDIR = lib
LUADIR = lua
BUILDDIR = build
DOCSDIR = docs
BIN = $(BUILDDIR)/radium

SRC = $(wildcard $(SRCDIR)/*.c)
LIBSRC = $(wildcard $(LIBDIR)/*/*.c)
OBJ = $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SRC)) \
	$(patsubst %.c, $(BUILDDIR)/%.o, $(LIBSRC))

all: $(BIN)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILDDIR)/$(LIBDIR)/%.o: $(LIBDIR)/%.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $< -o $@

$(BIN): $(OBJ) | $(BUILDDIR)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) radium-$(VERSION).tar.gz

dist: clean
	mkdir -p radium-$(VERSION)
	cp -R LICENSE Makefile README.md config.mk $(DOCSDIR)/radium.1\
		$(SRC) radium-$(VERSION)
	tar -cf radium-$(VERSION).tar radium-$(VERSION)
	gzip radium-$(VERSION).tar
	rm -rf radium-$(VERSION)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(BIN) $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/radium
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < $(DOCSDIR)/radium.1 > $(DESTDIR)$(MANPREFIX)/man1/radium.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/radium.1
	rm -rf $(DESTDIR)$(DATAPREFIX)/lua
	mkdir -p $(DESTDIR)$(DATAPREFIX)
	cp -R $(LUADIR) $(DESTDIR)$(DATAPREFIX)
	chmod -R u=rwX,go=rX $(DESTDIR)$(DATAPREFIX)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/radium\
		$(DESTDIR)$(MANPREFIX)/man1/radium.1
	rm -rf $(DESTDIR)$(DATAPREFIX)

.PHONY: all clean dist install uninstall
