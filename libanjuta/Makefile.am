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
	anjuta-token.c \
	anjuta-token.h \
	anjuta-debug.c \
	anjuta-debug.h \
	anjuta-utils.c \
	anjuta-utils.h \
	anjuta-token-list.h \
	anjuta-token-list.c \
	anjuta-token-file.h \
	anjuta-token-file.c \
	anjuta-project.c \
	anjuta-project.h \
	anjuta-token-stream.c \
	anjuta-token-stream.h \
    interfaces/ianjuta-project.c \
    interfaces/ianjuta-project.h \
    interfaces/libanjuta-iface-marshallers.c \
    interfaces/libanjuta-iface-marshallers.h

-include $(top_srcdir)/git.mk
