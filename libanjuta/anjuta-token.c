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

typedef struct _AnjutaTokenData AnjutaTokenData;

struct _AnjutaTokenData
{
	AnjutaTokenType type;	
	gint flags;
	gchar *pos;
	gsize length;
};

struct _AnjutaToken
{
	AnjutaTokenData	*data;
	AnjutaToken	*next;
	AnjutaToken	*prev;
	AnjutaToken	*parent;
	AnjutaToken	*children;
};
		
#define ANJUTA_TOKEN_DATA(node)  ((node) != NULL ? (AnjutaTokenData *)((node)->data) : NULL)


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

		if (anjuta_token_get_flags (sequence) & ANJUTA_TOKEN_SIGNIFICANT) g_message("match significant %p %s level %d", sequence, anjuta_token_get_value (sequence), level);
		if (anjuta_token_get_flags (sequence) & ANJUTA_TOKEN_OPEN) g_message("match %p open %s level %d", sequence, anjuta_token_get_value (sequence), level);
		if (anjuta_token_get_flags (sequence) & ANJUTA_TOKEN_CLOSE) g_message("match %p close %s level %d", sequence, anjuta_token_get_value (sequence), level);

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
					if (!(anjuta_token_get_flags (toka) & ANJUTA_TOKEN_IRRELEVANT) || (toka == sequence))
						break;
				}
			}
		}

		if (flags & ANJUTA_SEARCH_BACKWARD)
		{
			if (!recheck)
			{
				if (!(flags & ANJUTA_SEARCH_INTO) && (anjuta_token_get_flags (sequence) & ANJUTA_TOKEN_CLOSE)) level++;
				if (anjuta_token_get_flags (sequence) & ANJUTA_TOKEN_OPEN)
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
				if (!(flags & ANJUTA_SEARCH_INTO) && (anjuta_token_get_flags (sequence) & ANJUTA_TOKEN_OPEN)) level++;
				if (anjuta_token_get_flags (sequence) & ANJUTA_TOKEN_CLOSE)
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
anjuta_token_remove (AnjutaToken *token)
{
	ANJUTA_TOKEN_DATA (token)->flags |= ANJUTA_TOKEN_REMOVED;

	return TRUE;
}

gboolean
anjuta_token_compare (AnjutaToken *toka, AnjutaToken *tokb)
{
	AnjutaTokenData *data = ANJUTA_TOKEN_DATA (toka);
	AnjutaTokenData *datb = ANJUTA_TOKEN_DATA (tokb);

	if (datb != ANJUTA_TOKEN_NONE)
	{
		if (datb->length != 0)
		{
			if (data->length != datb->length) return FALSE;
		
			if ((data->flags & ANJUTA_TOKEN_CASE_INSENSITIVE)  && (datb->flags & ANJUTA_TOKEN_CASE_INSENSITIVE))
			{
				if (g_ascii_strncasecmp (data->pos, datb->pos, data->length) != 0) return FALSE;
			}
			else
			{
				if (strncmp (data->pos, datb->pos, data->length) != 0) return FALSE;
			}
		}
	}
		
	if (datb->flags & ANJUTA_TOKEN_PUBLIC_FLAGS)
	{
		if ((data->flags & datb->flags & ANJUTA_TOKEN_PUBLIC_FLAGS) == 0)
			return FALSE;
	}

	return TRUE;
}

static AnjutaToken *
anjuta_token_next_after_children (AnjutaToken *token)
{
	if (token->next != NULL)
	{
		return token->next;
	}
	else if (token->parent != NULL)
	{
		return anjuta_token_next_after_children (token->parent);
	}
	else
	{
		return NULL;
	}
}

AnjutaToken *
anjuta_token_next (AnjutaToken *token)
{
	if (token->children != NULL)
	{
		return token->children;
	}
	else if (token->next != NULL)
	{
		return token->next;
	}
	else if (token->parent != NULL)
	{
		return anjuta_token_next_after_children (token->parent);
	}
	else
	{
		return NULL;
	}
}

AnjutaToken *
anjuta_token_previous (AnjutaToken *token)
{
	if (token->prev != NULL)
	{
		return token->prev;
	}
	else
	{
		return token->parent;
	}
}

AnjutaToken *
anjuta_token_next_sibling (AnjutaToken *token)
{
	return token->next;
}

AnjutaToken *
anjuta_token_next_child (AnjutaToken *token)
{
	return token->children;
}

AnjutaToken *
anjuta_token_previous_sibling (AnjutaToken *token)
{
	return token->prev;
}

AnjutaToken *
anjuta_token_last_child (AnjutaToken *token)
{
	AnjutaToken *last = NULL;

	for (; token != NULL; token = (AnjutaToken *)g_node_last_child ((GNode *)last))
	{
		last = token;
	}

	return last;
}

AnjutaToken *
anjuta_token_parent (AnjutaToken *token)
{
	return token->parent;
}

void
anjuta_token_set_type (AnjutaToken *token, gint type)
{
	ANJUTA_TOKEN_DATA (token)->type = type;
}

void
anjuta_token_set_flags (AnjutaToken *token, gint flags)
{
	ANJUTA_TOKEN_DATA (token)->flags |= flags;
}

void
anjuta_token_clear_flags (AnjutaToken *token, gint flags)
{
	ANJUTA_TOKEN_DATA (token)->flags &= ~flags;
}

gint
anjuta_token_get_type (AnjutaToken *token)
{
	return ANJUTA_TOKEN_DATA (token)->type;
}

gint
anjuta_token_get_flags (AnjutaToken *token)
{
	return ANJUTA_TOKEN_DATA (token)->flags;
}

gchar *
anjuta_token_get_value (AnjutaToken *token)
{
	AnjutaTokenData *data = ANJUTA_TOKEN_DATA (token);

	return data && (data->pos != NULL) ? g_strndup (data->pos, data->length) : NULL;
}

const gchar *
anjuta_token_get_string (AnjutaToken *token)
{
	return ANJUTA_TOKEN_DATA (token)->pos;
}

guint
anjuta_token_get_length (AnjutaToken *token)
{
	return ANJUTA_TOKEN_DATA (token)->length;
}

static void
anjuta_token_evaluate_token (AnjutaToken *token, GString *value)
{
	//if (!(anjuta_token_get_flags (token) & ANJUTA_TOKEN_IRRELEVANT) && (anjuta_token_get_string (token) != NULL))
	//{
	g_string_append_len (value, anjuta_token_get_string (token), anjuta_token_get_length (token));
	//}
}	

gchar *
anjuta_token_evaluate_range (AnjutaToken *start, AnjutaToken *end)
{
	GString *value = g_string_new (NULL);

	for (;;)
	{
		if (start == NULL) break;
		anjuta_token_evaluate_token (start, value);
		if (start == end) break;
		
		start = anjuta_token_next (start);
	}
	

	return g_string_free (value, FALSE);
}

static  void
anjuta_token_evaluate_child (AnjutaToken *token, GString *value)
{
	anjuta_token_evaluate_token (token, value);

	if (token->children) anjuta_token_evaluate_child (token->children, value);

	if (token->next) anjuta_token_evaluate_child (token->next, value);
}

gchar *
anjuta_token_evaluate (AnjutaToken *token)
{
	GString *value = g_string_new (NULL);

	anjuta_token_evaluate_token (token, value);
	if (token->children) anjuta_token_evaluate_child (token->children, value);
	
	return g_string_free (value, FALSE);
}

static  void
anjuta_token_value_child (AnjutaToken *token, GString *value)
{
	g_string_append_len (value, anjuta_token_get_string (token), anjuta_token_get_length (token));

	if (token->children) anjuta_token_evaluate_child (token->children, value);

	if (token->next) anjuta_token_evaluate_child (token->next, value);
}

gchar *
anjuta_token_value (AnjutaToken *token)
{
	GString *value = g_string_new (NULL);

	g_string_append_len (value, anjuta_token_get_string (token), anjuta_token_get_length (token));
	
	if (token->children) anjuta_token_value_child (token->children, value);
	
	return g_string_free (value, FALSE);
}

#if 0
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
#endif

AnjutaToken *
anjuta_token_merge (AnjutaToken *first, AnjutaToken *end)
{
	AnjutaToken *child;
	AnjutaToken *tok;

	if (first == end) return first;

	child = (AnjutaToken *)g_node_last_child ((GNode *)first);
	do
	{
		tok = (AnjutaToken *)g_node_next_sibling ((GNode *)first);
		if (tok == NULL) break;
		
		g_node_unlink ((GNode *)tok);
		child = (AnjutaToken *)g_node_insert_after ((GNode *)first, (GNode *)child, (GNode *)tok);

	}
	while (tok != end);

	return first;
}

AnjutaToken *
anjuta_token_merge_previous (AnjutaToken *first, AnjutaToken *end)
{
	AnjutaToken *child;
	AnjutaToken *tok;

	if (first == end) return first;

	child = (AnjutaToken *)g_node_first_child ((GNode *)first);
	do
	{
		tok = (AnjutaToken *)g_node_prev_sibling ((GNode *)first);
		if (tok == NULL) break;
		
		g_node_unlink ((GNode *)tok);
		child = (AnjutaToken *)g_node_insert_before ((GNode *)first, (GNode *)child, (GNode *)tok);

	}
	while (tok != end);

	return first;
}

AnjutaToken *
anjuta_token_copy (AnjutaToken *token)
{
	AnjutaToken *copy = NULL;

	if (token != NULL)
	{
		AnjutaTokenData *org = ANJUTA_TOKEN_DATA (token);
		AnjutaTokenData *data = NULL;
		AnjutaToken *child;
		AnjutaToken *last;

		data = g_slice_new0 (AnjutaTokenData);
		data->type =org->type;
		data->flags = org->type;
		if ((data->flags & ANJUTA_TOKEN_STATIC) || (org->pos == NULL))
		{
			data->pos = org->pos;
		}
		else
		{
			data->pos = g_strdup (ANJUTA_TOKEN_DATA (token)->pos);
		}
		data->length = org->length;

		copy = (AnjutaToken *)g_node_new (data);

		last = NULL;
		for (child = anjuta_token_next_child (token); child != NULL; child = anjuta_token_next_sibling (child))
		{
			AnjutaToken *new_child = anjuta_token_copy (child);
			last =  last == NULL ? anjuta_token_insert_child (copy, new_child) : anjuta_token_insert_after (last, new_child);
		}
	}

	return copy;
}

AnjutaToken *
anjuta_token_insert_child (AnjutaToken *parent, AnjutaToken *sibling)
{
	return (AnjutaToken *)g_node_insert_after ((GNode *)parent, (GNode *)NULL, (GNode *)sibling);
}

AnjutaToken *
anjuta_token_insert_after (AnjutaToken *token, AnjutaToken *sibling)
{
	return (AnjutaToken *)g_node_insert_after ((GNode *)token->parent, (GNode *)token, (GNode *)sibling);
}

AnjutaToken *
anjuta_token_insert_before (AnjutaToken *token, AnjutaToken *sibling)
{
	return (AnjutaToken *)g_node_insert_before ((GNode *)token->parent, (GNode *)token, (GNode *)sibling);
}	


/* Constructor & Destructor
 *---------------------------------------------------------------------------*/

AnjutaToken *anjuta_token_new_string (AnjutaTokenType type, const char *value)
{
	AnjutaTokenData *data;

	data = g_slice_new0 (AnjutaTokenData);
	data->type = type  & ANJUTA_TOKEN_TYPE;
	data->flags = type & ANJUTA_TOKEN_FLAGS;
	data->pos = g_strdup (value);
	data->length = strlen (value);

	return (AnjutaToken *)g_node_new (data);
}
	
AnjutaToken *
anjuta_token_new_fragment (AnjutaTokenType type, const gchar *pos, gsize length)
{
	AnjutaTokenData *data;

	data = g_slice_new0 (AnjutaTokenData);
	data->type = type  & ANJUTA_TOKEN_TYPE;
	data->flags = (type & ANJUTA_TOKEN_FLAGS) | ANJUTA_TOKEN_STATIC;
	data->pos = (gchar *)pos;
	data->length = length;

	return (AnjutaToken *)g_node_new (data);
};

AnjutaToken *anjuta_token_new_static (AnjutaTokenType type, const char *value)
{
	return anjuta_token_new_fragment (type, value, value == NULL ? 0 : strlen (value));	
}


static void
free_token_data (GNode *node, gpointer data)
{
	g_slice_free (AnjutaTokenData, node->data);
}

void
anjuta_token_free (AnjutaToken *token)
{
	if (token == NULL) return;

	g_node_children_foreach ((GNode *)token, G_TRAVERSE_ALL, free_token_data, NULL);
	g_node_destroy ((GNode *)token);
}

/* Token style object
 *---------------------------------------------------------------------------*/

typedef struct _AnjutaTokenStyleSeparator AnjutaTokenStyleSeparator;

struct _AnjutaTokenStyleSeparator
{
	AnjutaToken* token;
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
	GHashTable *sep_list;
	AnjutaTokenStyleSeparator *sep;
	AnjutaTokenStyleSeparator *eol;
	AnjutaTokenStyleSeparator *value;
	AnjutaToken *arg;
	AnjutaToken *last;
	GHashTableIter iter;
	gchar *key;


	/* Find & Replace first separator */
	anjuta_token_free (style->first);
	arg = anjuta_token_next_child (list);
	if (anjuta_token_get_type (arg) == ANJUTA_TOKEN_SPACE)
	{
		style->first = anjuta_token_copy(arg);
		arg = anjuta_token_next_sibling (arg);
	}
	 
	/* Find intermediate separator */
	sep_list = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, anjuta_token_style_separator_free);
	last = NULL;
	for (; arg != NULL; arg = anjuta_token_next_sibling (arg))
	{
		if (anjuta_token_get_type (arg) == ANJUTA_TOKEN_SPACE)
		{
			last = arg;
		}
		else if (last != NULL)
		{
			key = anjuta_token_evaluate (last);
			sep = g_hash_table_lookup (sep_list, key);
			if (sep == NULL)
			{
				sep = anjuta_token_style_separator_new ();
				sep->token = last;
				sep->count = 1;
				g_hash_table_insert (sep_list, key, sep);
			}
			else
			{
				sep->count++;
				g_free (key);
			}
			last = NULL;
		}
	}
	
	/* Find & Replace last separator */
	anjuta_token_free (style->last);
	style->last = anjuta_token_copy (last);

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
	if (eol != NULL) style->eol = anjuta_token_copy (eol->token);
	if (sep != NULL) style->sep = anjuta_token_copy (sep->token);

	g_hash_table_destroy (sep_list);

	DEBUG_PRINT("Update style new:2");
	DEBUG_PRINT("first \"%s\"", style->first == NULL ? "(null)" : anjuta_token_evaluate (style->first));
	DEBUG_PRINT("sep \"%s\"", style->sep == NULL ? "(null)" : anjuta_token_evaluate (style->sep));
	DEBUG_PRINT("eol \"%s\"", style->eol == NULL ? "(null)" : anjuta_token_evaluate (style->eol));
	DEBUG_PRINT("last \"%s\"", style->last == NULL ? "(null)" : anjuta_token_evaluate (style->last));
}	
	
void anjuta_token_style_format (AnjutaTokenStyle *style, AnjutaToken *list)
{
	GList *args;
	GList *link;

#if 0	
	args = anjuta_token_split_list (list);

	if (args == NULL) return;

	if (((AnjutaToken *)args->next->data)->flags & (ANJUTA_TOKEN_ADDED | ANJUTA_TOKEN_REMOVED))
	{
		/* Update first separator */
		//anjuta_token_free_exclude_range ((AnjutaToken *)args->data, (AnjutaToken *)args->next->data);
	}

	/* FIXME: complete....*/
#endif
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

typedef struct _AnjutaTokenFileSaveData AnjutaTokenFileSaveData;

struct _AnjutaTokenFileSaveData
{
	GError **error;
	GFileOutputStream *stream;
	gboolean fail;
};

static gboolean
save_node (AnjutaToken *token, AnjutaTokenFileSaveData *data)
{
	if (!(anjuta_token_get_flags (token) & ANJUTA_TOKEN_REMOVED))
	{
		if (!(anjuta_token_get_flags (token) & ANJUTA_TOKEN_REMOVED) && (anjuta_token_get_length (token)))
		{
			if (g_output_stream_write (G_OUTPUT_STREAM (data->stream), anjuta_token_get_string (token), anjuta_token_get_length (token) * sizeof (char), NULL, data->error) < 0)
			{
				data->fail = TRUE;

				return TRUE;
			}
		}
	}
	
	return FALSE;
}

gboolean
anjuta_token_file_save (AnjutaTokenFile *file, GError **error)
{
	GFileOutputStream *stream;
	gboolean ok;
	GError *err = NULL;
	AnjutaTokenFileSaveData data;
	
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

	data.error = error;
	data.stream = stream;
	data.fail = FALSE;
	g_node_traverse ((GNode *)file->first, G_PRE_ORDER, G_TRAVERSE_ALL, -1, (GNodeTraverseFunc)save_node, &data);
	ok = g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, NULL);
	g_object_unref (stream);
	
	return !data.fail;
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
	else if (file->last == file->first)
	{
		g_node_insert_after ((GNode *)file->first, NULL, (GNode *)token);
	}
	else
	{
		while (file->last->parent != file->first)
		{
			file->last = file->last->parent;
		}
		g_node_insert_after ((GNode *)file->first, (GNode *)file->last, (GNode *)token);
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

	if (gfile)
	{
		file->file =  g_object_ref (gfile);
		file->first = anjuta_token_new_static (ANJUTA_TOKEN_FILE, NULL);
		file->last = file->first;
	}
	
	return file;
};
