# Plugin glade file
anjuta_gladedir = $(anjuta_glade_dir)
anjuta_glade_DATA = anjuta-build-file.glade

# Plugin UI file
anjuta_uidir = $(anjuta_ui_dir)
anjuta_ui_DATA = anjuta-build-file.ui

# Plugin Icon file
anjuta_pixmapsdir = $(anjuta_image_dir)
anjuta_pixmaps_DATA = \
	anjuta-build-file-48.png

# Plugin description file
plugin_in_files = anjuta-build-file.plugin.in
%.plugin: %.plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

anjuta_plugindir = $(anjuta_plugin_dir)
anjuta_plugin_DATA = $(plugin_in_files:.plugin.in=.plugin)

# Where to install the plugin
plugindir = $(anjuta_plugin_dir)
plugin_LTLIBRARIES = libanjuta-build-file.la

# Include paths
AM_CPPFLAGS = \
	$(WARN_CFLAGS) \
	$(DEPRECATED_FLAGS) \
	$(LIBANJUTA_CFLAGS)

libanjuta_build_file_la_LDFLAGS = $(ANJUTA_PLUGIN_LDFLAGS)

# Plugin dependencies
libanjuta_build_file_la_LIBADD = \
	$(GLADE_LIBS) \
	$(GNOME_LIBS) \
	$(LIBANJUTA_LIBS)

# Plugin sources
libanjuta_build_file_la_SOURCES = \
	plugin.c

EXTRA_DIST = \
	$(plugin_in_files) \
	$(anjuta_pixmaps_DATA) \
	$(anjuta_plugin_DATA) \
	$(anjuta_ui_DATA) \
	$(anjuta_glade_DATA)
