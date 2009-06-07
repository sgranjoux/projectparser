INCLUDES = \
	$(WARN_CFLAGS) \
	$(GTK_CFLAGS) \
	$(DEPRECATED_FLAGS) \
	$(LIBANJUTA_CFLAGS) \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"libanjuta-interfaces\" \
	"-DG_LOG_FATAL_MASK=\"(G_LOG_FLAG_RECURSION|G_LOG_LEVEL_ERROR|G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING)\""

MODULE_NAME = libanjuta
MODULE_VERSION = 0.1
MODULE_INCLUDEDIR = $(includedir)/libanjuta-1.0/libanjuta/interfaces
MODULE_LIBS = $(GTK_LIBS)

noinst_LTLIBRARIES = libanjuta-interfaces.la
libanjuta_interfaces_la_LIBADD = $(MODULE_LIBS)
libanjuta_interfaces_la_SOURCES = \
	ianjuta-buildable.c\
	ianjuta-builder.c\
	ianjuta-debug-manager.c\
	ianjuta-debugger.c\
	ianjuta-debugger-breakpoint.c\
	ianjuta-debugger-instruction.c\
	ianjuta-debugger-memory.c\
	ianjuta-debugger-register.c\
	ianjuta-debugger-variable.c\
	ianjuta-document.c\
	ianjuta-document-manager.c\
	ianjuta-editor.c\
	ianjuta-editor-assist.c\
	ianjuta-editor-cell.c\
	ianjuta-editor-cell-style.c\
	ianjuta-editor-comment.c\
	ianjuta-editor-convert.c\
	ianjuta-editor-factory.c\
	ianjuta-editor-folds.c\
	ianjuta-editor-goto.c\
	ianjuta-editor-hover.c\
	ianjuta-editor-language.c\
	ianjuta-editor-line-mode.c\
	ianjuta-editor-search.c\
	ianjuta-editor-selection.c\
	ianjuta-editor-view.c\
	ianjuta-editor-zoom.c\
	ianjuta-environment.c\
	ianjuta-file.c\
	ianjuta-file-loader.c\
	ianjuta-file-manager.c\
	ianjuta-file-savable.c\
	ianjuta-help.c\
	ianjuta-indicable.c\
	ianjuta-iterable.c\
	ianjuta-iterable-tree.c\
	ianjuta-language.c\
	ianjuta-language-support.c\
	ianjuta-loader.c\
	ianjuta-macro.c\
	ianjuta-markable.c\
	ianjuta-message-manager.c\
	ianjuta-message-view.c\
	ianjuta-plugin-factory.c\
	ianjuta-preferences.c\
	ianjuta-print.c\
	ianjuta-project-backend.c\
	ianjuta-project-manager.c\
	ianjuta-stream.c\
	ianjuta-stream-loader.c\
	ianjuta-stream-savable.c\
	ianjuta-symbol.c\
	ianjuta-symbol-manager.c\
	ianjuta-terminal.c\
	ianjuta-todo.c\
	ianjuta-vcs.c\
	ianjuta-wizard.c\
	libanjuta-iface-marshallers.c
libanjuta_interfaces_includedir = $(MODULE_INCLUDEDIR)
libanjuta_interfaces_include = \
	ianjuta-buildable.h\
	ianjuta-builder.h\
	ianjuta-debug-manager.h\
	ianjuta-debugger.h\
	ianjuta-debugger-breakpoint.h\
	ianjuta-debugger-instruction.h\
	ianjuta-debugger-memory.h\
	ianjuta-debugger-register.h\
	ianjuta-debugger-variable.h\
	ianjuta-document.h\
	ianjuta-document-manager.h\
	ianjuta-editor.h\
	ianjuta-editor-assist.h\
	ianjuta-editor-cell.h\
	ianjuta-editor-cell-style.h\
	ianjuta-editor-comment.h\
	ianjuta-editor-convert.h\
	ianjuta-editor-factory.h\
	ianjuta-editor-folds.h\
	ianjuta-editor-goto.h\
	ianjuta-editor-hover.h\
	ianjuta-editor-language.h\
	ianjuta-editor-line-mode.h\
	ianjuta-editor-search.h\
	ianjuta-editor-selection.h\
	ianjuta-editor-view.h\
	ianjuta-editor-zoom.h\
	ianjuta-environment.h\
	ianjuta-file.h\
	ianjuta-file-loader.h\
	ianjuta-file-manager.h\
	ianjuta-file-savable.h\
	ianjuta-help.h\
	ianjuta-indicable.h\
	ianjuta-iterable.h\
	ianjuta-iterable-tree.h\
	ianjuta-language.h\
	ianjuta-language-support.h\
	ianjuta-loader.h\
	ianjuta-macro.h\
	ianjuta-markable.h\
	ianjuta-message-manager.h\
	ianjuta-message-view.h\
	ianjuta-plugin-factory.h\
	ianjuta-preferences.h\
	ianjuta-print.h\
	ianjuta-project-backend.h\
	ianjuta-project-manager.h\
	ianjuta-stream.h\
	ianjuta-stream-loader.h\
	ianjuta-stream-savable.h\
	ianjuta-symbol.h\
	ianjuta-symbol-manager.h\
	ianjuta-terminal.h\
	ianjuta-todo.h\
	ianjuta-vcs.h\
	ianjuta-wizard.h\
	libanjuta-iface-marshallers.h\
	libanjuta-interfaces.h


BUILT_SOURCES = iface-built.stamp

libanjuta_interfaces_include_HEADERS =  $(libanjuta_interfaces_include)

iface-built.stamp: libanjuta.idl Makefile.am.iface anjuta-idl-compiler.pl
	savedir=`pwd` && \
	cd $(srcdir) && perl $(srcdir)/anjuta-idl-compiler.pl libanjuta && \
	echo 'GInterface' > hierarchy.txt && grep -e '^[[:space:]|\t]*interface' \
	libanjuta.idl  | sed -e 's/\binterface\b\s*/\t/' | sed -e 's/\t/    /g' \
	>> hierarchy.txt && \
	touch $(srcdir)/iface-built.stamp && \
	echo "changing to $$savedir" && \
	make && \
	cd $$savedir
	
files_idl = libanjuta.idl
idldir = $(MODULE_INCLUDEDIR)
idl_HEADERS = $(files_idl)

EXTRA_DIST = \
	iface-built.stamp \
	$(libanjuta_interfaces_include) \
	$(files_idl) \
	Makefile.am.iface \
	anjuta-idl-compiler.pl
