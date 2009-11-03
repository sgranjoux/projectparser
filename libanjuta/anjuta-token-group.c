/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta-token-group.c
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

/* 
 * This object represents a hierarchical list of tokens. Keeping the hierarchy
 * is useful by example to represent a rule we can have:
 *
 *	rule:
 *		+ target:
 *			+ first_target
 *				NAME
 *			+ second_target
 *				NAME
 *		+ dependencies:
 *			+ first_dependencies
 *				NULL
 *		+ commands:
 *			NAME
 *			NAME
 *  The tokens are the leaves having a name in uppercase.
 *
 * This hierarchy is a logical view of the component and containing only the
 * useful part, the separator are not included. It is possible that two
 * neighbour tokens are quite far in the file. Typically, if one comes from a
 * variable expansion. The physical location of each token is saved inside the
 * token data.
 *
 * The token are not copied, the group keeps only a reference to these tokens.
 */ 

#include "anjuta-token-group.h"

#include <stdio.h>

struct _AnjutaTokenGroup
{
	AnjutaToken *token;
	AnjutaTokenGroup	*next;
	AnjutaTokenGroup	*prev;
	AnjutaTokenGroup	*parent;
	AnjutaTokenGroup	*children;
};

/* Public function
 *---------------------------------------------------------------------------*/

AnjutaToken *anjuta_token_group_get_token (AnjutaTokenGroup *group)
{
	return group->token;
}

AnjutaTokenGroup *anjuta_token_group_first (AnjutaTokenGroup *list)
{
	return (AnjutaTokenGroup *)g_node_first_child ((GNode *)list);
}

AnjutaTokenGroup *anjuta_token_group_next (AnjutaTokenGroup *item)
{
	return (AnjutaTokenGroup *)g_node_next_sibling ((GNode *)item);
}

AnjutaTokenGroup *anjuta_token_group_append (AnjutaTokenGroup *parent, AnjutaTokenGroup *group)
{
	if (group != NULL) g_node_append ((GNode *)parent, (GNode *)group);

	return parent;
}

AnjutaTokenGroup *anjuta_token_group_append_token (AnjutaTokenGroup *parent, AnjutaToken *token)
{
	AnjutaTokenGroup *group;
	
	group = (AnjutaTokenGroup *)g_node_new (token);
	return anjuta_token_group_append (parent, group);
}

AnjutaTokenGroup *anjuta_token_group_append_children (AnjutaTokenGroup *parent, AnjutaTokenGroup *children)
{
	if (children != NULL)
	{
		AnjutaTokenGroup *child;
		
		for (child = anjuta_token_group_first (children); child != NULL;)
		{
			AnjutaTokenGroup *next = anjuta_token_group_next (child);

			g_node_unlink ((GNode *)child);
			g_node_append ((GNode *)parent, (GNode *)child);
			child = next;
		}
		g_node_destroy ((GNode *)children);
	}

	return parent;
}

AnjutaToken *anjuta_token_group_into_token (AnjutaTokenGroup *group)
{
	AnjutaToken *copy = NULL;

	if (group != NULL)
	{
		AnjutaTokenGroup *child;
		AnjutaToken *last;

		copy = anjuta_token_copy_token (anjuta_token_group_get_token (group));

		last = NULL;
		for (child = anjuta_token_group_first (group); child != NULL; child = anjuta_token_group_next (child))
		{
			AnjutaToken *new_child = anjuta_token_group_into_token (child);
			last =  last == NULL ? anjuta_token_insert_child (copy, new_child) : anjuta_token_insert_after (last, new_child);
		}
	}

	return copy;
}

gchar *
anjuta_token_group_evaluate (AnjutaTokenGroup *group)
{
	GString *value = g_string_new (NULL);
	gchar *str;
	gboolean raw = TRUE;

	if (group != NULL)
	{
		AnjutaTokenGroup *child;
		
		for (child = anjuta_token_group_first (group); child != NULL; child = anjuta_token_group_next (child))
		{
			AnjutaToken *token = anjuta_token_group_get_token (child);
			
			anjuta_token_evaluate_token (token, value, raw);
			token = anjuta_token_next_child (token);
			if (token) anjuta_token_evaluate_child (token, value, raw);
		}
	}

	str = g_string_free (value, FALSE);
	return *str == '\0' ? NULL : str; 	
}

static void
anjuta_token_group_dump_child (AnjutaTokenGroup *group, gint indent)
{
	AnjutaToken *token = group->token;
	
	fprintf (stdout, "%*s%p", indent, "", token);
	fprintf (stdout, ": %d \"%.*s\"\n", anjuta_token_get_type (token), anjuta_token_get_length (token), anjuta_token_get_string (token));
	
	for (group = anjuta_token_group_first (group); group != NULL; group = anjuta_token_group_next (group))
	{
		anjuta_token_group_dump_child (group, indent + 4);
	}
}

void
anjuta_token_group_dump (AnjutaTokenGroup *token)
{
	anjuta_token_group_dump_child (token, 0);
}

/* Constructor & Destructor
 *---------------------------------------------------------------------------*/

AnjutaTokenGroup *anjuta_token_group_new (AnjutaTokenType type, AnjutaTokenGroup* first)
{
	AnjutaToken *token;
	AnjutaTokenGroup *group;

	token = anjuta_token_new_static (type, NULL);

	group = (AnjutaTokenGroup *)g_node_new (token);
	if (first != NULL)
	{
		AnjutaTokenGroup *child;

		child = (AnjutaTokenGroup *)g_node_new (first);
		g_node_insert_before ((GNode *)group, NULL, (GNode *)child);
	}

	return group;
}

void
anjuta_token_group_free (AnjutaTokenGroup *token)
{
	if (token == NULL) return;

	g_node_destroy ((GNode *)token);
}
