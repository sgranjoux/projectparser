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

#include "gbf-mk-project.h"

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

struct _GbfMakeProject {
	MkpProject          parent;
};

struct _GbfMakeProjectClass {
	MkpProjectClass parent_class;
};

/* ----- Standard GObject types and variables ----- */

enum {
	PROP_0,
	PROP_PROJECT_DIR
};

static MkpProject *parent_class;

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

/* GbfProject implementation
 *---------------------------------------------------------------------------*/

static gboolean 
impl_probe (GbfProject  *_project,
	    const gchar *uri,
	    GError     **error)
{
	return mkp_project_probe (MKP_PROJECT (_project), uri, error);
}

static void 
impl_load (GbfProject  *_project,
	   const gchar *uri,
	   GError     **error)
{
	MkpProject *project = MKP_PROJECT (_project);
	
	g_return_if_fail (uri != NULL);

	/* unload current project */
	mkp_project_unload (project);

	/* some basic checks */
	if (!mkp_project_probe (project, uri, error)) return;
	
	/* now try loading the project */
	mkp_project_load (project, uri, error);	
}

static void
impl_refresh (GbfProject *_project,
	      GError    **error)
{
	MkpProject *project;

	g_return_if_fail (MKP_IS_PROJECT (_project));

	project = MKP_PROJECT (_project);

	if (mkp_project_reload (project, error))
		g_signal_emit_by_name (G_OBJECT (project), "project-updated");
}

static GbfProjectCapabilities
impl_get_capabilities (GbfProject *_project, GError    **error)
{
	g_return_val_if_fail (MKP_IS_PROJECT (_project),
			      GBF_PROJECT_CAN_ADD_NONE);
	
	return GBF_PROJECT_CAN_ADD_NONE;
}

static GbfProjectGroup * 
impl_get_group (GbfProject  *_project,
		const gchar *id,
		GError     **error)
{
	MkpProject *project;
	GbfProjectGroup *group;
	MkpGroup *g_node;
	MkpNode *node;
	
	g_return_val_if_fail (MKP_IS_PROJECT (_project), NULL);

	project = MKP_PROJECT (_project);
	if ((id == NULL) || (*id == '\0') || ((id[0] == '/') && (id[1] == '\0')))
	{
		g_node = mkp_project_get_root (project);
	}
	else
	{
		g_node = mkp_project_get_group (project, id);
	}
	if (g_node == NULL) {
		error_set (error, GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Group doesn't exist"));
		return NULL;
	}
	
	group = g_new0 (GbfProjectGroup, 1);
	group->id = g_file_get_uri (mkp_group_get_directory (g_node));
	group->name = g_file_get_basename (mkp_group_get_directory (g_node));
	if (mkp_node_parent (g_node))
		group->parent_id = g_file_get_uri (mkp_group_get_directory (mkp_node_parent (g_node)));
	else
		group->parent_id = NULL;
	group->groups = NULL;
	group->targets = NULL;

	/* add subgroups and targets of the group */
	for (node = mkp_node_first_child (g_node); node != NULL; node = mkp_node_next_sibling (node))
	{
		switch (mkp_node_get_type (node)) {
			case MKP_NODE_GROUP:
				group->groups = g_list_prepend (group->groups, mkp_group_get_id (node));
				break;
			case MKP_NODE_TARGET:
				group->targets = g_list_prepend (group->targets, mkp_target_get_id (node));
				break;
			default:
				break;
		}
	}
	group->groups = g_list_reverse (group->groups);
	group->targets = g_list_reverse (group->targets);
	
	return group;
}

static gboolean
foreach_group (MkpNode *node, gpointer data)
{
	GList **groups = data;

	if (mkp_node_get_type (node) == MKP_NODE_GROUP)
	{
		*groups = g_list_prepend (*groups, mkp_group_get_id (node));
	}

	return FALSE;
}

static GList *
impl_get_all_groups (GbfProject *_project,
		     GError    **error)
{
	MkpProject *project;
	GList *groups = NULL;

	g_return_val_if_fail (MKP_IS_PROJECT (_project), NULL);

	project = MKP_PROJECT (_project);

	mkp_node_all_foreach (mkp_project_get_root (project), foreach_group, &groups);

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

	return NULL;
}

static gchar * 
impl_add_group (GbfProject  *_project,
		const gchar *parent_id,
		const gchar *name,
		GError     **error)
{
	return NULL;
}

static void 
impl_remove_group (GbfProject  *_project,
		   const gchar *id,
		   GError     **error)
{
	return;
}

static GbfProjectTarget * 
impl_get_target (GbfProject  *_project,
		 const gchar *id,
		 GError     **error)
{
	MkpProject *project;
	GbfProjectTarget *target;
	MkpTarget *t_node;
	MkpNode *node;

	g_return_val_if_fail (MKP_IS_PROJECT (_project), NULL);

	project = MKP_PROJECT (_project);
	t_node = mkp_project_get_target (project, id);
	
	target = g_new0 (GbfProjectTarget, 1);
	target->group_id = mkp_group_get_id (mkp_node_parent (t_node));
	target->id = mkp_target_get_id (t_node);
	target->name = g_strdup (mkp_target_get_name (t_node));
	target->type = g_strdup (mkp_target_get_type (t_node));
	target->sources = NULL;

	/* add sources to the target */
	for (node = mkp_node_first_child (t_node); node != NULL; node = mkp_node_next_sibling (node))
	{
		switch (mkp_node_get_type (node)) {
			case MKP_NODE_SOURCE:
				target->sources = g_list_prepend (target->sources, mkp_source_get_id (node));
				break;
			default:
				break;
		}
	}
	target->sources = g_list_reverse (target->sources);

	return target;
}

static gboolean
foreach_target (MkpNode *node, gpointer data)
{
	GList **targets = data;

	if (mkp_node_get_type (node) == MKP_NODE_GROUP)
	{
		*targets = g_list_prepend (*targets, mkp_target_get_id (node));
	}

	return FALSE;
}

static GList *
impl_get_all_targets (GbfProject *_project,
		      GError    **error)
{
	MkpProject *project;
	GList *targets = NULL;

	g_return_val_if_fail (MKP_IS_PROJECT (_project), NULL);

	project = MKP_PROJECT (_project);

	mkp_node_all_foreach (mkp_project_get_root (project), foreach_target, &targets);

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
	g_return_val_if_fail (MKP_IS_PROJECT (_project), NULL);

	return NULL;
}

static char * 
impl_add_target (GbfProject  *_project,
		 const gchar *group_id,
		 const gchar *name,
		 const gchar *type,
		 GError     **error)
{
	g_return_val_if_fail (MKP_IS_PROJECT (_project), NULL);

	return NULL;
}

static void 
impl_remove_target (GbfProject  *_project,
		    const gchar *id,
		    GError     **error)
{
	g_return_if_fail (MKP_IS_PROJECT (_project));

	return;
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
	} else if (!strcmp (type, "lisp")) {
		return _("Lisp Module");
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
			   "man:data:script:info:java:python:lisp", ":", 0);
}

static GbfProjectTargetSource * 
impl_get_source (GbfProject  *_project,
		 const gchar *id,
		 GError     **error)
{
	MkpProject *project;
	GbfProjectTargetSource *source;
	MkpSource *s_node;

	g_return_val_if_fail (MKP_IS_PROJECT (_project), NULL);

	project = MKP_PROJECT (_project);
	s_node = mkp_project_get_source (project, id);
	
	source = g_new0 (GbfProjectTargetSource, 1);
	source->id = mkp_source_get_id (s_node);
	source->source_uri = g_file_get_uri (mkp_source_get_file (s_node));
	source->target_id = mkp_target_get_id (mkp_node_parent (s_node));

	return source;
}

static gboolean
foreach_source (MkpNode *node, gpointer user_data)
{
	GHashTable *hash = (GHashTable *)user_data;
	
	if (mkp_node_get_type (node) == MKP_NODE_SOURCE)
	{
		g_hash_table_insert (hash, mkp_source_get_file (node), mkp_source_get_id (node));
	}

	return FALSE;
}

/* List all sources, removing duplicate */
static GList *
impl_get_all_sources (GbfProject *_project,
		      GError    **error)
{
	MkpProject *project;
	GHashTable *hash;
	GList *sources;
	

	g_return_val_if_fail (MKP_IS_PROJECT (_project), NULL);

	project = MKP_PROJECT (_project);

	hash = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, NULL, g_free);
	mkp_node_all_foreach (mkp_project_get_root (project), foreach_source, hash);
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
	g_return_val_if_fail (MKP_IS_PROJECT (_project), NULL);

	return NULL;
}

static void 
impl_remove_source (GbfProject  *_project,
		    const gchar *id,
		    GError     **error)
{
	g_return_if_fail (MKP_IS_PROJECT (_project));

	return;
}

static GtkWidget *
impl_configure (GbfProject *_project, GError **error)
{
	g_return_val_if_fail (MKP_IS_PROJECT (_project), NULL);
	
	return NULL;
}

static GList *
impl_get_config_modules   (GbfProject *_project, GError **error)
{
	return NULL;
}

static GList *
impl_get_config_packages  (GbfProject *project,
			   const gchar* module,
			   GError **error)
{
	return NULL;
}

/* GObject implementation
 *---------------------------------------------------------------------------*/

static void
gbf_make_project_dispose (GObject *object)
{
	g_return_if_fail (GBF_IS_MAKE_PROJECT (object));

	G_OBJECT_CLASS (parent_class)->dispose (object);	
}

static void
gbf_make_project_get_property (GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
	gchar *uri;

	switch (prop_id) {
		case PROP_PROJECT_DIR:
			uri = mkp_project_get_uri (MKP_PROJECT (object));
			g_value_take_string (value, uri);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gbf_make_project_class_init (GbfMakeProjectClass *klass)
{
	GObjectClass *object_class;
	GbfProjectClass *project_class;

	object_class = G_OBJECT_CLASS (klass);
	project_class = GBF_PROJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gbf_make_project_dispose;
	object_class->get_property = gbf_make_project_get_property;

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
gbf_make_project_instance_init (GbfMakeProject *project)
{
}

GType
gbf_make_project_get_type (GTypeModule *module)
{
	static GType type = 0;
	
	if (!type)
	{
		static const GTypeInfo type_info =
		{
			sizeof (GbfMakeProjectClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc)gbf_make_project_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (GbfMakeProject),
			0,
			(GInstanceInitFunc)gbf_make_project_instance_init
		};
		
		if (module == NULL)
		{
			type = g_type_register_static (MKP_TYPE_PROJECT, "GbfMakeProject", &type_info, 0);
		}
		else
		{								\
			type = g_type_module_register_type (module, MKP_TYPE_PROJECT, "GbfMakeProject", &type_info, 0);
		}
	}
	
	return type;
}
