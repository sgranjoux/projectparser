/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4; coding: utf-8 -*- */
/* am-project.c
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
#include "am-project.h"
#include "ac-scanner.h"
#include "am-scanner.h"
//#include "am-config.h"
//#include "am-properties.h"


#define UNIMPLEMENTED  G_STMT_START { g_warning (G_STRLOC": unimplemented"); } G_STMT_END

/* Constant strings for parsing perl script error output */
#define ERROR_PREFIX      "ERROR("
#define WARNING_PREFIX    "WARNING("
#define MESSAGE_DELIMITER ": "

typedef enum {
	AMP_NODE_GROUP,
	AMP_NODE_TARGET,
	AMP_NODE_SOURCE
} AmpNodeType;

typedef GNode AmpNode;
typedef GNode AmpGroup;

struct _AmpProject {
	GbfProject          parent;

	/* uri of the project; this can be a full uri, even though we
	 * can only work with native local files */
	GFile			*root_file;

	/* project data */
	AnjutaTokenFile		*configure_file;		/* configure.in file */
	
	AmpGroup              *root_node;         	/* tree containing project data;
								 * each GNode's data is a
								 * GbfAmpNode, and the root of
								 * the tree is the root group. */

	/* shortcut hash tables, mapping id -> GNode from the tree above */
	GHashTable		*groups;
	GHashTable		*files;
	
	GHashTable	*modules;
	
	/* project files monitors */
	GHashTable         *monitors;
};

struct _AmpProjectClass {
	GbfProjectClass parent_class;
};

/* convenient shortcut macro the get the GbfAmpNode from a GNode */
#define AMP_NODE_DATA(node)  ((node) != NULL ? (AmpNodeData *)((node)->data) : NULL)
#define AMP_GROUP_DATA(node)  ((node) != NULL ? (AmpGroupData *)((node)->data) : NULL)
#define AMP_TARGET_DATA(node)  ((node) != NULL ? (AmpTargetData *)((node)->data) : NULL)
#define AMP_SOURCE_DATA(node)  ((node) != NULL ? (AmpSourceData *)((node)->data) : NULL)

typedef struct _AmpPackage AmpPackage;

struct _AmpPackage {
    gchar *name;
    gchar *version;
};

typedef struct _AmpModule AmpModule;
	
struct _AmpModule {
    GList *packages;
    AnjutaToken *module;
};

typedef struct _AmpNodeData AmpNodeData;

struct _AmpNodeData {
	AmpNodeType	type;			/* Node type */
	gchar				*id;				/* Node string id, FIXME: to be removed */
};

typedef struct _AmpGroupData AmpGroupData;

struct _AmpGroupData {
	AmpNodeData node;			/* Common node data */
    gchar *makefile;		
	gboolean dist_only;			/* TRUE if the group is distributed but not built */
	GFile *file;
	AnjutaTokenFile *tfile;		/* Corresponding Makefile */
	GList *tokens;					/* List of token used by this group */
};

typedef enum _AmpTargetFlag
{
	AM_TARGET_CHECK = 1 << 0,
	AM_TARGET_NOINST = 1 << 1,
	AM_TARGET_DIST = 1 << 2,
	AM_TARGET_NODIST = 1 << 3,
	AM_TARGET_NOBASE = 1 << 4,
	AM_TARGET_NOTRANS = 1 << 5,
	AM_TARGET_MAN = 1 << 6,
	AM_TARGET_MAN_SECTION = 31 << 7
} AmpTargetFlag;

typedef GNode AmpTarget;

typedef struct _AmpTargetData AmpTargetData;

struct _AmpTargetData {
	AmpNodeData node;
	gchar *name;
	gchar *type;
	gchar *install;
	gint flags;
	AnjutaToken* token;
};

typedef GNode AmpSource;

typedef struct _AmpSourceData AmpSourceData;

struct _AmpSourceData {
	AmpNodeData node;
	GFile *file;
	AnjutaToken* token;
};

typedef struct _AmpConfigFile AmpConfigFile;

struct _AmpConfigFile {
	GFile *parent;
	gchar *filename;
};


/* ----- Standard GObject types and variables ----- */

enum {
	PROP_0,
	PROP_PROJECT_DIR
};

static GbfProject *parent_class;


/* ----------------------------------------------------------------------
   Private prototypes
   ---------------------------------------------------------------------- */

#if 0
static gboolean        uri_is_equal                 (const gchar       *uri1,
						     const gchar       *uri2);
static gboolean        uri_is_parent                (const gchar       *parent_uri,
						     const gchar       *child_uri);
static gboolean        uri_is_local_path            (const gchar       *uri);
static gchar          *uri_normalize                (const gchar       *path_or_uri,
						     const gchar       *base_uri);
#endif

static gboolean        project_reload               (AmpProject      *project,
						     GError           **err);

//static void            project_data_unload         (AmpProject      *project);
static void            project_data_init            (AmpProject      *project);

static void            amp_project_class_init    (AmpProjectClass *klass);
static void            amp_project_instance_init (AmpProject      *project);
static void            amp_project_dispose       (GObject           *object);
static void            amp_project_get_property  (GObject           *object,
						     guint              prop_id,
						     GValue            *value,
						     GParamSpec        *pspec);


/* Helper functions
 *---------------------------------------------------------------------------*/

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

gboolean
remove_list_item (AnjutaToken *token)
{
	AnjutaTokenStyle *style;
	AnjutaToken *space;

	DEBUG_PRINT ("remove list item");

	style = anjuta_token_style_new (0);
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
	anjuta_token_style_free (style);
	
	return TRUE;
}

static gboolean
add_list_item (AnjutaToken *list, AnjutaToken *token)
{
	AnjutaTokenStyle *style;
	AnjutaToken *space;

	style = anjuta_token_style_new (0);
	anjuta_token_style_update (style, anjuta_token_parent (list));
	
	space = anjuta_token_new_static (ANJUTA_TOKEN_SPACE | ANJUTA_TOKEN_IRRELEVANT | ANJUTA_TOKEN_ADDED, " ");
	space = anjuta_token_insert_after (list, space);
	anjuta_token_insert_after (space, token);

	anjuta_token_style_format (style, anjuta_token_parent (list));
	anjuta_token_style_free (style);
	
	return TRUE;
}


#if 0
/*
 * URI and path manipulation functions -----------------------------
 */

static gboolean 
uri_is_equal (const gchar *uri1,
	      const gchar *uri2)
{
	gboolean retval = FALSE;
	GFile *file1, *file2;

	file1 = g_file_new_for_commandline_arg (uri1);
	file2 = g_file_new_for_commandline_arg (uri2);
	retval = g_file_equal (file1, file2);
	g_object_unref (file1);
	g_object_unref (file2);

	return retval;
}

static gboolean 
uri_is_parent (const gchar *parent_uri,
	       const gchar *child_uri)
{
	gboolean retval = FALSE;
	GFile *parent, *child;

	parent = g_file_new_for_commandline_arg (parent_uri);
	child = g_file_new_for_commandline_arg (child_uri);
	retval = g_file_has_prefix (child, parent);
	g_object_unref (parent);
	g_object_unref (child);

	return retval;
}

/*
 * This is very similar to the function that decides in 
 * g_file_new_for_commandline_arg
 */
static gboolean
uri_is_local_path (const gchar *uri)
{
	const gchar *p;
	
	for (p = uri;
	     g_ascii_isalnum (*p) || *p == '+' || *p == '-' || *p == '.';
	     p++)
		;

	if (*p == ':')
		return FALSE;
	else
		return TRUE;
}

/*
 * This is basically g_file_get_uri (g_file_new_for_commandline_arg (uri)) with
 * an option to give a basedir while g_file_new_for_commandline_arg always
 * uses the current dir.
 */
static gchar *
uri_normalize (const gchar *path_or_uri, const gchar *base_uri)
{
	gchar *normalized_uri = NULL;

	if (base_uri != NULL && uri_is_local_path (path_or_uri))
	{
		GFile *base;
		GFile *resolved;

		base = g_file_new_for_uri (base_uri);
		resolved = g_file_resolve_relative_path (base, path_or_uri);

		normalized_uri = g_file_get_uri (resolved);
		g_object_unref (base);
		g_object_unref (resolved);
	}
	else
	{
		GFile *file;

		file = g_file_new_for_commandline_arg (path_or_uri);
		normalized_uri = g_file_get_uri (file);
		g_object_unref (file);
	}

	return normalized_uri;
}
#endif

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

/* Automake parsing function
 *---------------------------------------------------------------------------*/

/* Remove invalid character according to automake rules */
gchar*
canonicalize_automake_variable (gchar *name)
{
	gchar *canon_name = g_strdup (name);
	gchar *ptr;
	
	for (ptr = canon_name; *ptr != '\0'; ptr++)
	{
		if (!g_ascii_isalnum (*ptr) && (*ptr != '@'))
		{
			*ptr = '_';
		}
	}

	return canon_name;
}

gboolean
split_automake_variable (gchar *name, gint *flags, gchar **module, gchar **primary)
{
	GRegex *regex;
	GMatchInfo *match_info;
	gint start_pos;
	gint end_pos;

	regex = g_regex_new ("(nobase_|notrans_)?"
	                     "(dist_|nodist_)?"
	                     "(noinst_|check_|man_|man[0-9al]_)?"
	                     "(.*_)?"
	                     "([^_]+)",
	                     G_REGEX_ANCHORED,
	                     G_REGEX_MATCH_ANCHORED,
	                     NULL);

	if (!g_regex_match (regex, name, G_REGEX_MATCH_ANCHORED, &match_info)) return FALSE;

	if (flags)
	{
		*flags = 0;
		g_match_info_fetch_pos (match_info, 1, &start_pos, &end_pos);
		if (start_pos >= 0)
		{
			if (*(name + start_pos + 2) == 'b') *flags |= AM_TARGET_NOBASE;
			if (*(name + start_pos + 2) == 't') *flags |= AM_TARGET_NOTRANS;
		}

		g_match_info_fetch_pos (match_info, 2, &start_pos, &end_pos);
		if (start_pos >= 0)
		{
			if (*(name + start_pos) == 'd') *flags |= AM_TARGET_DIST;
			if (*(name + start_pos) == 'n') *flags |= AM_TARGET_NODIST;
		}

		g_match_info_fetch_pos (match_info, 3, &start_pos, &end_pos);
		if (start_pos >= 0)
		{
			if (*(name + start_pos) == 'n') *flags |= AM_TARGET_NOINST;
			if (*(name + start_pos) == 'c') *flags |= AM_TARGET_CHECK;
			if (*(name + start_pos) == 'm')
			{
				gchar section = *(name + end_pos - 1);
				*flags |= AM_TARGET_MAN;
				if (section != 'n') *flags |= (section & 0x1F) << 7;
			}
		}
	}

	if (module)
	{
		g_match_info_fetch_pos (match_info, 4, &start_pos, &end_pos);
		if (start_pos >= 0)
		{
			*module = name + start_pos;
			*(name + end_pos - 1) = '\0';
		}
		else
		{
			*module = NULL;
		}
	}

	if (primary)
	{
		g_match_info_fetch_pos (match_info, 5, &start_pos, &end_pos);
		if (start_pos >= 0)
		{
			*primary = name + start_pos;
		}
		else
		{
			*primary = NULL;
		}
	}

	g_regex_unref (regex);

	return TRUE;
}

/* Config file objects
 *---------------------------------------------------------------------------*/

static AmpConfigFile*
amp_config_file_new (const gchar *pathname, GFile *project_root)
{
	AmpConfigFile *config;
	GFile *file;

	g_return_val_if_fail ((pathname != NULL) && (project_root != NULL), NULL);

	file = g_file_resolve_relative_path (project_root, pathname);
	
	config = g_slice_new0(AmpConfigFile);
	config->parent = g_file_get_parent (file);
	config->filename = g_file_get_basename (file);
	g_object_unref (file);

	return config;
}

static void
amp_config_file_free (AmpConfigFile *config)
{
	if (config)
	{
		g_object_unref (config->parent);
		g_free (config->filename);
		g_slice_free (AmpConfigFile, config);
	}
}

/* Package objects
 *---------------------------------------------------------------------------*/

void
amp_package_set_version (AmpPackage *package, const gchar *compare, const gchar *version)
{
	g_return_if_fail (package != NULL);
	g_return_if_fail ((version == NULL) || (compare != NULL));

	g_free (package->version);
	package->version = version != NULL ? g_strconcat (compare, version, NULL) : NULL;
}

static AmpPackage*
amp_package_new (const gchar *name)
{
	AmpPackage *package;

	g_return_val_if_fail (name != NULL, NULL);
	
	package = g_slice_new0(AmpPackage); 
	package->name = g_strdup (name);

	return package;
}

static void
amp_package_free (AmpPackage *package)
{
	if (package)
	{
		g_free (package->name);
		g_free (package->version);
		g_slice_free (AmpPackage, package);
	}
}

/* Module objects
 *---------------------------------------------------------------------------*/

static AmpModule*
amp_module_new (AnjutaToken *token)
{
	AmpModule *module;
	
	module = g_slice_new0(AmpModule); 
	module->module = token;

	return module;
}

static void
amp_module_free (AmpModule *module)
{
	if (module)
	{
		g_list_foreach (module->packages, (GFunc)amp_package_free, NULL);
		g_slice_free (AmpModule, module);
	}
}

static void
amp_project_new_module_hash (AmpProject *project)
{
	project->modules = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)amp_module_free);
}

static void
amp_project_free_module_hash (AmpProject *project)
{
	if (project->modules != NULL)
	{
		g_hash_table_destroy (project->modules);
		project->modules = NULL;
	}
}

/* Group objects
 *---------------------------------------------------------------------------*/

static void
amp_group_add_token (AmpGroup *node, AnjutaToken *token)
{
    AmpGroupData *group;
	
	g_return_if_fail ((node != NULL) && (node->data != NULL)); 

 	group = (AmpGroupData *)node->data;
	group->tokens = g_list_prepend (group->tokens, token);
}

static void
amp_group_set_dist_only (AmpGroup *node, gboolean dist_only)
{
    AmpGroupData *group;
	
	g_return_if_fail ((node != NULL) && (node->data != NULL)); 

 	group = (AmpGroupData *)node->data;
	group->dist_only = dist_only;
}

static AmpGroup*
amp_group_new (GFile *file, const gchar *makefile, gboolean dist_only)
{
    AmpGroupData *group = NULL;

	g_return_val_if_fail (file != NULL, NULL);

	group = g_slice_new0(AmpGroupData); 
	group->node.type = AMP_NODE_GROUP;
	group->file = g_object_ref (file);
	group->makefile = makefile != NULL ? g_strdup (makefile) : NULL;
	group->dist_only = dist_only;
	group->tokens = NULL;

    return g_node_new (group);
}

static void
amp_group_free (AmpGroup *node)
{
    AmpGroupData *group = (AmpGroupData *)node->data;
	
	if (group->tfile) g_object_unref (G_OBJECT (group->tfile));
	g_object_unref (group->file);
    g_slice_free (AmpGroupData, group);

	g_node_destroy (node);
}

/* Target objects
 *---------------------------------------------------------------------------*/

static AmpTarget*
amp_target_new (const gchar *name, const gchar *type, const gchar *install, gint flags)
{
    AmpTargetData *target = NULL;

	target = g_slice_new0(AmpTargetData); 
	target->node.type = AMP_NODE_TARGET;
	target->name = g_strdup (name);
	target->type = g_strdup (type);
	target->install = g_strdup (install);
	target->flags = flags;

    return g_node_new (target);
}

static void
amp_target_free (AmpTarget *node)
{
    AmpTargetData *target = (AmpTargetData *)node->data;
	
    g_free (target->name);
    g_free (target->type);
    g_free (target->install);
    g_slice_free (AmpTargetData, target);

	g_node_destroy (node);
}

/* Source objects
 *---------------------------------------------------------------------------*/

static AmpSource*
amp_source_new (GFile *parent, const gchar *name)
{
    AmpSourceData *source = NULL;

	source = g_slice_new0(AmpSourceData); 
	source->node.type = AMP_NODE_SOURCE;
	source->file = g_file_get_child (parent, name);

    return g_node_new (source);
}

static void
amp_source_free (AmpSource *node)
{
    AmpSourceData *source = (AmpSourceData *)node->data;
	
    g_object_unref (source->file);
    g_slice_free (AmpSourceData, source);

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
	AmpProject *project = data;

	g_return_if_fail (project != NULL && AMP_IS_PROJECT (project));

	switch (event_type) {
		case G_FILE_MONITOR_EVENT_CHANGED:
		case G_FILE_MONITOR_EVENT_DELETED:
			/* monitor will be removed here... is this safe? */
			project_reload (project, NULL);
			g_signal_emit_by_name (G_OBJECT (project), "project-updated");
			break;
		default:
			break;
	}
}


static void
monitor_add (AmpProject *project, GFile *file)
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
monitors_remove (AmpProject *project)
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
	AmpGroup *group_node = value;
	AmpProject *project = user_data;

	monitor_add (project, AMP_GROUP_DATA(group_node)->file);
}

static void
monitors_setup (AmpProject *project)
{
	g_return_if_fail (project != NULL);

	monitors_remove (project);
	
	/* setup monitors hash */
	project->monitors = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
						   (GDestroyNotify) g_file_monitor_cancel);

	monitor_add (project, anjuta_token_file_get_file (project->configure_file));
	if (project->groups)
		g_hash_table_foreach (project->groups, group_hash_foreach_monitor, project);
}


/*
 * ---------------- Data structures managment
 */

static void
amp_dump_node (GNode *g_node)
{
	gchar *name = NULL;
	
	switch (AMP_NODE_DATA (g_node)->type) {
		case AMP_NODE_GROUP:
			name = g_file_get_uri (AMP_GROUP_DATA (g_node)->file);
			DEBUG_PRINT ("GROUP: %s", name);
			break;
		case AMP_NODE_TARGET:
			name = g_strdup (AMP_TARGET_DATA (g_node)->name);
			DEBUG_PRINT ("TARGET: %s", name);
			break;
		case AMP_NODE_SOURCE:
			name = g_file_get_uri (AMP_SOURCE_DATA (g_node)->file);
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
	switch (AMP_NODE_DATA (g_node)->type) {
		case AMP_NODE_GROUP:
			//g_hash_table_remove (project->groups, g_file_get_uri (AMP_GROUP_NODE (g_node)->file));
			amp_group_free (g_node);
			break;
		case AMP_NODE_TARGET:
			amp_target_free (g_node);
			break;
		case AMP_NODE_SOURCE:
			//g_hash_table_remove (project->sources, AMP_NODE (g_node)->id);
			amp_source_free (g_node);
			break;
		default:
			g_assert_not_reached ();
			break;
	}
	

	return FALSE;
}

static void
project_node_destroy (AmpProject *project, GNode *g_node)
{
	g_return_if_fail (project != NULL);
	g_return_if_fail (AMP_IS_PROJECT (project));
	
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
project_unload (AmpProject *project)
{
	g_return_if_fail (AMP_IS_PROJECT (project));
	
	monitors_remove (project);
	
	/* project data */
	project_node_destroy (project, project->root_node);
	project->root_node = NULL;

	if (project->configure_file)	g_object_unref (G_OBJECT (project->configure_file));
	project->configure_file = NULL;

	if (project->root_file) g_object_unref (project->root_file);
	project->root_file = NULL;
	
	/* shortcut hash tables */
	if (project->groups) g_hash_table_destroy (project->groups);
	if (project->files) g_hash_table_destroy (project->files);
	project->groups = NULL;
	project->files = NULL;

	amp_project_free_module_hash (project);
}

static void
project_reload_packages   (AmpProject *project)
{
	AnjutaToken *pkg_check_tok;
	AnjutaToken *sequence;
	
	pkg_check_tok = anjuta_token_new_static (ANJUTA_TOKEN_KEYWORD | ANJUTA_TOKEN_SIGNIFICANT, "PKG_CHECK_MODULES(");
	
    sequence = anjuta_token_file_first (project->configure_file);
	for (;;)
	{
		AnjutaToken *module;
		AnjutaToken *arg;
		gchar *value;
		AmpModule *mod;
		AmpPackage *pack;
		gchar *compare;
		
		if (!anjuta_token_match (pkg_check_tok, ANJUTA_SEARCH_OVER, sequence, &module)) break;

		arg = anjuta_token_next_child (module);	/* Name */

		value = anjuta_token_evaluate (arg);
		mod = amp_module_new (arg);
		mod->packages = NULL;
		g_hash_table_insert (project->modules, value, mod);

		arg = anjuta_token_next_sibling (arg);	/* Separator */

		arg = anjuta_token_next_sibling (arg);	/* Package list */
		
		pack = NULL;
		compare = NULL;
		for (arg = anjuta_token_next_child (arg); arg != NULL; arg = anjuta_token_next_sibling (arg))
		{
			if ((anjuta_token_get_type (arg) == ANJUTA_TOKEN_SPACE) || (anjuta_token_get_type (arg) == ANJUTA_TOKEN_COMMENT)) continue;
			
			value = anjuta_token_evaluate (arg);

			if ((pack != NULL) && (compare != NULL))
			{
				amp_package_set_version (pack, compare, value);
				g_free (value);
				g_free (compare);
				pack = NULL;
				compare = NULL;
			}
			else if ((pack != NULL) && (anjuta_token_get_type (arg) == ANJUTA_TOKEN_OPERATOR))
			{
				compare = value;
			}
			else
			{
				pack = amp_package_new (value);
				mod->packages = g_list_prepend (mod->packages, pack);
				g_free (value);
				compare = NULL;
			}
		}
		mod->packages = g_list_reverse (mod->packages);

		sequence = anjuta_token_next_sibling (module);
	}
	anjuta_token_free (pkg_check_tok);
	
}

/* Add a GFile in the list for each makefile in the token list */
void
amp_project_add_config_files (AmpProject *project, AnjutaToken *list, GList **config_files)
{
	AnjutaToken* arg;

	for (arg = anjuta_token_next_child (list); arg != NULL; arg = anjuta_token_next_sibling (arg))
	{
		gchar *value;
		
		if ((anjuta_token_get_type (arg) == ANJUTA_TOKEN_SPACE) || (anjuta_token_get_type (arg) == ANJUTA_TOKEN_COMMENT)) continue;
			
		value = anjuta_token_evaluate (arg);
		if ((value != NULL) && (*value != '\0'))
		{
			*config_files = g_list_prepend (*config_files, amp_config_file_new (value, project->root_file));
		}
		g_free (value);
	}
}							   
                           
static GList *
project_list_config_files (AmpProject *project)
{
	AnjutaToken *config_files_tok;
	AnjutaToken *sequence;
	GList *config_files = NULL;

	//g_message ("load config project %p root file %p", project, project->root_file);	
	/* Search the new AC_CONFIG_FILES macro */
	config_files_tok = anjuta_token_new_static (ANJUTA_TOKEN_KEYWORD | ANJUTA_TOKEN_SIGNIFICANT, "AC_CONFIG_FILES(");

    sequence = anjuta_token_file_first (project->configure_file);
	while (sequence != NULL)
	{
		AnjutaToken *arg;

		if (!anjuta_token_match (config_files_tok, ANJUTA_SEARCH_OVER, sequence, &sequence)) break;
		arg = anjuta_token_next_child (sequence);	/* List */
		amp_project_add_config_files (project, arg, &config_files);
		sequence = anjuta_token_next_sibling (sequence);
	}
	
	if (config_files == NULL)
	{
		/* Search the old AC_OUTPUT macro */
	    anjuta_token_free(config_files_tok);
	    config_files_tok = anjuta_token_new_static (ANJUTA_TOKEN_KEYWORD | ANJUTA_TOKEN_SIGNIFICANT, "AC_OUTPUT(");
		
	    sequence = anjuta_token_file_first (project->configure_file);
		while (sequence != NULL)
		{
			AnjutaToken *arg;

			if (!anjuta_token_match (config_files_tok, ANJUTA_SEARCH_OVER, sequence, &sequence)) break;
			arg = anjuta_token_next_child (sequence);	/* List */
			amp_project_add_config_files (project, arg, &config_files);
			sequence = anjuta_token_next_sibling (sequence);
		}
	}
	
	anjuta_token_free (config_files_tok);

	return config_files;
}

static void
find_target (GNode *node, gpointer data)
{
	if (AMP_NODE_DATA (node)->type == AMP_NODE_TARGET)
	{
		if (strcmp (AMP_TARGET_DATA (node)->name, *(gchar **)data) == 0)
		{
			/* Find target, return node value in pointer */
			*(GNode **)data = node;

			return;
		}
	}
}

static void
find_canonical_target (GNode *node, gpointer data)
{
	if (AMP_NODE_DATA (node)->type == AMP_NODE_TARGET)
	{
		gchar *canon_name = canonicalize_automake_variable (AMP_TARGET_DATA (node)->name);	
		DEBUG_PRINT ("compare canon %s vs %s node %p", canon_name, *(gchar **)data, node);
		if (strcmp (canon_name, *(gchar **)data) == 0)
		{
			/* Find target, return node value in pointer */
			*(GNode **)data = node;
			g_free (canon_name);

			return;
		}
		g_free (canon_name);
	}
}

static AnjutaToken*
project_load_target (AmpProject *project, AnjutaToken *start, GNode *parent, GHashTable *orphan_sources)
{
	AnjutaToken *arg;
	const gchar *type;
	gchar *install;
	gchar *name;
	gint flags;

	switch (anjuta_token_get_type (start))
	{
	case AM_TOKEN__DATA:
			type = "data";
			break;
	case AM_TOKEN__HEADERS:
			type = "headers";
			break;
	case AM_TOKEN__LIBRARIES:
			type =  "static_lib";
			break;
	case AM_TOKEN__LISP:
			type = "lisp";
			break;
	case AM_TOKEN__LTLIBRARIES:
			type = "shared_lib";
			break;
	case AM_TOKEN__MANS:
			type = "man";
			break;
	case AM_TOKEN__PROGRAMS:
			type = "programs";
			break;
	case AM_TOKEN__PYTHON:
			type = "python";
			break;
	case AM_TOKEN__JAVA:
			type = "java";
			break;
	case AM_TOKEN__SCRIPTS:
			type = "script";
			break;
	case AM_TOKEN__TEXINFOS:
			type = "info";
			break;
	default:
			return NULL;
	}

	name = anjuta_token_get_value (start);
	split_automake_variable (name, &flags, &install, NULL);
	g_free (name);

	arg = anjuta_token_next_child (start);		/* Get variable data */
	if (arg == NULL) return NULL;
	if (anjuta_token_get_type (arg) == ANJUTA_TOKEN_SPACE) arg = anjuta_token_next_sibling (arg); /* Skip space */
	if (arg == NULL) return NULL;
	arg = anjuta_token_next_sibling (arg);			/* Skip equal */
	if (arg == NULL) return NULL;

	for (arg = anjuta_token_next_child (arg); arg != NULL; arg = anjuta_token_next_sibling (arg))
	{
		gchar *value;
		gchar *canon_id;
		AmpTarget *target;
		GList *sources;
		gchar *orig_key;
		gpointer find;

		if ((anjuta_token_get_type (arg) == ANJUTA_TOKEN_SPACE) || (anjuta_token_get_type (arg) == ANJUTA_TOKEN_COMMENT)) continue;
			
		value = anjuta_token_evaluate (arg);
		canon_id = canonicalize_automake_variable (value);		
		
		/* Check if target already exists */
		find = value;
		g_node_children_foreach (parent, G_TRAVERSE_ALL, find_target, &find);
		if ((gchar *)find != value)
		{
			/* Find target */
			g_free (canon_id);
			g_free (value);
			continue;
		}

		/* Create target */
		target = amp_target_new (value, type, install, flags);
		AMP_TARGET_DATA (target)->token = arg;
		g_node_append (parent, target);
		DEBUG_PRINT ("create target %p name %s", target, value);

		/* Check if there are source availables */
		if (g_hash_table_lookup_extended (orphan_sources, canon_id, (gpointer *)&orig_key, (gpointer *)&sources))
		{
			GList *src;
			g_hash_table_steal (orphan_sources, canon_id);
			for (src = sources; src != NULL; src = g_list_next (src))
			{
				AmpSource *source = src->data;

				g_node_prepend (target, source);
			}
			g_free (orig_key);
			g_list_free (sources);
		}

		g_free (canon_id);
		g_free (value);
	}

	return NULL;
}

static AnjutaToken*
project_load_sources (AmpProject *project, AnjutaToken *start, GNode *parent, GHashTable *orphan_sources)
{
	AnjutaToken *arg;
	AmpGroupData *group = (AmpGroupData *)parent->data;
	GFile *parent_file = g_object_ref (group->file);
	gchar *target_id = NULL;
	GList *orphan = NULL;

	target_id = anjuta_token_get_value (start);
	if (target_id)
	{
		gchar *end = strrchr (target_id, '_');
		if (end)
		{
			*end = '\0';
		}
	}

	if (target_id)
	{
		gpointer find;
		
		find = target_id;
		DEBUG_PRINT ("search for canonical %s", target_id);
		g_node_children_foreach (parent, G_TRAVERSE_ALL, find_canonical_target, &find);
		parent = (gchar *)find != target_id ? (GNode *)find : NULL;

		arg = anjuta_token_next_child (start);		/* Get variable data */
		if (arg == NULL) return NULL;
		if (anjuta_token_get_type (arg) == ANJUTA_TOKEN_SPACE) arg = anjuta_token_next_sibling (arg); /* Skip space */
		if (arg == NULL) return NULL;
		arg = anjuta_token_next_sibling (arg);			/* Skip equal */
		if (arg == NULL) return NULL;

		for (arg = anjuta_token_next_child (arg); arg != NULL; arg = anjuta_token_next_sibling (arg))
		{
			gchar *value;
			AmpSource *source;
		
			if ((anjuta_token_get_type (arg) == ANJUTA_TOKEN_SPACE) || (anjuta_token_get_type (arg) == ANJUTA_TOKEN_COMMENT)) continue;
			
			value = anjuta_token_evaluate (arg);

			/* Create source */
			source = amp_source_new (parent_file, value);
			AMP_SOURCE_DATA(source)->token = arg;

			if (parent == NULL)
			{
				/* Add in orphan list */
				orphan = g_list_prepend (orphan, source);
			}
			else
			{
				DEBUG_PRINT ("add target child %p", parent);
				/* Add as target child */
				g_node_append (parent, source);
			}

			g_free (value);
		}
		
		if (parent == NULL)
		{
			gchar *orig_key;
			GList *orig_sources;

			if (g_hash_table_lookup_extended (orphan_sources, target_id, (gpointer *)&orig_key, (gpointer *)&orig_sources))
			{
				g_hash_table_steal (orphan_sources, target_id);
				orphan = g_list_concat (orphan, orig_sources);	
				g_free (orig_key);
			}
			g_hash_table_insert (orphan_sources, target_id, orphan);
		}
		else
		{
			g_free (target_id);
		}
	}

	g_object_unref (parent_file);

	return NULL;
}

static AmpGroup* project_load_makefile (AmpProject *project, GFile *file, AmpGroup *parent, gboolean dist_only, GList **config_files);

static void
project_load_subdirs (AmpProject *project, AnjutaToken *start, AmpGroup *parent, gboolean dist_only, GList **config_files)
{
	AnjutaToken *arg;

	arg = anjuta_token_next_child (start);		/* Get variable data */
	if (arg == NULL) return;
	if (anjuta_token_get_type (arg) == ANJUTA_TOKEN_SPACE) arg = anjuta_token_next_sibling (arg); /* Skip space */
	if (arg == NULL) return;
	arg = anjuta_token_next_sibling (arg);			/* Skip equal */
	if (arg == NULL) return;

	for (arg = anjuta_token_next_child (arg); arg != NULL; arg = anjuta_token_next_sibling (arg))
	{
		gchar *value;
		
		if ((anjuta_token_get_type (arg) == ANJUTA_TOKEN_SPACE) || (anjuta_token_get_type (arg) == ANJUTA_TOKEN_COMMENT)) continue;
			
		value = anjuta_token_evaluate (arg);
		
		/* Skip ., it is a special case, used to defined build order */
		if (strcmp (value, ".") != 0)
		{
			GFile *subdir;
			gchar *group_id;
			AmpGroup *group;

			subdir = g_file_resolve_relative_path (AMP_GROUP_DATA (parent)->file, value);
			
			/* Look for already existing group */
			group_id = g_file_get_uri (subdir);
			group = g_hash_table_lookup (project->groups, group_id);
			g_free (group_id);

			if (group != NULL)
			{
				/* Already existing group, mark for built if needed */
				if (!dist_only) amp_group_set_dist_only (group, FALSE);
				amp_group_add_token (group, arg);
			}
			else
			{
				/* Create new group */
				group = project_load_makefile (project, subdir, parent, dist_only, config_files);
				amp_group_add_token (group, arg);
			}
			g_object_unref (subdir);
		}
		g_free (value);
	}
}

static void
free_source_list (GList *source_list)
{
	g_list_foreach (source_list, (GFunc)amp_source_free, NULL);
	g_list_free (source_list);
}

static void
remove_config_file (gpointer data, GObject *object, gboolean is_last_ref)
{
	if (is_last_ref)
	{
		AmpProject *project = (AmpProject *)data;
		g_hash_table_remove (project->files, anjuta_token_file_get_file (ANJUTA_TOKEN_FILE (object)));
	}
}

static AmpGroup*
project_load_makefile (AmpProject *project, GFile *file, GNode *parent, gboolean dist_only, GList **config_files)
{
	GHashTable *orphan_sources = NULL;
	gchar *filename = NULL;
	AmpAmScanner *scanner;
	AmpGroup *group;
	GList *elem;
	AnjutaToken *significant_tok;
	AnjutaToken *arg;
	GFile *makefile = NULL;
	gchar *group_id;

	/* Find makefile name
	 * It has to be in the config_files list with .am extension */
	filename = NULL;
	for (elem = g_list_first (*config_files); elem != NULL; elem = g_list_next (elem))
	{
		AmpConfigFile *config = (AmpConfigFile *)elem->data;

		//g_message ("search %s compare %s %s", g_file_get_path (file), g_file_get_path (config->parent), config->filename);
		if (g_file_equal (config->parent, file))
		{
			filename = g_strconcat (config->filename, ".am", NULL);
			if (file_type (file, filename) == G_FILE_TYPE_REGULAR)
			{
				/* Removed used config file */
				amp_config_file_free (config);
				*config_files = g_list_delete_link (*config_files, elem);

				break;
			}
			else
			{
				g_free (filename);
				filename = NULL;
			}
		}
	}
	if (filename == NULL) return NULL;

	/* Create group */
	group = amp_group_new (file, filename, dist_only);
	g_hash_table_insert (project->groups, g_file_get_uri (file), group);
	if (parent == NULL)
	{
		project->root_node = group;
	}
	else
	{
		g_node_append (parent, group);
	}

	/* Parse makefile.am */	
	DEBUG_PRINT ("Parse: %s", g_file_get_uri (file));
	makefile = g_file_get_child (file, filename);
	AMP_GROUP_DATA (group)->tfile = anjuta_token_file_new (makefile);
	g_hash_table_insert (project->files, makefile, AMP_GROUP_DATA (group)->tfile);
	g_object_add_toggle_ref (G_OBJECT (AMP_GROUP_DATA (group)->tfile), remove_config_file, project);
	scanner = amp_am_scanner_new ();
	amp_am_scanner_parse (scanner, AMP_GROUP_DATA (group)->tfile);
	amp_am_scanner_free (scanner);
	g_free (filename);

	/* Find significant token */
	significant_tok = anjuta_token_new_static (ANJUTA_TOKEN_SIGNIFICANT, NULL);
	
	arg = anjuta_token_file_first (AMP_GROUP_DATA (group)->tfile);
	//anjuta_token_old_dump_range (arg, NULL);

	/* Create hash table for sources list */
	orphan_sources = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify)g_free, (GDestroyNotify)free_source_list);
	
	for (anjuta_token_match (significant_tok, ANJUTA_SEARCH_OVER, arg, &arg); arg != NULL; arg = anjuta_token_next_sibling (arg))
	{
		switch (anjuta_token_get_type (arg))
		{
		case AM_TOKEN_SUBDIRS:
				project_load_subdirs (project, arg, group, FALSE, config_files);
				break;
		case AM_TOKEN_DIST_SUBDIRS:
				project_load_subdirs (project, arg, group, TRUE, config_files);
				break;
		case AM_TOKEN__DATA:
		case AM_TOKEN__HEADERS:
		case AM_TOKEN__LIBRARIES:
		case AM_TOKEN__LISP:
		case AM_TOKEN__LTLIBRARIES:
		case AM_TOKEN__MANS:
		case AM_TOKEN__PROGRAMS:
		case AM_TOKEN__PYTHON:
		case AM_TOKEN__JAVA:
		case AM_TOKEN__SCRIPTS:
		case AM_TOKEN__TEXINFOS:
				project_load_target (project, arg, group, orphan_sources);
				break;
		case AM_TOKEN__SOURCES:
				project_load_sources (project, arg, group, orphan_sources);
				break;
		}
	}

	/* Free unused sources files */
	g_hash_table_destroy (orphan_sources);

	return group;
}

static gboolean
project_reload (AmpProject *project, GError **error) 
{
	AmpAcScanner *scanner;
	GFile *root_file;
	GList *config_files;
	GFile *configure_file;
	gboolean ok;

	/* Unload current project */
	root_file = g_object_ref (project->root_file);
	project_unload (project);
	project->root_file = root_file;
	DEBUG_PRINT ("reload project %p root file %p", project, project->root_file);

	/* shortcut hash tables */
	project->groups = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	project->files = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, g_object_unref);
	
	/* Find configure file */
	if (file_type (root_file, "configure.ac") == G_FILE_TYPE_REGULAR)
	{
		configure_file = g_file_get_child (root_file, "configure.ac");
	}
	else if (file_type (root_file, "configure.in") == G_FILE_TYPE_REGULAR)
	{
		configure_file = g_file_get_child (root_file, "configure.in");
	}
	else
	{
		g_set_error (error, GBF_PROJECT_ERROR, 
		             GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Project doesn't exist or invalid path"));

		return FALSE;
	}
	
	/* Parse configure */	
	project->configure_file = anjuta_token_file_new (configure_file);
	g_hash_table_insert (project->files, configure_file, project->configure_file);
	g_object_add_toggle_ref (G_OBJECT (project->configure_file), remove_config_file, project);
	g_object_unref (configure_file);
	scanner = amp_ac_scanner_new ();
	ok = amp_ac_scanner_parse (scanner, project->configure_file);
	amp_ac_scanner_free (scanner);
		     
	monitors_setup (project);

	amp_project_new_module_hash (project);
	project_reload_packages (project);
	
	/* Load all makefiles recursively */
	config_files = project_list_config_files (project);
	if (project_load_makefile (project, project->root_file, NULL, FALSE, &config_files) == NULL)
	{
		g_set_error (error, GBF_PROJECT_ERROR, 
		             GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Project doesn't exist or invalid path"));

		ok = FALSE;
	}
	g_list_foreach (config_files, (GFunc)amp_config_file_free, NULL);
	g_list_free (config_files);
	
	return ok;
}

static void
project_data_init (AmpProject *project)
{
	g_return_if_fail (project != NULL);
	g_return_if_fail (AMP_IS_PROJECT (project));
	
	/* project data */
	project->root_file = NULL;
	project->configure_file = NULL;
	project->root_node = NULL;
}

#if 0
AmConfigMapping *
amp_project_get_config (AmpProject *project, GError **error)
{
	g_return_val_if_fail (IS_AMP_PROJECT (project), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	
	return am_config_mapping_copy (project->project_config);
}

AmConfigMapping *
amp_project_get_group_config (AmpProject *project, const gchar *group_id,
				 GError **error)
{
	AmpNode *node;
	GNode *g_node;
	
	g_return_val_if_fail (IS_AMP_PROJECT (project), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	
	g_node = g_hash_table_lookup (project->groups, group_id);
	if (g_node == NULL) {
		error_set (error, PROJECT_ERROR_DOESNT_EXIST,
			   _("Group doesn't exist"));
		return NULL;
	}
	node = AMP_NODE_DATA (g_node);
	return am_config_mapping_copy (node->config);
}

AmConfigMapping *
amp_project_get_target_config (AmpProject *project, const gchar *target_id,
				  GError **error)
{
	AmpNode *node;
	GNode *g_node;

	g_return_val_if_fail (IS_AMP_PROJECT (project), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	
	g_node = g_hash_table_lookup (project->targets, target_id);
	if (g_node == NULL) {
		error_set (error, PROJECT_ERROR_DOESNT_EXIST,
			   _("Target doesn't exist"));
		return NULL;
	}
	node = AMP_NODE_DATA (g_node);
	return am_config_mapping_copy (node->config);
}

void
amp_project_set_config (AmpProject *project,
			   AmConfigMapping *new_config, GError **error)
{
	xmlDocPtr doc;
	GSList *change_set = NULL;
	
	g_return_if_fail (IS_AMP_PROJECT (project));
	g_return_if_fail (new_config != NULL);
	g_return_if_fail (error == NULL || *error == NULL);
	
	/* Create the update xml */
	doc = xml_new_change_doc (project);
	
	if (!xml_write_set_config (project, doc, NULL, new_config)) {
		xmlFreeDoc (doc);
		return;
	}

	DEBUG ({
		xmlSetDocCompressMode (doc, 0);
		xmlSaveFile ("/tmp/set-config.xml", doc);
	});

	/* Update the project */
	if (!project_update (project, doc, &change_set, error)) {
		error_set (error, PROJECT_ERROR_PROJECT_MALFORMED,
			   _("Unable to update project"));
		xmlFreeDoc (doc);
		return;
	}
	xmlFreeDoc (doc);
	change_set_destroy (change_set);
}

void
amp_project_set_group_config (AmpProject *project, const gchar *group_id,
				 AmConfigMapping *new_config, GError **error)
{
	AmpNode *node;
	xmlDocPtr doc;
	GNode *g_node;
	GSList *change_set = NULL;
	
	g_return_if_fail (IS_AMP_PROJECT (project));
	g_return_if_fail (new_config != NULL);
	g_return_if_fail (error == NULL || *error == NULL);
	
	g_node = g_hash_table_lookup (project->groups, group_id);
	if (g_node == NULL) {
		error_set (error, PROJECT_ERROR_DOESNT_EXIST,
			   _("Group doesn't exist"));
		return;
	}
	node = AMP_NODE_DATA (g_node);
	
	/* Create the update xml */
	doc = xml_new_change_doc (project);
	if (!xml_write_set_config (project, doc, g_node, new_config)) {
		xmlFreeDoc (doc);
		return;
	}

	DEBUG ({
		xmlSetDocCompressMode (doc, 0);
		xmlSaveFile ("/tmp/set-config.xml", doc);
	});
	
	/* Update the project */
	if (!project_update (project, doc, &change_set, error)) {
		error_set (error, PROJECT_ERROR_PROJECT_MALFORMED,
			   _("Unable to update project"));
		xmlFreeDoc (doc);
		return;
	}
	xmlFreeDoc (doc);
	change_set_destroy (change_set);
}

void
amp_project_set_target_config (AmpProject *project,
				  const gchar *target_id,
				  AmConfigMapping *new_config,
				  GError **error)
{
	xmlDocPtr doc;
	GNode *g_node;
	GSList *change_set = NULL;
	
	g_return_if_fail (IS_AMP_PROJECT (project));
	g_return_if_fail (new_config != NULL);
	g_return_if_fail (error == NULL || *error == NULL);
	
	g_node = g_hash_table_lookup (project->targets, target_id);
	if (g_node == NULL) {
		error_set (error, PROJECT_ERROR_DOESNT_EXIST,
			   _("Target doesn't exist"));
	}
	
	/* Create the update xml */
	doc = xml_new_change_doc (project);
	if (!xml_write_set_config (project, doc, g_node, new_config)) {
		xmlFreeDoc (doc);
		return;
	}

	DEBUG ({
		xmlSetDocCompressMode (doc, 0);
		xmlSaveFile ("/tmp/set-config.xml", doc);
	});

	/* Update the project */
	if (!project_update (project, doc, &change_set, error)) {
		error_set (error, PROJECT_ERROR_PROJECT_MALFORMED,
			   _("Unable to update project"));
		xmlFreeDoc (doc);
		return;
	}
	xmlFreeDoc (doc);
	change_set_destroy (change_set);
}
#endif


/* GbfProject implementation
 *---------------------------------------------------------------------------*/

static gboolean 
impl_probe (GbfProject  *_project,
	    const gchar *uri,
	    GError     **error)
{
	GFile *file;
	gboolean probe;
	gboolean dir;
	
	g_return_val_if_fail (AMP_IS_PROJECT (_project), FALSE);

	file = g_file_new_for_path (uri);

	dir = (file_type (file, NULL) == G_FILE_TYPE_DIRECTORY);
	if (!dir)
	{
		g_set_error (error, GBF_PROJECT_ERROR, 
		             GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Project doesn't exist or invalid path"));
	}
	
	probe =  dir &&
			((file_type (file, "Makefile.am") == G_FILE_TYPE_REGULAR) ||
			 (file_type (file, "makefile.am") == G_FILE_TYPE_REGULAR) ||
			 (file_type (file, "GNUmakefile.am") == G_FILE_TYPE_REGULAR)) &&
			((file_type (file, "configure.ac") == G_FILE_TYPE_REGULAR) ||
			 (file_type (file, "configure.in") == G_FILE_TYPE_REGULAR));
	
	g_object_unref (file);
	
	return probe;
}

static void 
impl_load (GbfProject  *_project,
	   const gchar *uri,
	   GError     **error)
{
	AmpProject *project;
	
	g_return_if_fail (AMP_IS_PROJECT (_project));
	g_return_if_fail (uri != NULL);

	/* unload current project */
	project = AMP_PROJECT (_project);
	project_unload (project);

	/* some basic checks */
	if (!impl_probe (_project, uri, error))
	{
		g_set_error (error, GBF_PROJECT_ERROR, 
		             GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Project doesn't exist or invalid path"));
		
		return;
	}
	
	/* now try loading the project */
	project->root_file = g_file_new_for_path (uri);
	if (!project_reload (project, error))
	{
		g_object_unref (project->root_file);
		project->root_file = NULL;
	}
}

static void
impl_refresh (GbfProject *_project,
	      GError    **error)
{
	AmpProject *project;

	g_return_if_fail (AMP_IS_PROJECT (_project));

	project = AMP_PROJECT (_project);

	if (project_reload (project, error))
		g_signal_emit_by_name (G_OBJECT (project), "project-updated");
}

static GbfProjectCapabilities
impl_get_capabilities (GbfProject *_project, GError    **error)
{
	g_return_val_if_fail (AMP_IS_PROJECT (_project),
			      GBF_PROJECT_CAN_ADD_NONE);
	return (GBF_PROJECT_CAN_ADD_GROUP |
		GBF_PROJECT_CAN_ADD_TARGET |
		GBF_PROJECT_CAN_ADD_SOURCE);
}

static GbfProjectGroup * 
impl_get_group (GbfProject  *_project,
		const gchar *id,
		GError     **error)
{
	AmpProject *project;
	GbfProjectGroup *group;
	GNode *g_node;
	AmpNodeData *node;
	gchar *target_id;
	
	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);

	project = AMP_PROJECT (_project);
	if ((id == NULL) || (*id == '\0') || ((id[0] == '/') && (id[1] == '\0')))
	{
		gchar *id = g_file_get_uri (project->root_file);
		g_node = g_hash_table_lookup (project->groups, id);
		g_free (id);
	}
	else
	{
		g_node = g_hash_table_lookup (project->groups, id);
	}
	//g_message ("get node %p, id \"%s\" hash %p\n", g_node, id, project->groups);
	if (g_node == NULL) {
		error_set (error, GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Group doesn't exist"));
		return NULL;
	}
	group = g_new0 (GbfProjectGroup, 1);
	group->id = g_file_get_uri (AMP_GROUP_DATA(g_node)->file);
	group->name = g_file_get_basename (AMP_GROUP_DATA(g_node)->file);
	if (g_node->parent)
		group->parent_id = g_file_get_uri (AMP_GROUP_DATA (g_node->parent)->file);
	else
		group->parent_id = NULL;
	group->groups = NULL;
	group->targets = NULL;

	/* add subgroups and targets of the group */
	g_node = g_node_first_child (g_node);
	while (g_node) {
		node = AMP_NODE_DATA (g_node);
		switch (node->type) {
			case AMP_NODE_GROUP:
				group->groups = g_list_prepend (group->groups,
								g_file_get_uri (((AmpGroupData *)node)->file));
				break;
			case AMP_NODE_TARGET:
				target_id  = canonicalize_automake_variable (((AmpTargetData *)node)->name);
				group->targets = g_list_prepend (group->targets,
								 g_base64_encode ((guchar *)&g_node, sizeof (g_node)));
				g_free (target_id);
				break;
			default:
				break;
		}
		g_node = g_node_next_sibling (g_node);
	}
	group->groups = g_list_reverse (group->groups);
	group->targets = g_list_reverse (group->targets);
	
	return group;
}

static void
foreach_group (gpointer key, gpointer value, gpointer data)
{
	GList **groups = data;

	*groups = g_list_prepend (*groups, g_strdup (key));
}


static GList *
impl_get_all_groups (GbfProject *_project,
		     GError    **error)
{
	AmpProject *project;
	GList *groups = NULL;

	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);

	project = AMP_PROJECT (_project);
	g_hash_table_foreach (project->groups, foreach_group, &groups);

	return groups;
}

static GtkWidget *
impl_configure_new_group (GbfProject *_project,
			  GError    **error)
{
	UNIMPLEMENTED;

	return NULL;
}

static GtkWidget * 
impl_configure_group (GbfProject  *_project,
		      const gchar *id,
		      GError     **error)
{
	GtkWidget *wid = NULL;
	GError *err = NULL;
	
	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);
	g_return_val_if_fail (id != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	
	//wid = am_properties_get_group_widget (AMP_PROJECT (_project),
	//						  id, &err);
	if (err) {
		g_propagate_error (error, err);
	}
	return wid;
}

static gchar * 
impl_add_group (GbfProject  *_project,
		const gchar *parent_id,
		const gchar *name,
		GError     **error)
{
	AmpProject *project;
	AmpGroup *group;
	AmpGroup *last;
	AmpGroup *child;
	GNode **buffer;
	gsize dummy;
	AnjutaToken* token;
	
	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);
	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (parent_id != NULL, NULL);
	
	project = AMP_PROJECT (_project);
	buffer = (GNode **)g_base64_decode (parent_id, &dummy);
	group = (AmpTarget *)*buffer;
	g_free (buffer);

	if (AMP_NODE_DATA (group)->type != AMP_NODE_GROUP) return NULL;

	/* Add token */
	for (last = g_node_last_child (group); (last != NULL) && (AMP_NODE_DATA (last)->type != AMP_NODE_GROUP); last = g_node_prev_sibling (last));
	if (last == NULL)
	{
#if 0
		/* First child */
		AnjutaToken *tok;
		AnjutaToken *close_tok;
		AnjutaToken *eol_tok;
		gchar *target_var;
		gchar *canon_name;


		/* Search where the target is declared */
		tok = AMP_TARGET_DATA (target)->token;
		close_tok = anjuta_token_new_static (ANJUTA_TOKEN_CLOSE, NULL);
		eol_tok = anjuta_token_new_static (ANJUTA_TOKEN_EOL, NULL);
		anjuta_token_match (close_tok, ANJUTA_SEARCH_OVER, tok, &tok);
		anjuta_token_match (eol_tok, ANJUTA_SEARCH_OVER, tok, &tok);
		anjuta_token_free (close_tok);
		anjuta_token_free (eol_tok);

		/* Add a _SOURCES variable just after */
		canon_name = canonicalize_automake_variable (AMP_TARGET_DATA (target)->name);
		target_var = g_strconcat (canon_name,  "_SOURCES", NULL);
		g_free (canon_name);
		tok = anjuta_token_insert_after (tok, anjuta_token_new_string (ANJUTA_TOKEN_NAME | ANJUTA_TOKEN_ADDED, target_var));
		g_free (target_var);
		tok = anjuta_token_insert_after (tok, anjuta_token_new_static (ANJUTA_TOKEN_SPACE | ANJUTA_TOKEN_IRRELEVANT | ANJUTA_TOKEN_ADDED, " "));
		tok = anjuta_token_insert_after (tok, anjuta_token_new_static (ANJUTA_TOKEN_OPERATOR | ANJUTA_TOKEN_ADDED, "="));
		tok = anjuta_token_insert_after (tok, anjuta_token_new_static (ANJUTA_TOKEN_SPACE | ANJUTA_TOKEN_IRRELEVANT | ANJUTA_TOKEN_ADDED, " "));
		token = anjuta_token_new_static (ANJUTA_TOKEN_SPACE | ANJUTA_TOKEN_IRRELEVANT | ANJUTA_TOKEN_ADDED, " ");
		tok = anjuta_token_insert_after (tok, token);
		token = anjuta_token_new_string (ANJUTA_TOKEN_NAME | ANJUTA_TOKEN_ADDED, uri);
		tok = anjuta_token_insert_after (tok, token);
		tok = anjuta_token_insert_after (tok, anjuta_token_new_static (ANJUTA_TOKEN_EOL | ANJUTA_TOKEN_ADDED, "\n"));
#endif
	}
	else
	{
		token = anjuta_token_new_string (ANJUTA_TOKEN_NAME | ANJUTA_TOKEN_ADDED, name);
		add_list_item (AMP_SOURCE_DATA (last)->token, token);
	}
	
	/* Add source node in project tree */
	child = amp_group_new (AMP_GROUP_DATA (group)->file, name, TRUE);
	//AMP_GROUP_DATA(child)->token = token;
	g_node_append (group, child);

	return g_base64_encode ((guchar *)&child, sizeof (child));
	
#if 0
	AmpProject *project;
	GNode *g_node, *iter_node;
	//xmlDocPtr doc;
	GSList *change_set = NULL;
	//AmChange *change;
	gchar *retval;
	
	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);

	project = AMP_PROJECT (_project);
	
	/* Validate group name */
	if (!name || strlen (name) <= 0)
	{
		error_set (error, GBF_PROJECT_ERROR_VALIDATION_FAILED,
			   _("Please specify group name"));
		return NULL;
	}
	{
		gboolean failed = FALSE;
		const gchar *ptr = name;
		while (*ptr) {
			if (!isalnum (*ptr) && *ptr != '.' && *ptr != '-' &&
			    *ptr != '_')
				failed = TRUE;
			ptr++;
		}
		if (failed) {
			error_set (error, GBF_PROJECT_ERROR_VALIDATION_FAILED,
				   _("Group name can only contain alphanumeric, '_', '-' or '.' characters"));
			return NULL;
		}
	}
	
	/* find the parent group */
	g_node = g_hash_table_lookup (project->groups, parent_id);
	if (g_node == NULL) {
		error_set (error, GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Parent group doesn't exist"));
		return NULL;
	}

	/* check that the new group doesn't already exist */
	iter_node = g_node_first_child (g_node);
	while (iter_node) {
		AmpNode *node = AMP_NODE_DATA (iter_node);
		if (node->type == AMP_NODE_GROUP &&
		    !strcmp (node->name, name)) {
			error_set (error, GBF_PROJECT_ERROR_DOESNT_EXIST,
				   _("Group already exists"));
			return NULL;
		}
		iter_node = g_node_next_sibling (iter_node);
	}
			
	/* Create the update xml */
	/*doc = xml_new_change_doc (project);
	if (!xml_write_add_group (project, doc, g_node, name)) {
		error_set (error, PROJECT_ERROR_DOESNT_EXIST,
			   _("Group couldn't be created"));
		xmlFreeDoc (doc);
		return NULL;
	}

	DEBUG ({
		xmlSetDocCompressMode (doc, 0);
		xmlSaveFile ("/tmp/add-group.xml", doc);
	});*/

	/* Update the project */
	/*if (!project_update (project, doc, &change_set, error)) {
		error_set (error, PROJECT_ERROR_PROJECT_MALFORMED,
			   _("Unable to update project"));
		xmlFreeDoc (doc);
		return NULL;
	}
	xmlFreeDoc (doc);*/

	/* get newly created group id */
	retval = NULL;
	DEBUG (change_set_debug_print (change_set));
	change = change_set_find (change_set, AM_CHANGE_ADDED, AMP_NODE_GROUP);
	if (change) {
		retval = g_strdup (change->id);
	} else {
		error_set (error, PROJECT_ERROR_DOESNT_EXIST,
			   _("Group couldn't be created"));
	}
	change_set_destroy (change_set);
#endif
	
	return NULL;
}

static void 
impl_remove_group (GbfProject  *_project,
		   const gchar *id,
		   GError     **error)
{
#if 0
	AmpProject *project;
	GNode *g_node;
	//xmlDocPtr doc;
	GSList *change_set = NULL;
	
	g_return_if_fail (AMP_IS_PROJECT (_project));

	project = AMP_PROJECT (_project);
	
	/* Find the target */
	g_node = g_hash_table_lookup (project->groups, id);
	if (g_node == NULL) {
		error_set (error,GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Group doesn't exist"));
		return;
	}

	/* Create the update xml */
	doc = xml_new_change_doc (project);
	if (!xml_write_remove_group (project, doc, g_node)) {
		error_set (error, PROJECT_ERROR_DOESNT_EXIST,
			   _("Group couldn't be removed"));
		xmlFreeDoc (doc);
		return;
	}

	DEBUG ({
		xmlSetDocCompressMode (doc, 0);
		xmlSaveFile ("/tmp/remove-group.xml", doc);
	});

	/* Update the project */
	if (!project_update (project, doc, &change_set, error)) {
		error_set (error, PROJECT_ERROR_PROJECT_MALFORMED,
			   _("Unable to update project"));
	}
	xmlFreeDoc (doc);
	change_set_destroy (change_set);
#endif
}

static GbfProjectTarget * 
impl_get_target (GbfProject  *_project,
		 const gchar *id,
		 GError     **error)
{
	AmpProject *project;
	GbfProjectTarget *target;
	GNode *g_node;
	AmpSourceData *child_node;
	GNode **buffer;
	gsize dummy;

	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);

	project = AMP_PROJECT (_project);
	buffer = (GNode **)g_base64_decode (id, &dummy);
	g_node = *buffer;
	g_free (buffer);
	
	target = g_new0 (GbfProjectTarget, 1);
	target->group_id = g_strdup (AMP_NODE_DATA (g_node->parent)->id);
	target->id = g_strconcat (target->group_id, AMP_TARGET_DATA(g_node)->name, NULL);
	target->name = g_strdup (AMP_TARGET_DATA(g_node)->name);
	target->type = g_strdup (AMP_TARGET_DATA(g_node)->type);
	target->sources = NULL;

	/* add sources to the target */
	g_node = g_node_first_child (g_node);
	while (g_node) {
		child_node = AMP_SOURCE_DATA (g_node);
		switch (child_node->node.type) {
			case AMP_NODE_SOURCE:
				target->sources = g_list_prepend (target->sources,
								  g_base64_encode ((guchar *)&g_node, sizeof (g_node)));
				//DEBUG_PRINT ("sources id %p  \"%s\"", child_node, (const gchar *)target->sources->data);
				break;
			default:
				break;
		}
		g_node = g_node_next_sibling (g_node);
	}
	target->sources = g_list_reverse (target->sources);

	return target;
}

static gboolean
get_all_target (GNode *node, gpointer data)
{
	GList **targets = data;

	if (AMP_NODE_DATA (node)->type == AMP_NODE_TARGET)
	{
		gchar *id = g_base64_encode ((guchar *)&node, sizeof (node));
		*targets = g_list_prepend (*targets, id);
	}

	return FALSE;
}

static GList *
impl_get_all_targets (GbfProject *_project,
		      GError    **error)
{
	AmpProject *project;
	GList *targets = NULL;

	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);

	project = AMP_PROJECT (_project);
	g_node_traverse (project->root_node, G_IN_ORDER, G_TRAVERSE_ALL, -1, get_all_target, &targets);

	return targets;
}

static GtkWidget *
impl_configure_new_target (GbfProject *_project,
			   GError    **error)
{
	UNIMPLEMENTED;

	return NULL;
}

static GtkWidget * 
impl_configure_target (GbfProject  *_project,
		       const gchar *id,
		       GError     **error)
{
	GtkWidget *wid = NULL;
	GError *err = NULL;
	
	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);
	g_return_val_if_fail (id != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	
	//wid = am_properties_get_target_widget (AMP_PROJECT (_project),
	//					   id, &err);
	if (err) {
		g_propagate_error (error, err);
	}
	return wid;
}

static char * 
impl_add_target (GbfProject  *_project,
		 const gchar *group_id,
		 const gchar *name,
		 const gchar *type,
		 GError     **error)
{
#if 0	
	AmpProject *project;
	GNode *g_node, *iter_node;
	//xmlDocPtr doc;
	GSList *change_set = NULL;
	//AmChange *change;
	gchar *retval;

	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);
	g_return_val_if_fail (type != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	project = AMP_PROJECT (_project);
	
	/* find the group */
	g_node = g_hash_table_lookup (project->groups, group_id);
	if (g_node == NULL) {
		error_set (error, GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Group doesn't exist"));
		return NULL;
	}
	
	/* Validate target name */
	if (!name || strlen (name) <= 0)
	{
		error_set (error, GBF_PROJECT_ERROR_VALIDATION_FAILED,
			   _("Please specify target name"));
		return NULL;
	}
	{
		gboolean failed = FALSE;
		const gchar *ptr = name;
		while (*ptr) {
			if (!isalnum (*ptr) && *ptr != '.' && *ptr != '-' &&
			    *ptr != '_')
				failed = TRUE;
			ptr++;
		}
		if (failed) {
			error_set (error, GBF_PROJECT_ERROR_VALIDATION_FAILED,
				   _("Target name can only contain alphanumeric, '_', '-' or '.' characters"));
			return NULL;
		}
	}
	if (!strcmp (type, "shared_lib")) {
		if (strlen (name) < 7 ||
		    strncmp (name, "lib", strlen("lib")) != 0 ||
		    strcmp (&name[strlen(name) - 3], ".la") != 0) {
			error_set (error, GBF_PROJECT_ERROR_VALIDATION_FAILED,
				   _("Shared library target name must be of the form 'libxxx.la'"));
			return NULL;
		}
	}
	else if (!strcmp (type, "static_lib")) {
		if (strlen (name) < 6 ||
		    strncmp (name, "lib", strlen("lib")) != 0 ||
		    strcmp (&name[strlen(name) - 2], ".a") != 0) {
			error_set (error, GBF_PROJECT_ERROR_VALIDATION_FAILED,
				   _("Static library target name must be of the form 'libxxx.a'"));
			return NULL;
		}
	}
	
	/* check that the target doesn't already exist */
	iter_node = g_node_first_child (g_node);
	while (iter_node) {
		AmpNode *node = AMP_NODE (iter_node);
		if (node->type == AMP_NODE_TARGET &&
		    !strcmp (node->name, name)) {
			error_set (error, GBF_PROJECT_ERROR_ALREADY_EXISTS,
				   _("Target already exists"));
			return NULL;
		}
		iter_node = g_node_next_sibling (iter_node);
	}
			
	/* Create the update xml */
	doc = xml_new_change_doc (project);
	if (!xml_write_add_target (project, doc, g_node, name, type)) {
		error_set (error, PROJECT_ERROR_GENERAL_FAILURE,
			   _("General failure in target creation"));
		xmlFreeDoc (doc);
		return NULL;
	}

	DEBUG ({
		xmlSetDocCompressMode (doc, 0);
		xmlSaveFile ("/tmp/add-target.xml", doc);
	});

	/* Update the project */
	if (!project_update (project, doc, &change_set, error)) {
		error_set (error, PROJECT_ERROR_PROJECT_MALFORMED,
			   _("Unable to update project"));
		xmlFreeDoc (doc);
		return NULL;
	}
	xmlFreeDoc (doc);
	
	/* get newly created target id */
	retval = NULL;
	DEBUG (change_set_debug_print (change_set));
	change = change_set_find (change_set, AM_CHANGE_ADDED,
				  AMP_NODE_TARGET);
	if (change) {
		retval = g_strdup (change->id);
	} else {
		error_set (error, PROJECT_ERROR_DOESNT_EXIST,
			   _("Newly created target could not be identified"));
	}
	change_set_destroy (change_set);
	
	return retval;
#endif
	return NULL;
}

static void 
impl_remove_target (GbfProject  *_project,
		    const gchar *id,
		    GError     **error)
{
#if 0
	AmpProject *project;
	GNode *g_node;
	//xmlDocPtr doc;
	GSList *change_set = NULL;
	
	g_return_if_fail (AMP_IS_PROJECT (_project));

	project = AMP_PROJECT (_project);
	
	/* Find the target */
	g_node = g_hash_table_lookup (project->targets, id);
	if (g_node == NULL) {
		error_set (error, GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Target doesn't exist"));
		return;
	}

	/* Create the update xml */
	doc = xml_new_change_doc (project);
	if (!xml_write_remove_target (project, doc, g_node)) {
		error_set (error, PROJECT_ERROR_DOESNT_EXIST,
			   _("Target couldn't be removed"));
		xmlFreeDoc (doc);
		return;
	}

	DEBUG ({
		xmlSetDocCompressMode (doc, 0);
		xmlSaveFile ("/tmp/remove-target.xml", doc);
	});

	/* Update the project */
	if (!project_update (project, doc, &change_set, error)) {
		error_set (error, PROJECT_ERROR_PROJECT_MALFORMED,
			   _("Unable to update project"));
	}
	xmlFreeDoc (doc);
	change_set_destroy (change_set);
#endif
}

static const gchar * 
impl_name_for_type (GbfProject  *_project,
		    const gchar *type)
{
	if (!strcmp (type, "static_lib")) {
		return _("Static Library");
	} else if (!strcmp (type, "shared_lib")) {
		return _("Shared Library");
	} else if (!strcmp (type, "headers")) {
		return _("Header Files");
	} else if (!strcmp (type, "man")) {
		return _("Man Documentation");
	} else if (!strcmp (type, "data")) {
		return _("Miscellaneous Data");
	} else if (!strcmp (type, "program")) {
		return _("Program");
	} else if (!strcmp (type, "script")) {
		return _("Script");
	} else if (!strcmp (type, "info")) {
		return _("Info Documentation");
	} else if (!strcmp (type, "java")) {
		return _("Java Module");
	} else if (!strcmp (type, "python")) {
		return _("Python Module");
	} else {
		return _("Unknown");
	}
}

static const gchar * 
impl_mimetype_for_type (GbfProject  *_project,
			const gchar *type)
{
	if (!strcmp (type, "static_lib")) {
		return "application/x-archive";
	} else if (!strcmp (type, "shared_lib")) {
		return "application/x-sharedlib";
	} else if (!strcmp (type, "headers")) {
		return "text/x-chdr";
	} else if (!strcmp (type, "man")) {
		return "text/x-troff-man";
	} else if (!strcmp (type, "data")) {
		return "application/octet-stream";
	} else if (!strcmp (type, "program")) {
		return "application/x-executable";
	} else if (!strcmp (type, "script")) {
		return "text/x-shellscript";
	} else if (!strcmp (type, "info")) {
		return "application/x-tex-info";
	} else if (!strcmp (type, "java")) {
		return "application/x-java";
	} else if (!strcmp (type, "python")) {
		return "application/x-python";
	} else {
		return "text/plain";
	}
}

static gchar **
impl_get_types (GbfProject *_project)
{
	return g_strsplit ("program:shared_lib:static_lib:headers:"
			   "man:data:script:info:java:python", ":", 0);
}

static GbfProjectTargetSource * 
impl_get_source (GbfProject  *_project,
		 const gchar *id,
		 GError     **error)
{
	AmpProject *project;
	GbfProjectTargetSource *source;
	GNode *g_node;
	GNode **buffer;
	gsize dummy;

	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);

	project = AMP_PROJECT (_project);
	buffer = (GNode **)g_base64_decode (id, &dummy);
	g_node = *buffer;
	g_free (buffer);
	//DEBUG_PRINT (" get sources id \"%s\" %p", id, g_node);
	
	source = g_new0 (GbfProjectTargetSource, 1);
	source->id = g_file_get_uri (AMP_SOURCE_DATA (g_node)->file);
	source->source_uri = g_file_get_uri (AMP_SOURCE_DATA (g_node)->file);
	source->target_id = g_strdup (AMP_NODE_DATA (g_node->parent)->id);

	return source;
}

static gboolean
foreach_source (GNode *node, gpointer user_data)
{
	AmpSourceData *source = AMP_SOURCE_DATA (node);
	GHashTable *hash = (GHashTable *)user_data;
	
	if (source->node.type == AMP_NODE_SOURCE)
	{
		gchar *id = g_base64_encode ((guchar *)&node, sizeof (node));
		g_hash_table_insert (hash, source->file, id);
	}

	return FALSE;
}

/* List all sources, removing duplicate */
static GList *
impl_get_all_sources (GbfProject *_project,
		      GError    **error)
{
	AmpProject *project;
	GHashTable *hash;
	GList *sources;
	

	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);

	project = AMP_PROJECT (_project);

	hash = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, NULL, g_free);
	g_node_traverse (project->root_node, G_IN_ORDER, G_TRAVERSE_ALL, -1, foreach_source, hash);
	sources = g_hash_table_get_values (hash);
	g_hash_table_steal_all (hash);
	g_hash_table_destroy (hash);
	
	return sources;
}

static GtkWidget *
impl_configure_new_source (GbfProject *_project,
			   GError    **error)
{
	UNIMPLEMENTED;

	return NULL;
}

static GtkWidget * 
impl_configure_source (GbfProject  *_project,
		       const gchar *id,
		       GError     **error)
{
	UNIMPLEMENTED;

	return NULL;
}

/**
 * impl_add_source:
 * @project: 
 * @target_id: the target ID to where to add the source
 * @uri: an uri to the file, which can be absolute or relative to the target's group
 * @error: 
 * 
 * Add source implementation.  The uri must have the project root as its parent.
 * 
 * Return value: 
 **/
static gchar * 
impl_add_source (GbfProject  *_project,
		 const gchar *target_id,
		 const gchar *uri,
		 GError     **error)
{
	AmpProject *project;
	AmpGroup *group;
	AmpTarget *target;
	AmpSource *last;
	AmpSource *source;
	GNode **buffer;
	gsize dummy;
	AnjutaToken* token;
	
	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);
	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (target_id != NULL, NULL);
	
	project = AMP_PROJECT (_project);
	buffer = (GNode **)g_base64_decode (target_id, &dummy);
	target = (AmpTarget *)*buffer;
	g_free (buffer);

	if (AMP_NODE_DATA (target)->type != AMP_NODE_TARGET) return NULL;

	group = (AmpGroup *)(target->parent);

	/* Add token */
	last = g_node_last_child (target);
	if (last == NULL)
	{
		/* First child */
		AnjutaToken *tok;
		AnjutaToken *close_tok;
		AnjutaToken *eol_tok;
		gchar *target_var;
		gchar *canon_name;


		/* Search where the target is declared */
		tok = AMP_TARGET_DATA (target)->token;
		close_tok = anjuta_token_new_static (ANJUTA_TOKEN_CLOSE, NULL);
		eol_tok = anjuta_token_new_static (ANJUTA_TOKEN_EOL, NULL);
		anjuta_token_match (close_tok, ANJUTA_SEARCH_OVER, tok, &tok);
		anjuta_token_match (eol_tok, ANJUTA_SEARCH_OVER, tok, &tok);
		anjuta_token_free (close_tok);
		anjuta_token_free (eol_tok);

		/* Add a _SOURCES variable just after */
		canon_name = canonicalize_automake_variable (AMP_TARGET_DATA (target)->name);
		target_var = g_strconcat (canon_name,  "_SOURCES", NULL);
		g_free (canon_name);
		tok = anjuta_token_insert_after (tok, anjuta_token_new_string (ANJUTA_TOKEN_NAME | ANJUTA_TOKEN_ADDED, target_var));
		g_free (target_var);
		tok = anjuta_token_insert_after (tok, anjuta_token_new_static (ANJUTA_TOKEN_SPACE | ANJUTA_TOKEN_IRRELEVANT | ANJUTA_TOKEN_ADDED, " "));
		tok = anjuta_token_insert_after (tok, anjuta_token_new_static (ANJUTA_TOKEN_OPERATOR | ANJUTA_TOKEN_ADDED, "="));
		tok = anjuta_token_insert_after (tok, anjuta_token_new_static (ANJUTA_TOKEN_SPACE | ANJUTA_TOKEN_IRRELEVANT | ANJUTA_TOKEN_ADDED, " "));
		token = anjuta_token_new_static (ANJUTA_TOKEN_SPACE | ANJUTA_TOKEN_IRRELEVANT | ANJUTA_TOKEN_ADDED, " ");
		tok = anjuta_token_insert_after (tok, token);
		token = anjuta_token_new_string (ANJUTA_TOKEN_NAME | ANJUTA_TOKEN_ADDED, uri);
		tok = anjuta_token_insert_after (tok, token);
		tok = anjuta_token_insert_after (tok, anjuta_token_new_static (ANJUTA_TOKEN_EOL | ANJUTA_TOKEN_ADDED, "\n"));
	}
	else
	{
		token = anjuta_token_new_string (ANJUTA_TOKEN_NAME | ANJUTA_TOKEN_ADDED, uri);
		add_list_item (AMP_SOURCE_DATA (last)->token, token);
	}
	
	/* Add source node in project tree */
	source = amp_source_new (AMP_GROUP_DATA (group)->file, uri);
	AMP_SOURCE_DATA(source)->token = token;
	g_node_append (target, source);

	return g_base64_encode ((guchar *)&source, sizeof (source));
}

static void 
impl_remove_source (GbfProject  *_project,
		    const gchar *id,
		    GError     **error)
{
	AmpProject *project;
	GNode *g_node;
	AmpSourceData *source;
	GNode **buffer;
	gsize dummy;

	g_return_if_fail (AMP_IS_PROJECT (_project));

	project = AMP_PROJECT (_project);
	buffer = (GNode **)g_base64_decode (id, &dummy);
	g_node = *buffer;
	g_free (buffer);

	amp_dump_node (g_node);
	if (AMP_NODE_DATA (g_node)->type != AMP_NODE_SOURCE) return;
	
	source = AMP_SOURCE_DATA (g_node);

	remove_list_item (source->token);
	
	g_node_destroy (g_node);
}

static GtkWidget *
impl_configure (GbfProject *_project, GError **error)
{
	GtkWidget *wid = NULL;
	GError *err = NULL;
	
	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	
	//wid = am_properties_get_widget (AMP_PROJECT (_project), &err);
	if (err) {
		g_propagate_error (error, err);
	}
	return wid;
}

static GList *
impl_get_config_modules   (GbfProject *_project, GError **error)
{
	AmpProject *project;
	
	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);
	project = AMP_PROJECT (_project);

	return project->modules == NULL ? NULL : g_hash_table_get_keys (project->modules);
}

#if 0
static gboolean
package_is_valid (const gchar* package)
{
	const gchar* c = package;
	while (*c != '\0')
	{
		if (!g_ascii_isalnum (*c) &&
		    (*c != '_') && (*c != '-') && (*c != '.') && (*c != '+'))
		{
			return FALSE;
		}
		c++;
	}
	return TRUE;
}
#endif

static GList *
impl_get_config_packages  (GbfProject *project,
			   const gchar* module,
			   GError **error)
{
	AmpModule *mod;
	GList *packages = NULL;

	g_return_val_if_fail (project != NULL, NULL);
	g_return_val_if_fail (module != NULL, NULL);

	mod = g_hash_table_lookup (AMP_PROJECT (project)->modules, module);

	if (mod != NULL)
	{
		GList *node;

		for (node = mod->packages; node != NULL; node = g_list_next (node))
		{
			packages = g_list_prepend (packages, ((AmpPackage *)node->data)->name);
		}

		packages = g_list_reverse (packages);
	}

	return packages;
}

static void
amp_project_class_init (AmpProjectClass *klass)
{
	GObjectClass *object_class;
	GbfProjectClass *project_class;

	object_class = G_OBJECT_CLASS (klass);
	project_class = GBF_PROJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = amp_project_dispose;
	object_class->get_property = amp_project_get_property;

	project_class->load = impl_load;
	project_class->probe = impl_probe;
	project_class->refresh = impl_refresh;
	project_class->get_capabilities = impl_get_capabilities;

	project_class->add_group = impl_add_group;
	project_class->remove_group = impl_remove_group;
	project_class->get_group = impl_get_group;
	project_class->get_all_groups = impl_get_all_groups;
	project_class->configure_group = impl_configure_group;
	project_class->configure_new_group = impl_configure_new_group;

	project_class->add_target = impl_add_target;
	project_class->remove_target = impl_remove_target;
	project_class->get_target = impl_get_target;
	project_class->get_all_targets = impl_get_all_targets;
	project_class->configure_target = impl_configure_target;
	project_class->configure_new_target = impl_configure_new_target;

	project_class->add_source = impl_add_source;
	project_class->remove_source = impl_remove_source;
	project_class->get_source = impl_get_source;
	project_class->get_all_sources = impl_get_all_sources;
	project_class->configure_source = impl_configure_source;
	project_class->configure_new_source = impl_configure_new_source;

	project_class->configure = impl_configure;
	project_class->get_config_modules = impl_get_config_modules;
	project_class->get_config_packages = impl_get_config_packages;
	
	project_class->name_for_type = impl_name_for_type;
	project_class->mimetype_for_type = impl_mimetype_for_type;
	project_class->get_types = impl_get_types;
	
	/* default signal handlers */
	project_class->project_updated = NULL;

	/* FIXME: shouldn't we use '_' instead of '-' ? */
	g_object_class_install_property 
		(object_class, PROP_PROJECT_DIR,
		 g_param_spec_string ("project-dir", 
				      _("Project directory"),
				      _("Project directory"),
				      NULL,
				      G_PARAM_READABLE));
}

static void
amp_project_instance_init (AmpProject *project)
{
	project_data_init (project);
}

static void
amp_project_dispose (GObject *object)
{
	g_return_if_fail (AMP_IS_PROJECT (object));

	project_unload (AMP_PROJECT (object));

	G_OBJECT_CLASS (parent_class)->dispose (object);	
}

static void
amp_project_get_property (GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
	AmpProject *project = AMP_PROJECT (object);
	gchar *uri;

	switch (prop_id) {
		case PROP_PROJECT_DIR:
			uri = g_file_get_uri (project->root_file);
			g_value_take_string (value, uri);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

/* Public functions
 *---------------------------------------------------------------------------*/

gboolean
amp_project_save (AmpProject *project, GError **error)
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
amp_project_move (AmpProject *project, const gchar *path)
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
		AmpGroup *group = (AmpGroup *)value;
		
		relative = get_relative_path (old_root_file, AMP_GROUP_DATA (group)->file);
		new_file = g_file_resolve_relative_path (project->root_file, relative);
		g_free (relative);
		g_object_unref (AMP_GROUP_DATA (group)->file);
		AMP_GROUP_DATA (group)->file = new_file;

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

gchar *
amp_project_get_node_id (AmpProject *project, const gchar *path)
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

	return g_base64_encode ((guchar *)&node, sizeof (node));
}

gchar *
amp_project_get_uri (AmpProject *project)
{

	g_return_val_if_fail (project != NULL, NULL);

	return project->root_file != NULL ? g_file_get_uri (project->root_file) : NULL;
}


GbfProject *
amp_project_new (void)
{
	return GBF_PROJECT (g_object_new (AMP_TYPE_PROJECT, NULL));
}


GBF_BACKEND_BOILERPLATE (AmpProject, amp_project);
