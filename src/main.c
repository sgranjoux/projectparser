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

#include "config.h"

#include "am-project.h"
#include "libanjuta/anjuta-debug.h"

#include <gio/gio.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static gchar* output_file = NULL;
static FILE* output_stream = NULL;

static GOptionEntry entries[] =
{
  { "output", 'o', 0, G_OPTION_ARG_FILENAME, &output_file, "Output file (default stdout)", "output_file" },
  { NULL }
};

#define INDENT 4

void open_output (void)
{
	output_stream = output_file == NULL ? stdout : fopen (output_file, "wt");
}

void close_output (void)
{
	if (output_stream != NULL) fclose (output_stream);
	output_stream = NULL;
}

void print (const gchar *message, ...)
{
	va_list args;

	if (output_stream == NULL) open_output();
	
	va_start (args, message);
	vfprintf (output_stream, message, args);
	va_end (args);
	fputc('\n', output_stream);
}

void list_target (GbfProject *project, const gchar *id, gint indent, const gchar *path)
{
	GbfProjectTarget* target = gbf_project_get_target (project, id, NULL);
	GList *node;
	guint count = 0;
	GFile *root;

	if (target == NULL) return;

	root = g_file_new_for_uri (amp_project_get_uri (project));
	
	indent++;
	print ("%*sTARGET (%s): %s", indent * INDENT, "", path, target->name); 
	indent++;
	for (node = g_list_first (target->sources); node != NULL; node = g_list_next (node))
	{
		GbfProjectTargetSource* source = gbf_project_get_source (project, node->data, NULL);
		gchar *child_path = g_strdup_printf ("%s:%d", path, count);
		GFile *child_file = g_file_new_for_uri (source->source_uri);
		gchar *rel_path = g_file_get_relative_path (root, child_file);
		
		print ("%*sSOURCE (%s): %s", indent * INDENT, "", child_path, rel_path);
		g_free (rel_path);
		g_object_unref (child_file);
		g_free (child_path);
		count++;
	}

	g_object_unref (root);
}

void list_group (GbfProject *project, const gchar *id, gint indent, const gchar *path)
{
	GbfProjectGroup* group = gbf_project_get_group (project, id, NULL);
	GList *node;
	guint count = 0;
	
	if (group == NULL) return;

	indent++;
	print ("%*sGROUP (%s): %s", indent * INDENT, "", path, group->name); 
	for (node = g_list_first (group->groups); node != NULL; node = g_list_next (node))
	{
		gchar *child_path = g_strdup_printf ("%s:%d", path, count);
		list_group (project, (const gchar *)node->data, indent, child_path);
		g_free (child_path);
		count++;
	}
	for (node = g_list_first (group->targets); node != NULL; node = g_list_next (node))
	{
		gchar *child_path = g_strdup_printf ("%s:%d", path, count);
		list_target (project, (const gchar *)node->data, indent, child_path);
		g_free (child_path);
		count++;
	}
}


void list_module (GbfProject *project)
{
	GList *modules;
	GList *node;
	
	modules = gbf_project_get_config_modules (project, NULL);
	for (node = modules; node != NULL; node = g_list_next (node))
	{
		GList *packages;
		GList *pack;

		print ("%*sMODULE: %s", INDENT, "", (const gchar *)node->data);
		
		packages = gbf_project_get_config_packages (project, (const gchar *)node->data, NULL);
		for (pack = packages; pack != NULL; pack = g_list_next (pack))
		{
			print ("%*sPACKAGE: %s", 2 * INDENT, "", (const gchar *)pack->data);
		}
		g_list_free (packages);
	}
	g_list_free (modules);
}

/* Automake parsing function
 *---------------------------------------------------------------------------*/

int
main(int argc, char *argv[])
{
	GbfProject *project;
	char **command;
	GOptionContext *context;
	GError *error = NULL;

	/* Initialize program */
	g_type_init ();
	
	anjuta_debug_init (FALSE);

	/* Parse options */
 	context = g_option_context_new ("list [args]");
  	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_context_set_summary (context, "test new autotools project manger");
	if (!g_option_context_parse (context, &argc, &argv, &error))
    {
		exit (1);
    }
	if (argc < 2)
	{
		printf ("PROJECT: %s", g_option_context_get_help (context, TRUE, NULL));
		exit (1);
	}

	/* Create project */
	project = amp_project_new ();

	/* Execute commands */
	for (command = &argv[1]; *command != NULL; command++)
	{
		if (g_ascii_strcasecmp (*command, "load") == 0)
		{
			GFile *file = g_file_new_for_commandline_arg (*(++command));
			gchar *path;

			path = g_file_get_path (file);
			gbf_project_load (project, path, &error);
			g_free (path);
			g_object_unref (file);
		}
		else if (g_ascii_strcasecmp (*command, "list") == 0)
		{
			list_module (project);

			list_group (project, "", 0, "0");
		}
		else if (g_ascii_strcasecmp (*command, "move") == 0)
		{
			amp_project_move (AMP_PROJECT (project), *(++command));
		}
		else if (g_ascii_strcasecmp (*command, "save") == 0)
		{
			amp_project_save (AMP_PROJECT (project), NULL);
		}
		else if (g_ascii_strcasecmp (*command, "remove") == 0)
		{
			gchar *id = amp_project_get_node_id (AMP_PROJECT (project), *(++command));
			gbf_project_remove_source (project, id, NULL);
			g_free (id);
		}
		else if (g_ascii_strcasecmp (command[0], "add") == 0)
		{
			gchar *parent;
			
			parent = amp_project_get_node_id (AMP_PROJECT (project), command[2]);
			if (g_ascii_strcasecmp (command[1], "group") == 0)
			{
				gbf_project_add_group (project, parent, command[3], NULL);
			}
			else if (g_ascii_strcasecmp (command[1], "target") == 0)
			{
				gbf_project_add_target (project, parent, command[3], command[4], NULL);
				command++;
			}
			else if (g_ascii_strcasecmp (command[1], "source") == 0)
			{
				gbf_project_add_source (project, parent, command[3], NULL);
			}
			else
			{
				fprintf (stderr, "Error: unknown command %s\n", *command);

				break;
			}
			g_free (parent);
				
			command += 3;
		}
		else
		{
			fprintf (stderr, "Error: unknown command %s\n", *command);

			break;
		}
		if (error != NULL)
		{
			fprintf (stderr, "Error: %s\n", error->message == NULL ? "unknown error" : error->message);

			g_error_free (error);
			break;
		}
	}

	/* Free objects */
	g_object_unref (project);
	close_output ();
	
	return (0);
}
