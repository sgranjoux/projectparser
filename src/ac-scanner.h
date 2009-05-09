/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ac-scanner.h
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

#ifndef _AC_SCANNER_H_
#define _AC_SCANNER_H_

#include "libanjuta/anjuta-token.h"


#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

//#define YYSTYPE AnjutaTokenRange

typedef struct _AmpAcScanner AmpAcScanner;

AmpAcScanner *amp_ac_scanner_new (void);
void amp_ac_scanner_free (AmpAcScanner *scanner);

AnjutaToken *amp_ac_scanner_parse (AmpAcScanner *scanner, GFile *file);
gboolean amp_ac_scanner_read (AmpAcScanner *scanner, GFile *file);
AnjutaToken* amp_ac_scanner_get_parse_tree (AmpAcScanner *scanner);

G_END_DECLS

#endif
