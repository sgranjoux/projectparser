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
anjuta_token_copy (AnjutaToken *model)
{
	AnjutaToken *token;

	token = g_slice_new0 (AnjutaToken);
	token->vtable = model->vtable;
	token->type = model->type;
	token->flags = model->flags;
	token->pos = token->flags & ANJUTA_TOKEN_STATIC ? model->pos : g_strdup (model->pos);
	token->length = model->length;

	return token;
}

AnjutaToken *
anjuta_token_copy_include_range (AnjutaToken *start, AnjutaToken *end)
{
	AnjutaToken *token = NULL;
	AnjutaToken *last;


	token = anjuta_token_copy (start);
	last = token;
	
	while ((start != NULL) && (start != end))
	{
		start = anjuta_token_next (start);

		last = anjuta_token_insert_after (last, anjuta_token_copy (start));
	}

	return token;
}

AnjutaToken *
anjuta_token_copy_exclude_range (AnjutaToken *start, AnjutaToken *end)
{

	if ((start == end) || (start->next == end)) return NULL;

	return anjuta_token_copy_include_range (anjuta_token_next (start), anjuta_token_previous (end));
}

AnjutaToken *
anjuta_token_insert_after (AnjutaToken *token, AnjutaToken *sibling)
{
	AnjutaToken *last = sibling;

	g_return_val_if_fail (token != NULL, NULL);
	g_return_val_if_fail ((sibling != NULL) && (sibling->prev == NULL), NULL);
	
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
	gboolean recheck = FALSE;
	
	for (; sequence != NULL; /*sequence = flags & ANJUTA_SEARCH_BACKWARD ? anjuta_token_previous (sequence) : anjuta_token_next (sequence)*/)
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

		if (flags & ANJUTA_SEARCH_BACKWARD)
		{
			if (!recheck)
			{
				if (!(flags & ANJUTA_SEARCH_INTO) && (sequence->flags & ANJUTA_TOKEN_CLOSE)) level++;
				if (sequence->flags & ANJUTA_TOKEN_OPEN)
				{
					level--;
					/* Check the OPEN token */
					if (level < 0)
					{
						break;
					}
					else if (level == 0)
					{
						recheck = TRUE;
						continue;
					}
				}
			}
			recheck = FALSE;
			sequence = anjuta_token_previous (sequence);
		}
		else
		{
			if (!recheck)
			{
				if (!(flags & ANJUTA_SEARCH_INTO) && (sequence->flags & ANJUTA_TOKEN_OPEN)) level++;
				if (sequence->flags & ANJUTA_TOKEN_CLOSE)
				{
					level--;
					/* Check the CLOSE token */
					if (level < 0)
					{
						break;
					}
					else if (level == 0)
					{
						recheck = TRUE;
						continue;
					}
				}
			}
			recheck = FALSE;
			sequence = anjuta_token_next (sequence);
		}
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

gchar *
anjuta_token_get_value_range (AnjutaToken *start, AnjutaToken *end)
{
	GString *value = g_string_new (NULL);

	for (;;)
	{
		if (start == NULL) break;
		if (start->pos != NULL)
		{
			g_string_append_len (value, start->pos, start->length);
		}
		if (start == end) break;
		
		start = anjuta_token_next (start);
	}
	

	return g_string_free (value, FALSE);
}

/* Read a List of token and return a list containings:
 * - the first token
 * - the beginning of the first item
 * - the end of the first item
 * - the beginning of the next item
 * - the end of the next item
 * ....
 * - the last token
 */
GList *
anjuta_token_split_list (AnjutaToken *list)
{
	AnjutaToken *next_tok;
	AnjutaToken *open_tok;
	AnjutaToken *arg;
	GList *split_list = NULL;
	gboolean match;

	g_return_val_if_fail (list != NULL, NULL);

	/* Look for the start of the list */
	open_tok = anjuta_token_new_static (ANJUTA_TOKEN_OPEN, NULL);
	match = anjuta_token_match (open_tok, ANJUTA_SEARCH_INTO, list, &list);
	anjuta_token_free (open_tok);
	if (!match) return NULL;
	split_list = g_list_prepend (split_list, list);

	/* Look for all elements */
	next_tok = anjuta_token_new_static (ANJUTA_TOKEN_NEXT, NULL);
	do
	{
		AnjutaToken *tok;
		
		/* Look for end of element */
		tok = anjuta_token_next (list);
		match = anjuta_token_match (next_tok, ANJUTA_SEARCH_OVER, tok, &arg);

		/* Remove space at the beginning */
		for (; tok != arg; tok = anjuta_token_next (tok))
		{
			switch (tok->type)
			{
				case ANJUTA_TOKEN_NONE:
				case ANJUTA_TOKEN_SPACE:
				case ANJUTA_TOKEN_COMMENT:
					continue;
				default:
					break;
			}

			if (tok->flags & (ANJUTA_TOKEN_IRRELEVANT | ANJUTA_TOKEN_NEXT))
			{
				continue;
			}
			break;
		}
		split_list = g_list_prepend (split_list, tok);

		/* Remove space at the end */
		for (tok = anjuta_token_previous (arg); tok != list; tok = anjuta_token_previous (tok))
		{
			switch (tok->type)
			{
				case ANJUTA_TOKEN_NONE:
				case ANJUTA_TOKEN_SPACE:
				case ANJUTA_TOKEN_COMMENT:
					continue;
				default:
					break;
			}

			if (tok->flags & (ANJUTA_TOKEN_IRRELEVANT | ANJUTA_TOKEN_NEXT))
			{
				continue;
			}
			break;
		}
		split_list = g_list_prepend (split_list, tok);

		list = arg;
	}
	while (match);
		
	/* Add end marker */
	split_list = g_list_prepend (split_list, list);

	split_list = g_list_reverse (split_list);
	
	anjuta_token_free (next_tok);
	
	return split_list;
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
	if (token == NULL) return;
	if (token->prev != NULL)	token->prev->next = NULL;

	do
	{
		AnjutaToken *next;
		
		next = token->next;
		g_slice_free (AnjutaToken, token);
		token = next;
	}
	while (token != NULL);
}

/* Token style object
 *---------------------------------------------------------------------------*/

typedef struct _AnjutaTokenStyleSeparator AnjutaTokenStyleSeparator;

struct _AnjutaTokenStyleSeparator
{
	AnjutaTokenRange range;
	guint count;
};

struct _AnjutaTokenStyle
{
	guint line_width;
	AnjutaToken *first;
	AnjutaToken *sep;
	AnjutaToken *eol;
	AnjutaToken *last;
};

AnjutaTokenStyleSeparator*
anjuta_token_style_separator_new (void)
{
	AnjutaTokenStyleSeparator *sep;

	sep = g_slice_new0 (AnjutaTokenStyleSeparator);
	
	return sep;	
}

void
anjuta_token_style_separator_free (AnjutaTokenStyleSeparator *sep)
{
	g_slice_free (AnjutaTokenStyleSeparator, sep);
}

void anjuta_token_style_update (AnjutaTokenStyle *style, AnjutaToken *list)
{
	GList *split_list;
	GList *link;
	GHashTable *sep_list;
	AnjutaTokenStyleSeparator *sep;
	AnjutaTokenStyleSeparator *eol;
	AnjutaTokenStyleSeparator *value;
	GHashTableIter iter;
	gchar *key;

	split_list = anjuta_token_split_list (list);

	if (split_list == NULL) return;
	
	/* Find & Replace first separator */
	anjuta_token_free (style->first);
	style->first = anjuta_token_copy_exclude_range ((AnjutaToken *)split_list->data, (AnjutaToken *)split_list->next->data);

	/* Find intermediate separator */
	sep_list = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, anjuta_token_style_separator_free);
	link = split_list->next->next;
	if (link != NULL)
	{
		for (;link->next->next != NULL; link = link->next->next)
		{
			AnjutaToken *first;
			AnjutaToken *last;

			first = anjuta_token_next ((AnjutaToken *)link->data);
			last =anjuta_token_previous ((AnjutaToken *)link->next->data);
			
			key = anjuta_token_get_value_range (first, last);
			sep = g_hash_table_lookup (sep_list, key);
			if (sep == NULL)
			{
				sep = anjuta_token_style_separator_new ();
				sep->range.first = first;
				sep->range.last = last;
				sep->count = 1;
				g_hash_table_insert (sep_list, key, sep);
			}
			else
			{
				sep->count++;
				g_free (key);
			}
		}
	}	
	
	/* Find & Replace last separator */
	anjuta_token_free (style->last);
	if (link == NULL)
	{
		style->last = NULL;
	}
	else
	{
		style->last = anjuta_token_copy_exclude_range ((AnjutaToken *)link->data, (AnjutaToken *)link->next->data);
	}
	g_list_free (split_list);

	/* Replace the most used intermediate separator */
	anjuta_token_free (style->sep);
	anjuta_token_free (style->eol);
	style->sep = NULL;
	style->eol = NULL;
	eol = NULL;
	sep = NULL;
	g_hash_table_iter_init (&iter, sep_list);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		if (strchr (key, '\n') != NULL)
		{
			/* Separator with end of line */
			if ((eol == NULL) || (eol->count < value->count))
			{
				eol = value;
			}
		}
		else
		{
			/* Separaror without end of line */
			if ((sep == NULL) || (sep->count < value->count))
			{
				sep = value;
			}
		}
	}
	if (eol != NULL) style->eol = anjuta_token_copy_include_range (eol->range.first, eol->range.last);
	if (sep != NULL) style->sep = anjuta_token_copy_include_range (sep->range.first, sep->range.last);

	g_hash_table_destroy (sep_list);

	DEBUG_PRINT("Update style new:2");
	DEBUG_PRINT("first \"%s\"", style->first == NULL ? "(null)" : anjuta_token_get_value_range (style->first, NULL));
	DEBUG_PRINT("sep \"%s\"", style->sep == NULL ? "(null)" : anjuta_token_get_value_range (style->sep, NULL));
	DEBUG_PRINT("eol \"%s\"", style->eol == NULL ? "(null)" : anjuta_token_get_value_range (style->eol, NULL));
	DEBUG_PRINT("last \"%s\"", style->last == NULL ? "(null)" : anjuta_token_get_value_range (style->last, NULL));
}	
	
void anjuta_token_style_format (AnjutaTokenStyle *style, AnjutaToken *list)
{
	GList *args;
	GList *link;

	args = anjuta_token_split_list (list);

	if (args == NULL) return;

	if (((AnjutaToken *)args->next->data)->flags & (ANJUTA_TOKEN_ADDED | ANJUTA_TOKEN_REMOVED))
	{
		/* Update first separator */
		//anjuta_token_free_exclude_range ((AnjutaToken *)args->data, (AnjutaToken *)args->next->data);
	}

	/* FIXME: complete....*/
}

AnjutaTokenStyle *
anjuta_token_style_new (guint line_width)
{
	AnjutaTokenStyle *style;

	style = g_slice_new0 (AnjutaTokenStyle);
	style->line_width = line_width;
	
	return style;	
}

void
anjuta_token_style_free (AnjutaTokenStyle *style)
{
	anjuta_token_free (style->first);
	anjuta_token_free (style->sep);
	anjuta_token_free (style->eol);
	anjuta_token_free (style->last);
	g_slice_free (AnjutaTokenStyle, style);
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

	guint line_width;
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

void
anjuta_token_file_update_line_width (AnjutaTokenFile *file, guint width)
{
	if (width > file->line_width) file->line_width = width;
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

guint
anjuta_token_file_get_line_width (AnjutaTokenFile *file)
{
	return file->line_width;
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

	anjuta_token_free (file->first);

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
