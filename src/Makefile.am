## Process this file with automake to produce Makefile.in

## Created by Anjuta

AM_CPPFLAGS = \
	-DDEBUG \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-I$(srcdir)/.. \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

AM_CFLAGS =\
	 -Wall\
	 -g

bin_PROGRAMS = projectparser

projectparser_SOURCES = \
	main.c \
	am-project.c \
	am-project.h \
	am-scanner.l \
	am-parser.y \
	am-scanner.h \
	ac-scanner.l \
	ac-parser.y \
	ac-scanner.h

projectparser_LDFLAGS = 

projectparser_LDADD = \
	../libanjuta/libanjuta.a \
	$(GLIB_LIBS) \
	$(GTK_LIBS)

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

project.c: ac-scanner.h am-scanner.h

EXTRA_DIST = ac-parser.h am-parser.h

-include $(top_srcdir)/git.mk
