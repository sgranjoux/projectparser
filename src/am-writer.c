/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4; coding: utf-8 -*- */
/* am-writer.c
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

#include "am-writer.h"
#include "ac-scanner.h"
#include "ac-parser.h"

#include "am-project-private.h"

#include <libanjuta/anjuta-debug.h>
#include <libanjuta/anjuta-utils.h>

/* Types
  *---------------------------------------------------------------------------*/


/* Helper functions
 *---------------------------------------------------------------------------*/

/* Private functions
 *---------------------------------------------------------------------------*/

/* Public functions
 *---------------------------------------------------------------------------*/

AnjutaToken *
amp_project_write_config_list (AmpProject *project)
{
	AnjutaToken *pos;
	AnjutaToken *token;
	static gint output_type[] = {AC_TOKEN_AC_OUTPUT, 0};
	static gint eol_type[] = {ANJUTA_TOKEN_EOL, ANJUTA_TOKEN_SPACE, ANJUTA_TOKEN_COMMENT, 0};
	
	pos = anjuta_token_find_type (project->configure_token, 0, output_type);
	if (pos == NULL)
	{
		gint other_type[] = {AC_TOKEN_AC_INIT,
			AC_TOKEN_PKG_CHECK_MODULES,
			AC_TOKEN_AC_CONFIG_FILES, 
			AC_TOKEN_OBSOLETE_AC_OUTPUT,
			AC_TOKEN_AC_PREREQ,
			0};
			
		pos = anjuta_token_find_type (project->configure_token, ANJUTA_TOKEN_SEARCH_LAST, other_type);
		if (pos == NULL)
		{
			pos = anjuta_token_skip_comment (project->configure_token);
		}
		else
		{
			AnjutaToken* next;

			next = anjuta_token_find_type (pos, ANJUTA_TOKEN_SEARCH_NOT, eol_type);
		}
		
	}

	token = anjuta_token_insert_token_before (pos,
	    		AC_TOKEN_AC_CONFIG_FILES, "AC_CONFIG_FILES(",
	    		ANJUTA_TOKEN_LIST, NULL,
	    		ANJUTA_TOKEN_LAST, NULL,
	    		RIGHT_PAREN, ")",
	    		NULL);
	
	return token;
}

AnjutaToken *
amp_project_write_config_file_before (AmpProject *project, AnjutaToken *list, AnjutaToken *sibling, const gchar *filename)
{
	AnjutaToken *token;

	token = anjuta_token_new_string (ANJUTA_TOKEN_NAME | ANJUTA_TOKEN_ADDED, filename);
	fprintf (stdout, "Dump config list:\n");
	anjuta_token_dump (list);
	anjuta_token_insert_word_before (list, sibling, token);
	fprintf (stdout, "Dump config list after insertion:\n");
	anjuta_token_dump (list);
	
	anjuta_token_style_format (project->space_list, list);
	
	anjuta_token_file_update (project->configure_file, list);
	
	return token;
}

AnjutaToken *
amp_project_write_subdirs_list (AmpGroup *project)
{
	AnjutaToken *pos;
	AnjutaToken *token;
	static gint eol_type[] = {ANJUTA_TOKEN_EOL, ANJUTA_TOKEN_SPACE, ANJUTA_TOKEN_COMMENT, 0};
	
	pos = anjuta_token_find_type (pos, ANJUTA_TOKEN_SEARCH_NOT, eol_type);

	token = anjuta_token_insert_token_before (pos,
	    		AC_TOKEN_AC_CONFIG_FILES, "AC_CONFIG_FILES(",
	    		ANJUTA_TOKEN_LIST, NULL,
	    		ANJUTA_TOKEN_LAST, NULL,
	    		RIGHT_PAREN, ")",
	    		NULL);
	
	return token;
}
