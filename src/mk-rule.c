/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4; coding: utf-8 -*- */
/* mk-rule.c
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

#include "mk-rule.h"

#include "mk-project-private.h"
#include "mk-scanner.h"

struct _MkpRule {
	gchar *name;
	gboolean phony;
	gboolean pattern;
	GList *prerequisite;
	AnjutaToken *rule;
};

/* Rule object
 *---------------------------------------------------------------------------*/

static MkpRule*
mkp_rule_new (gchar *name, AnjutaToken *token)
{
    MkpRule *rule = NULL;

	g_return_val_if_fail (name != NULL, NULL);
	
	rule = g_slice_new0(MkpRule); 
	rule->name = g_strdup (name);
	rule->rule = token;

	return rule;
}

static void
mkp_rule_free (MkpRule *rule)
{
	g_free (rule->name);
	g_list_foreach (rule->prerequisite, (GFunc)g_free, NULL);
	g_list_free (rule->prerequisite);
	
    g_slice_free (MkpRule, rule);
}

/* Private functions
 *---------------------------------------------------------------------------*/

/* Public functions
 *---------------------------------------------------------------------------*/

void
mkp_project_add_rule (MkpProject *project, AnjutaToken *rule)
{
	AnjutaToken *targ;
	AnjutaToken *dep;
	AnjutaToken *arg;
	gboolean double_colon = FALSE;

	targ = anjuta_token_list_first (rule);
	arg = anjuta_token_list_next (targ);
	if (anjuta_token_get_type (arg) == MK_TOKEN_DOUBLE_COLON) double_colon = TRUE;
	dep = anjuta_token_list_next (arg);
	for (arg = anjuta_token_list_first (targ); arg != NULL; arg = anjuta_token_list_next (arg))
	{
		AnjutaToken *src;
		gchar *target;
		gboolean order = FALSE;
		gboolean no_token = TRUE;
		MkpRule *rule;

		switch (anjuta_token_get_type (arg))
		{
		case MK_TOKEN__PHONY:
			for (src = anjuta_token_list_first (dep); src != NULL; src = anjuta_token_list_next (src))
			{
				if (anjuta_token_get_type (src) != MK_TOKEN_ORDER)
				{
					target = mkp_project_token_evaluate (project, src);
					
					rule = g_hash_table_lookup (project->rules, target);
					if (rule == NULL)
					{
						rule = mkp_rule_new (target, NULL);
						g_hash_table_insert (project->rules, rule->name, rule);
					}
					rule->phony = TRUE;
					
					g_message ("    with target %s", target);
					if (target != NULL) g_free (target);
				}
			}
			break;
		case MK_TOKEN__SUFFIXES:
			for (src = anjuta_token_list_first (dep); src != NULL; src = anjuta_token_list_next (src))
			{
				if (anjuta_token_get_type (src) != MK_TOKEN_ORDER)
				{
					gchar *suffix;

					suffix = mkp_project_token_evaluate (project, src);
					/* The pointer value must only be not NULL, it does not matter if it is
	 				 * invalid */
					g_hash_table_replace (project->suffix, suffix, suffix);
					g_message ("    with suffix %s", suffix);
					no_token = FALSE;
				}
			}
			if (no_token == TRUE)
			{
				/* Clear all suffix */
				g_hash_table_remove_all (project->suffix);
			}
			break;
		case MK_TOKEN__DEFAULT:
		case MK_TOKEN__PRECIOUS:
		case MK_TOKEN__INTERMEDIATE:
		case MK_TOKEN__SECONDARY:
		case MK_TOKEN__SECONDEXPANSION:
		case MK_TOKEN__DELETE_ON_ERROR:
		case MK_TOKEN__IGNORE:
		case MK_TOKEN__LOW_RESOLUTION_TIME:
		case MK_TOKEN__SILENT:
		case MK_TOKEN__EXPORT_ALL_VARIABLES:
		case MK_TOKEN__NOTPARALLEL:
			/* Do nothing with these targets, just ignore them */
			break;
		default:
			target = mkp_project_token_evaluate (project, arg);
			g_message ("add rule %s", target);

			rule = g_hash_table_lookup (project->rules, target);
			if (rule == NULL)
			{
				rule = mkp_rule_new (target, arg);
				g_hash_table_insert (project->rules, rule->name, rule);
			}
			else
			{
				rule->rule = arg;
			}
				
			for (src = anjuta_token_list_first (dep); src != NULL; src = anjuta_token_list_next (src))
			{
				gchar *src_name = mkp_project_token_evaluate (project, src);
				
				g_message ("    with source %s", src_name);
				if (anjuta_token_get_type (src) == MK_TOKEN_ORDER)
				{
					order = TRUE;
				}
				rule->prerequisite = g_list_prepend (rule->prerequisite, src_name);
			}

			if (target != NULL) g_free (target);
		}
	}
}

void 
mkp_project_init_rules (MkpProject *project)
{
	project->rules = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify)mkp_rule_free);
	project->suffix = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

void 
mkp_project_free_rules (MkpProject *project)
{
	if (project->rules) g_hash_table_destroy (project->rules);
	project->rules = NULL;
	if (project->suffix) g_hash_table_destroy (project->suffix);
	project->suffix = NULL;
}

