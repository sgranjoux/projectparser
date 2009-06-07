## Process this file with automake to produce Makefile.in

## Created by Anjuta

AM_CPPFLAGS = \
	-DDEBUG \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

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
	anjuta-debug.h

-include $(top_srcdir)/git.mk
