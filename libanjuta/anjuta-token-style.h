/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta-token-style.h
 * Copyright (C) Sébastien Granjoux 2009 <seb.sfo@free.fr>
 * 
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ANJUTA_TOKEN_STYLE_H_
#define _ANJUTA_TOKEN_STYLE_H_

#include <glib.h>

#include "anjuta-token.h"

G_BEGIN_DECLS

typedef struct _AnjutaTokenStyle AnjutaTokenStyle;

AnjutaTokenStyle *anjuta_token_style_new (guint line_width);
void anjuta_token_style_free (AnjutaTokenStyle *style);

void anjuta_token_style_set_eol (AnjutaTokenStyle *style, const gchar *eol);

void anjuta_token_style_update (AnjutaTokenStyle *style, AnjutaToken *token);
void anjuta_token_style_format (AnjutaTokenStyle *style, AnjutaToken *token);

G_END_DECLS

#endif
