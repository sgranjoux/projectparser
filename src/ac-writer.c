/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4; coding: utf-8 -*- */
/* ac-writer.c
 *
 * Copyright (C) 2009  Sébastien Granjoux
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ac-writer.h"

#include "am-project-private.h"

#include <libanjuta/anjuta-debug.h>
#include <libanjuta/anjuta-utils.h>

/* Types
  *---------------------------------------------------------------------------*/


/* Helper functions
 *---------------------------------------------------------------------------*/

/* Private functions
 *---------------------------------------------------------------------------*/

static gboolean
remove_list_item (AnjutaToken *token, AnjutaTokenStyle *user_style)
{
	AnjutaTokenStyle *style;
	AnjutaToken *space;

	DEBUG_PRINT ("remove list item");

	style = user_style != NULL ? user_style : anjuta_token_style_new (0);
	anjuta_token_style_update (style, anjuta_token_parent (token));
	
	anjuta_token_remove (token);
	space = anjuta_token_next_sibling (token);
	if (space && (anjuta_token_get_type (space) == ANJUTA_TOKEN_SPACE) && (anjuta_token_next (space) != NULL))
	{
		/* Remove following space */
		anjuta_token_remove (space);
	}
	else
	{
		space = anjuta_token_previous_sibling (token);
		if (space && (anjuta_token_get_type (space) == ANJUTA_TOKEN_SPACE) && (anjuta_token_previous (space) != NULL))
		{
			anjuta_token_remove (space);
		}
	}
	
	anjuta_token_style_format (style, anjuta_token_parent (token));
	if (user_style == NULL) anjuta_token_style_free (style);
	
	return TRUE;
}

static gboolean
add_list_item (AnjutaToken *list, AnjutaToken *token, AnjutaTokenStyle *user_style)
{
	AnjutaTokenStyle *style;
	AnjutaToken *space;

	style = user_style != NULL ? user_style : anjuta_token_style_new (0);
	anjuta_token_style_update (style, anjuta_token_parent (list));
	
	space = anjuta_token_new_static (ANJUTA_TOKEN_SPACE | ANJUTA_TOKEN_ADDED, " ");
	space = anjuta_token_insert_after (list, space);
	anjuta_token_insert_after (space, token);

	anjuta_token_style_format (style, anjuta_token_parent (list));
	if (user_style == NULL) anjuta_token_style_free (style);
	
	return TRUE;
}

/* Public functions
 *---------------------------------------------------------------------------*/

gboolean
amp_project_update_property (AmpProject *project, AmpPropertyType type)
{
	AnjutaToken *token;
	
	if (project->property == NULL)
	{
		return FALSE;
	}


/*	for (token = project->property;; token = anjuta_next_sibling (token))
	{
			if ((arg != NULL) && (anjuta_token_get_type (arg) == ANJUTA_TOKEN_SEPARATOR))
	{
		arg = anjuta_token_next_sibling (arg);
	}
	if ((arg != NULL) && (anjuta_token_get_type (arg) != ANJUTA_TOKEN_SEPARATOR))

		switch (type)
		{
			case AMP_PROPERTY_NAME:
			case AMP_PROPERTY_VERSION:
			case AMP_PROPERTY_BUG_REPORT:
			case AMP_PROPERTY_TARNAME:
			case AMP_PROPERTY_URL:
		}
	}*/	
	
	return TRUE;
}

