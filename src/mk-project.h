/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4; coding: utf-8 -*- */
/* mk-project.h
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

#ifndef _MK_PROJECT_H_
#define _MK_PROJECT_H_

#include <glib-object.h>

#include <libanjuta/anjuta-token.h>
#include <libanjuta/anjuta-token-file.h>
#include <libanjuta/anjuta-token-style.h>
#include <libanjuta/gbf-project.h>

G_BEGIN_DECLS

#define MKP_TYPE_PROJECT		(mkp_project_get_type (NULL))
#define MKP_PROJECT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), MKP_TYPE_PROJECT, MkpProject))
#define MKP_PROJECT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), MKP_TYPE_PROJECT, MkpProjectClass))
#define MKP_IS_PROJECT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), MKP_TYPE_PROJECT))
#define MKP_IS_PROJECT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), MKP_TYPE_PROJECT))

typedef struct _MkpProject        MkpProject;
typedef struct _MkpProjectClass   MkpProjectClass;

struct _MkpProjectClass {
	GbfProjectClass parent_class;
};

typedef GNode MkpNode;
typedef GNode MkpGroup;
typedef GNode MkpTarget;
typedef GNode MkpSource;
typedef struct _MkpProperty MkpProperty;

typedef enum {
	MKP_PROPERTY_NAME = 0,
	MKP_PROPERTY_VERSION,
	MKP_PROPERTY_BUG_REPORT,
	MKP_PROPERTY_TARNAME,
	MKP_PROPERTY_URL
} MkpPropertyType;

typedef GNodeTraverseFunc MkpNodeFunc;

typedef enum {
	MKP_NODE_GROUP,
	MKP_NODE_TARGET,
	MKP_NODE_SOURCE
} MkpNodeType;


GType         mkp_project_get_type (GTypeModule *plugin);
MkpProject   *mkp_project_new      (void);


gboolean mkp_project_probe (MkpProject  *project, const gchar *uri, GError     **error);
gboolean mkp_project_load (MkpProject *project, const gchar *uri, GError **error);
gboolean mkp_project_reload (MkpProject *project, GError **error);
void mkp_project_unload (MkpProject *project);

MkpGroup *mkp_project_get_root (MkpProject *project);
MkpGroup *mkp_project_get_group (MkpProject *project, const gchar *id);
MkpTarget *mkp_project_get_target (MkpProject *project, const gchar *id);
MkpSource *mkp_project_get_source (MkpProject *project, const gchar *id);


gboolean mkp_project_move (MkpProject *project, const gchar *path);
gboolean mkp_project_save (MkpProject *project, GError **error);

gchar * mkp_project_get_uri (MkpProject *project);
GFile* mkp_project_get_file (MkpProject *project);

MkpGroup* mkp_project_add_group (MkpProject  *project, const gchar *parent_id,	const gchar *name, GError **error);
void mkp_project_remove_group (MkpProject  *project, const gchar *id, GError **error);

MkpTarget* mkp_project_add_target (MkpProject  *project, const gchar *group_id, const gchar *name, const gchar *type, GError **error);
void mkp_project_remove_target (MkpProject  *project, const gchar *id, GError **error);

MkpSource* mkp_project_add_source (MkpProject  *project, const gchar *target_id, const gchar *uri, GError **error);
void mkp_project_remove_source (MkpProject  *project, const gchar *id, GError **error);


GList *mkp_project_get_config_modules (MkpProject *project, GError **error);
GList *mkp_project_get_config_packages  (MkpProject *project, const gchar* module, GError **error);

gchar* mkp_project_get_property (MkpProject *project, MkpPropertyType type);
gboolean mkp_project_set_property (MkpProject *project, MkpPropertyType type, const gchar* value);

gchar * mkp_project_get_node_id (MkpProject *project, const gchar *path);

MkpNode *mkp_node_parent (MkpNode *node);
MkpNode *mkp_node_first_child (MkpNode *node);
MkpNode *mkp_node_last_child (MkpNode *node);
MkpNode *mkp_node_next_sibling (MkpNode *node);
MkpNode *mkp_node_prev_sibling (MkpNode *node);
MkpNodeType mkp_node_get_type (MkpNode *node);
void mkp_node_all_foreach (MkpNode *node, MkpNodeFunc func, gpointer data);

GFile *mkp_group_get_directory (MkpGroup *group);
GFile *mkp_group_get_makefile (MkpGroup *group);
gchar *mkp_group_get_id (MkpGroup *group);

const gchar *mkp_target_get_name (MkpTarget *target);
const gchar *mkp_target_get_type (MkpTarget *target);
gchar *mkp_target_get_id (MkpTarget *target);

gchar *mkp_source_get_id (MkpSource *source);
GFile *mkp_source_get_file (MkpSource *source);

G_END_DECLS

#endif /* _MK_PROJECT_H_ */
