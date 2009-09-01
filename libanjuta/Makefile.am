## Process this file with automake to produce Makefile.in

## Created by Anjuta

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GCONF_CFLAGS) \
	-DDEBUG \
	-DDATADIR=\""$(datadir)/projectparser"\"

AM_CFLAGS =\
	 -Wall\
	 -g

noinst_LIBRARIES = libanjuta.a

libanjuta_a_SOURCES = \
	gbf-project.c \
	gbf-project.h \
	anjuta-token.c \
	anjuta-token.h \
	anjuta-debug.c \
	anjuta-debug.h \
	anjuta-utils.c \
	anjuta-utils.h \
	anjuta-token-style.h \
	anjuta-token-style.c \
	anjuta-token-file.h \
	anjuta-token-file.c

-include $(top_srcdir)/git.mk
