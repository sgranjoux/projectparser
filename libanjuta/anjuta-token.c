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

/**
 * SECTION:anjuta-token
 * @title: Anjuta token
 * @short_description: Anjuta token
 * @see_also: 
 * @stability: Unstable
 * @include: libanjuta/anjuta-token.h
 *  
 * A #AnjutaToken represents a token. It is a sequence of characters associated
 * with a type representing its meaning. By example, a token can represent
 * a keyword, a comment, a variable...
 *
 * The token can own the string or has only a pointer on some data allocated
 * somewhere else with a length.
 *
 * A token is linked with other tokens using three double linked lists.
 *
 * The first list using next and prev fields is used to keep the token in the
 * order where there are in the file. The first character of the first token is
 * the first character in the file.
 *
 * A second list is used to represent included
 * files. Such file is represented by a special token in the first list which
 * has a pointer, named children to a token list. Each token in this secondary
 * list has a pointer to its parent, it means the token representing the file
 * where is the token. It looks like a tree. In fact, every file is represented
 * by a special token, so the root node is normally a file token and has as
 * children all the token representing the file content. This parent/child list
 * is used for expanded variable too.
 *
 * A third list is used to group several tokens. A token can have a pointer to
 * another last token. It means that this token is a group starting from this
 * token to the one indicated by the last field. In addition each token in this
 * group has a pointer on the first token of the group. This grouping is
 * independent of the parent/child list. So a group can start in one file and
 * end in another included file. The grouping can be nested too. Typically
 * we can have a group representing a command, a sub group representing the 
 * arguments and then one sub group for each argument.
 */ 


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
	AnjutaToken	*next;
	AnjutaToken	*prev;
	AnjutaToken	*parent;
	AnjutaToken *last;
	AnjutaToken *group;
	AnjutaToken	*children;
	AnjutaTokenData data;
};

/* Helpers functions
 *---------------------------------------------------------------------------*/

/* Private functions
 *---------------------------------------------------------------------------*/

static AnjutaToken *
anjuta_token_next_child (AnjutaToken *child, AnjutaToken **last)
{
	if (child == NULL) return child;
	
	if (child->children != NULL)
	{
		child = child->children;
	}
	else
	{
		for (;;)
		{
			if ((*last == NULL) || (child == *last))
			{
				if (child->last == NULL)
				{
					child = NULL;
					break;
				}
				*last = child->last;
			}
			if (child->next != NULL)
			{
				child = child->next;
				break;
			}
			child = child->parent;
		}
	}

	return child;
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
anjuta_token_copy (AnjutaToken *token)
{
	AnjutaToken *copy = NULL;

	if (token != NULL)
	{
		copy = g_slice_new0 (AnjutaToken);
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

static AnjutaToken *
anjuta_token_unlink (AnjutaToken *token)
{

	if (token->prev != NULL)
	{
		token->prev->next = token->next;
	}
	else if ((token->parent != NULL) && (token->parent->children == token))
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

static void
anjuta_token_evaluate_token (AnjutaToken *token, GString *value, gboolean raw)
{
	if ((token != NULL) && (token->data.length != 0))
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

static void
anjuta_token_show (AnjutaToken *token, gint indent)
{
	fprintf (stdout, "%*s%p", indent, "", token);
	fprintf (stdout, ": %d \"%.*s\" %p/%p %s\n",
	    anjuta_token_get_type (token),
	    anjuta_token_get_length (token),
	    anjuta_token_get_string (token),
	    token->last, token->children,
	    anjuta_token_get_flags (token) & ANJUTA_TOKEN_REMOVED ? " (removed)" : "");
}

static AnjutaToken*
anjuta_token_dump_child (AnjutaToken *token, gint indent)
{

	anjuta_token_show (token, indent);
	
	if (token->children != NULL)
	{
		AnjutaToken *child;
		AnjutaToken *next = NULL;
		AnjutaToken *last = anjuta_token_next_after_children (token);
		
		for (child = token->children; child != NULL;)
		{
			if (child == last)
			{
				return child;
			}
			if (next == NULL)
			{
				next = anjuta_token_dump_child (child, indent + 4);
			}
			if (child == next)
			{
				next = NULL;
				
#if 0
				/* Look for previous children */
				for (child = anjuta_token_next (child); child != NULL; child = child->parent)
				{
					if (child->parent == token) break;
				};
				continue;
#endif
			}
			child = anjuta_token_next (child);
		}
	}
	
	if (token->last != NULL)
	{
		AnjutaToken *child;
		AnjutaToken *next = NULL;
		
		for (child = anjuta_token_next (token); child != NULL; child = anjuta_token_next (child))
		{
			if (next == NULL)
			{
				next = anjuta_token_dump_child (child, indent + 4);
				if (child == token->last)
				{
					return child;
				}
			}
			if (child == next) next = NULL;
			if (child == token->last)
			{
				return child;
			}
		}
	}

	return token;
}

static gboolean
anjuta_token_check_child (AnjutaToken *token, AnjutaToken *parent)
{
	if (token->parent != parent)
	{
		anjuta_token_show (token, 0);
		fprintf(stderr, "Error: Children has %p as parent instead of %p\n", token->parent, parent);
		return FALSE;
	}

	return anjuta_token_check (token);
}

/* Get and set functions
 *---------------------------------------------------------------------------*/

void
anjuta_token_set_type (AnjutaToken *token, gint type)
{
	token->data.type = type;
}

gint
anjuta_token_get_type (AnjutaToken *token)
{
	return token->data.type;
}

void
anjuta_token_set_flags (AnjutaToken *token, gint flags)
{
	AnjutaToken *child;
	AnjutaToken *last = token->last;
	
	for (child = token; child != NULL; child = anjuta_token_next_child (child, &last))
	{
		child->data.flags |= flags;
	}
}

void
anjuta_token_clear_flags (AnjutaToken *token, gint flags)
{
	token->data.flags &= ~flags;
}

gint
anjuta_token_get_flags (AnjutaToken *token)
{
	return token->data.flags;
}

void
anjuta_token_set_string (AnjutaToken *token, const gchar *data, guint length)
{
	if (!(token->data.flags & ANJUTA_TOKEN_STATIC))
	{
		g_free (token->data.pos);
		token->data.flags |= ANJUTA_TOKEN_STATIC;
	}
	token->data.pos = (gchar *)data;
	token->data.length = length;
}

const gchar *
anjuta_token_get_string (AnjutaToken *token)
{
	return token->data.pos;
}

guint
anjuta_token_get_length (AnjutaToken *token)
{
	return token->data.length;
}

/* Basic move functions
 *---------------------------------------------------------------------------*/

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
anjuta_token_last (AnjutaToken *token)
{
	AnjutaToken *last;
	
	for (last = token; last->last != NULL; last = last->last);
	if (last->children != NULL)
	{
		for (last = last->children; last->next != NULL; last = last->next);
	}

	return last;
}

AnjutaToken *
anjuta_token_parent (AnjutaToken *token)
{
	return token->parent;
}

AnjutaToken *
anjuta_token_list (AnjutaToken *token)
{
	return token->group;
}

AnjutaToken *
anjuta_token_parent_group (AnjutaToken *token)
{
	return token->group;
}

/* Item move functions
 *---------------------------------------------------------------------------*/

AnjutaToken *
anjuta_token_last_item (AnjutaToken *list)
{
	return list->last;
}

AnjutaToken *
anjuta_token_first_item (AnjutaToken *list)
{
	if (list == NULL) return NULL;
	if (list->children != NULL) return list->children;
	if (list->last != NULL) return list->next;

	return NULL;
}

AnjutaToken *
anjuta_token_next_item (AnjutaToken *item)
{
	AnjutaToken *last;

	if (item == NULL) return NULL;

	if ((item->group != NULL) && (item == item->group->last)) return NULL;
	for (last = item; last->last != NULL; last = last->last);

	return last->next;
}

AnjutaToken *
anjuta_token_previous_item (AnjutaToken *item)
{
	AnjutaToken *first;

	if (item == NULL) return NULL;

	for (first = item->prev; (first != NULL) && (first->group != item->group); first = first->group);

	return first;
}

/* Add/Insert/Remove tokens
 *---------------------------------------------------------------------------*/

/**
 * anjuta_token_append_child:
 * @parent: a #AnjutaToken object used as parent.
 * @children: a #AnjutaToken object.
 *
 * Insert all tokens in children as the last children of the given parent.
 *
 * Return value: The first token append.
 */
AnjutaToken *
anjuta_token_append_child (AnjutaToken *parent, AnjutaToken *children)
{
	AnjutaToken *token;
	AnjutaToken *last;
	AnjutaToken *old_group;
	AnjutaToken *old_parent;

	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (children != NULL, NULL);

	old_group = children->group;
	old_parent = children->parent;
	
	if (parent->children == NULL)
	{
		parent->children = children;
		
		children->prev = NULL;
	}
	else
	{
		/* Find last children */
		for (last = parent->children; last->next != NULL;)
		{
			if ((last->last != NULL) && (last->last->parent == last->parent))
			{
				last = last->last;
			}
			else
			{
				last = last->next;
			}
		}

		last->next = children;
		children->prev = last;
	}

	/* Update each token */	
	for (token = children;;)
	{
		if (token->parent == old_parent) token->parent = parent;
		if (token->group == old_group) token->group = parent->group;

		if (token->children != NULL)
		{
			token = token->children;
		}
		else if (token->next != NULL)
		{
			token = token->next;
		}
		else
		{
			while (token->parent != parent)
			{
				token = token->parent;
				if (token->next != NULL) break;
			}
			if (token->next == NULL) break;
			token = token->next;
		}
	}

	return children;
}

/**
 * anjuta_token_prepend_child:
 * @parent: a #AnjutaToken object used as parent.
 * @children: a #AnjutaToken object.
 *
 * Insert all tokens in children as the first children of the given parent.
 *
 * Return value: The first token append.
 */
AnjutaToken *
anjuta_token_prepend_child (AnjutaToken *parent, AnjutaToken *children)
{
	AnjutaToken *child;
	AnjutaToken *last = NULL;

	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (children != NULL, NULL);

	/* Update each token */	
	for (child = children;;)
	{
		AnjutaToken *next;
		
		if (child->parent == children->parent) child->parent = parent;
		if (child->group == children->group) child->group = parent->group;

		next = anjuta_token_next_child (child, &last);
		if (next == NULL) break;
		child = next;
	}

	child->next = parent->children;
	if (child->next) child->next->prev = child;
	parent->children = children;

	return children;
}

/**
 * anjuta_token_prepend_items:
 * @list: a #AnjutaToken object used as list.
 * @item: a #AnjutaToken object.
 *
 * Insert all tokens in item as item of the given list.
 *
 * Return value: The first token append.
 */
AnjutaToken *
anjuta_token_prepend_items (AnjutaToken *list, AnjutaToken *item)
{
	AnjutaToken *token;
	AnjutaToken *old_group;
	AnjutaToken *old_parent;

	g_return_val_if_fail (list != NULL, NULL);
	g_return_val_if_fail (item != NULL, NULL);

	old_group = item->group;
	old_parent = item->parent;

	/* Update each token */	
	for (token = item;;)
	{
		if (token->parent == old_parent) token->parent = list->parent;
		if (token->group == old_group) token->group = list;

		if (token->children != NULL)
		{
			token = token->children;
		}
		else if (token->next != NULL)
		{
			token = token->next;
		}
		else
		{
			while (token->parent != list->parent)
			{
				token = token->parent;
				if (token->next != NULL) break;
			}
			if (token->next == NULL) break;
			token = token->next;
		}
	}

	token->next = list->next;
	if (token->next) token->next->prev = token;

	list->next = item;
	item->prev = list;

	if (list->last == NULL)
	{
		while (token->group != list) token = token->group;
		list->last = token;
	}

	return item;
}

/**
 * anjuta_token_insert_after:
 * @sibling: a #AnjutaToken object.
 * @item: a #AnjutaToken object.
 *
 * Insert all tokens after sibling.
 *
 * Return value: The first token inserted.
 */
AnjutaToken *
anjuta_token_insert_after (AnjutaToken *sibling, AnjutaToken *list)
{
	AnjutaToken *last;
	AnjutaToken *token;
	AnjutaToken *old_group;
	AnjutaToken *old_parent;

	g_return_val_if_fail (sibling != NULL, NULL);
	g_return_val_if_fail (list != NULL, NULL);

	old_group = list->group;
	old_parent = list->parent;

	/* Update each token */	
	for (token = list;;)
	{
		if (token->parent == old_parent) token->parent = sibling->parent;
		if (token->group == old_group) token->group = sibling->group;

		if (token->children != NULL)
		{
			token = token->children;
		}
		else if (token->next != NULL)
		{
			token = token->next;
		}
		else
		{
			while (token->parent != sibling->parent)
			{
				token = token->parent;
				if (token->next != NULL) break;
			}
			if (token->next == NULL) break;
			token = token->next;
		}
	}

	for (last = sibling; last->last != NULL; last = last->last);

	token->next = last->next;
	if (token->next) token->next->prev = token;
	
	last->next = list;
	list->prev = last;

	if ((sibling->group != NULL) && (sibling->group->last == sibling))
	{
		while (token->group != sibling->group) token = token->group;
		sibling->group->last = token;
	}

	return list;
}

/**
 * anjuta_token_insert_before:
 * @sibling: a #AnjutaToken object.
 * @item: a #AnjutaToken object.
 *
 * Insert all tokens before sibling.
 *
 * Return value: The first token inserted.
 */
AnjutaToken *
anjuta_token_insert_before (AnjutaToken *sibling, AnjutaToken *list)
{
	AnjutaToken *last;
	AnjutaToken *token;
	AnjutaToken *old_group;
	AnjutaToken *old_parent;

	g_return_val_if_fail (sibling != NULL, NULL);
	g_return_val_if_fail (list != NULL, NULL);

	old_group = list->group;
	old_parent = list->parent;

	/* Update each token */	
	for (token = list;;)
	{
		if (token->parent == old_parent) token->parent = sibling->parent;
		if (token->group == old_group) token->group = sibling->group;

		if (token->children != NULL)
		{
			token = token->children;
		}
		else if (token->next != NULL)
		{
			token = token->next;
		}
		else
		{
			while (token->parent != sibling->parent)
			{
				token = token->parent;
				if (token->next != NULL) break;
			}
			if (token->next == NULL) break;
			token = token->next;
		}
	}

	for (last = sibling; last->last != NULL; last = last->last);

	token->next = sibling;
	list->prev = sibling->prev;
	sibling->prev = token;

	if (list->prev) list->prev->next = list;

	if ((list->parent != NULL) && (list->parent->children == sibling)) list->parent->children = list;
	
	return list;
}

/**
 * anjuta_token_delete_parent:
 * @parent: a #AnjutaToken object used as parent.
 *
 * Delete only the parent token.
 *
 * Return value: the first children
 */
AnjutaToken *
anjuta_token_delete_parent (AnjutaToken *parent)
{
	AnjutaToken *token;

	g_return_val_if_fail (parent != NULL, NULL);

	if (parent->children == NULL) return NULL;
	
	/* Update each token */	
	for (token = parent->children;;)
	{
		if (token->parent == parent) token->parent = parent->parent;

		if (token->children != NULL)
		{
			token = token->children;
		}
		else if (token->next != NULL)
		{
			token = token->next;
		}
		else
		{
			while (token->parent != parent->parent)
			{
				token = token->parent;
				if (token->next != NULL) break;
			}
			if (token->next == NULL) break;
			token = token->next;
		}
	}

	token->next = parent->next;
	if (token->next) token->next->prev = token;

	parent->next = parent->children;
	parent->children->prev = parent;
	parent->children = NULL;

	return anjuta_token_free (parent);
}

/* Merge function
 *---------------------------------------------------------------------------*/

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
	end->group = first;

	return first;
}

AnjutaToken *
anjuta_token_merge_own_children (AnjutaToken *group)
{
	AnjutaToken *token;
	AnjutaToken *next = NULL;
	
	if (group->last != NULL) return group;

	if (group->last->last != NULL) group->last = group->last->last;

	for (token = anjuta_token_next (group); (token != NULL) && (token != group->last); token = anjuta_token_next (token))
	{
		if (next == NULL)
		{
			if (token->last != NULL)
			{
				next = token->last;
				//token->last = NULL;
			}
			token->group = group;
		}
		else if (next == token)
		{
			next = NULL;
		}
	}

	return group;
}

AnjutaToken *
anjuta_token_merge_children (AnjutaToken *first, AnjutaToken *end)
{
	if ((first == end) || (end == NULL))
	{
		return first;
	}

	if (first->parent == NULL)
	{
		first->parent = end->parent;
	}
	if (first->next == NULL)
	{
		anjuta_token_insert_before (end, first);
	}
	anjuta_token_unlink (end);
	if (end->last != NULL)
	{
		first->last = end->last;
		end->last->group = first;
	}
	end->group = first;
	anjuta_token_free (end);

	return first;
}

AnjutaToken *
anjuta_token_merge_previous (AnjutaToken *first, AnjutaToken *end)
{
	if ((end == NULL) || (first == end)) return first;

	anjuta_token_unlink (first);
	anjuta_token_insert_before (end, first);
	end->group = first;

	return first;
}

AnjutaToken *anjuta_token_split (AnjutaToken *token, guint size)
{
	if (token->data.length > size)
	{
		AnjutaToken *copy;

		copy = anjuta_token_copy (token);
		anjuta_token_insert_before (token, copy);

		copy->data.length = size;
		if (token->data.flags & ANJUTA_TOKEN_STATIC)
		{
			token->data.pos += size;
			token->data.length -= size;
		}
		else
		{
			memcpy(token->data.pos, token->data.pos + size, token->data.length - size);
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

	copy = anjuta_token_copy (token);

	if (pos >= token->data.length)
	{
		if (!(copy->data.flags & ANJUTA_TOKEN_STATIC))
		{
			g_free (copy->data.pos);
		}
		copy->data.pos = NULL;
		copy->data.length = 0;
	}
	if ((pos + size) > token->data.length)
	{
		size = token->data.length - pos;
	}
		
	if (copy->data.flags & ANJUTA_TOKEN_STATIC)
	{
		copy->data.pos += pos;
	}
	else
	{
		memcpy(copy->data.pos, copy->data.pos + pos, size);
	}
	copy->data.length = size;

	return copy;
}

/* Token evaluation
 *---------------------------------------------------------------------------*/

gchar *
anjuta_token_evaluate (AnjutaToken *token)
{
	GString *value = g_string_new (NULL);

	if (token != NULL)
	{
		AnjutaToken *last = token->last;
		AnjutaToken *child;
		
		for (child = token; child != NULL; child = anjuta_token_next_child (child, &last))
		{
			anjuta_token_evaluate_token (child, value, TRUE);
		}
	}

	/* Return NULL and free data for an empty string */
	return g_string_free (value, *(value->str) == '\0');
}

/* Other functions
 *---------------------------------------------------------------------------*/

gboolean
anjuta_token_compare (AnjutaToken *toka, AnjutaToken *tokb)
{
	if (tokb->data.type)
	{
		if (tokb->data.type != toka->data.type) return FALSE;
	}
	
	if (tokb->data.type != ANJUTA_TOKEN_NONE)
	{
		if (tokb->data.length != 0)
		{
			if (toka->data.length != tokb->data.length) return FALSE;
		
			if ((toka->data.flags & ANJUTA_TOKEN_CASE_INSENSITIVE)  && (tokb->data.flags & ANJUTA_TOKEN_CASE_INSENSITIVE))
			{
				if (g_ascii_strncasecmp (toka->data.pos, tokb->data.pos, toka->data.length) != 0) return FALSE;
			}
			else
			{
				if (strncmp (toka->data.pos, tokb->data.pos, toka->data.length) != 0) return FALSE;
			}
		}
	}
		
	if (tokb->data.flags & ANJUTA_TOKEN_PUBLIC_FLAGS)
	{
		if ((toka->data.flags & tokb->data.flags & ANJUTA_TOKEN_PUBLIC_FLAGS) == 0)
			return FALSE;
	}

	return TRUE;
}

void
anjuta_token_dump (AnjutaToken *token)
{
	if (token == NULL) return;
	
	anjuta_token_dump_child (token, 0);
}

void
anjuta_token_dump_link (AnjutaToken *token)
{
	AnjutaToken *last = token;

	while (last->last != NULL) last = last->last;

	for (; token != last; token = anjuta_token_next (token))
	{
		anjuta_token_show (token, 0);
	}
}

gboolean
anjuta_token_check (AnjutaToken *token)
{
	if ((token->children != NULL) && (token->last != NULL))
	{
		anjuta_token_show (token, 0);
		fprintf(stderr, "Error: Previous token has both non NULL children and last\n");

		return FALSE;
	}

	if (token->children != NULL)
	{
		AnjutaToken *child;
		
		for (child = token->children; child != NULL; child = child->next)
		{
			if (!anjuta_token_check_child (child, token)) return FALSE;
		}
	}

	if (token->last != NULL)
	{
		AnjutaToken *child;
		
		for (child = anjuta_token_next (token); child != NULL; child = anjuta_token_next (child))
		{
			if (!anjuta_token_check (child)) return FALSE;
			if (child == token->last) break;
		}
	}
	
	return TRUE;
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

AnjutaToken*
anjuta_token_free_children (AnjutaToken *token)
{
	AnjutaToken *child;
	
	if (token == NULL) return NULL;

	for (child = token->children; child != NULL; child = token->children)
	{
		anjuta_token_free (child);
	}
	token->children = NULL;
	
	if (token->last != NULL)
	{
		for (child = anjuta_token_next (token); child != NULL; child = anjuta_token_next (token))
		{
			anjuta_token_free (child);
			if (child == token->last) break;
		}
	}
	token->last = NULL;

	return token;	
}

AnjutaToken*
anjuta_token_free (AnjutaToken *token)
{
	AnjutaToken *next;
	
	if (token == NULL) return NULL;

	anjuta_token_free_children (token);

	next = anjuta_token_next (token);
	anjuta_token_unlink (token);
	if ((token->data.pos != NULL) && !(token->data.flags & ANJUTA_TOKEN_STATIC))
	{
		g_free (token->data.pos);
	}
	g_slice_free (AnjutaToken, token);

	return next;
}
