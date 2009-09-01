/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta-token-style.c
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

#include "anjuta-token-style.h"

#include "libanjuta/anjuta-debug.h"

#include <string.h>

/* Type definition
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

/* Private functions
 *---------------------------------------------------------------------------*/

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

/* Public functions
 *---------------------------------------------------------------------------*/

void
anjuta_token_style_set_eol (AnjutaTokenStyle *style, const gchar *eol)
{
	if (style->eol) anjuta_token_free (style->eol);
	style->eol = anjuta_token_new_string (ANJUTA_TOKEN_SPACE, eol);
}

void
anjuta_token_style_update (AnjutaTokenStyle *style, AnjutaToken *list)
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
	sep_list = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)anjuta_token_style_separator_free);
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
	eol = NULL;
	sep = NULL;
	g_hash_table_iter_init (&iter, sep_list);
	while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
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
	if (eol != NULL) 
	{
		if (style->eol != NULL) anjuta_token_free (style->eol);
		style->eol = anjuta_token_copy (eol->token);
	}
	if (sep != NULL)
	{
		if (style->sep != NULL) anjuta_token_free (style->sep);
		style->sep = anjuta_token_copy (sep->token);
	}

	g_hash_table_destroy (sep_list);

	DEBUG_PRINT("Update style new:2");
	DEBUG_PRINT("first \"%s\"", style->first == NULL ? "(null)" : anjuta_token_evaluate (style->first));
	DEBUG_PRINT("sep \"%s\"", style->sep == NULL ? "(null)" : anjuta_token_evaluate (style->sep));
	DEBUG_PRINT("eol \"%s\"", style->eol == NULL ? "(null)" : anjuta_token_evaluate (style->eol));
	DEBUG_PRINT("last \"%s\"", style->last == NULL ? "(null)" : anjuta_token_evaluate (style->last));
}	

static void
anjuta_token_style_format_line (AnjutaTokenStyle *style, AnjutaToken *bol, AnjutaToken *eol)
{
	
}

void
anjuta_token_style_format (AnjutaTokenStyle *style, AnjutaToken *list)
{
	AnjutaToken *arg;

	if (style->sep == NULL)
	{
		for (arg = anjuta_token_next_child (list); arg != NULL; arg = anjuta_token_next_sibling (arg))
		{
			if ((anjuta_token_get_type (arg) == ANJUTA_TOKEN_SPACE) && (anjuta_token_get_flags (arg) & (ANJUTA_TOKEN_ADDED)))
			{
				anjuta_token_insert_after (arg, anjuta_token_copy (style->eol));
				anjuta_token_free (arg);
			}
		}
	}
	else
	{
		AnjutaToken *bol = anjuta_token_next_child (list);
		gboolean modified = FALSE;
		
		for (arg = bol; arg != NULL; arg = anjuta_token_next_sibling (arg))
		{
			gchar *value = anjuta_token_evaluate (arg);
			if (anjuta_token_get_flags (arg) & (ANJUTA_TOKEN_REMOVED | ANJUTA_TOKEN_ADDED)) modified = TRUE;
			if (strchr (value, '\n'))
			{
				if (modified) anjuta_token_style_format_line (style, list, arg);
				bol = arg;
				if (style->sep == NULL) modified = FALSE;
			}
			g_free (value);
		}
		if (modified) anjuta_token_style_format_line (style, bol, NULL);
	}
}

/* Constructor & Destructor
 *---------------------------------------------------------------------------*/

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
