/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4; coding: utf-8 -*- */
/* mk-project.c
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

#include "mk-project.h"

#include "mk-project-private.h"

#include <libanjuta/anjuta-debug.h>

#include <string.h>
#include <memory.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <signal.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <glib.h>
#include "mk-scanner.h"


#define UNIMPLEMENTED  G_STMT_START { g_warning (G_STRLOC": unimplemented"); } G_STMT_END

/* Constant strings for parsing perl script error output */
#define ERROR_PREFIX      "ERROR("
#define WARNING_PREFIX    "WARNING("
#define MESSAGE_DELIMITER ": "

static const gchar *valid_makefiles[] = {"GNUmakefile", "makefile", "Makefile", NULL};

/* convenient shortcut macro the get the MkpNode from a GNode */
#define MKP_NODE_DATA(node)  ((node) != NULL ? (MkpNodeData *)((node)->data) : NULL)
#define MKP_GROUP_DATA(node)  ((node) != NULL ? (MkpGroupData *)((node)->data) : NULL)
#define MKP_TARGET_DATA(node)  ((node) != NULL ? (MkpTargetData *)((node)->data) : NULL)
#define MKP_SOURCE_DATA(node)  ((node) != NULL ? (MkpSourceData *)((node)->data) : NULL)

typedef struct _MkpPackage MkpPackage;

struct _MkpPackage {
    gchar *name;
    gchar *version;
};

typedef struct _MkpModule MkpModule;
	
struct _MkpModule {
    GList *packages;
    AnjutaToken *module;
};

typedef struct _MkpNodeData MkpNodeData;

struct _MkpNodeData {
	MkpNodeType	type;			/* Node type */
	gchar				*id;				/* Node string id, FIXME: to be removed */
};

typedef enum {
	AM_GROUP_TOKEN_CONFIGURE,
	AM_GROUP_TOKEN_SUBDIRS,
	AM_GROUP_TOKEN_DIST_SUBDIRS,
	AM_GROUP_TARGET,
	AM_GROUP_TOKEN_LAST
} MkpGroupTokenCategory;

typedef struct _MkpGroupData MkpGroupData;

struct _MkpGroupData {
	MkpNodeData node;			/* Common node data */
	gboolean dist_only;			/* TRUE if the group is distributed but not built */
	GFile *directory;				/* GFile corresponding to group directory */
	GFile *makefile;				/* GFile corresponding to group makefile */
	AnjutaTokenFile *tfile;		/* Corresponding Makefile */
	GList *tokens[AM_GROUP_TOKEN_LAST];					/* List of token used by this group */
};

typedef enum _MkpTargetFlag
{
	AM_TARGET_CHECK = 1 << 0,
	AM_TARGET_NOINST = 1 << 1,
	AM_TARGET_DIST = 1 << 2,
	AM_TARGET_NODIST = 1 << 3,
	AM_TARGET_NOBASE = 1 << 4,
	AM_TARGET_NOTRANS = 1 << 5,
	AM_TARGET_MAN = 1 << 6,
	AM_TARGET_MAN_SECTION = 31 << 7
} MkpTargetFlag;

typedef struct _MkpTargetData MkpTargetData;

struct _MkpTargetData {
	MkpNodeData node;
	gchar *name;
	gchar *type;
	gchar *install;
	gint flags;
	GList* tokens;
};

typedef struct _MkpSourceData MkpSourceData;

struct _MkpSourceData {
	MkpNodeData node;
	GFile *file;
	AnjutaToken* token;
};

/* ----- Standard GObject types and variables ----- */

enum {
	PROP_0,
	PROP_PROJECT_DIR
};

static GObject *parent_class;

/* Helper functions
 *---------------------------------------------------------------------------*/

static void
error_set (GError **error, gint code, const gchar *message)
{
        if (error != NULL) {
                if (*error != NULL) {
                        gchar *tmp;

                        /* error already created, just change the code
                         * and prepend the string */
                        (*error)->code = code;
                        tmp = (*error)->message;
                        (*error)->message = g_strconcat (message, "\n\n", tmp, NULL);
                        g_free (tmp);

                } else {
                        *error = g_error_new_literal (GBF_PROJECT_ERROR,
                                                      code,
                                                      message);
                }
        }
}

/* Work even if file is not a descendant of parent */
static gchar*
get_relative_path (GFile *parent, GFile *file)
{
	gchar *relative;

	relative = g_file_get_relative_path (parent, file);
	if (relative == NULL)
	{
		if (g_file_equal (parent, file))
		{
			relative = g_strdup ("");
		}
		else
		{
			GFile *grand_parent = g_file_get_parent (parent);
			gint level;
			gchar *grand_relative;
			gchar *ptr;
			gsize len;
			
			
			for (level = 1;  !g_file_has_prefix (file, grand_parent); level++)
			{
				GFile *next = g_file_get_parent (grand_parent);
				
				g_object_unref (grand_parent);
				grand_parent = next;
			}

			grand_relative = g_file_get_relative_path (grand_parent, file);
			g_object_unref (grand_parent);

			len = strlen (grand_relative);
			relative = g_new (gchar, len + level * 3 + 1);
			ptr = relative;
			for (; level; level--)
			{
				memcpy(ptr, ".." G_DIR_SEPARATOR_S, 3);
				ptr += 3;
			}
			memcpy (ptr, grand_relative, len + 1);
			g_free (grand_relative);
		}
	}

	return relative;
}

static GFileType
file_type (GFile *file, const gchar *filename)
{
	GFile *child;
	GFileInfo *info;
	GFileType type;

	child = filename != NULL ? g_file_get_child (file, filename) : g_object_ref (file);

	//g_message ("check file %s", g_file_get_path (file));
	
	info = g_file_query_info (child,
	                          G_FILE_ATTRIBUTE_STANDARD_TYPE, 
	                          G_FILE_QUERY_INFO_NONE,
	                          NULL,
	                          NULL);
	if (info != NULL)
	{
		type = g_file_info_get_file_type (info);
		g_object_unref (info);
	}
	else
	{
		type = G_FILE_TYPE_UNKNOWN;
	}
	
	g_object_unref (child);
	
	return type;
}

/* Group objects
 *---------------------------------------------------------------------------*/

static void
mkp_group_add_token (MkpGroup *node, AnjutaToken *token, MkpGroupTokenCategory category)
{
    MkpGroupData *group;
	
	g_return_if_fail ((node != NULL) && (node->data != NULL)); 

 	group = (MkpGroupData *)node->data;
	group->tokens[category] = g_list_prepend (group->tokens[category], token);
}

static GList *
mkp_group_get_token (MkpGroup *node, MkpGroupTokenCategory category)
{
    MkpGroupData *group;
	
	g_return_val_if_fail ((node != NULL) && (node->data != NULL), NULL); 

 	group = (MkpGroupData *)node->data;
	return group->tokens[category];
}

static AnjutaTokenFile*
mkp_group_set_makefile (MkpGroup *node, GFile *makefile)
{
    MkpGroupData *group;
	
	g_return_val_if_fail ((node != NULL) && (node->data != NULL), NULL); 

 	group = (MkpGroupData *)node->data;
	if (group->makefile != NULL) g_object_unref (group->makefile);
	if (group->tfile != NULL) anjuta_token_file_free (group->tfile);
	if (makefile != NULL)
	{
		group->makefile = g_object_ref (makefile);
		group->tfile = anjuta_token_file_new (makefile);
	}
	else
	{
		group->makefile = NULL;
		group->tfile = NULL;
	}

	return group->tfile;
}

static MkpGroup*
mkp_group_new (GFile *file)
{
    MkpGroupData *group = NULL;

	g_return_val_if_fail (file != NULL, NULL);
	
	group = g_slice_new0(MkpGroupData); 
	group->node.type = MKP_NODE_GROUP;
	group->directory = g_object_ref (file);

    return g_node_new (group);
}

static void
mkp_group_free (MkpGroup *node)
{
    MkpGroupData *group = (MkpGroupData *)node->data;
	gint i;
	
	if (group->tfile) anjuta_token_file_free (group->tfile);
	if (group->directory) g_object_unref (group->directory);
	if (group->makefile) g_object_unref (group->makefile);
	for (i = 0; i < AM_GROUP_TOKEN_LAST; i++)
	{
		if (group->tokens[i] != NULL) g_list_free (group->tokens[i]);
	}
    g_slice_free (MkpGroupData, group);

	g_node_destroy (node);
}

/* Target objects
 *---------------------------------------------------------------------------*/

static void
mkp_target_add_token (MkpGroup *node, AnjutaToken *token)
{
    MkpTargetData *target;
	
	g_return_if_fail ((node != NULL) && (node->data != NULL)); 

 	target = (MkpTargetData *)node->data;
	target->tokens = g_list_prepend (target->tokens, token);
}

static GList *
mkp_target_get_token (MkpGroup *node)
{
    MkpTargetData *target;
	
	g_return_val_if_fail ((node != NULL) && (node->data != NULL), NULL); 

 	target = (MkpTargetData *)node->data;
	return target->tokens;
}


static MkpTarget*
mkp_target_new (const gchar *name, const gchar *type)
{
    MkpTargetData *target = NULL;

	target = g_slice_new0(MkpTargetData); 
	target->node.type = MKP_NODE_TARGET;
	target->name = g_strdup (name);
	target->type = g_strdup (type);

    return g_node_new (target);
}

static void
mkp_target_free (MkpTarget *node)
{
    MkpTargetData *target = (MkpTargetData *)node->data;
	
    g_free (target->name);
    g_free (target->type);
    g_slice_free (MkpTargetData, target);

	g_node_destroy (node);
}

/* Source objects
 *---------------------------------------------------------------------------*/

static MkpSource*
mkp_source_new (GFile *parent, const gchar *name)
{
    MkpSourceData *source = NULL;

	source = g_slice_new0(MkpSourceData); 
	source->node.type = MKP_NODE_SOURCE;
	source->file = g_file_get_child (parent, name);

    return g_node_new (source);
}

static void
mkp_source_free (MkpSource *node)
{
    MkpSourceData *source = (MkpSourceData *)node->data;
	
    g_object_unref (source->file);
    g_slice_free (MkpSourceData, source);

	g_node_destroy (node);
}

/*
 * File monitoring support --------------------------------
 * FIXME: review these
 */
static void
monitor_cb (GFileMonitor *monitor,
			GFile *file,
			GFile *other_file,
			GFileMonitorEvent event_type,
			gpointer data)
{
	MkpProject *project = data;

	g_return_if_fail (project != NULL && MKP_IS_PROJECT (project));

	switch (event_type) {
		case G_FILE_MONITOR_EVENT_CHANGED:
		case G_FILE_MONITOR_EVENT_DELETED:
			/* monitor will be removed here... is this safe? */
			mkp_project_reload (project, NULL);
			g_signal_emit_by_name (G_OBJECT (project), "project-updated");
			break;
		default:
			break;
	}
}


static void
monitor_add (MkpProject *project, GFile *file)
{
	GFileMonitor *monitor = NULL;
	
	g_return_if_fail (project != NULL);
	g_return_if_fail (project->monitors != NULL);
	
	if (file == NULL)
		return;
	
	monitor = g_hash_table_lookup (project->monitors, file);
	if (!monitor) {
		gboolean exists;
		
		/* FIXME clarify if uri is uri, path or both */
		exists = g_file_query_exists (file, NULL);
		
		if (exists) {
			monitor = g_file_monitor_file (file, 
						       G_FILE_MONITOR_NONE,
						       NULL,
						       NULL);
			if (monitor != NULL)
			{
				g_signal_connect (G_OBJECT (monitor),
						  "changed",
						  G_CALLBACK (monitor_cb),
						  project);
				g_hash_table_insert (project->monitors,
						     g_object_ref (file),
						     monitor);
			}
		}
	}
}

static void
monitors_remove (MkpProject *project)
{
	g_return_if_fail (project != NULL);

	if (project->monitors)
		g_hash_table_destroy (project->monitors);
	project->monitors = NULL;
}

static void
group_hash_foreach_monitor (gpointer key,
			    gpointer value,
			    gpointer user_data)
{
	MkpGroup *group_node = value;
	MkpProject *project = user_data;

	monitor_add (project, MKP_GROUP_DATA(group_node)->directory);
}

static void
monitors_setup (MkpProject *project)
{
	g_return_if_fail (project != NULL);

	monitors_remove (project);
	
	/* setup monitors hash */
	project->monitors = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
						   (GDestroyNotify) g_file_monitor_cancel);

	monitor_add (project, anjuta_token_file_get_file (project->make_file));
	if (project->groups)
		g_hash_table_foreach (project->groups, group_hash_foreach_monitor, project);
}


/*
 * ---------------- Data structures managment
 */

static void
mkp_dump_node (GNode *g_node)
{
	gchar *name = NULL;
	
	switch (MKP_NODE_DATA (g_node)->type) {
		case MKP_NODE_GROUP:
			name = g_file_get_uri (MKP_GROUP_DATA (g_node)->directory);
			DEBUG_PRINT ("GROUP: %s", name);
			break;
		case MKP_NODE_TARGET:
			name = g_strdup (MKP_TARGET_DATA (g_node)->name);
			DEBUG_PRINT ("TARGET: %s", name);
			break;
		case MKP_NODE_SOURCE:
			name = g_file_get_uri (MKP_SOURCE_DATA (g_node)->file);
			DEBUG_PRINT ("SOURCE: %s", name);
			break;
		default:
			g_assert_not_reached ();
			break;
	}
	g_free (name);
}

static gboolean 
foreach_node_destroy (GNode    *g_node,
		      gpointer  data)
{
	switch (MKP_NODE_DATA (g_node)->type) {
		case MKP_NODE_GROUP:
			//g_hash_table_remove (project->groups, g_file_get_uri (MKP_GROUP_NODE (g_node)->file));
			mkp_group_free (g_node);
			break;
		case MKP_NODE_TARGET:
			mkp_target_free (g_node);
			break;
		case MKP_NODE_SOURCE:
			//g_hash_table_remove (project->sources, MKP_NODE (g_node)->id);
			mkp_source_free (g_node);
			break;
		default:
			g_assert_not_reached ();
			break;
	}
	

	return FALSE;
}

static void
project_node_destroy (MkpProject *project, GNode *g_node)
{
	g_return_if_fail (project != NULL);
	g_return_if_fail (MKP_IS_PROJECT (project));
	
	if (g_node) {
		/* free each node's data first */
		g_node_traverse (g_node,
				 G_POST_ORDER, G_TRAVERSE_ALL, -1,
				 foreach_node_destroy, project);

		/* now destroy the tree itself */
		//g_node_destroy (g_node);
	}
}

static void
find_target (GNode *node, gpointer data)
{
	if (MKP_NODE_DATA (node)->type == MKP_NODE_TARGET)
	{
		if (strcmp (MKP_TARGET_DATA (node)->name, *(gchar **)data) == 0)
		{
			/* Find target, return node value in pointer */
			*(GNode **)data = node;

			return;
		}
	}
}

static AnjutaToken*
project_load_rule (MkpProject *project, AnjutaToken *rule, GNode *parent)
{
	AnjutaToken *arg;
	AnjutaToken *prerequisite;
	gchar *name;

	/* Search for prerequisite */
	prerequisite = NULL;
	for (arg = anjuta_token_list_first (rule); arg != NULL; arg = anjuta_token_list_next (arg))
	{
		if (anjuta_token_get_type (arg) == MK_TOKEN_PREREQUISITE)
		{
			prerequisite = anjuta_token_list_next (arg);
		}
	}
	
	for (arg = anjuta_token_list_first (rule); arg != NULL; arg = anjuta_token_list_next (arg))
	{
		gchar *value;
		gpointer find;
		MkpTarget *target;

		value = anjuta_token_evaluate (arg);

		/* Check if target already exists */
		find = value;
		g_node_children_foreach (parent, G_TRAVERSE_ALL, find_target, &find);
		if ((gchar *)find != value)
		{
			/* Find target */
			target = (MkpTarget *)find;
		}
		else
		{
			/* Create target */
			target = mkp_target_new (value, "unknown");
			mkp_target_add_token (target, arg);
			g_node_append (parent, target);
		}

		g_free (value);

		/* Add prerequisite */
		for (arg = prerequisite; arg != NULL; arg = anjuta_token_list_next (arg))
		{
			MkpSource *source;

			value = anjuta_token_evaluate (arg);

			source = mkp_source_new (project->root_file, name);
			g_node_append (target, source);
		}
	}

	return NULL;
}

static void
remove_make_file (gpointer data, GObject *object, gboolean is_last_ref)
{
	if (is_last_ref)
	{
		MkpProject *project = (MkpProject *)data;
		g_hash_table_remove (project->files, anjuta_token_file_get_file (ANJUTA_TOKEN_FILE (object)));
	}
}

static MkpGroup*
project_load_makefile (MkpProject *project, GFile *file, GNode *parent, GError **error)
{
	MkpScanner *scanner;
	MkpGroup *group;
	AnjutaToken *rule_tok;
	AnjutaToken *arg;
	AnjutaTokenFile *tfile;
	GFile *makefile = NULL;
	gboolean found;
	gboolean ok;
	GError *err = NULL;

	/* Create group */
	group = mkp_group_new (file);
	g_hash_table_insert (project->groups, g_file_get_uri (file), group);
	if (parent == NULL)
	{
		project->root_node = group;
	}
	else
	{
		g_node_append (parent, group);
	}
		
	/* Parse makefile */	
	DEBUG_PRINT ("Parse: %s", g_file_get_uri (makefile));
	tfile = mkp_group_set_makefile (group, makefile);
	g_hash_table_insert (project->files, file, tfile);
	g_object_add_toggle_ref (G_OBJECT (project->make_file), remove_make_file, project);
	scanner = mkp_scanner_new ();
	ok = mkp_scanner_parse (scanner, tfile, &err);
	mkp_scanner_free (scanner);
	if (!ok)
	{
		g_set_error (error, GBF_PROJECT_ERROR, 
		             	GBF_PROJECT_ERROR_PROJECT_MALFORMED,
		    			err == NULL ? _("Unable to parse make file") : err->message);
		if (err != NULL) g_error_free (err);

		return NULL;
	}

	/* Load target */
	rule_tok = anjuta_token_new_static (MK_TOKEN_RULE, NULL);
	
	arg = anjuta_token_file_first (MKP_GROUP_DATA (group)->tfile);
	for (found = anjuta_token_match (rule_tok, ANJUTA_SEARCH_INTO, arg, &arg); found; found = anjuta_token_match (rule_tok, ANJUTA_SEARCH_INTO, anjuta_token_next_sibling (arg), &arg))
	{
		project_load_rule (project, arg, group);
	}

	return group;
}

/* Public functions
 *---------------------------------------------------------------------------*/

gboolean
mkp_project_reload (MkpProject *project, GError **error) 
{
	GFile *root_file;
	GFile *make_file;
	const gchar **makefile;
	gboolean ok;

	/* Unload current project */
	root_file = g_object_ref (project->root_file);
	mkp_project_unload (project);
	project->root_file = root_file;
	DEBUG_PRINT ("reload project %p root file %p", project, project->root_file);

	/* shortcut hash tables */
	project->groups = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	project->files = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, g_object_unref);

	/* Initialize list styles */
	project->space_list = anjuta_token_style_new (NULL, " ", "\\n", NULL, 0);
	project->arg_list = anjuta_token_style_new (NULL, ", ", ",\\n ", ")", 0);

	/* Find make file */
	for (makefile = valid_makefiles; *makefile != NULL; makefile++)
	{
		if (file_type (root_file, *makefile) == G_FILE_TYPE_REGULAR)
		{
			make_file = g_file_get_child (root_file, *makefile);
			break;
		}
	}
	if (make_file == NULL)
	{
		g_set_error (error, GBF_PROJECT_ERROR, 
		             GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Project doesn't exist or invalid path"));

		return FALSE;
	}
	
	/* Parse make file */	
	project->make_file = anjuta_token_file_new (make_file);
		     

	project_load_makefile (project, make_file, NULL, error);

	monitors_setup (project);
	
	return ok;
}

gboolean
mkp_project_load (MkpProject  *project,
    const gchar *uri,
	GError     **error)
{
	g_return_val_if_fail (uri != NULL, FALSE);

	project->root_file = g_file_new_for_path (uri);
	if (!mkp_project_reload (project, error))
	{
		g_object_unref (project->root_file);
		project->root_file = NULL;
	}

	return project->root_file != NULL;
}

void
mkp_project_unload (MkpProject *project)
{
	monitors_remove (project);
	
	/* project data */
	project_node_destroy (project, project->root_node);
	project->root_node = NULL;

	if (project->root_file) g_object_unref (project->root_file);
	project->root_file = NULL;

	/* shortcut hash tables */
	if (project->groups) g_hash_table_destroy (project->groups);
	if (project->files) g_hash_table_destroy (project->files);
	project->groups = NULL;
	project->files = NULL;

	/* List styles */
	if (project->space_list) anjuta_token_style_free (project->space_list);
	if (project->arg_list) anjuta_token_style_free (project->arg_list);
}

gboolean 
mkp_project_probe (MkpProject  *project,
	    const gchar *uri,
	    GError     **error)
{
	GFile *file;
	gboolean probe;
	gboolean dir;
	
	file = g_file_new_for_path (uri);

	dir = (file_type (file, NULL) == G_FILE_TYPE_DIRECTORY);
	if (!dir)
	{
		g_set_error (error, GBF_PROJECT_ERROR, 
		             GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Project doesn't exist or invalid path"));
	}
	
	probe =  dir;
	if (probe)
	{
		const gchar **makefile;

		/* Look for makefiles */
		probe = FALSE;
		for (makefile = valid_makefiles; *makefile != NULL; makefile++)
		{
			if (file_type (file, *makefile) == G_FILE_TYPE_REGULAR)
			{
				probe = TRUE;
				break;
			}
		}
	}
	
	g_object_unref (file);
	
	return probe;
}

static AnjutaTokenType
anjuta_token_for_target_type (const gchar *type)
{
	return ANJUTA_TOKEN_NAME;
}

static const gchar*
autotool_prefix_for_target_type (const gchar *type)
{
	if (!strcmp (type, "static_lib")) {
		return "_LIBRARIES";
	} else if (!strcmp (type, "shared_lib")) {
		return "_LTLIBRARIES";
	} else if (!strcmp (type, "headers")) {
		return "_HEADERS";
	} else if (!strcmp (type, "man")) {
		return "_MANS";
	} else if (!strcmp (type, "data")) {
		return "_DATA";
	} else if (!strcmp (type, "program")) {
		return "_PROGRAMS";
	} else if (!strcmp (type, "script")) {
		return "_SCRIPTS";
	} else if (!strcmp (type, "info")) {
		return "_TEXINFOS";
	} else if (!strcmp (type, "java")) {
		return "_JAVA";
	} else if (!strcmp (type, "python")) {
		return "_PYTHON";
	} else if (!strcmp (type, "lisp")) {
		return "_LISP";
	} else {
		return "";
	}
}

static const gchar * 
default_install_for_target_type (const gchar *type)
{
	if (!strcmp (type, "static_lib")) {
		return _("lib");
	} else if (!strcmp (type, "shared_lib")) {
		return _("lib");
	} else if (!strcmp (type, "headers")) {
		return _("include");
	} else if (!strcmp (type, "man")) {
		return _("man");
	} else if (!strcmp (type, "data")) {
		return _("data");
	} else if (!strcmp (type, "program")) {
		return _("bin");
	} else if (!strcmp (type, "script")) {
		return _("bin");
	} else if (!strcmp (type, "info")) {
		return _("info");
	} else if (!strcmp (type, "java")) {
		return _("Java Module");
	} else if (!strcmp (type, "python")) {
		return _("Python Module");
	} else if (!strcmp (type, "lisp")) {
		return _("lisp");
	} else {
		return _("Unknown");
	}
}

/* Public functions
 *---------------------------------------------------------------------------*/

gboolean
mkp_project_save (MkpProject *project, GError **error)
{
	gpointer key;
	gpointer value;
	GHashTableIter iter;

	g_return_val_if_fail (project != NULL, FALSE);

	g_hash_table_iter_init (&iter, project->files);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		GError *error = NULL;
		AnjutaTokenFile *tfile = (AnjutaTokenFile *)value;
		;
		anjuta_token_file_save (tfile, &error);
	}

	return TRUE;
}

gboolean
mkp_project_move (MkpProject *project, const gchar *path)
{
	GFile	*old_root_file;
	GFile *new_file;
	gchar *relative;
	GHashTableIter iter;
	gpointer key;
	gpointer value;
	AnjutaTokenFile *tfile;
	GHashTable* old_hash;

	/* Change project root directory */
	old_root_file = project->root_file;
	project->root_file = g_file_new_for_path (path);

	/* Change project root directory in groups */
	old_hash = project->groups;
	project->groups = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	g_hash_table_iter_init (&iter, old_hash);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		MkpGroup *group = (MkpGroup *)value;
		
		relative = get_relative_path (old_root_file, MKP_GROUP_DATA (group)->directory);
		new_file = g_file_resolve_relative_path (project->root_file, relative);
		g_free (relative);
		g_object_unref (MKP_GROUP_DATA (group)->directory);
		MKP_GROUP_DATA (group)->directory = new_file;

		g_hash_table_insert (project->groups, g_file_get_uri (new_file), group);
	}
	g_hash_table_destroy (old_hash);

	/* Change all files */
	old_hash = project->files;
	project->files = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, g_object_unref);
	g_hash_table_iter_init (&iter, old_hash);
	while (g_hash_table_iter_next (&iter, &key, (gpointer *)&tfile))
	{
		relative = get_relative_path (old_root_file, anjuta_token_file_get_file (tfile));
		new_file = g_file_resolve_relative_path (project->root_file, relative);
		g_free (relative);
		anjuta_token_file_move (tfile, new_file);
		
		g_hash_table_insert (project->files, new_file, tfile);
		g_object_unref (key);
	}
	g_hash_table_steal_all (old_hash);
	g_hash_table_destroy (old_hash);

	g_object_unref (old_root_file);

	return TRUE;
}

MkpProject *
mkp_project_new (void)
{
	return MKP_PROJECT (g_object_new (MKP_TYPE_PROJECT, NULL));
}

/* Project access functions
 *---------------------------------------------------------------------------*/

MkpGroup *
mkp_project_get_root (MkpProject *project)
{
	MkpGroup *g_node = NULL;
	
	if (project->root_file != NULL)
	{
		gchar *id = g_file_get_uri (project->root_file);
		g_node = (MkpGroup *)g_hash_table_lookup (project->groups, id);
		g_free (id);
	}

	return g_node;
}

MkpGroup *
mkp_project_get_group (MkpProject *project, const gchar *id)
{
	return (MkpGroup *)g_hash_table_lookup (project->groups, id);
}

MkpTarget *
mkp_project_get_target (MkpProject *project, const gchar *id)
{
	MkpTarget **buffer;
	MkpTarget *target;
	gsize dummy;

	buffer = (MkpTarget **)g_base64_decode (id, &dummy);
	target = *buffer;
	g_free (buffer);

	return target;
}

MkpSource *
mkp_project_get_source (MkpProject *project, const gchar *id)
{
	MkpSource **buffer;
	MkpSource *source;
	gsize dummy;

	buffer = (MkpSource **)g_base64_decode (id, &dummy);
	source = *buffer;
	g_free (buffer);

	return source;
}

gchar *
mkp_project_get_node_id (MkpProject *project, const gchar *path)
{
	GNode *node = NULL;

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
				if (child == 0) node = project->root_node;
			}
			else
			{
				node = g_node_nth_child (node, child);
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

	switch (MKP_NODE_DATA (node)->type)
	{
		case MKP_NODE_GROUP:
			return g_file_get_uri (MKP_GROUP_DATA (node)->directory);
		case MKP_NODE_TARGET:
		case MKP_NODE_SOURCE:
			return g_base64_encode ((guchar *)&node, sizeof (node));
		default:
			return NULL;
	}
}

gchar *
mkp_project_get_uri (MkpProject *project)
{
	g_return_val_if_fail (project != NULL, NULL);

	return project->root_file != NULL ? g_file_get_uri (project->root_file) : NULL;
}

GFile*
mkp_project_get_file (MkpProject *project)
{
	g_return_val_if_fail (project != NULL, NULL);

	return project->root_file;
}
	
gchar *
mkp_project_get_property (MkpProject *project, MkpPropertyType type)
{
	return NULL;
}

gboolean
mkp_project_set_property (MkpProject *project, MkpPropertyType type, const gchar *value)
{
	return TRUE;
}


/* Node access functions
 *---------------------------------------------------------------------------*/

MkpNode *
mkp_node_parent(MkpNode *node)
{
	return node->parent;
}

MkpNode *
mkp_node_first_child(MkpNode *node)
{
	return g_node_first_child (node);
}

MkpNode *
mkp_node_last_child(MkpNode *node)
{
	return g_node_last_child (node);
}

MkpNode *
mkp_node_next_sibling (MkpNode *node)
{
	return g_node_next_sibling (node);
}

MkpNode *
mkp_node_prev_sibling (MkpNode *node)
{
	return g_node_prev_sibling (node);
}

MkpNodeType
mkp_node_get_type (MkpNode *node)
{
	return MKP_NODE_DATA (node)->type;
}

void
mkp_node_all_foreach (MkpNode *node, MkpNodeFunc func, gpointer data)
{
	g_node_traverse (node, G_PRE_ORDER, G_TRAVERSE_ALL, -1, func, data);
}

/* Group access functions
 *---------------------------------------------------------------------------*/

GFile*
mkp_group_get_directory (MkpGroup *group)
{
	return MKP_GROUP_DATA (group)->directory;
}

GFile*
mkp_group_get_makefile (MkpGroup *group)
{
	return MKP_GROUP_DATA (group)->makefile;
}

gchar *
mkp_group_get_id (MkpGroup *group)
{
	return g_file_get_uri (MKP_GROUP_DATA (group)->directory);
}

/* Target access functions
 *---------------------------------------------------------------------------*/

const gchar *
mkp_target_get_name (MkpTarget *target)
{
	return MKP_TARGET_DATA (target)->name;
}

const gchar *
mkp_target_get_type (MkpTarget *target)
{
	return MKP_TARGET_DATA (target)->type;
}

gchar *
mkp_target_get_id (MkpTarget *target)
{
	return g_base64_encode ((guchar *)&target, sizeof (target));
}

/* Source access functions
 *---------------------------------------------------------------------------*/

gchar *
mkp_source_get_id (MkpSource *source)
{
	return g_base64_encode ((guchar *)&source, sizeof (source));
}

GFile*
mkp_source_get_file (MkpSource *source)
{
	return MKP_SOURCE_DATA (source)->file;
}

/* GbfProject implementation
 *---------------------------------------------------------------------------*/

static void
mkp_project_dispose (GObject *object)
{
	g_return_if_fail (MKP_IS_PROJECT (object));

	mkp_project_unload (MKP_PROJECT (object));

	G_OBJECT_CLASS (parent_class)->dispose (object);	
}

static void
mkp_project_instance_init (MkpProject *project)
{
	g_return_if_fail (project != NULL);
	g_return_if_fail (MKP_IS_PROJECT (project));
	
	/* project data */
	project->root_file = NULL;
	project->root_node = NULL;
	project->property = NULL;

	project->space_list = NULL;
	project->arg_list = NULL;
}

static void
mkp_project_class_init (MkpProjectClass *klass)
{
	GObjectClass *object_class;
	
	parent_class = g_type_class_peek_parent (klass);

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = mkp_project_dispose;
}

GBF_BACKEND_BOILERPLATE (MkpProject, mkp_project);
