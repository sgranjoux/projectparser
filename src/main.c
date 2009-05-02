/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) Sébastien Granjoux 2009 <seb.sfo@free.fr>
 * 
 * main.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * main.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "project.h"

#include <gio/gio.h>

#include <stdio.h>

void list_target (GbfProject *project, const gchar *id, gint indent)
{
	GbfProjectTarget* target = gbf_project_get_target (project, id, NULL);
	GList *node;

	if (target == NULL) return;
	
	printf ("%*sT %s\n", indent + 4, "", target->name); 
	for (node = g_list_first (target->sources); node != NULL; node = g_list_next (node))
	{
		GbfProjectTargetSource* source = gbf_project_get_source (project, node->data, NULL);
		
		printf ("%*s%s\n", indent + 8, "", source->source_uri); 
	}
}

void list_group (GbfProject *project, const gchar *id, gint indent)
{
	GbfProjectGroup* group = gbf_project_get_group (project, id, NULL);
	GList *node;

	if (group == NULL) return;
	
	printf ("%*sG %s\n", indent + 4, "", group->name); 
	for (node = g_list_first (group->groups); node != NULL; node = g_list_next (node))
	{
		list_group (project, (const gchar *)node->data, indent + 4);
	}
	for (node = g_list_first (group->targets); node != NULL; node = g_list_next (node))
	{
		list_target (project, (const gchar *)node->data, indent + 4);
	}
}

int main()
{
	GbfProject *project;
	GList *modules;
	GList *node;
	GFile *project_root;
	gchar *uri;

	g_type_init ();

	project = amp_project_new  ();

	project_root = g_file_new_for_commandline_arg ("test/anjuta");
	uri = g_file_get_uri (project_root);
	gbf_project_load (project, uri, NULL);
	g_free (uri);
	g_object_unref (project_root);

	printf ("get all modules\n");
 	modules = gbf_project_get_config_modules (project, NULL);
	for (node = modules; node != NULL; node = g_list_next (node))
	{
		GList *packages;
		GList *pack;

		printf ("  module: %s\n", (const gchar *)node->data);
		
		packages = gbf_project_get_config_packages (project, (const gchar *)node->data, NULL);
		for (pack = packages; pack != NULL; pack = g_list_next (pack))
		{
			printf ("    %s\n", (const gchar *)pack->data);
		}

		//g_list_foreach (packages, (GFunc)g_free, NULL);
		g_list_free (packages);
	}
	//g_list_foreach (modules, (GFunc)g_free, NULL);
	g_list_free (modules);

	printf ("\nget all groups\n");
	list_group (project, "", 0);

	printf ("unref project\n");
	g_object_unref (project);

	return (0);
}
