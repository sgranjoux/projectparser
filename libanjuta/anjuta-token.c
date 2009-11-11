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
	AnjutaTokenData *data;
	AnjutaTokenType type;	
	gint flags;
	gchar *pos;
	gsize length;
};

struct _AnjutaToken
{
	union
	{
		AnjutaTokenData	*data;
		AnjutaToken *token;
	} dummy;
	AnjutaToken	*next;
	AnjutaToken	*prev;
	AnjutaToken	*parent;
	AnjutaToken *last;
	AnjutaToken	*children;
	AnjutaTokenData data;
};
		
#define ANJUTA_TOKEN_DATA(node)  ((node) != NULL ? (&(node->data)) : NULL)

/* Helpers functions
 *---------------------------------------------------------------------------*/

/* Return true and update end if the token is found.
 * If a close token is found, return FALSE but still update end */
gboolean
anjuta_token_match (AnjutaToken *token, gint flags, AnjutaToken *sequence, AnjutaToken **end)
{

	for (; sequence != NULL; /*sequence = flags & ANJUTA_SEARCH_BACKWARD ? anjuta_token_previous (sequence) : anjuta_token_next (sequence)*/)
	{
		AnjutaToken *toka;
		AnjutaToken *tokb = token;

		for (toka = sequence; toka != NULL; toka = anjuta_token_next_sibling (toka))
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
				break;
			}
		}

		if (flags & ANJUTA_SEARCH_BACKWARD)
		{
			sequence = flags & ANJUTA_SEARCH_INTO ? anjuta_token_previous (sequence) : anjuta_token_previous_sibling (sequence);
		}
		else
		{
			sequence = flags & ANJUTA_SEARCH_INTO ? anjuta_token_next (sequence) : anjuta_token_next_sibling (sequence);
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

	if (datb->type)
	{
		if (datb->type != data->type) return FALSE;
	}
	
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

AnjutaToken *
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
	return token ? token->next : NULL;
}

AnjutaToken *
anjuta_token_next_child (AnjutaToken *token)
{
	return token ? token->children : NULL;
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

	for (token = anjuta_token_next_child (token); token != NULL; token = anjuta_token_next_sibling (token))
	{
		last = token;
	}

	return last;
}

AnjutaToken *
anjuta_token_first_group (AnjutaToken *list)
{
	if ((list == NULL) || (list->last == NULL)) return NULL;

	return list->next;
}

AnjutaToken *
anjuta_token_next_group (AnjutaToken *group)
{
	AnjutaToken *last;

	if (group == NULL) return NULL;
	
	for (last = group; last->last != NULL; last = last->last);

	return last->next;
}

AnjutaToken *
anjuta_token_first_item (AnjutaToken *list)
{
	AnjutaToken *item;

	for (item = anjuta_token_first_group (list); item != NULL; item = anjuta_token_next_group (item))
	{
		switch (anjuta_token_get_type (item))
		{
		case ANJUTA_TOKEN_START:
		case ANJUTA_TOKEN_NEXT:
		case ANJUTA_TOKEN_LAST:
			continue;
		default:
			break;
		}
		break;
	}

	return item;
}

AnjutaToken *
anjuta_token_next_item (AnjutaToken *item)
{
	for (item = anjuta_token_next_group (item); item != NULL; item = anjuta_token_next_group (item))
	{
		switch (anjuta_token_get_type (item))
		{
		case ANJUTA_TOKEN_START:
		case ANJUTA_TOKEN_NEXT:
			continue;
		case ANJUTA_TOKEN_LAST:
			item = NULL;
			break;	
		default:
			break;
		}
		break;
	}

	return item;
}

AnjutaToken *
anjuta_token_parent (AnjutaToken *token)
{
	return token->parent;
}

AnjutaToken *
anjuta_token_unlink (AnjutaToken *token)
{
	if (token->prev != NULL)
	{
		token->prev->next = token->next;
	}
	else if (token->parent)
	{
		token->parent->children = token->next;
	}
	token->parent = NULL;
	if (token->next != NULL)
	{
		token->next->prev = token->prev;
		token->next = NULL;
	}
	token->prev = NULL;

	return token;
}

AnjutaToken*
anjuta_token_next_lex (AnjutaToken *token)
{
	AnjutaToken* next = token;
	
	for (; next != NULL;)
	{
		AnjutaTokenType type = anjuta_token_get_type (next);
		
		switch (type)
		{
		case ANJUTA_TOKEN_VARIABLE:
		case ANJUTA_TOKEN_COMMENT:
			next = anjuta_token_next_after_children (next);
			break;
		case ANJUTA_TOKEN_FILE:
		case ANJUTA_TOKEN_CONTENT:
		case ANJUTA_TOKEN_ARGUMENT:
			next = anjuta_token_next (next);
			break;
		default:
			if ((type < ANJUTA_TOKEN_FIRST) && (next != token))
			{
				return next;
			}
			next = anjuta_token_next_after_children (next);
			break;
		}
	}

	return NULL;
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

void
anjuta_token_evaluate_token (AnjutaToken *token, GString *value, gboolean raw)
{
	if ((token != NULL) && (ANJUTA_TOKEN_DATA (token)->length != 0))
	{
		if (!raw)
		{
			switch (anjuta_token_get_type (token))
			{
			case ANJUTA_TOKEN_COMMENT:
			case ANJUTA_TOKEN_OPEN_QUOTE:
			case ANJUTA_TOKEN_CLOSE_QUOTE:
			case ANJUTA_TOKEN_ESCAPE:
			case ANJUTA_TOKEN_MACRO:
			case ANJUTA_TOKEN_EOV:
				return;
			default:
				break;
			}
		}
		g_string_append_len (value, anjuta_token_get_string (token), anjuta_token_get_length (token));
	}
}	

void
anjuta_token_evaluate_child (AnjutaToken *token, GString *value, gboolean raw)
{
	anjuta_token_evaluate_token (token, value, raw);

	if (token->children) anjuta_token_evaluate_child (token->children, value, raw);

	if (token->next) anjuta_token_evaluate_child (token->next, value, raw);
}

void
anjuta_token_evaluate_group (AnjutaToken *token, GString *value, gboolean raw)
{
	AnjutaToken *child;
	
	anjuta_token_evaluate_token (token, value, raw);

	if (token->last != NULL)
	{
		for (child = anjuta_token_next (token); child != NULL; child = anjuta_token_next (child))
		{
			anjuta_token_evaluate_group (child, value, raw);
			if (child == token->last) break;
		}
	}
}

gchar *
anjuta_token_evaluate (AnjutaToken *token)
{
	GString *value = g_string_new (NULL);
	gchar *str;

	if (token != NULL)
	{
		if (token->last == NULL)
		{
			anjuta_token_evaluate_token (token, value, FALSE);
			if (token->children) anjuta_token_evaluate_child (token->children, value, FALSE);
		}
		else
		{
			anjuta_token_evaluate_group (token, value, FALSE);
		}
	}

	str = g_string_free (value, FALSE);
	return *str == '\0' ? NULL : str; 	
}

gchar *
anjuta_token_value (AnjutaToken *token)
{
	GString *value = g_string_new (NULL);
	gchar *str;

	if (token != NULL)
	{
		anjuta_token_evaluate_token (token, value, TRUE);
		if (token->children) anjuta_token_evaluate_child (token->children, value, TRUE);
	}

	str = g_string_free (value, FALSE);
	return *str == '\0' ? NULL : str; 	
}

AnjutaToken *
anjuta_token_merge (AnjutaToken *first, AnjutaToken *end)
{
	if ((first == end) || (end == NULL)) return first;
	
	if (first->parent == NULL)
	{
		first->parent = end->parent;
	}
	if (first->next == NULL)
	{
		anjuta_token_insert_before (end, first);
	}
	first->last = end;

	return first;
}

AnjutaToken *
anjuta_token_merge_children (AnjutaToken *first, AnjutaToken *end)
{
	if ((first == end) || (end == NULL)) return first;

	if (first->parent == NULL)
	{
		first->parent = end->parent;
	}
	if (first->next == NULL)
	{
		anjuta_token_insert_before (end, first);
	}
	if (end->last != NULL)
	{
		first->last = end->last;
	}
	anjuta_token_free (end);

	return first;
}

AnjutaToken *
anjuta_token_merge_previous (AnjutaToken *first, AnjutaToken *end)
{
	AnjutaToken *child;
	AnjutaToken *tok;

	if (first == end) return first;

	child = anjuta_token_next_child (first);
	do
	{
		tok = anjuta_token_previous_sibling (first);
		if (tok == NULL) break;
		
		anjuta_token_unlink (tok);
		child = anjuta_token_insert_before (child, tok);

	}
	while (tok != end);

	return first;
}

AnjutaToken *
anjuta_token_copy_token (AnjutaToken *token)
{
	AnjutaToken *copy = NULL;

	if (token != NULL)
	{
		copy = g_slice_new0 (AnjutaToken);
		copy->dummy.data = &copy->data;
		copy->data.type = token->data.type;
		copy->data.flags = token->data.flags;
		if ((copy->data.flags & ANJUTA_TOKEN_STATIC) || (token->data.pos == NULL))
		{
			copy->data.pos = token->data.pos;
		}
		else
		{
			copy->data.pos = g_strdup (token->data.pos);
		}
		copy->data.length = token->data.length;
	}

	return copy;
}

AnjutaToken *
anjuta_token_copy (AnjutaToken *token)
{
	AnjutaToken *copy;

	copy = anjuta_token_copy_token (token);
	if (copy != NULL)
	{
		AnjutaToken *child;
		AnjutaToken *last;

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
anjuta_token_clear (AnjutaToken *token)
{
	AnjutaTokenData *data = ANJUTA_TOKEN_DATA (token);

	if (!(data->flags & ANJUTA_TOKEN_STATIC))
	{
		g_free (data->pos);
	}
	data->length = 0;
	data->pos = NULL;

	return token;
}

AnjutaToken *
anjuta_token_append_child (AnjutaToken *parent, AnjutaToken *child)
{
	AnjutaToken *sibling;
	
	child->parent = parent;
  
	if (parent->children)
	{
  		sibling = parent->children;
		while (sibling->next)
		{
			sibling = sibling->next;
		}
  		child->prev = sibling;
  		sibling->next = child;
	}
   	else
	{
		child->parent->children = child;
   	}

	return child;
}

AnjutaToken *
anjuta_token_insert_child (AnjutaToken *parent, AnjutaToken *child)
{
	child->parent = parent;
	
	if (parent->children)
	{
		child->next = parent->children;
		parent->children->prev = child;
	}
	parent->children = child;

	return child;
}

AnjutaToken *
anjuta_token_insert_after (AnjutaToken *sibling, AnjutaToken *token)
{
	token->parent = sibling->parent;
	
	if (sibling->next)
	{
		sibling->next->prev = token;
	}
	token->next = sibling->next;
	token->prev = sibling;
	sibling->next = token;

	return token;
}

AnjutaToken *
anjuta_token_insert_before (AnjutaToken *sibling, AnjutaToken *baby)
{
	AnjutaToken *next = NULL;
	AnjutaToken *token;

	for (token = baby; token != NULL; token = next)
	{
		next = anjuta_token_next (token);
		
		token->parent = sibling->parent;
		if (sibling->prev)
		{
			token->prev = sibling->prev;
			token->prev->next = token;
			token->next = sibling;
	  		sibling->prev = token;
		}
		else
		{
	  		token->parent->children = token;
	  		token->next = sibling;
	  		sibling->prev = token;
		}
	}

	return baby;
}	

void
anjuta_token_foreach (AnjutaToken *token, AnjutaTokenTraverseFunc func, gpointer data)
{
	AnjutaToken *child;
	
	func (token, data);
		
	for (child = token->children; child != NULL;)
	{
		AnjutaToken *next = child->next;

		anjuta_token_foreach (child, func, data);
		child = next;
	}
}

AnjutaToken *anjuta_token_group (AnjutaToken *parent, AnjutaToken *last)
{
	AnjutaToken *child;
	AnjutaToken *tok;

	if (parent == last) return parent; 
	if (parent->children == last) return parent;

	child = anjuta_token_last_child (parent);
	do
	{
		tok = anjuta_token_next_sibling (parent);
		if (tok == NULL) break;
		
		anjuta_token_unlink (tok);
		if (child == NULL)
		{
			child = anjuta_token_append_child (parent, tok);
		}
		else
		{
			child = anjuta_token_insert_after (child, tok);
		}
	}
	while (tok != last);

	return parent;
	
}

AnjutaToken *anjuta_token_new_group (AnjutaTokenType type, AnjutaToken* first)
{
	AnjutaToken *parent = anjuta_token_new_static (type, NULL);

	anjuta_token_insert_before (first, parent);
	return anjuta_token_group (parent, first);
}

AnjutaToken *anjuta_token_ungroup (AnjutaToken *token)
{
	AnjutaToken *last = token;
	AnjutaToken *child;
	
	for (child = anjuta_token_next_child (token); child != NULL; child = anjuta_token_next_child (token))
	{
		anjuta_token_unlink (child);
		last = anjuta_token_insert_after (last, child);
	}

	return token;
}

AnjutaToken *anjuta_token_split (AnjutaToken *token, guint size)
{
	if (ANJUTA_TOKEN_DATA (token)->length > size)
	{
		AnjutaToken *copy;

		copy = anjuta_token_copy (token);
		anjuta_token_insert_before (token, copy);

		ANJUTA_TOKEN_DATA (copy)->length = size;
		if (ANJUTA_TOKEN_DATA (token)->flags & ANJUTA_TOKEN_STATIC)
		{
			ANJUTA_TOKEN_DATA (token)->pos += size;
			ANJUTA_TOKEN_DATA (token)->length -= size;
		}
		else
		{
			memcpy(ANJUTA_TOKEN_DATA (token)->pos, ANJUTA_TOKEN_DATA (token)->pos + size, ANJUTA_TOKEN_DATA (token)->length - size);
		}

		return copy;
	}
	else
	{
		return token;
	}
}

AnjutaToken *anjuta_token_cut (AnjutaToken *token, guint pos, guint size)
{
	AnjutaToken *copy;

	copy = anjuta_token_copy_token (token);

	if (pos >= ANJUTA_TOKEN_DATA (token)->length)
	{
		if (!(ANJUTA_TOKEN_DATA (copy)->flags & ANJUTA_TOKEN_STATIC))
		{
			g_free (ANJUTA_TOKEN_DATA (copy)->pos);
		}
		ANJUTA_TOKEN_DATA (copy)->pos = NULL;
		ANJUTA_TOKEN_DATA (copy)->length = 0;
	}
	if ((pos + size) > ANJUTA_TOKEN_DATA (token)->length)
	{
		size = ANJUTA_TOKEN_DATA (token)->length - pos;
	}
		
	if (ANJUTA_TOKEN_DATA (copy)->flags & ANJUTA_TOKEN_STATIC)
	{
		ANJUTA_TOKEN_DATA (copy)->pos += pos;
	}
	else
	{
		memcpy(ANJUTA_TOKEN_DATA (copy)->pos, ANJUTA_TOKEN_DATA (copy)->pos + pos, size);
	}
	ANJUTA_TOKEN_DATA (copy)->length = size;

	return copy;
}


AnjutaToken *anjuta_token_get_next_arg (AnjutaToken *arg, gchar ** value)
{
	for (;arg != NULL;)
	{
		switch (anjuta_token_get_type (arg))
		{
			case ANJUTA_TOKEN_START:
			case ANJUTA_TOKEN_NEXT:
			case ANJUTA_TOKEN_LAST:
				arg = anjuta_token_next_sibling (arg);
				continue;
			default:
				*value = anjuta_token_evaluate (arg);
				arg = anjuta_token_next_sibling (arg);
				break;
		}
		break;
	}
	
	return arg;
}

static AnjutaToken*
anjuta_token_dump_child (AnjutaToken *token, gint indent)
{
	fprintf (stdout, "%*s%p", indent, "", token);
	fprintf (stdout, ": %d \"%.*s\"\n", anjuta_token_get_type (token), anjuta_token_get_length (token), anjuta_token_get_string (token));

	if (token->last != NULL)
	{
		AnjutaToken *child;
		AnjutaToken *next;
		gboolean show = TRUE;
		
		for (child = anjuta_token_next (token); child != NULL; child = anjuta_token_next (child))
		{
			if (show)
			{
				next = anjuta_token_dump_child (child, indent + 4);
				show = FALSE;
			}
			if (child == next) show = TRUE;
			if (child == token->last) return child;
		}
	}

	return token;
}

void
anjuta_token_dump (AnjutaToken *token)
{
	anjuta_token_dump_child (token, 0);
}

/* Constructor & Destructor
 *---------------------------------------------------------------------------*/

AnjutaToken *anjuta_token_new_string (AnjutaTokenType type, const gchar *value)
{
	AnjutaToken *token;
	
	if (value == NULL)
	{
		token = anjuta_token_new_static (type, NULL);
	}
	else
	{
		token = g_slice_new0 (AnjutaToken);
		token->dummy.data = &token->data;
		token->data.type = type  & ANJUTA_TOKEN_TYPE;
		token->data.flags = type & ANJUTA_TOKEN_FLAGS;
		token->data.pos = g_strdup (value);
		token->data.length = strlen (value);
	}

	return token;
}

AnjutaToken*
anjuta_token_new_with_string (AnjutaTokenType type, gchar *value, gsize length)
{
	AnjutaToken *token;
	
	if (value == NULL)
	{
		token = anjuta_token_new_static (type, NULL);
	}
	else
	{
		token = g_slice_new0 (AnjutaToken);
		token->dummy.data = &token->data;
		token->data.type = type  & ANJUTA_TOKEN_TYPE;
		token->data.flags = type & ANJUTA_TOKEN_FLAGS;
		token->data.pos = value;
		token->data.length = length;
	}

	return token;
}

AnjutaToken *
anjuta_token_new_fragment (gint type, const gchar *pos, gsize length)
{
	AnjutaToken *token;

	token = g_slice_new0 (AnjutaToken);
	token->dummy.data = &token->data;
	token->data.type = type  & ANJUTA_TOKEN_TYPE;
	token->data.flags = (type & ANJUTA_TOKEN_FLAGS) | ANJUTA_TOKEN_STATIC;
	token->data.pos = (gchar *)pos;
	token->data.length = length;

	return token;
};

AnjutaToken *anjuta_token_new_static (AnjutaTokenType type, const char *value)
{
	return anjuta_token_new_fragment (type, value, value == NULL ? 0 : strlen (value));	
}

void
anjuta_token_free (AnjutaToken *token)
{
	AnjutaToken *child;
	
	if (token == NULL) return;

	for (child = token->children; child != NULL; child = token->children)
	{
		anjuta_token_free (child);
	}

	anjuta_token_unlink (token);
	if ((token->data.pos != NULL) && !(token->data.flags & ANJUTA_TOKEN_STATIC))
	{
		g_free (token->data.pos);
	}
	g_slice_free (AnjutaToken, token);
}
