## Process this file with automake to produce Makefile.in
## Created by Anjuta

SUBDIRS = libanjuta src po

projectparserdocdir = ${prefix}/doc/projectparser
projectparserdoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog\
	INSTALL\
	NEWS


INTLTOOL_FILES = intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

EXTRA_DIST = $(projectparserdoc_DATA) \
	$(INTLTOOL_FILES)

DISTCLEANFILES = intltool-extract \
	intltool-merge \
	intltool-update \
	po/.intltool-merge-cache


# Remove doc directory on uninstall
uninstall-local:
	-rm -r $(projectparserdocdir)

-include $(top_srcdir)/git.mk
