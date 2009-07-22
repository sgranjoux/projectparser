/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4; coding: utf-8 -*- */
/* am-project.h
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
 */

#ifndef _AM_PROJECT_H_
#define _AM_PROJECT_H_

#include <glib-object.h>

#include <libanjuta/anjuta-token.h>
#include <libanjuta/gbf-project.h>

G_BEGIN_DECLS

#define AMP_TYPE_PROJECT		(amp_project_get_type (NULL))
#define AMP_PROJECT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), AMP_TYPE_PROJECT, AmpProject))
#define AMP_PROJECT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), AMP_TYPE_PROJECT, AmpProjectClass))
#define AMP_IS_PROJECT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), AMP_TYPE_PROJECT))
#define AMP_IS_PROJECT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), AMP_TYPE_PROJECT))

typedef struct _AmpProject        AmpProject;
typedef struct _AmpProjectClass   AmpProjectClass;

struct _AmpProjectClass {
	GbfProjectClass parent_class;
};

typedef GNode AmpNode;
typedef GNode AmpGroup;
typedef GNode AmpTarget;
typedef GNode AmpSource;
typedef struct _AmpProperty AmpProperty;

typedef enum {
	AMP_PROPERTY_NAME,
	AMP_PROPERTY_VERSION,
	AMP_PROPERTY_BUG_REPORT,
	AMP_PROPERTY_TARNAME
} AmpPropertyType;

typedef GNodeTraverseFunc AmpNodeFunc;

typedef enum {
	AMP_NODE_GROUP,
	AMP_NODE_TARGET,
	AMP_NODE_SOURCE
} AmpNodeType;


GType         amp_project_get_type (GTypeModule *plugin);
AmpProject   *amp_project_new      (void);


gboolean amp_project_probe (AmpProject  *project, const gchar *uri, GError     **error);
gboolean amp_project_load (AmpProject *project, const gchar *uri, GError **error);
gboolean amp_project_reload (AmpProject *project, GError **error);
void amp_project_unload (AmpProject *project);

AmpGroup *amp_project_get_root (AmpProject *project);
AmpGroup *amp_project_get_group (AmpProject *project, const gchar *id);
AmpTarget *amp_project_get_target (AmpProject *project, const gchar *id);
AmpSource *amp_project_get_source (AmpProject *project, const gchar *id);


gboolean amp_project_move (AmpProject *project, const gchar *path);
gboolean amp_project_save (AmpProject *project, GError **error);

gchar * amp_project_get_uri (AmpProject *project);

AmpGroup* amp_project_add_group (AmpProject  *project, const gchar *parent_id,	const gchar *name, GError **error);
void amp_project_remove_group (AmpProject  *project, const gchar *id, GError **error);

AmpTarget* amp_project_add_target (AmpProject  *project, const gchar *group_id, const gchar *name, const gchar *type, GError **error);
void amp_project_remove_target (AmpProject  *project, const gchar *id, GError **error);

AmpSource* amp_project_add_source (AmpProject  *project, const gchar *target_id, const gchar *uri, GError **error);
void amp_project_remove_source (AmpProject  *project, const gchar *id, GError **error);


GList *amp_project_get_config_modules (AmpProject *project, GError **error);
GList *amp_project_get_config_packages  (AmpProject *project, const gchar* module, GError **error);

gchar* amp_project_get_property (AmpProject *project, AmpPropertyType type);
gboolean amp_project_set_property (AmpProject *project, AmpPropertyType type, const gchar* value);

gchar * amp_project_get_node_id (AmpProject *project, const gchar *path);

AmpNode *amp_node_parent (AmpNode *node);
AmpNode *amp_node_first_child (AmpNode *node);
AmpNode *amp_node_last_child (AmpNode *node);
AmpNode *amp_node_next_sibling (AmpNode *node);
AmpNode *amp_node_prev_sibling (AmpNode *node);
AmpNodeType amp_node_get_type (AmpNode *node);
void amp_node_all_foreach (AmpNode *node, AmpNodeFunc func, gpointer data);

GFile *amp_group_get_directory (AmpGroup *group);
GFile *amp_group_get_makefile (AmpGroup *group);
gchar *amp_group_get_id (AmpGroup *group);

const gchar *amp_target_get_name (AmpTarget *target);
const gchar *amp_target_get_type (AmpTarget *target);
gchar *amp_target_get_id (AmpTarget *target);

gchar *amp_source_get_id (AmpSource *source);
GFile *amp_source_get_file (AmpSource *source);

/* FIXME: The config infrastructure should probably be made part of GbfProject
 * so that other backend implementations could use them directly and we don't
 * have to create separate configuration widgets. But then different back end
 * implementations could have significantly different config management
 * warranting separate implementations.
 */
/* These functions returns a copy of the config. It should be free with
 * gbf_am_config_value_free() when no longer required
 */
/*AmConfigMapping *gbf_am_project_get_config (GbfAmProject *project, GError **error);
AmConfigMapping *gbf_am_project_get_group_config (GbfAmProject *project, const gchar *group_id, GError **error);
AmConfigMapping *gbf_am_project_get_target_config (GbfAmProject *project, const gchar *target_id, GError **error);

void am_project_set_config (AmProject *project, GbfAmConfigMapping *new_config, GError **error);
void am_project_set_group_config (AmProject *project, const gchar *group_id, GbfAmConfigMapping *new_config, GError **error);
void am_project_set_target_config (AmProject *project, const gchar *target_id, GbfAmConfigMapping *new_config, GError **error);*/

G_END_DECLS

#endif /* _AM_PROJECT_H_ */
