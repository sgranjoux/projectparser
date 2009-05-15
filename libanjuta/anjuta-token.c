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

#include "anjuta-debug.h"

#include <glib-object.h>

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

/* Helpers functions
 *---------------------------------------------------------------------------*/

/* Create a directories, including parents if necessary. This function
 * exists in GLIB 2.18, but we need only 2.16 currently.
 * */

static gboolean
make_directory_with_parents (GFile *file,
							   GCancellable *cancellable,
							   GError **error)
{
	GError *path_error = NULL;
	GList *children = NULL;

	for (;;)
	{
		if (g_file_make_directory (file, cancellable, &path_error))
		{
			/* Making child directory succeed */
			if (children == NULL)
			{
				/* All directories have been created */
				return TRUE;
			}
			else
			{
				/* Get next child directory */
				g_object_unref (file);
				file = (GFile *)children->data;
				children = g_list_delete_link (children, children);
			}
		}
		else if (g_error_matches (path_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
		{
			g_clear_error (&path_error);
			children = g_list_prepend (children, file);
			file = g_file_get_parent (file);
		}
		else
		{
			g_object_unref (file);
			g_list_foreach (children, (GFunc)g_object_unref, NULL);
			g_list_free (children);
			g_propagate_error (error, path_error);
			
			return FALSE;
		}
	}				
}


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

AnjutaToken *
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

	return sibling;
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

gboolean
anjuta_token_remove (AnjutaToken *token, AnjutaToken *end)
{
	AnjutaToken *prev = anjuta_token_previous (token);
	
	if (end->flags & ANJUTA_TOKEN_CLOSE)
	{
		if (prev->flags & ANJUTA_TOKEN_NEXT)
		{
			/* Remove last element, remove last next, keep end token */
			token = prev;
			end = anjuta_token_previous (end);
		}
		else if (prev->flags & ANJUTA_TOKEN_OPEN)
		{
			/* Remove all element, remove start token too */
			token = prev;
		}
	}
	
	for (;token != NULL;)
	{
		token->flags |= ANJUTA_TOKEN_REMOVED;
		if (token == end) break;
		token = anjuta_token_next (token);
	}

	return TRUE;
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
	GObject parent;
	
	GFile* file;
	
	gsize length;
	gchar *content;
	
	AnjutaToken *first;
	AnjutaToken *last;
};

struct _AnjutaTokenFileClass
{
	GObjectClass parent_class;
};

static GObjectClass *parent_class = NULL;

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

gboolean
anjuta_token_file_save (AnjutaTokenFile *file, GError **error)
{
	AnjutaToken *tok;
	GFileOutputStream *stream;
	gboolean ok;
	GError *err = NULL;

	stream = g_file_replace (file->file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &err);
	if (stream == NULL)
	{
		if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
		{
			/* Perhaps parent directory is missing, try to create it */
			GFile *parent = g_file_get_parent (file->file);
			
			if (make_directory_with_parents (parent, NULL, NULL))
			{
				g_object_unref (parent);
				g_clear_error (&err);
				stream = g_file_replace (file->file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, error);
				if (stream == NULL) return FALSE;
			}
			else
			{
				g_object_unref (parent);
				g_propagate_error (error, err);

				return FALSE;
			}
		}
		else
		{
			g_propagate_error (error, err);
			return FALSE;
		}
	}

	for (tok = file->first; tok != NULL; tok = anjuta_token_next (tok))
	{
		if (anjuta_token_get_flags (tok) & ANJUTA_TOKEN_REMOVED)
		{
			DEBUG_PRINT ("get token with removed flags");
		}
		if (!(anjuta_token_get_flags (tok) & ANJUTA_TOKEN_REMOVED) && (tok->length))
		{
			if (g_output_stream_write (G_OUTPUT_STREAM (stream), tok->pos, tok->length * sizeof (char), NULL, error) < 0)
			{
				g_object_unref (stream);

				return FALSE;
			}
		}
	}
	
	ok = g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, error);
	g_object_unref (stream);

	return ok;
}

void
anjuta_token_file_move (AnjutaTokenFile *file, GFile *new_file)
{
	if (file->file) g_object_unref (file->file);
	file->file = new_file != NULL ? g_object_ref (new_file) : NULL;
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

/* GObject functions
 *---------------------------------------------------------------------------*/

/* dispose is the first destruction step. It is used to unref object created
 * with instance_init in order to break reference counting cycles. This
 * function could be called several times. All function should still work
 * after this call. It has to called its parents.*/

static void
anjuta_token_file_dispose (GObject *object)
{
	AnjutaTokenFile *file = ANJUTA_TOKEN_FILE (object);

	while (file->first)
	{
		AnjutaToken *next = anjuta_token_next (file->first);

		anjuta_token_free (file->first);
		file->first = next;
	};

	if (file->content) g_free (file->content);
	file->content = NULL;
	
	if (file->file) g_object_unref (file->file);
	file->file = NULL;

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

/* instance_init is the constructor. All functions should work after this
 * call. */

static void
anjuta_token_file_instance_init (AnjutaTokenFile *file)
{
	file->file = NULL;
}

/* class_init intialize the class itself not the instance */

static void
anjuta_token_file_class_init (AnjutaTokenFileClass * klass)
{
	GObjectClass *gobject_class;

	g_return_if_fail (klass != NULL);
	
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
	gobject_class = G_OBJECT_CLASS (klass);
	
	gobject_class->dispose = anjuta_token_file_dispose;
}

GType
anjuta_token_file_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo type_info = 
		{
			sizeof (AnjutaTokenFileClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) anjuta_token_file_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,           /* class_data */
			sizeof (AnjutaTokenFile),
			0,              /* n_preallocs */
			(GInstanceInitFunc) anjuta_token_file_instance_init,
			NULL            /* value_table */
		};

		type = g_type_register_static (G_TYPE_OBJECT,
		                            "AnjutaTokenFile", &type_info, 0);
	}
	
	return type;
}


/* Constructor & Destructor
 *---------------------------------------------------------------------------*/

AnjutaTokenFile *
anjuta_token_file_new (GFile *gfile)
{
	AnjutaTokenFile *file = g_object_new (ANJUTA_TOKEN_FILE_TYPE, NULL);
	
	file->file =  gfile ? g_object_ref (gfile) : NULL;

	return file;
};
