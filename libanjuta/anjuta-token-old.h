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

#ifndef _ANJUTA_TOKEN_OLD_H_
#define _ANJUTA_TOKEN_OLD_H_

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
	ANJUTA_TOKEN_OLD_NONE 							= 0,
	ANJUTA_TOKEN_OLD_EOL							= '\n',

	ANJUTA_TOKEN_OLD_TYPE 							= 0xFFFF,
	
	ANJUTA_TOKEN_OLD_FILE 								= 16384,
	ANJUTA_TOKEN_OLD_KEYWORD,
	ANJUTA_TOKEN_OLD_OPERATOR,
	ANJUTA_TOKEN_OLD_NAME,
	ANJUTA_TOKEN_OLD_MACRO,
	ANJUTA_TOKEN_OLD_VARIABLE,
	ANJUTA_TOKEN_OLD_COMMENT,
	ANJUTA_TOKEN_OLD_SPACE,
	ANJUTA_TOKEN_OLD_ERROR,
	ANJUTA_TOKEN_OLD_USER,
		
	ANJUTA_TOKEN_OLD_FLAGS 							= 0xFFFF << 16,
	
	ANJUTA_TOKEN_OLD_PUBLIC_FLAGS 				= 0x00FF << 16,
	
	ANJUTA_TOKEN_OLD_IRRELEVANT 					= 1 << 16,
	ANJUTA_TOKEN_OLD_OPEN 							= 1 << 17,
	ANJUTA_TOKEN_OLD_CLOSE 							= 1 << 18,
	ANJUTA_TOKEN_OLD_NEXT 							= 1 << 19,
	ANJUTA_TOKEN_OLD_SIGNIFICANT					= 1 << 20,

	ANJUTA_TOKEN_OLD_PRIVATE_FLAGS 			= 0x00FF << 24,
	
	ANJUTA_TOKEN_OLD_CASE_INSENSITIVE 		= 1 << 24,
	ANJUTA_TOKEN_OLD_STATIC 							= 1 << 25,
	ANJUTA_TOKEN_OLD_REMOVED						= 1 << 26,
	ANJUTA_TOKEN_OLD_ADDED							= 1 << 27
	
} AnjutaTokenOldType;

typedef struct _AnjutaTokenOld AnjutaTokenOld;

typedef struct _AnjutaTokenOldRange
{
	AnjutaTokenOld *first;
	AnjutaTokenOld *last;
} AnjutaTokenOldRange;

enum AnjutaTokenOldSearchFlag
{
	ANJUTA_SEARCH_OVER	  = 0,
	ANJUTA_SEARCH_INTO		= 1 << 0,
	ANJUTA_SEARCH_ALL	   = 1 << 1,
	ANJUTA_SEARCH_BACKWARD = 1 << 2
};

typedef struct _AnjutaTokenOldStyle AnjutaTokenOldStyle;

AnjutaTokenOld *anjuta_token_old_new_string (AnjutaTokenOldType type, const gchar *value);
AnjutaTokenOld *anjuta_token_old_new_static (AnjutaTokenOldType type, const gchar *value);
AnjutaTokenOld *anjuta_token_old_new_fragment (AnjutaTokenOldType type, const gchar *pos, gsize length);

void anjuta_token_old_free (AnjutaTokenOld *token);

AnjutaTokenOld *anjuta_token_old_copy (AnjutaTokenOld *token);
AnjutaTokenOld *anjuta_token_old_copy_include_range (AnjutaTokenOld *token, AnjutaTokenOld *end);
AnjutaTokenOld *anjuta_token_old_copy_exclude_range (AnjutaTokenOld *token, AnjutaTokenOld *end);
AnjutaTokenOld *anjuta_token_old_insert_after (AnjutaTokenOld *token, AnjutaTokenOld *sibling);
void anjuta_token_old_foreach (AnjutaTokenOld *token, GFunc func, gpointer user_data);
gboolean anjuta_token_old_match (AnjutaTokenOld *token, gint flags, AnjutaTokenOld *sequence, AnjutaTokenOld **end);
gboolean anjuta_token_old_remove (AnjutaTokenOld *token, AnjutaTokenOld *end);
gboolean anjuta_token_old_free_range (AnjutaTokenOld *token, AnjutaTokenOld *end);
GList *anjuta_token_old_split_list (AnjutaTokenOld *token);

void anjuta_token_old_set_type (AnjutaTokenOld *token, gint type);
void anjuta_token_old_set_flags (AnjutaTokenOld *token, gint flags);
void anjuta_token_old_clear_flags (AnjutaTokenOld *token, gint flags);

gchar *anjuta_token_old_evaluate (AnjutaTokenOld *start, AnjutaTokenOld *end);

AnjutaTokenOld *anjuta_token_old_next (AnjutaTokenOld *token);
AnjutaTokenOld *anjuta_token_old_previous (AnjutaTokenOld *token);
gboolean anjuta_token_old_compare (AnjutaTokenOld *tokena, AnjutaTokenOld *tokenb);


gint anjuta_token_old_get_type (AnjutaTokenOld *token);
gint anjuta_token_old_get_flags (AnjutaTokenOld *token);
gchar* anjuta_token_old_get_value (AnjutaTokenOld *token);
gchar* anjuta_token_old_get_value_range (AnjutaTokenOld *token, AnjutaTokenOld *end);


AnjutaTokenOldStyle *anjuta_token_old_style_new (guint line_width);
void anjuta_token_old_style_free (AnjutaTokenOldStyle *style);

void anjuta_token_old_style_update (AnjutaTokenOldStyle *style, AnjutaTokenOld *token);
void anjuta_token_old_style_format (AnjutaTokenOldStyle *style, AnjutaTokenOld *token);


#define ANJUTA_TOKEN_OLD_FILE_TYPE                     (anjuta_token_old_file_get_type ())
#define ANJUTA_TOKEN_OLD_FILE(obj)                      (G_TYPE_CHECK_INSTANCE_CAST ((obj), ANJUTA_TOKEN_OLD_FILE_TYPE, AnjutaTokenOldFile))
#define ANJUTA_TOKEN_OLD_FILE_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), ANJUTA_TOKEN_OLD_FILE_TYPE, AnjutaTokenOldFileClass))
#define IS_ANJUTA_TOKEN_OLD_FILE(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ANJUTA_TOKEN_OLD_FILE_TYPE))
#define IS_ANJUTA_TOKEN_OLD_FILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), ANJUTA_TOKEN_OLD_FILE_TYPE))
#define GET_ANJUTA_TOKEN_OLD_FILE_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), ANJUTA_TOKEN_OLD_FILE_TYPE, AnjutaTokenOldFileClass))

typedef struct _AnjutaTokenOldFile AnjutaTokenOldFile;
typedef struct _AnjutaTokenOldFileClass AnjutaTokenOldFileClass;

GType anjuta_token_old_file_get_type (void);

AnjutaTokenOldFile *anjuta_token_old_file_new (GFile *file);
void anjuta_token_old_file_free (AnjutaTokenOldFile *file);

const gchar* anjuta_token_old_file_get_content (AnjutaTokenOldFile *file, GError **error);
void anjuta_token_old_file_move (AnjutaTokenOldFile *file, GFile *new_file);
gboolean anjute_token_file_save (AnjutaTokenOldFile *file, GError **error);

void anjuta_token_old_file_append (AnjutaTokenOldFile *file, AnjutaTokenOld *token);
void anjuta_token_old_update_line_width (AnjutaTokenOldFile *file, guint width);

AnjutaTokenOld* anjuta_token_old_file_first (AnjutaTokenOldFile *file);
AnjutaTokenOld* anjuta_token_old_file_last (AnjutaTokenOldFile *file);
GFile *anjuta_token_file_get_file (AnjutaTokenOldFile *file);
guint anjuta_token_file_get_line_width (AnjutaTokenOldFile *file);

G_END_DECLS

#endif
