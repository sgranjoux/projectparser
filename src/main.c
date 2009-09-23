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
#include "mk-project.h"
#include "libanjuta/anjuta-debug.h"
#include "libanjuta/anjuta-project.h"
#include "libanjuta/interfaces/ianjuta-project.h"

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

void list_target (IAnjutaProject *project, AnjutaProjectTarget *target, gint indent, const gchar *path)
{
	AnjutaProjectSource *source;
	guint count = 0;
	GFile *root;

	if (target == NULL) return;

	root = anjuta_project_group_get_directory (ianjuta_project_get_root (project, NULL));
	
	indent++;
	print ("%*sTARGET (%s): %s", indent * INDENT, "", path, anjuta_project_target_get_name (target)); 
	indent++;
	for (source = anjuta_project_node_first_child (target); source != NULL; source = anjuta_project_node_next_sibling (source))
	{
		gchar *child_path = g_strdup_printf ("%s:%d", path, count);
		GFile *child_file = anjuta_project_source_get_file (source);
		gchar *rel_path = g_file_get_relative_path (root, child_file);
		
		print ("%*sSOURCE (%s): %s", indent * INDENT, "", child_path, rel_path);
		g_free (rel_path);
		g_free (child_path);
		count++;
	}
}

void list_group (IAnjutaProject *project, AnjutaProjectGroup *group, gint indent, const gchar *path)
{
	AnjutaProjectNode *node;
	AnjutaProjectGroup *parent;
	guint count;
	gchar *rel_path;
	
	
	indent++;
	parent = anjuta_project_node_parent (group);
	if (parent == NULL)
	{
		GFile *root;
		root = g_file_get_parent (anjuta_project_group_get_directory (ianjuta_project_get_root (project, NULL)));
		rel_path = g_file_get_relative_path (root, anjuta_project_group_get_directory (group));
		g_object_unref (root);
	}
	else
	{
		GFile *root;
		root = anjuta_project_group_get_directory (parent);
		rel_path = g_file_get_relative_path (root, anjuta_project_group_get_directory (group));
	}
	print ("%*sGROUP (%s): %s", indent * INDENT, "", path, rel_path);
	g_free (rel_path);

	count = 0;
	for (node = anjuta_project_node_first_child (group); node != NULL; node = anjuta_project_node_next_sibling (node))
	{
		if (anjuta_project_node_get_type (node) == ANJUTA_PROJECT_GROUP)
		{
			gchar *child_path = g_strdup_printf ("%s:%d", path, count);
			list_group (project, node, indent, child_path);
			g_free (child_path);
		}
		count++;
	}
	
	count = 0;
	for (node = anjuta_project_node_first_child (group); node != NULL; node = anjuta_project_node_next_sibling (node))
	{
		if (anjuta_project_node_get_type (node) == ANJUTA_PROJECT_TARGET)
		{
			gchar *child_path = g_strdup_printf ("%s:%d", path, count);
			list_target (project, node, indent, child_path);
			g_free (child_path);
		}
		count++;
	}
}

void list_property (IAnjutaProject *project)
{
	gchar *value;

	value = amp_project_get_property (AMP_PROJECT (project), AMP_PROPERTY_NAME);
	if (value) print ("%*sNAME: %s", INDENT, "", value);
	g_free (value);

	value = amp_project_get_property (AMP_PROJECT (project), AMP_PROPERTY_VERSION);
	if (value) print ("%*sVERSION: %s", INDENT, "", value);
	g_free (value);

	value = amp_project_get_property (AMP_PROJECT (project), AMP_PROPERTY_BUG_REPORT);
	if (value) print ("%*sBUG_REPORT: %s", INDENT, "", value);
	g_free (value);

	value = amp_project_get_property (AMP_PROJECT (project), AMP_PROPERTY_TARNAME);
	if (value) print ("%*sTARNAME: %s", INDENT, "", value);
	g_free (value);
	
	value = amp_project_get_property (AMP_PROJECT (project), AMP_PROPERTY_URL);
	if (value) print ("%*sURL: %s", INDENT, "", value);
	g_free (value);
}

void list_package (IAnjutaProject *project)
{
	GList *packages;
	GList *node;
	
	packages = ianjuta_project_get_packages (project, NULL);
	for (node = packages; node != NULL; node = g_list_next (node))
	{
		print ("%*sPACKAGE: %s", INDENT, "", (const gchar *)node->data);
	}
	g_list_free (packages);
}

static AnjutaProjectNode *
get_node (IAnjutaProject *project, const char *path)
{
	AnjutaProjectNode *node = NULL;

	if (path != NULL)
	{
		for (; *path != '\0';)
		{
			gchar *end;
			guint child = g_ascii_strtoull (path, &end, 10);

			if (end == path)
			{
				/* error */
				return NULL;
			}

			if (node == NULL)
			{
				if (child == 0) node = ianjuta_project_get_root (project, NULL);
			}
			else
			{
				node = anjuta_project_node_nth_child (node, child);
			}
			if (node == NULL)
			{
				/* no node */
				return NULL;
			}

			if (*end == '\0') break;
			path = end + 1;
		}
	}

	return node;
}

static GFile *
get_file (AnjutaProjectTarget *target, const char *id)
{
	AnjutaProjectGroup *group = (AnjutaProjectGroup *)anjuta_project_node_parent (target);
	
	return g_file_resolve_relative_path (anjuta_project_group_get_directory (group), id);
}

static AnjutaProjectTargetType
get_type (IAnjutaProject *project, const char *id)
{
	AnjutaProjectTargetType type;
	GList *list;
	GList *item;
	guint num = atoi (id);
	
	list = ianjuta_project_get_target_types (project, NULL);
	type = (AnjutaProjectTargetType)list->data;
	for (item = list; item != NULL; item = g_list_next (item))
	{
		if (num == 0)
		{
			type = (AnjutaProjectTargetType)item->data;
			break;
		}
		num--;
	}
	g_list_free (list);

	return type;
}

/* Automake parsing function
 *---------------------------------------------------------------------------*/

int
main(int argc, char *argv[])
{
	IAnjutaProject *project;
	AnjutaProjectNode *node;
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
	project = IANJUTA_PROJECT (g_object_new (AMP_TYPE_PROJECT, NULL));

	/* Execute commands */
	for (command = &argv[1]; *command != NULL; command++)
	{
		if (g_ascii_strcasecmp (*command, "load") == 0)
		{
			GFile *file = g_file_new_for_commandline_arg (*(++command));
			
			ianjuta_project_load (project, file, &error);
			g_object_unref (file);
		}
		else if (g_ascii_strcasecmp (*command, "list") == 0)
		{
			list_property (project);
			
			list_package (project);

			list_group (project, ianjuta_project_get_root (project, NULL), 0, "0");
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
			node = get_node (project, *(++command));
			ianjuta_project_remove_node (project, node, NULL);
		}
		else if (g_ascii_strcasecmp (command[0], "add") == 0)
		{
			node = get_node (project, command[2]);
			if (g_ascii_strcasecmp (command[1], "group") == 0)
			{
				ianjuta_project_add_group (project, node, command[3], NULL);
			}
			else if (g_ascii_strcasecmp (command[1], "target") == 0)
			{
				
				ianjuta_project_add_target (project, node, command[3], get_type (project, command[4]), NULL);
				command++;
			}
			else if (g_ascii_strcasecmp (command[1], "source") == 0)
			{
				GFile *file = get_file (node, command[3]);
				
				ianjuta_project_add_source (project, node, file, NULL);
				g_object_unref (file);
			}
			else
			{
				fprintf (stderr, "Error: unknown command %s\n", *command);

				break;
			}
			command += 3;
		}
		else if (g_ascii_strcasecmp (command[0], "set") == 0)
		{
			amp_project_set_property (AMP_PROJECT (project), atoi(command[1]), command[2]);
			command += 2;
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
