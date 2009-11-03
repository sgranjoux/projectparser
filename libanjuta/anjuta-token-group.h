/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta-token-group.h
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

#ifndef _ANJUTA_TOKEN_GROUP_H_
#define _ANJUTA_TOKEN_GROUP_H_

#include "anjuta-token.h"

G_BEGIN_DECLS

typedef struct _AnjutaTokenGroup AnjutaTokenGroup;

AnjutaTokenGroup *anjuta_token_group_new (AnjutaTokenType type, AnjutaTokenGroup* first);
void anjuta_token_group_free (AnjutaTokenGroup *group);

AnjutaTokenGroup *anjuta_token_group_first (AnjutaTokenGroup *list);
AnjutaTokenGroup *anjuta_token_group_next (AnjutaTokenGroup *item);

AnjutaTokenGroup *anjuta_token_group_append (AnjutaTokenGroup *parent, AnjutaTokenGroup *group);
AnjutaTokenGroup *anjuta_token_group_append_token (AnjutaTokenGroup *parent, AnjutaToken *token);
AnjutaTokenGroup *anjuta_token_group_append_children (AnjutaTokenGroup *parent, AnjutaTokenGroup *children);

void anjuta_token_group_dump (AnjutaTokenGroup *group);
gchar *anjuta_token_group_evaluate (AnjutaTokenGroup *group);

AnjutaToken *anjuta_token_group_into_token (AnjutaTokenGroup *group);
AnjutaToken *anjuta_token_group_get_token (AnjutaTokenGroup *group);

G_END_DECLS

#endif
