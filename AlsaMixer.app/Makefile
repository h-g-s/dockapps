#
# AlsaMixer.app Makefile
#
# Copyright (c) 1998-2002 Per Liden
# Copyright (C) 2004, Petr Hlavka
#
# SPDX-License-Identifier: GPL-2.0+

PREFIX ?= /usr/local
DESTDIR ?=
bindir ?= $(PREFIX)/bin
GNUSTEP_BINDIR ?= $(PREFIX)/GNUstep/Apps/AlsaMixer.app

CXX ?= c++
PKG_CONFIG ?= pkg-config
PKG_PACKAGES = x11 xext xpm alsa

CPPFLAGS += $(shell $(PKG_CONFIG) --cflags $(PKG_PACKAGES) 2>/dev/null)
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic
LDLIBS += $(shell $(PKG_CONFIG) --libs $(PKG_PACKAGES) 2>/dev/null)

OBJECTS = Main.o Mixer.o Xpm.o AMixer/AMixer.o AMixer/AItem.o AMixer/AChannel.o
DEPFILES = $(OBJECTS:.o=.d)

.PHONY: all check-deps install install-gnustep clean distclean

all: check-deps AlsaMixer.app

check-deps:
	@$(PKG_CONFIG) --exists $(PKG_PACKAGES) || { \
		echo "Missing development packages; install pkg-config metadata for: $(PKG_PACKAGES)" >&2; \
		exit 1; \
	}

AlsaMixer.app: $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@ $(LDLIBS)

%.o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c -o $@ $<

-include $(DEPFILES)

install: all
	install -d $(DESTDIR)$(bindir)
	install -m 0755 AlsaMixer.app $(DESTDIR)$(bindir)/AlsaMixer.app

install-gnustep: all
	install -d  $(DESTDIR)$(GNUSTEP_BINDIR)
	install -m 0755 AlsaMixer.app $(DESTDIR)$(GNUSTEP_BINDIR)/AlsaMixer

clean:
	rm -f *~ $(OBJECTS) $(DEPFILES) AlsaMixer.app

distclean: clean

# End of file
