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

#include "gbf-am-project.h"

#include "am-project-private.h"

#include <libanjuta/anjuta-debug.h>
#include <libanjuta/anjuta-project.h>

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
#include "ac-scanner.h"
#include "am-scanner.h"
#include "am-dialogs.h"


#define UNIMPLEMENTED  G_STMT_START { g_warning (G_STRLOC": unimplemented"); } G_STMT_END

struct _GbfAmProject {
	AmpProject          parent;
};

struct _GbfAmProjectClass {
	AmpProjectClass parent_class;
};

typedef struct {
	const gchar *name;
	AnjutaProjectTargetClass base;
} GbfAmTypeName;


/* ----- Standard GObject types and variables ----- */

enum {
	PROP_0,
	PROP_PROJECT_DIR
};

static AmpProject *parent_class;

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
	return amp_project_probe (AMP_PROJECT (_project), uri, error);
}

static void 
impl_load (GbfProject  *_project,
	   const gchar *uri,
	   GError     **error)
{
	AmpProject *project = AMP_PROJECT (_project);
	
	g_return_if_fail (uri != NULL);

	/* unload current project */
	amp_project_unload (project);

	/* some basic checks */
	if (!amp_project_probe (project, uri, error)) return;
	
	/* now try loading the project */
	amp_project_load (project, uri, error);	
}

static void
impl_refresh (GbfProject *_project,
	      GError    **error)
{
	AmpProject *project;

	g_return_if_fail (AMP_IS_PROJECT (_project));

	project = AMP_PROJECT (_project);

	if (amp_project_reload (project, error))
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
	AmpGroup *g_node;
	AnjutaProjectNode *node;
	
	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);

	project = AMP_PROJECT (_project);
	if ((id == NULL) || (*id == '\0') || ((id[0] == '/') && (id[1] == '\0')))
	{
		g_node = amp_project_get_root (project);
	}
	else
	{
		g_node = amp_project_get_group (project, id);
	}
	if (g_node == NULL) {
		error_set (error, GBF_PROJECT_ERROR_DOESNT_EXIST,
			   _("Group doesn't exist"));
		return NULL;
	}
	
	group = g_new0 (GbfProjectGroup, 1);
	group->id = g_file_get_uri (amp_group_get_directory (g_node));
	group->name = g_file_get_basename (amp_group_get_directory (g_node));
	if (anjuta_project_node_parent (g_node))
		group->parent_id = g_file_get_uri (amp_group_get_directory (anjuta_project_node_parent (g_node)));
	else
		group->parent_id = NULL;
	group->groups = NULL;
	group->targets = NULL;

	/* add subgroups and targets of the group */
	for (node = anjuta_project_node_first_child (g_node); node != NULL; node = anjuta_project_node_next_sibling (node))
	{
		switch (anjuta_project_node_get_type (node)) {
			case ANJUTA_PROJECT_GROUP:
				group->groups = g_list_prepend (group->groups, amp_group_get_id (node));
				break;
			case ANJUTA_PROJECT_TARGET:
				group->targets = g_list_prepend (group->targets, amp_target_get_id (node));
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
foreach_group (AnjutaProjectNode *node, gpointer data)
{
	GList **groups = data;

	if (anjuta_project_node_get_type (node) == ANJUTA_PROJECT_GROUP)
	{
		*groups = g_list_prepend (*groups, amp_group_get_id (node));
	}

	return FALSE;
}

static GList *
impl_get_all_groups (GbfProject *_project,
		     GError    **error)
{
	AmpProject *project;
	GList *groups = NULL;

	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);

	project = AMP_PROJECT (_project);

	anjuta_project_node_all_foreach (amp_project_get_root (project), foreach_group, &groups);

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
	AmpGroup *group;

	group = amp_project_add_group(AMP_PROJECT (_project), parent_id, name, error);

	return group == NULL ? NULL : amp_group_get_id (group);
}

static void 
impl_remove_group (GbfProject  *_project,
		   const gchar *id,
		   GError     **error)
{
	amp_project_remove_group (AMP_PROJECT (_project), id, error);
}

static GbfProjectTarget * 
impl_get_target (GbfProject  *_project,
		 const gchar *id,
		 GError     **error)
{
	AmpProject *project;
	GbfProjectTarget *target;
	AmpTarget *t_node;
	AnjutaProjectNode *node;

	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);

	project = AMP_PROJECT (_project);
	t_node = amp_project_get_target (project, id);
	
	target = g_new0 (GbfProjectTarget, 1);
	target->group_id = amp_group_get_id (anjuta_project_node_parent (t_node));
	target->id = amp_target_get_id (t_node);
	target->name = g_strdup (anjuta_project_target_get_name (t_node));
	target->type = g_strdup ("unknown");
	target->sources = NULL;

	/* add sources to the target */
	for (node = anjuta_project_node_first_child (t_node); node != NULL; node = anjuta_project_node_next_sibling (node))
	{
		switch (anjuta_project_node_get_type (node)) {
			case ANJUTA_PROJECT_SOURCE:
				target->sources = g_list_prepend (target->sources, amp_source_get_id (node));
				break;
			default:
				break;
		}
	}
	target->sources = g_list_reverse (target->sources);

	return target;
}

static gboolean
foreach_target (AnjutaProjectNode *node, gpointer data)
{
	GList **targets = data;

	if (anjuta_project_node_get_type (node) == ANJUTA_PROJECT_GROUP)
	{
		*targets = g_list_prepend (*targets, amp_target_get_id (node));
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

	anjuta_project_node_all_foreach (amp_project_get_root (project), foreach_target, &targets);

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
	AmpTarget *target;
	static GbfAmTypeName TypeNames[] = {
		{"program", ANJUTA_TARGET_EXECUTABLE},
		{"script", ANJUTA_TARGET_EXECUTABLE},
		{"static_lib", ANJUTA_TARGET_STATICLIB},
		{"shared_lib", ANJUTA_TARGET_SHAREDLIB},
		{NULL, ANJUTA_TARGET_UNKNOWN}};
	GbfAmTypeName *types;
	AnjutaProjectTargetClass base;
	AnjutaProjectTargetType target_type;
	GList *list;
	GList *item;
	
	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);
	
	types = TypeNames;
	while (types->name != NULL)
	{
		if (strcmp (type, types->name) == 0)
		{
			break;
		}
	};
	base = types->base;

	list = amp_project_get_target_types (AMP_PROJECT (_project), NULL);
	target_type = (AnjutaProjectTargetType)list->data;
	for (item = list; item != NULL; item = g_list_next (item))
	{
		if (anjuta_project_target_type_class ((AnjutaProjectTargetType)item->data) == base)
		{
			target_type = (AnjutaProjectTargetType)item->data;
		}
	}
	g_list_free (list);
	
	target = amp_project_add_target(AMP_PROJECT (_project), group_id, name, target_type, error);

	return target == NULL ? NULL : amp_target_get_id (target);
}

static void 
impl_remove_target (GbfProject  *_project,
		    const gchar *id,
		    GError     **error)
{
	g_return_if_fail (AMP_IS_PROJECT (_project));

	amp_project_remove_target (AMP_PROJECT (_project), id, error);
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
	AmpProject *project;
	GbfProjectTargetSource *source;
	AmpSource *s_node;

	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);

	project = AMP_PROJECT (_project);
	s_node = amp_project_get_source (project, id);
	
	source = g_new0 (GbfProjectTargetSource, 1);
	source->id = amp_source_get_id (s_node);
	source->source_uri = g_file_get_uri (amp_source_get_file (s_node));
	source->target_id = amp_target_get_id (anjuta_project_node_parent (s_node));

	return source;
}

static gboolean
foreach_source (AnjutaProjectNode *node, gpointer user_data)
{
	GHashTable *hash = (GHashTable *)user_data;
	
	if (anjuta_project_node_get_type (node) == ANJUTA_PROJECT_SOURCE)
	{
		g_hash_table_insert (hash, amp_source_get_file (node), amp_source_get_id (node));
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
	anjuta_project_node_all_foreach (amp_project_get_root (project), foreach_source, hash);
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
	AmpSource *source;

	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);
	
	source = amp_project_add_source(AMP_PROJECT (_project), target_id, uri, error);

	return source == NULL ? NULL : amp_source_get_id (source);
}

static void 
impl_remove_source (GbfProject  *_project,
		    const gchar *id,
		    GError     **error)
{
	g_return_if_fail (AMP_IS_PROJECT (_project));

	amp_project_remove_source (AMP_PROJECT (_project), id, error);
}

static GtkWidget *
impl_configure (GbfProject *_project, GError **error)
{
	g_return_val_if_fail (AMP_IS_PROJECT (_project), NULL);
	
	return amp_configure_project_dialog (AMP_PROJECT (_project), error);
}

static GList *
impl_get_config_modules   (GbfProject *_project, GError **error)
{
	return amp_project_get_config_modules (AMP_PROJECT(_project), error);
}

static GList *
impl_get_config_packages  (GbfProject *project,
			   const gchar* module,
			   GError **error)
{
	return amp_project_get_config_packages (AMP_PROJECT(project), module, error);
}

/* GObject implementation
 *---------------------------------------------------------------------------*/

static void
gbf_am_project_dispose (GObject *object)
{
	g_return_if_fail (GBF_IS_AM_PROJECT (object));

	G_OBJECT_CLASS (parent_class)->dispose (object);	
}

static void
gbf_am_project_get_property (GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
	gchar *uri;

	switch (prop_id) {
		case PROP_PROJECT_DIR:
			uri = amp_project_get_uri (AMP_PROJECT (object));
			g_value_take_string (value, uri);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gbf_am_project_class_init (GbfAmProjectClass *klass)
{
	GObjectClass *object_class;
	GbfProjectClass *project_class;

	object_class = G_OBJECT_CLASS (klass);
	project_class = GBF_PROJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gbf_am_project_dispose;
	object_class->get_property = gbf_am_project_get_property;

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
gbf_am_project_instance_init (GbfAmProject *project)
{
}

GType
gbf_am_project_get_type (GTypeModule *module)
{
	static GType type = 0;
	
	if (!type)
	{
		static const GTypeInfo type_info =
		{
			sizeof (GbfAmProjectClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc)gbf_am_project_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (GbfAmProject),
			0,
			(GInstanceInitFunc)gbf_am_project_instance_init
		};
		
		if (module == NULL)
		{
			type = g_type_register_static (AMP_TYPE_PROJECT, "GbfAmProject", &type_info, 0);
		}
		else
		{								\
			type = g_type_module_register_type (module, AMP_TYPE_PROJECT, "GbfAmProject", &type_info, 0);
		}
	}
	
	return type;
}
