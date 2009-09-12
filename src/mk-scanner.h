/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * mk-scanner.h
 * Copyright (C) Sébastien Granjoux 2009 <seb.sfo@free.fr>
 * 
 * main.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * main.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MK_SCANNER_H_
#define _MK_SCANNER_H_

#include "libanjuta/anjuta-token.h"
#include "libanjuta/anjuta-token-file.h"

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _MkpScanner MkpScanner;

MkpScanner *mkp_scanner_new (void);
void mkp_scanner_free (MkpScanner *scanner);

gboolean mkp_scanner_parse (MkpScanner *scanner, AnjutaTokenFile *file, GError **error);

const gchar* mkp_scanner_get_filename (MkpScanner *scanner);

typedef enum
{
	MK_TOKEN_SUBDIRS = ANJUTA_TOKEN_USER,
	MK_TOKEN_DIST_SUBDIRS,
	MK_TOKEN__DATA,
	MK_TOKEN__HEADERS,
	MK_TOKEN__LIBRARIES,
	MK_TOKEN__LISP,
	MK_TOKEN__LTLIBRARIES,
	MK_TOKEN__MANS,
	MK_TOKEN__PROGRAMS,
	MK_TOKEN__PYTHON,
	MK_TOKEN__SCRIPTS,
	MK_TOKEN__SOURCES,
	MK_TOKEN__TEXINFOS,
	MK_TOKEN__JAVA
} MakeTokenType;

G_END_DECLS

#endif
