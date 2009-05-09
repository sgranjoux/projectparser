/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta-token.c
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

#include "anjuta-token.h"

#include <stdio.h>
#include <string.h>



typedef void (*AnjutaTokenFreeFunc) (AnjutaToken *token); 
typedef gboolean (*AnjutaTokenCompareFunc) (AnjutaToken *toka, AnjutaToken *tokb); 

typedef struct _AnjutaTokenVTable AnjutaTokenVTable;

struct _AnjutaTokenVTable
{
	AnjutaTokenFreeFunc free;
	AnjutaTokenCompareFunc compare;
};


struct _AnjutaToken
{
	AnjutaTokenType type;	
	gint flags;
	gchar *pos;
	gsize length;
	gpointer value;
	AnjutaTokenVTable *vtable;
	AnjutaToken *next;
	AnjutaToken *prev;
	guint ref_count;	
};

/* Public functions
 *---------------------------------------------------------------------------*/

/* String token
 *---------------------------------------------------------------------------*/

static void
anjuta_token_string_free (AnjutaToken *token)
{
	g_slice_free (AnjutaToken, token);
}

static gboolean
anjuta_token_string_compare (AnjutaToken *toka, AnjutaToken *tokb)
{
	if (tokb->length == 0) return TRUE;
	if (toka->length != tokb->length) return FALSE;
	
	if ((toka->flags & ANJUTA_TOKEN_CASE_INSENSITIVE)  && (tokb->flags & ANJUTA_TOKEN_CASE_INSENSITIVE))
	{
		return g_ascii_strncasecmp (toka->pos, tokb->pos, toka->length) == 0;
	}
	else
	{
		return strncmp (toka->pos, tokb->pos, toka->length) == 0;
	}
}

static AnjutaTokenVTable anjuta_token_string_vtable = {anjuta_token_string_free, anjuta_token_string_compare};

/* Common token functions
 *---------------------------------------------------------------------------*/

void
anjuta_token_insert_after (AnjutaToken *token, AnjutaToken *sibling)
{
	AnjutaToken *last = sibling;

	g_return_if_fail (token != NULL);
	g_return_if_fail ((sibling != NULL) && (sibling->prev == NULL));
	
	/* Find end of sibling list */
	while (last->next != NULL) last = last->next;

	last->next = token->next;
	if (last->next != NULL)	last->next->prev = last;

	token->next = sibling;
	last->prev = token;
}

void
anjuta_token_dump (AnjutaToken *token)
{
	g_message ("%d: %.*s", token->type, token->length, token->pos);
}

void
anjuta_token_dump_range (AnjutaToken *start, AnjutaToken *end)
{
	for (; start != NULL; start = start->next)
	{
		anjuta_token_dump(start);
		if (start == end) break;
	}
}

void
anjuta_token_foreach (AnjutaToken *token, GFunc func, gpointer user_data)
{
	AnjutaToken *next = NULL;
	
	for (;token != NULL; token = next)
	{
		next = token->next;
		func (token, user_data);
	}
}

/* Return true and update end if the token is found.
 * If a close token is found, return FALSE but still update end */
gboolean
anjuta_token_match (AnjutaToken *token, gint flags, AnjutaToken *sequence, AnjutaToken **end)
{
	gint level = 0;
	
	for (; sequence != NULL; sequence = anjuta_token_next (sequence))
	{
		AnjutaToken *toka;
		AnjutaToken *tokb = token;

		/*if (anjuta_token_get_flags (sequence) & ANJUTA_TOKEN_SIGNIFICANT) g_message("match significant %p %s level %d", sequence, anjuta_token_evaluate (sequence, sequence), level);
		if (anjuta_token_get_flags (sequence) & ANJUTA_TOKEN_OPEN) g_message("match %p open %s level %d", sequence, anjuta_token_evaluate (sequence, sequence), level);
		if (anjuta_token_get_flags (sequence) & ANJUTA_TOKEN_CLOSE) g_message("match %p close %s level %d", sequence, anjuta_token_evaluate (sequence, sequence), level);*/
		
		if (level == 0)
		{
			for (toka = sequence; toka != NULL; toka = anjuta_token_next (toka))
			{
				if (anjuta_token_compare (toka, tokb))
				{
					tokb = anjuta_token_next (tokb);
					if (tokb == NULL)
					{
						if (end) *end = sequence;
						return TRUE;
					}
				}
				else
				{
					if (!(toka->flags & ANJUTA_TOKEN_IRRELEVANT) || (toka == sequence))
						break;
				}
			}
		}
		
		if (!(flags & ANJUTA_SEARCH_INTO) && (sequence->flags & ANJUTA_TOKEN_OPEN)) level++;
		if (sequence->flags & ANJUTA_TOKEN_CLOSE) level--;

		if (level < 0) break;
	}
	/*g_message ("matched %p %d", sequence, level);*/
	
	if (end) *end = sequence;
	
	return FALSE;
}

AnjutaToken *
anjuta_token_next (AnjutaToken *token)
{
	return token == NULL ? NULL : token->next;
}

AnjutaToken *
anjuta_token_previous (AnjutaToken *token)
{
	return token->prev;
}

gboolean
anjuta_token_compare (AnjutaToken *toka, AnjutaToken *tokb)
{
	if (tokb->type != ANJUTA_TOKEN_NONE)
	{
		if ((toka->vtable != tokb->vtable) ||
			(toka->type != tokb->type) ||
			!toka->vtable->compare (toka, tokb)) return FALSE;
	}

	// Check flags
	if (tokb->flags & ANJUTA_TOKEN_PUBLIC_FLAGS)
	{
		if ((toka->flags & tokb->flags & ANJUTA_TOKEN_PUBLIC_FLAGS) == 0)
			return FALSE;
	}

	return TRUE;
}

void
anjuta_token_set_type (AnjutaToken *token, gint type)
{
	token->type = type;	
}

void
anjuta_token_set_flags (AnjutaToken *token, gint flags)
{
	token->flags |= flags;	
}

void
anjuta_token_clear_flags (AnjutaToken *token, gint flags)
{
	token->flags &= ~flags;	
}

gint
anjuta_token_get_type (AnjutaToken *token)
{
	return token->type;
}

gint
anjuta_token_get_flags (AnjutaToken *token)
{
	return token->flags;
}

gchar *
anjuta_token_get_value (AnjutaToken *token)
{
	return token && (token->pos != NULL) ? g_strndup (token->pos, token->length) : NULL;
}

gchar *
anjuta_token_evaluate (AnjutaToken *start, AnjutaToken *end)
{
	GString *value = g_string_new (NULL);

	for (;;)
	{
		if (start == NULL) break;
		if (!(start->flags & ANJUTA_TOKEN_IRRELEVANT) && (start->pos != NULL))
		{
			g_string_append_len (value, start->pos, start->length);
		}
		if (start == end) break;
		
		start = anjuta_token_next (start);
	}
	

	return g_string_free (value, FALSE);
}

/* Constructor & Destructor
 *---------------------------------------------------------------------------*/

AnjutaToken *anjuta_token_new_string (AnjutaTokenType type, const char *value)
{
	AnjutaToken *token;

	token = g_slice_new0 (AnjutaToken);
	token->vtable = &anjuta_token_string_vtable;
	token->type = type  & ANJUTA_TOKEN_TYPE;
	token->flags = type & ANJUTA_TOKEN_FLAGS;
	token->pos = g_strdup (value);
	token->length = strlen (value);

	return token;
}
	
AnjutaToken *
anjuta_token_new_fragment (AnjutaTokenType type, const gchar *pos, gsize length)
{
	AnjutaToken *token;

	token = g_slice_new0 (AnjutaToken);
	token->vtable = &anjuta_token_string_vtable;
	token->type = type  & ANJUTA_TOKEN_TYPE;
	token->flags = (type & ANJUTA_TOKEN_FLAGS) | ANJUTA_TOKEN_STATIC;
	token->pos = (gchar *)pos;
	token->length = length;

	return token;
};

AnjutaToken *anjuta_token_new_static (AnjutaTokenType type, const char *value)
{
	return anjuta_token_new_fragment (type, value, value == NULL ? 0 : strlen (value));	
}


void
anjuta_token_free (AnjutaToken *token)
{
	g_slice_free (AnjutaToken, token);
}


/* Token file objects
 *---------------------------------------------------------------------------*/

struct _AnjutaTokenFile
{
	GFile* file;
	
	gsize length;
	gchar *content;
	
	AnjutaToken *first;
	AnjutaToken *last;
};

/* Public functions
 *---------------------------------------------------------------------------*/

const gchar *
anjuta_token_file_get_content (AnjutaTokenFile *file, GError **error)
{
	if (!file->content)
	{
		gchar *content;
		gsize length;
	
		if (g_file_load_contents (file->file, NULL, &content, &length, NULL, error))
		{
			file->content = content;
			file->length = length;
		}
	}
	
	return file->content;
}

void
anjuta_token_file_append (AnjutaTokenFile *file, AnjutaToken *token)
{
	if (file->last == NULL)
	{
		file->first = token;
	}
	else
	{
		anjuta_token_insert_after (file->last, token);
	}
	file->last = token;
}

AnjutaToken*
anjuta_token_file_first (AnjutaTokenFile *file)
{
	return file->first;
}

AnjutaToken*
anjuta_token_file_last (AnjutaTokenFile *file)
{
	return file->last;
}

GFile*
anjuta_token_file_get_file (AnjutaTokenFile *file)
{
	return file->file;
}

/* Constructor & Destructor
 *---------------------------------------------------------------------------*/

AnjutaTokenFile *
anjuta_token_file_new (GFile *gfile)
{
	AnjutaTokenFile *file = g_slice_new0 (AnjutaTokenFile);
	
	file->file = g_object_ref (gfile);

	return file;
};

void
anjuta_token_file_free (AnjutaTokenFile *file)
{
	g_return_if_fail (file != NULL);

	while (file->first)
	{
		AnjutaToken *next = anjuta_token_next (file->first);

		anjuta_token_free (file->first);
		file->first = next;
	};

	g_free (file->content);
	
	g_object_unref (file->file);
	
	g_slice_free (AnjutaTokenFile, file);
};
