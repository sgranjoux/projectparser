/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta-token.h
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

#ifndef _ANJUTA_TOKEN_H_
#define _ANJUTA_TOKEN_H_

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

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
	ANJUTA_TOKEN_STATIC 							= 1 << 25,
	ANJUTA_TOKEN_REMOVED						= 1 << 26,
	ANJUTA_TOKEN_ADDED							= 1 << 27
	
} AnjutaTokenType;

typedef struct _AnjutaToken AnjutaToken;

typedef struct _AnjutaTokenRange
{
	AnjutaToken *first;
	AnjutaToken *last;
} AnjutaTokenRange;

enum AnjutaTokenSearchFlag
{
	ANJUTA_SEARCH_OVER	  = 0,
	ANJUTA_SEARCH_INTO		= 1 << 0,
	ANJUTA_SEARCH_ALL	   = 1 << 1
};

AnjutaToken *anjuta_token_new_string (AnjutaTokenType type, const gchar *value);
AnjutaToken *anjuta_token_new_static (AnjutaTokenType type, const gchar *value);
AnjutaToken *anjuta_token_new_fragment (AnjutaTokenType type, const gchar *pos, gsize length);

void anjuta_token_free (AnjutaToken *token);

AnjutaToken *anjuta_token_insert_after (AnjutaToken *token, AnjutaToken *sibling);
void anjuta_token_foreach (AnjutaToken *token, GFunc func, gpointer user_data);
gboolean anjuta_token_match (AnjutaToken *token, gint flags, AnjutaToken *sequence, AnjutaToken **end);
gboolean anjuta_token_remove (AnjutaToken *token, AnjutaToken *end);

void anjuta_token_set_type (AnjutaToken *token, gint type);
void anjuta_token_set_flags (AnjutaToken *token, gint flags);
void anjuta_token_clear_flags (AnjutaToken *token, gint flags);

gchar *anjuta_token_evaluate (AnjutaToken *start, AnjutaToken *end);

AnjutaToken *anjuta_token_next (AnjutaToken *token);
AnjutaToken *anjuta_token_previous (AnjutaToken *token);
gboolean anjuta_token_compare (AnjutaToken *tokena, AnjutaToken *tokenb);


gint anjuta_token_get_type (AnjutaToken *token);
gint anjuta_token_get_flags (AnjutaToken *token);
gchar* anjuta_token_get_value (AnjutaToken *token);



#define ANJUTA_TOKEN_FILE_TYPE                     (anjuta_token_file_get_type ())
#define ANJUTA_TOKEN_FILE(obj)                      (G_TYPE_CHECK_INSTANCE_CAST ((obj), ANJUTA_TOKEN_FILE_TYPE, AnjutaTokenFile))
#define ANJUTA_TOKEN_FILE_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), ANJUTA_TOKEN_FILE_TYPE, AnjutaTokenFileClass))
#define IS_ANJUTA_TOKEN_FILE(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ANJUTA_TOKEN_FILE_TYPE))
#define IS_ANJUTA_TOKEN_FILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), ANJUTA_TOKEN_FILE_TYPE))
#define GET_ANJUTA_TOKEN_FILE_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), ANJUTA_TOKEN_FILE_TYPE, AnjutaTokenFileClass))

typedef struct _AnjutaTokenFile AnjutaTokenFile;
typedef struct _AnjutaTokenFileClass AnjutaTokenFileClass;

GType anjuta_token_file_get_type (void);

AnjutaTokenFile *anjuta_token_file_new (GFile *file);
void anjuta_token_file_free (AnjutaTokenFile *file);

const gchar* anjuta_token_file_get_content (AnjutaTokenFile *file, GError **error);
void anjuta_token_file_move (AnjutaTokenFile *file, GFile *new_file);
gboolean anjute_token_file_save (AnjutaTokenFile *file, GError **error);

void anjuta_token_file_append (AnjutaTokenFile *file, AnjutaToken *token);
AnjutaToken* anjuta_token_file_first (AnjutaTokenFile *file);
AnjutaToken* anjuta_token_file_last (AnjutaTokenFile *file);
GFile *anjuta_token_file_get_file (AnjutaTokenFile *file);

G_END_DECLS

#endif
