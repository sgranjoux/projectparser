/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * tokens.h
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

#ifndef _TOKENS_H_
#define _TOKENS_H_

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _AnjutaTokenVTable AnjutaTokenVTable;

typedef enum
{
	ANJUTA_TOKEN_NONE 							= 0,
	ANJUTA_TOKEN_EOL							= '\n',

	ANJUTA_TOKEN_TYPE 							= 0xFFFF,
	
	ANJUTA_TOKEN_FILE 								= 16384,
	ANJUTA_TOKEN_KEYWORD,
	ANJUTA_TOKEN_OPERATOR,
	ANJUTA_TOKEN_NAME,
	ANJUTA_TOKEN_MACRO,
	ANJUTA_TOKEN_VARIABLE,
	ANJUTA_TOKEN_COMMENT,
	ANJUTA_TOKEN_SPACE,
	ANJUTA_TOKEN_ERROR,
	ANJUTA_TOKEN_USER,
		
	ANJUTA_TOKEN_FLAGS 							= 0xFFFF << 16,
	
	ANJUTA_TOKEN_PUBLIC_FLAGS 				= 0x00FF << 16,
	
	ANJUTA_TOKEN_IRRELEVANT 					= 1 << 16,
	ANJUTA_TOKEN_OPEN 							= 1 << 17,
	ANJUTA_TOKEN_CLOSE 							= 1 << 18,
	ANJUTA_TOKEN_NEXT 							= 1 << 19,
	ANJUTA_TOKEN_SIGNIFICANT					= 1 << 20,

	ANJUTA_TOKEN_PRIVATE_FLAGS 			= 0x00FF << 24,
	
	ANJUTA_TOKEN_CASE_INSENSITIVE 		= 1 << 24,
	ANJUTA_TOKEN_STATIC 							= 1 << 25
	
} AnjutaTokenType;

enum AnjutaTokenSearchFlag
{
	ANJUTA_SEARCH_OVER	  = 0,
	ANJUTA_SEARCH_INTO		= 1 << 0,
	ANJUTA_SEARCH_ALL	   = 1 << 1
};

typedef struct _AnjutaToken AnjutaToken;
typedef struct _AnjutaToken AnjutaTokenFile;

typedef struct _AnjutaTokenRange
{
	AnjutaToken *first;
	AnjutaToken *last;
} AnjutaTokenRange;

AnjutaToken *anjuta_token_new_string (AnjutaTokenType type, const char *value);
AnjutaToken *anjuta_token_new_static (AnjutaTokenType type, const char *value);
AnjutaToken *anjuta_token_new_fragment (AnjutaTokenType type, const gchar *pos, gsize length);

AnjutaToken *anjuta_token_new_file (GFile *file, GError **error);

void anjuta_token_unref (AnjutaToken *token);

void anjuta_token_insert_after (AnjutaToken *token, AnjutaToken *sibling);
void anjuta_token_foreach (AnjutaToken *token, GFunc func, gpointer user_data);
gboolean anjuta_token_match (AnjutaToken *token, gint flags, AnjutaToken *sequence, AnjutaToken **end);

void anjuta_token_set_type (AnjutaToken *token, gint type);
void anjuta_token_set_flags (AnjutaToken *token, gint flags);
void anjuta_token_clear_flags (AnjutaToken *token, gint flags);

gchar *anjuta_token_evaluate (AnjutaToken *start, AnjutaToken *end);

AnjutaToken *anjuta_token_next (AnjutaToken *token);
AnjutaToken *anjuta_token_previous (AnjutaToken *token);
gboolean anjuta_token_compare (AnjutaToken *tokena, AnjutaToken *tokenb);

const gchar* anjuta_token_file_get_content (AnjutaTokenFile *tAnjutaTokenoken);
gint anjuta_token_get_type (AnjutaToken *token);
gint anjuta_token_get_flags (AnjutaToken *token);

void pm_token_dump (AnjutaToken *token);

G_END_DECLS

#endif
