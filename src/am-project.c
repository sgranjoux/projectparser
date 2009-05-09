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

typedef struct _AmpNode           	AmpNode;

typedef enum {
	AMP_NODE_GROUP,
	AMP_NODE_TARGET,
	AMP_NODE_SOURCE
} AmpNodeType;
	
struct _AmpNode {
	AmpNodeType       type;
	gchar              *id;        /* unique id among nodes of the same type */
	gchar              *name;      /* user visible string */
	//GbfAmConfigMapping *config;
	gchar              *uri;       /* groups: path to Makefile.am;
					  targets: NULL;
					  sources: file uri */
	gchar              *detail;    /* groups: NULL;
					  targets: target type;
					  sources: NULL or target dependency for built sources */
};

struct _AmpProject {
	GbfProject          parent;

	/* uri of the project; this can be a full uri, even though we
	 * can only work with native local files */
	GFile			*root_file;

	/* project data */
	AnjutaTokenFile		*configure_file;		/* configure.in file */
	
	GNode              *root_node;         	/* tree containing project data;
								 * each GNode's data is a
								 * GbfAmpNode, and the root of
								 * the tree is the root group. */

	/* shortcut hash tables, mapping id -> GNode from the tree above */
	GHashTable         *groups;
	GHashTable         *targets;
	GHashTable         *sources;

	GHashTable	*modules;
	
	/* project files monitors */
	GHashTable         *monitors;
};

struct _AmpProjectClass {
	GbfProjectClass parent_class;
};

/* convenient shortcut macro the get the GbfAmpNode from a GNode */
#define AMP_NODE(node)  ((node) != NULL ? (AmpNode *)((node)->data) : NULL)
#define AMP_GROUP_NODE(node)  ((node) != NULL ? (AmpGroup *)((node)->data) : NULL)
#define AMP_TARGET_NODE(node)  ((node) != NULL ? (AmpTarget *)((node)->data) : NULL)
#define AMP_SOURCE_NODE(node)  ((node) != NULL ? (AmpSource *)((node)->data) : NULL)


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

typedef struct _AmpGroup AmpGroup;

struct _AmpGroup {
	AmpNode node;
    gchar *makefile;
	GFile *file;
	AnjutaTokenFile *tfile;
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
	AM_TARGET_MAN_SECTION = 31 << 7,
};

typedef struct _AmpTarget AmpTarget;

struct _AmpTarget {
	AmpNode node;
	gchar *name;
	gchar *type;
	gchar *install;
	gint flags;
};

typedef struct _AmpSource AmpSource;

struct _AmpSource {
	AmpNode node;
	GFile *file;
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

static gboolean        uri_is_equal                 (const gchar       *uri1,
						     const gchar       *uri2);
static gboolean        uri_is_parent                (const gchar       *parent_uri,
						     const gchar       *child_uri);
static gboolean        uri_is_local_path            (const gchar       *uri);
static gchar          *uri_normalize                (const gchar       *path_or_uri,
						     const gchar       *base_uri);

static gboolean        project_reload               (AmpProject      *project,
						     GError           **err);

static void            amp_node_free             (AmpNode         *node);
static GNode          *project_node_new             (AmpNodeType      type);
static void            project_node_destroy         (AmpProject      *project,
						     GNode             *g_node);
static void            project_data_unload         (AmpProject      *project);
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
	gchar *ptr;
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

	g_message ("match : ==%s==", g_match_info_fetch (match_info, 0));
	g_message ("match 1: ==%s==", g_match_info_fetch (match_info, 1));
	g_message ("match 2: ==%s==", g_match_info_fetch (match_info, 2));
	g_message ("match 3: ==%s==", g_match_info_fetch (match_info, 3));
	g_message ("match 4: ==%s==", g_match_info_fetch (match_info, 4));
	g_message ("match 5: ==%s==", g_match_info_fetch (match_info, 5));
	
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
amp_group_set_makefile (AmpGroup *group, const gchar *makefile)
{
	g_return_if_fail (group != NULL);
	
	g_free (group->makefile);
	group->makefile = makefile != NULL ? g_strdup (makefile) : NULL;
}

static AmpGroup*
amp_group_new (GFile *file, const gchar *makefile)
{
    AmpGroup *group = NULL;

	g_return_val_if_fail (file != NULL, NULL);
	
	group = g_slice_new0(AmpGroup); 
	group->node.type = AMP_NODE_GROUP;
	group->file = g_object_ref (file);
	group->makefile = makefile != NULL ? g_strdup (makefile) : NULL;

    return group;
}

static void
amp_group_free (AmpGroup *group)
{
	if (group->tfile) anjuta_token_file_free (group->tfile);
	g_object_unref (group->file);
    g_slice_free (AmpGroup, group);
}

/* Target objects
 *---------------------------------------------------------------------------*/

static AmpTarget*
amp_target_new (const gchar *name, const gchar *type, const gchar *install, gint flags)
{
    AmpTarget *target = NULL;

	target = g_slice_new0(AmpTarget); 
	target->node.type = AMP_NODE_TARGET;
	target->name = g_strdup (name);
	target->type = g_strdup (type);
	target->install = g_strdup (install);
	target->flags = flags;

    return target;
}

static void
amp_target_free (AmpTarget *target)
{
    g_free (target->name);
    g_free (target->type);
    g_free (target->install);
    g_slice_free (AmpTarget, target);
}

/* Source objects
 *---------------------------------------------------------------------------*/

static AmpSource*
amp_source_new (GFile *parent, const gchar *name)
{
    AmpSource *source = NULL;

	source = g_slice_new0(AmpSource); 
	source->node.type = AMP_NODE_SOURCE;
	source->file = g_file_get_child (parent, name);

    return source;
}

static void
amp_source_free (AmpSource *source)
{
	g_message ("free %p", source);
	g_message ("free source %s", g_file_get_uri (source->file));
    g_object_unref (source->file);
    g_slice_free (AmpSource, source);
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
	GNode *group_node = value;
	AmpProject *project = user_data;

	monitor_add (project, AMP_GROUP_NODE(group_node)->file);
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
amp_node_free (AmpNode *node)
{
	if (node) {
		g_free (node->id);
		g_free (node->name);
		g_free (node->detail);
		g_free (node->uri);
		//am_config_mapping_destroy (node->config);

		g_free (node);
	}
}

static GNode *
project_node_new (AmpNodeType type)
{
	AmpNode *node;

	node = g_new0 (AmpNode, 1);
	node->type = type;

	return g_node_new (node);
}

static gboolean 
foreach_node_destroy (GNode    *g_node,
		      gpointer  data)
{
	switch (AMP_NODE (g_node)->type) {
		case AMP_NODE_GROUP:
			//g_hash_table_remove (project->groups, g_file_get_uri (AMP_GROUP_NODE (g_node)->file));
			amp_group_free (AMP_GROUP_NODE (g_node));
			break;
		case AMP_NODE_TARGET:
			//g_hash_table_remove (project->targets, AMP_NODE (g_node)->id);
			amp_target_free (AMP_TARGET_NODE (g_node));
			break;
		case AMP_NODE_SOURCE:
			//g_hash_table_remove (project->sources, AMP_NODE (g_node)->id);
			amp_source_free (AMP_SOURCE_NODE (g_node));
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
				 G_IN_ORDER, G_TRAVERSE_ALL, -1,
				 foreach_node_destroy, project);

		/* now destroy the tree itself */
		g_node_destroy (g_node);
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

	if (project->configure_file)	anjuta_token_file_free (project->configure_file);
	project->configure_file = NULL;

	if (project->root_file) g_object_unref (project->root_file);
	project->root_file = NULL;
	
	/* shortcut hash tables */
	if (project->groups) g_hash_table_destroy (project->groups);
	if (project->targets) g_hash_table_destroy (project->targets);
	if (project->sources) g_hash_table_destroy (project->sources);
	project->groups = NULL;
	project->targets = NULL;
	project->sources = NULL;

	amp_project_free_module_hash (project);
}

static void
project_reload_packages   (AmpProject *project)
{
	AnjutaToken *pkg_check_tok;
	AnjutaToken *next_tok;
	AnjutaToken *close_tok;
	AnjutaToken *open_tok;
	AnjutaToken *sequence;
	
	pkg_check_tok = anjuta_token_new_static (ANJUTA_TOKEN_KEYWORD | ANJUTA_TOKEN_SIGNIFICANT, "PKG_CHECK_MODULES(");
	next_tok = anjuta_token_new_static (ANJUTA_TOKEN_NEXT, NULL);
	open_tok = anjuta_token_new_static (ANJUTA_TOKEN_OPEN, NULL);
	close_tok = anjuta_token_new_static (ANJUTA_TOKEN_CLOSE, NULL);


	g_message("reload package");
	
    sequence = anjuta_token_file_first (project->configure_file);
	for (;;)
	{
		AnjutaToken *module;
		AnjutaToken *arg;
		AnjutaToken *next;
		gchar *value;
		AmpModule *mod;
		AmpPackage *pack;
		gchar *compare;
		
		if (!anjuta_token_match (pkg_check_tok, ANJUTA_SEARCH_OVER, sequence, &module)) break;

		arg = anjuta_token_next (module);
		anjuta_token_match (next_tok, ANJUTA_SEARCH_OVER, arg, &next);
		value = anjuta_token_evaluate (arg, next);
		mod = amp_module_new (arg);
		mod->packages = NULL;
		g_hash_table_insert (project->modules, value, mod);

		//g_message ("new module %s", value);

		// Dump a few following tokens
		gint i;
		AnjutaToken *tok;

		tok = arg;
		for (i = 0; i < 50; i++)
		{
			//g_message ("token %p %x flag %x value %s", tok, anjuta_token_get_type (tok), anjuta_token_get_flags (tok), anjuta_token_evaluate (tok, tok));
			tok = anjuta_token_next (tok);
		}
		

		//g_message ("next token %p %s", next, anjuta_token_evaluate (next, next));
		arg = anjuta_token_next (next);
		//g_message ("following token %p %s", arg, anjuta_token_evaluate (arg, arg));
		anjuta_token_match (open_tok, ANJUTA_SEARCH_INTO, arg, &next);
		//g_message ("open token %p %s", next, anjuta_token_evaluate (next, next));
		arg = next;
		pack = NULL;
		compare = NULL;
		for (;;)
		{
			gchar *value;
			
			anjuta_token_match (next_tok, ANJUTA_SEARCH_INTO, arg, &next);

			//g_message ("next token %p", next);
			if (next == NULL) break;
			value = anjuta_token_evaluate (arg, next);
			//g_message ("next value %s", value);

			if ((pack != NULL) && (compare != NULL))
			{
				amp_package_set_version (pack, compare, value);
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
				compare = NULL;
			}
			
			if (anjuta_token_get_flags (next) & ANJUTA_TOKEN_CLOSE) break;
			arg = anjuta_token_next (next);
		}
		mod->packages = g_list_reverse (mod->packages);

		anjuta_token_match (close_tok, ANJUTA_SEARCH_OVER, anjuta_token_next (arg), &next);
		//g_message ("next %p", next);
		if (next == NULL) break;
		sequence = anjuta_token_next (next);
	}
	anjuta_token_free (pkg_check_tok);
	anjuta_token_free (next_tok);
	anjuta_token_free (close_tok);
}

static void
amp_project_add_group (AmpProject *project, GFile *file, const gchar *makefile)
{
	GList *files = NULL;
	GList *item;
	GNode *parent;
	GNode *child;
	

	/* Get all parents */
	while (file)
	{
		files = g_list_prepend (files, file);
		if (g_file_equal (file, project->root_file)) break;		
		file = g_file_get_parent (file);
	}

	/* Add all non existing parents in tree and the new group */
	if (project->root_node == NULL)
	{
		project->root_node = g_node_new (amp_group_new (project->root_file, NULL));
		g_hash_table_insert (project->groups, g_strdup (""), project->root_node);
		//g_message ("insert node %p, id \"%s\" hash %p\n", project->root_node, "", project->groups);
	}
	//g_message ("project->root_node %p", project->root_node);
	child = project->root_node;
	for (item = g_list_first (files); item != NULL; item = g_list_next (item))
	{
		GFile *file = (GFile *)(item->data);
		AmpGroup *group;

		/* Search for an already existing group with the same path */
		for (; child != NULL; child = g_node_next_sibling (child))
		{
			group = (AmpGroup *)child->data;
			if ((group->node.type == AMP_NODE_GROUP) && g_file_equal (group->file, file)) break;
			//g_message ("group %s file %s", g_file_get_uri (group->file), g_file_get_uri (file));
		}
		
		if (child == NULL)
		{
			if (g_list_next (item) != NULL)
			{
				/* Add parent */
				child = g_node_append_data (parent, amp_group_new (file, NULL));
				g_hash_table_insert (project->groups, g_file_get_uri (file), child);
			}
			else
			{
				/* Add makefile */
				child = g_node_append_data (parent, amp_group_new (file, makefile));
				g_hash_table_insert (project->groups, g_file_get_uri (file), child);
			}
		}
		else
		{
			/* Update makefile, the group could have been created without a
			 * makefile if another child group appears first */
			if (g_list_next (item) == NULL)
			{
				amp_group_set_makefile (group, makefile);
			}
		}

		parent = child;
		child = g_node_first_child (parent);
	}
	
}

/* Add a GFile in the list for each makefile in the token list */
AnjutaToken *
amp_project_add_config_files (AmpProject *project, AnjutaToken *list, GList **config_files)
{
	AnjutaToken* arg;
	AnjutaToken* next;
	AnjutaToken *next_tok;
	AnjutaToken *open_tok;

	//g_message ("add config project %p root file %p", project, project->root_file);	
	next_tok = anjuta_token_new_static (ANJUTA_TOKEN_NEXT, NULL);
	open_tok = anjuta_token_new_static (ANJUTA_TOKEN_OPEN, NULL);
	
	for (anjuta_token_match (open_tok, ANJUTA_SEARCH_INTO, list, &arg); arg != NULL; arg = anjuta_token_next (next))
	{
		gchar *value;
			
		anjuta_token_match (next_tok, ANJUTA_SEARCH_INTO, arg, &next);
		if (next == NULL) break;
		
		value = anjuta_token_evaluate (arg, next);
		//g_message ("add config %s", value);
		*config_files = g_list_prepend (*config_files, amp_config_file_new (value, project->root_file));
		g_free (value);

		if (anjuta_token_get_flags (next) & ANJUTA_TOKEN_CLOSE) break;
	}

	anjuta_token_free (open_tok);
	anjuta_token_free (next_tok);
	
	return next;
}							   
                           
static GList *
project_list_config_files (AmpProject *project)
{
	AnjutaToken *config_files_tok;
	AnjutaToken *close_tok;
	AnjutaToken *sequence;
	GList *config_files = NULL;
	
	//g_message ("load config project %p root file %p", project, project->root_file);	
	/* Search the new AC_CONFIG_FILES macro */
	config_files_tok = anjuta_token_new_static (ANJUTA_TOKEN_KEYWORD | ANJUTA_TOKEN_SIGNIFICANT, "AC_CONFIG_FILES(");
	close_tok = anjuta_token_new_static (ANJUTA_TOKEN_CLOSE, NULL);

    sequence = anjuta_token_file_first (project->configure_file);

	for (anjuta_token_match (config_files_tok, ANJUTA_SEARCH_OVER, sequence, &sequence); sequence != NULL; sequence = anjuta_token_next (sequence))
	{
		sequence = amp_project_add_config_files (project, anjuta_token_next (sequence), &config_files);
		
		anjuta_token_match (close_tok, ANJUTA_SEARCH_OVER, anjuta_token_next (sequence), &sequence);
		if (sequence == NULL) break;
	}
	
	if (config_files == NULL)
	{
		/* Search the old AC_OUTPUT macro */
	    anjuta_token_free(config_files_tok);
	    config_files_tok = anjuta_token_new_static (ANJUTA_TOKEN_KEYWORD | ANJUTA_TOKEN_SIGNIFICANT, "AC_OUTPUT(");
		
	    sequence = anjuta_token_file_first (project->configure_file);

		for (anjuta_token_match (config_files_tok, ANJUTA_SEARCH_OVER, sequence, &sequence); sequence != NULL; sequence = anjuta_token_next (sequence))
		{	
			sequence = amp_project_add_config_files (project, anjuta_token_next (sequence), &config_files);
		
			anjuta_token_match (close_tok, ANJUTA_SEARCH_OVER, anjuta_token_next (sequence), &sequence);
			if (sequence == NULL) break;
		}
	}
	
	
	anjuta_token_free (config_files_tok);
	anjuta_token_free (close_tok);

	return config_files;
}

static void
project_reload_group (AmpProject *project, AmpGroup *group)
{
	AmpAmScanner *scanner;
	AnjutaTokenFile *makefile;
	GFile *file;
	gchar *makefile_am;
	
	makefile_am = g_strconcat (group->makefile, ".am", NULL);
	file = g_file_get_child (group->file, makefile_am);
	g_free (makefile_am);

	g_message ("Parse %s", g_file_get_uri (file));
	/* Parse makefile.am */	
	makefile = anjuta_token_file_new (file);
	scanner = amp_am_scanner_new ();
	amp_am_scanner_parse (scanner, makefile);
	amp_am_scanner_free (scanner);
	anjuta_token_file_free (file);
}

static void
foreach_group_reload (gpointer key, GNode *node, AmpProject *project)
{
	AmpGroup *group = AMP_GROUP_NODE (node);

	if (group->makefile != NULL)
	{
		project_reload_group (project, group);
	}
}

static AnjutaToken*
project_load_target (AmpProject *project, AnjutaToken *start, GNode *parent, GHashTable *orphan_sources)
{
	AnjutaToken *open_tok;
	AnjutaToken *next_tok;
	AnjutaToken *next = NULL;
	AnjutaToken *arg;
	AmpGroup *group = (AmpGroup *)parent->data;
	gchar *group_id = g_file_get_uri (group->file);
	const gchar *type;
	gchar *install;
	gint flags;

	open_tok = anjuta_token_new_static (ANJUTA_TOKEN_OPEN, NULL);
	next_tok = anjuta_token_new_static (ANJUTA_TOKEN_NEXT, NULL);

	if (anjuta_token_match (open_tok, ANJUTA_SEARCH_INTO, start, &next))
	{
		gchar *name;
		gchar *primary;

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
		}

		name = anjuta_token_evaluate (start, anjuta_token_previous (next));
		g_message ("group %s name %s", group_id, name);
		split_automake_variable (name, &flags, &install, NULL);
		g_free (name);
	}

	g_message("open token %s", anjuta_token_get_value (next));
	for (arg = next; arg != NULL; arg = anjuta_token_next (next))
	{
		gchar *value;
		gchar *target_id;
		gchar *canon_id;
		AmpTarget *target;
		GNode *node;
		gboolean last;
		GList *sources;
		gchar *orig_key;

		last = !anjuta_token_match (next_tok, ANJUTA_SEARCH_INTO, arg, &next);
		g_message("next token %s", anjuta_token_get_value (next));
		
		value = anjuta_token_evaluate (arg, next);
		canon_id = canonicalize_automake_variable (value);		
		
		/* Check if target already exists */
		target_id = g_strconcat (group_id, ":", canon_id, NULL);
		if (g_hash_table_lookup (project->targets, target_id) != NULL)
		{
			g_free (value);
			g_free (target_id);
			continue;
		}

		/* Create target */
		target = amp_target_new (value, type, install, flags);
		node = g_node_new (target);
		g_hash_table_insert (project->targets, target_id, node);
		g_node_append (parent, node);

		/* Check if there are source availables */
		if (g_hash_table_lookup_extended (orphan_sources, target_id, &orig_key, &sources))
		{
			GList *src;
			g_hash_table_steal (orphan_sources, target_id);
			for (src = sources; src != NULL; src = g_list_next (src))
			{
				GNode *s_node = g_node_new (src->data);

				g_hash_table_insert (project->sources, g_file_get_uri (((AmpSource *)sources->data)->file), s_node);
				g_node_prepend (node, s_node);
			}
			g_free (orig_key);
			g_list_free (sources);
		}
	
		g_free (value);
		
		if (last) break;
	}

	g_free (group_id);
	anjuta_token_free (open_tok);
	anjuta_token_free (next_tok);

	return next;
}

static AnjutaToken*
project_load_sources (AmpProject *project, AnjutaToken *start, GNode *parent, GHashTable *orphan_sources)
{
	AnjutaToken *open_tok;
	AnjutaToken *next_tok;
	AnjutaToken *next = NULL;
	AnjutaToken *arg;
	AmpGroup *group = (AmpGroup *)parent->data;
	GFile *parent_file = g_object_ref (group->file);
	gchar *group_id = g_file_get_uri (group->file);
	gchar *target_id = NULL;
	GList *orphan = NULL;
	gint flags;
			gchar *orig_key;
			GList *orig_sources;

	
	open_tok = anjuta_token_new_static (ANJUTA_TOKEN_OPEN, NULL);
	next_tok = anjuta_token_new_static (ANJUTA_TOKEN_NEXT, NULL);

	if (anjuta_token_match (open_tok, ANJUTA_SEARCH_INTO, start, &next))
	{
		gchar *name;

		name = anjuta_token_evaluate (start, anjuta_token_previous (next));
		if (name)
		{
			gchar *end = strrchr (name, '_');
			if (end)
			{
				*end = '\0';
				target_id = g_strconcat (group_id, ":", name, NULL);
			}
		}
		g_free (name);
	}

	if (target_id)
	{
		parent = g_hash_table_lookup (project->targets, target_id) ;
		
		g_message("open token %s", anjuta_token_get_value (next));
		for (arg = next; arg != NULL; arg = anjuta_token_next (next))
		{
			gchar *value;
			AmpSource *source;
			gboolean last;

			last = !anjuta_token_match (next_tok, ANJUTA_SEARCH_INTO, arg, &next);
			g_message("next token %s", anjuta_token_get_value (next));
		
			value = anjuta_token_evaluate (arg, next);
		
			/* Create source */
			source = amp_source_new (parent_file, value);

			if (parent == NULL)
			{
				/* Add in orphan list */
				g_message ("add orphan %p", source);
				orphan = g_list_prepend (orphan, source);
			}
			else
			{
				/* Add as target child */
				GNode *node = g_node_new (source);
				g_hash_table_insert (project->sources, g_file_get_uri (source->file), node);
				g_node_append (parent, node);
			}

			g_free (value);
			
			if (last) break;
		}
		
		if (parent == NULL)
		{
			gchar *orig_key;
			GList *orig_sources;

			g_message ("orphan_sources %p", orphan_sources);
			if (g_hash_table_lookup_extended (orphan_sources, target_id, &orig_key, &orig_sources))
			{
				g_message ("lookup find %s key %s orphan %p",  target_id, orig_key, orig_sources);
				g_hash_table_steal (orphan_sources, target_id);
				orphan = g_list_concat (orphan, orig_sources);	
				g_free (orig_key);
			}
			g_hash_table_insert (orphan_sources, target_id, orphan);
			g_message ("insert %s %p", target_id, orphan);
		}
		else
		{
			g_free (target_id);
		}
	}

	g_object_unref (parent_file);
	anjuta_token_free (open_tok);
	anjuta_token_free (next_tok);

	return next;
}

static void project_load_makefile (AmpProject *project, GFile *file, GNode *parent, GList **config_files);

static AnjutaToken*
project_load_subdirs (AmpProject *project, AnjutaToken *start, GFile *file, GNode *node, GList **config_files)
{
	AnjutaToken *open_tok;
	AnjutaToken *next_tok;
	AnjutaToken *next;
	AnjutaToken *arg;

	//g_message ("find subdirs");
	open_tok = anjuta_token_new_static (ANJUTA_TOKEN_OPEN, NULL);
	next_tok = anjuta_token_new_static (ANJUTA_TOKEN_NEXT, NULL);

	for (anjuta_token_match (open_tok, ANJUTA_SEARCH_INTO, start, &arg); arg != NULL; arg = anjuta_token_next (next))
	{
		gchar *value;
		GFile *subdir;
		gboolean last;
			
		last = !anjuta_token_match (next_tok, ANJUTA_SEARCH_INTO, arg, &next);
		value = anjuta_token_evaluate (arg, next);
		subdir = g_file_resolve_relative_path (file, value);
		project_load_makefile (project, subdir, node, config_files);
		g_object_unref (subdir);
		g_free (value);
		
		if (last) break;
	}

	anjuta_token_free (open_tok);
	anjuta_token_free (next_tok);

	return next;
}

static void
free_source_list (GList *source_list)
{
	g_list_foreach (source_list, amp_source_free, NULL);
	g_list_free (source_list);
}

static void
project_load_makefile (AmpProject *project, GFile *file, GNode *parent, GList **config_files)
{
	GHashTable *orphan_sources = NULL;
	gchar *filename = NULL;
	AmpAmScanner *scanner;
	AmpGroup *group;
	GNode *node;
	GList *elem;
	AnjutaToken *significant_tok;
	AnjutaToken *arg;
	GFile *makefile = NULL;
	gchar *group_id;

	/* Check if group already exists */
	/* some Anjuta Makefile.am contains . by example */
	group_id = g_file_get_uri (file);
	if (g_hash_table_lookup (project->groups, group_id) != NULL) return;
	
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
	if (filename == NULL) return;

	/* Create group */
	group = amp_group_new (file, filename);
	node = g_node_new (group);
	g_hash_table_insert (project->groups, group_id, node);
	if (parent == NULL)
	{
		project->root_node = node;
	}
	else
	{
		g_node_append (parent, node);
	}

	/* Parse makefile.am */	
	g_message ("Parse: %s", g_file_get_uri (file));
	makefile = g_file_get_child (file, filename);
	group->tfile = anjuta_token_file_new (makefile);
	scanner = amp_am_scanner_new ();
	amp_am_scanner_parse (scanner, group->tfile);
	amp_am_scanner_free (scanner);
	g_object_unref (makefile);
	g_free (filename);

	/* Find significant token */
	significant_tok = anjuta_token_new_static (ANJUTA_TOKEN_SIGNIFICANT, NULL);
	
	arg = anjuta_token_file_first (group->tfile);
	//anjuta_token_dump_range (arg, NULL);

	/* Create hash table for sources list */
	orphan_sources = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify)g_free, (GDestroyNotify)free_source_list);
	g_message ("orphan_sources create %p %x", orphan_sources, *((int *)orphan_sources));
	
	for (anjuta_token_match (significant_tok, ANJUTA_SEARCH_OVER, arg, &arg); arg != NULL; arg = anjuta_token_next (arg))
	{
		g_message("significant %s", anjuta_token_get_value (arg));
		switch (anjuta_token_get_type (arg))
		{
		case AM_TOKEN_SUBDIRS:
				arg = project_load_subdirs (project, arg, file, node, config_files);
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
				arg = project_load_target (project, arg, node, orphan_sources);
				break;
		case AM_TOKEN__SOURCES:
				arg = project_load_sources (project, arg, node, orphan_sources);
				break;
		}
	}

	/* Free unused sources files */
	for (elem = g_hash_table_get_keys (orphan_sources); elem != NULL; elem = g_list_next (elem))
	{
		g_message ("free key %s", elem->data);
	}
	g_hash_table_destroy (orphan_sources);
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
	g_message ("reload project %p root file %p", project, project->root_file);

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
	g_object_unref (configure_file);
	scanner = amp_ac_scanner_new ();
	ok = amp_ac_scanner_parse (scanner, project->configure_file);
	amp_ac_scanner_free (scanner);
		     
	monitors_setup (project);

	/* shortcut hash tables */
	project->groups = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	project->targets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	project->sources = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	amp_project_new_module_hash (project);
	project_reload_packages (project);
	
	/* Load all makefiles recursively */
	config_files = project_list_config_files (project);
	project_load_makefile (project, project->root_file, NULL, &config_files);
	g_list_foreach (config_files, (GFunc)amp_config_file_free, NULL);
	g_list_free (config_files);
	
	//g_hash_table_foreach (project->groups, (GHFunc)foreach_group_reload, project);
	//foreach_group_reload ("", g_hash_table_lookup (project->groups, ""), project);
	
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
	node = AMP_NODE (g_node);
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
	node = AMP_NODE (g_node);
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
	node = AMP_NODE (g_node);
	
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

	/* unload current project */
	project = AMP_PROJECT (_project);
	project_unload (project);

	/* allow this? */
	if (uri == NULL)
		return;

	/* some basic checks */
	if (!impl_probe (_project, uri, error))
		return;
	
	/* now try loading the project */
	project->root_file = g_file_new_for_path (uri);
	if (!project_reload (project, error))
	{
		g_object_unref (project->root_file);
		project->root_file = NULL;
	}
}

static gboolean
file_exists (const gchar *path, const gchar *filename)
{
	gchar *full_path;
	gboolean retval;
	
	full_path = g_build_filename (path, filename, NULL);
	retval = g_file_test (full_path, G_FILE_TEST_EXISTS);
	g_free (full_path);

	return retval;
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
	AmpGroup* group_node;
	AmpNode *node;
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
	group_node = AMP_GROUP_NODE (g_node);

	group = g_new0 (GbfProjectGroup, 1);
	group->id = g_file_get_uri (group_node->file);
	group->name = g_file_get_basename (group_node->file);
	if (g_node->parent)
		group->parent_id = g_file_get_uri (AMP_GROUP_NODE (g_node->parent)->file);
	else
		group->parent_id = NULL;
	group->groups = NULL;
	group->targets = NULL;

	/* add subgroups and targets of the group */
	g_node = g_node_first_child (g_node);
	while (g_node) {
		node = AMP_NODE (g_node);
		switch (node->type) {
			case AMP_NODE_GROUP:
				group->groups = g_list_prepend (group->groups,
								g_file_get_uri (((AmpGroup *)node)->file));
				break;
			case AMP_NODE_TARGET:
				target_id  = canonicalize_automake_variable (((AmpTarget *)node)->name);
				group->targets = g_list_prepend (group->targets,
								 g_strconcat (id, ":", target_id, NULL));
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
		AmpNode *node = AMP_NODE (iter_node);
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
#if 0
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
	
	return retval;
}

static void 
impl_remove_group (GbfProject  *_project,
		   const gchar *id,
		   GError     **error)
{
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
#if 0
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
	AmpTarget *node;
	AmpSource *child_node;

	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);

	project = AMP_PROJECT (_project);
	g_node = g_hash_table_lookup (project->targets, id);
	if (g_node == NULL) {
		error_set (error, GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Target doesn't exist"));
		return NULL;
	}
	node = AMP_TARGET_NODE (g_node);

	target = g_new0 (GbfProjectTarget, 1);
	target->group_id = g_strdup (AMP_NODE (g_node->parent)->id);
	target->id = g_strconcat (target->group_id, node->name, NULL);
	target->name = g_strdup (node->name);
	target->type = g_strdup (node->type);
	target->sources = NULL;

	/* add sources to the target */
	g_node = g_node_first_child (g_node);
	while (g_node) {
		child_node = AMP_SOURCE_NODE (g_node);
		switch (child_node->node.type) {
			case AMP_NODE_SOURCE:
				target->sources = g_list_prepend (target->sources,
								  g_file_get_uri (child_node->file));
				break;
			default:
				break;
		}
		g_node = g_node_next_sibling (g_node);
	}
	target->sources = g_list_reverse (target->sources);

	return target;
}

static void
foreach_target (gpointer key, gpointer value, gpointer data)
{
	GList **targets = data;

	*targets = g_list_prepend (*targets, g_strdup (key));
}

static GList *
impl_get_all_targets (GbfProject *_project,
		      GError    **error)
{
	AmpProject *project;
	GList *targets = NULL;

	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);

	project = AMP_PROJECT (_project);
	g_hash_table_foreach (project->targets, foreach_target, &targets);

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
#if 0
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
#endif
	return retval;
}

static void 
impl_remove_target (GbfProject  *_project,
		    const gchar *id,
		    GError     **error)
{
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
#if 0
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
	AmpSource *node;

	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);

	project = AMP_PROJECT (_project);
	g_node = g_hash_table_lookup (project->sources, id);
	if (g_node == NULL) {
		error_set (error, GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Source doesn't exist"));
		return NULL;
	}
	node = AMP_SOURCE_NODE (g_node);

	source = g_new0 (GbfProjectTargetSource, 1);
	source->id = g_file_get_uri (node->file);
	source->source_uri = g_file_get_uri (node->file);
	source->target_id = g_strdup (AMP_NODE (g_node->parent)->id);

	return source;
}

static void
foreach_source (gpointer key, gpointer value, gpointer data)
{
	GList **sources = data;

	*sources = g_list_prepend (*sources, g_strdup (key));
}

static GList *
impl_get_all_sources (GbfProject *_project,
		      GError    **error)
{
	AmpProject *project;
	GList *sources = NULL;

	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);

	project = AMP_PROJECT (_project);
	g_hash_table_foreach (project->sources, foreach_source, &sources);

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
	GNode *g_node, *iter_node;
	//xmlDocPtr doc;
	gboolean abort_action = FALSE;
	gchar *full_uri = NULL;
	gchar *group_uri;
	GSList *change_set = NULL;
	//AmChange *change;
	gchar *retval;
	gchar *filename;
	const gchar *ptr;
	gboolean failed = FALSE;
	
	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);
	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (target_id != NULL, NULL);
	
	project = AMP_PROJECT (_project);
	
	filename = g_path_get_basename (uri);
	
	/* Validate target name */
	ptr = filename;
	while (*ptr) {
		if (!isalnum (*ptr) && *ptr != '.' && *ptr != '-' &&
		    *ptr != '_')
			failed = TRUE;
		ptr++;
	}
	if (failed) {
		error_set (error, GBF_PROJECT_ERROR_VALIDATION_FAILED,
			   _("Source file name can only contain alphanumeric, '_', '-' or '.' characters"));
		g_free (filename);
		return NULL;
	}
	
	/* check target */
	g_node = g_hash_table_lookup (project->targets, target_id);
	if (g_node == NULL) {
		error_set (error,GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Target doesn't exist"));
		return NULL;
	}

	/* if the uri is relative, resolve it against the target's
	 * group directory; we need to compute the group's uri
	 * first */
	/*group_uri = uri_normalize (g_path_skip_root (AMP_NODE (g_node->parent)->id),
				   project->project_root_uri);*/
	
	full_uri = uri_normalize (uri, group_uri);
	
	/* Check that the source uri is inside the project root */
	/*if (!uri_is_parent (project->project_root_uri, full_uri)) {
		GFile *src_file, *group_file, *dst_file;
		GError *err = NULL;
		
		src_file = g_file_new_for_commandline_arg (uri);
		group_file = g_file_new_for_commandline_arg (group_uri);
		dst_file = g_file_get_child (group_file, filename);
		g_object_unref (group_file);
		
		if (!g_file_copy (src_file, dst_file,
			     G_FILE_COPY_NONE,
			     NULL, NULL,
			     NULL, &err))
		{
			if (err->code != G_IO_ERROR_EXISTS)
			{
				GbfProjectError gbf_err;
				gchar *error_str = 
					g_strdup_printf ("Failed to copy source file inside project: %s",
							 err->message);
				switch (err->code)
				{
				case G_IO_ERROR_NOT_FOUND:
					gbf_err = GBF_PROJECT_ERROR_DOESNT_EXIST;
					break;
				default:
					gbf_err = GBF_PROJECT_ERROR_GENERAL_FAILURE;
					break;
				}
				error_set (error, gbf_err, error_str);
				g_free (error_str);
				g_error_free (err);
				abort_action = TRUE;
			}
			else
			{
				g_free (full_uri);
				full_uri = g_file_get_uri (dst_file);
			}
		}
		g_object_unref (src_file);
		g_object_unref (dst_file);		
	}
	
	g_free (group_uri);
	g_free (filename);*/
	
	/* check for source duplicates */
	iter_node = g_node_first_child (g_node);
	while (!abort_action && iter_node) {
		AmpNode *node = AMP_NODE (iter_node);
		
		if (node->type == AMP_NODE_SOURCE &&
		    uri_is_equal (full_uri, node->uri)) {
			error_set (error, GBF_PROJECT_ERROR_ALREADY_EXISTS,
				   _("Source file is already in given target"));
			abort_action = TRUE;
		}
		iter_node = g_node_next_sibling (iter_node);
	}

	/* have there been any errors? */
	if (abort_action) {
		g_free (full_uri);
		return NULL;
	}
	
	/* Create the update xml */
#if 0
	doc = xml_new_change_doc (project);

	if (!xml_write_add_source (project, doc, g_node, full_uri)) {
		error_set (error, PROJECT_ERROR_GENERAL_FAILURE,
			   _("General failure in adding source file"));
		abort_action = TRUE;
	}

	g_free (full_uri);
	if (abort_action) {
		xmlFreeDoc (doc);
		return NULL;
	}
	
	DEBUG ({
		xmlSetDocCompressMode (doc, 0);
		xmlSaveFile ("/tmp/add-source.xml", doc);
	});

	/* Update the project */
	if (!project_update (project, doc, &change_set, error)) {
		error_set (error, PROJECT_ERROR_PROJECT_MALFORMED,
			   _("Unable to update project"));
		xmlFreeDoc (doc);
		return NULL;
	}
	xmlFreeDoc (doc);

	/* get newly created source id */
	retval = NULL;
	DEBUG (change_set_debug_print (change_set));
	change = change_set_find (change_set, AM_CHANGE_ADDED,
				  AMP_NODE_SOURCE);
	if (change) {
		retval = g_strdup (change->id);
	} else {
		error_set (error, PROJECT_ERROR_GENERAL_FAILURE,
			   _("Newly added source file could not be identified"));
	}
	change_set_destroy (change_set);
#endif
	
	return retval;
}

static void 
impl_remove_source (GbfProject  *_project,
		    const gchar *id,
		    GError     **error)
{
	AmpProject *project;
	GNode *g_node;
	//xmlDocPtr doc;
	
	g_return_if_fail (AMP_IS_PROJECT (_project));

	project = AMP_PROJECT (_project);
	
	/* Find the source */
	g_node = g_hash_table_lookup (project->sources, id);
	if (g_node == NULL) {
		error_set (error, GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Source doesn't exist"));
		return;
	}

	/* Create the update xml */
#if 0
	doc = xml_new_change_doc (project);
	if (!xml_write_remove_source (project, doc, g_node)) {
		error_set (error, PROJECT_ERROR_DOESNT_EXIST,
			   _("Source couldn't be removed"));
		xmlFreeDoc (doc);
		return;
	}

	DEBUG ({
		xmlSetDocCompressMode (doc, 0);
		xmlSaveFile ("/tmp/remove-source.xml", doc);
	});

	/* Update the project */
	/* FIXME: should get and process the change set to verify that
	 * the source has been removed? */
	if (!project_update (project, doc, NULL, error)) {
		error_set (error, PROJECT_ERROR_PROJECT_MALFORMED,
			   _("Unable to update project"));
	}
	xmlFreeDoc (doc);
#endif
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
	GList *modules = NULL;
	AmpProject *project;
	
	g_return_if_fail (AMP_IS_PROJECT (_project));
	project = AMP_PROJECT (_project);

	return g_hash_table_get_keys (project->modules);
	

#if 0	
	GList *modules = NULL;
	AnjutaToken *pkg_check_tok;
	AnjutaToken *next_tok;
	AnjutaToken *close_tok;
	AnjutaToken *sequence;
	AmpProject *project;
	
	g_return_if_fail (AMP_IS_PROJECT (_project));
	project = AMP_PROJECT (_project);


	pkg_check_tok = anjuta_token_new_static (ANJUTA_TOKEN_KEYWORD, "PKG_CHECK_MODULES(");
	next_tok = anjuta_token_new_static (ANJUTA_TOKEN_NEXT | ANJUTA_TOKEN_CLOSE, NULL);
	close_tok = anjuta_token_new_static (ANJUTA_TOKEN_CLOSE, NULL);
	
	sequence = project->configure_sequence;
	for (;;)
	{
		AnjutaToken *module;
		AnjutaToken *next;
		gchar *value;
		
		if (!anjuta_token_match (pkg_check_tok, sequence, &module)) break;

		module = anjuta_token_next (module);

		if (anjuta_token_match (next_tok, module, &sequence))
		{
			value = anjuta_token_evaluate (module, sequence);
			modules = g_list_prepend (modules, value);
			anjuta_token_match (close_tok, sequence, &sequence);
		}
		sequence = anjuta_token_next (sequence);
	}
	anjuta_token_free (pkg_check_tok);
	anjuta_token_free (next_tok);
	anjuta_token_free (close_tok);
	
	modules = g_list_reverse (modules);
	
	return modules;
#endif
}

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

static GList *
impl_get_config_packages  (GbfProject *project,
			   const gchar* module,
			   GError **error)
{
	AmpModule *mod;
	GList *packages = NULL;

	g_return_if_fail (project != NULL);
	g_return_if_fail (module != NULL);

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
#if 0	
	//AmConfigMapping *config;
	//AmConfigValue *module_info;
	//AmConfigMapping *module_info_map;
	GError* err = NULL;
	GList* result = NULL;


	config = amp_project_get_config (AMP_PROJECT(project), &err);
	if (err) 
	{
		g_propagate_error (error, err);
		return NULL;
	}
			
	gchar *module_key = g_strconcat ("pkg_check_modules_",
					 module,
					 NULL);
	
	if ((module_info = am_config_mapping_lookup (config, module_key)) &&
	    (module_info_map = am_config_value_get_mapping (module_info))) 
	{
		AmConfigValue *pkgs_val;
		const gchar *packages;
		
		if ((pkgs_val = am_config_mapping_lookup (module_info_map, "packages")) &&
		    (packages = am_config_value_get_string (pkgs_val))) 
		{
			gchar **pkgs, **pkg;
			pkgs = g_strsplit (packages, ", ", -1);
			for (pkg = pkgs; *pkg != NULL; ++pkg) 
			{
				gchar* version;
				if ((version = strchr (*pkg, ' ')))
					*version = '\0';
				if (package_is_valid (*pkg))
				{
					result = g_list_append (result, 
								g_strdup (*pkg));
				}
			}			
			g_strfreev (pkgs);
		}		
	}
	g_free (module_key);

	return result;
#endif
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

/* Public functions
 *---------------------------------------------------------------------------*/

GbfProject *
amp_project_new (void)
{
	return GBF_PROJECT (g_object_new (AMP_TYPE_PROJECT, NULL));
}


GBF_BACKEND_BOILERPLATE (AmpProject, amp_project);
