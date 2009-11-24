## Process this file with automake to produce Makefile.in

## Created by Anjuta

uidir = $(datadir)/projectparser/glade
ui_DATA = am-project.ui

AM_CPPFLAGS = \
	-DDEBUG \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)/projectparser"\" \
	-I$(srcdir)/.. \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GCONF_CFLAGS)

AM_CFLAGS =\
	 -Wall\
	 -g

bin_PROGRAMS = projectparser

projectparser_SOURCES = \
	main.c \
	am-project.c \
	am-project.h \
	am-project-private.h \
	am-scanner.l \
	am-parser.y \
	am-scanner.h \
	ac-scanner.l \
	ac-parser.y \
	ac-scanner.h \
	ac-writer.h \
	ac-writer.c \
	am-writer.h \
	am-writer.c \
	am-dialogs.c \
	am-dialogs.h \
	mk-project.c \
	mk-project.h \
	mk-project-private.h \
	mk-parser.y \
	mk-scanner.l \
	mk-scanner.h \
	mk-rule.c \
	mk-rule.h

projectparser_LDFLAGS = 

projectparser_LDADD = \
	../libanjuta/libanjuta.a \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(GCONF_LIBS)

AM_YFLAGS = -t -v -g -rall

ac-scanner.c: $(srcdir)/ac-scanner.l ac-parser.c
	$(LEXCOMPILE) -o $@ $<

ac-parser.c: $(srcdir)/ac-parser.y
	$(YACCCOMPILE) -o $@ $<

ac-scanner.h: ac-parser.c

am-scanner.c: $(srcdir)/am-scanner.l am-parser.c
	$(LEXCOMPILE) -o $@ $<

am-parser.c: $(srcdir)/am-parser.y
	$(YACCCOMPILE) -o $@ $<

am-scanner.h: am-parser.c

mk-scanner.c: $(srcdir)/mk-scanner.l mk-parser.c
	$(LEXCOMPILE) -o $@ $<

mk-parser.c: $(srcdir)/mk-parser.y
	$(YACCCOMPILE) -o $@ $<

mk-scanner.h: mk-parser.c

am-project.c: ac-scanner.h am-scanner.h

mk-project.c: mk-scanner.h

EXTRA_DIST = ac-parser.h am-parser.h mk-parser.h

-include $(top_srcdir)/git.mk
