/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta-project.h
 * Copyright (C) Sébastien Granjoux 2009 <seb.sfo@free.fr>
 * 
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ANJUTA_PROJECT_H_
#define _ANJUTA_PROJECT_H_

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define ANJUTA_IS_PROJECT_GROUP(obj) (((AnjutaProjectNodeData *)obj->data)->type == ANJUTA_PROJECT_GROUP)
#define ANJUTA_IS_PROJECT_TARGET(obj) (((AnjutaProjectNodeData *)obj->data)->type == ANJUTA_PROJECT_TARGET)
#define ANJUTA_IS_PROJECT_NODE(obj) (1)

typedef enum
{
	ANJUTA_PROJECT_UNKNOWN,
	ANJUTA_PROJECT_GROUP,
	ANJUTA_PROJECT_TARGET,
	ANJUTA_PROJECT_SOURCE,
	ANJUTA_PROJECT_VARIABLE
} AnjutaProjectNodeType;
	
typedef enum
{
	ANJUTA_TARGET_UNKNOWN,
	ANJUTA_TARGET_SHAREDLIB,
	ANJUTA_TARGET_STATICLIB,
	ANJUTA_TARGET_EXECUTABLE,
	ANJUTA_TARGET_PYTHON,
	ANJUTA_TARGET_JAVA
} AnjutaProjectTargetClass;

typedef struct 
{
	gchar *name;
	AnjutaProjectTargetClass base;
	gchar *mime_type;
} AnjutaProjectTargetInformation;

typedef AnjutaProjectTargetInformation* AnjutaProjectTargetType;

typedef struct
{
	AnjutaProjectNodeType type;
} AnjutaProjectNodeData;

typedef struct {
	AnjutaProjectNodeData node;
	GFile *directory;
} AnjutaProjectGroupData;

typedef struct {
	AnjutaProjectNodeData node;
	gchar *name;
	AnjutaProjectTargetType type;
} AnjutaProjectTargetData;

typedef struct {
	AnjutaProjectNodeData node;
	GFile *file;
} AnjutaProjectSourceData;

typedef GNode AnjutaProjectNode;
typedef GNode AnjutaProjectGroup;
typedef GNode AnjutaProjectTarget;
typedef GNode AnjutaProjectSource;

typedef GNodeTraverseFunc AnjutaProjectNodeFunc;

AnjutaProjectNode *anjuta_project_node_parent (AnjutaProjectNode *node);
AnjutaProjectNode *anjuta_project_node_first_child (AnjutaProjectNode *node);
AnjutaProjectNode *anjuta_project_node_last_child (AnjutaProjectNode *node);
AnjutaProjectNode *anjuta_project_node_next_sibling (AnjutaProjectNode *node);
AnjutaProjectNode *anjuta_project_node_prev_sibling (AnjutaProjectNode *node);
AnjutaProjectNode *anjuta_project_node_nth_child (AnjutaProjectNode *node, guint n);
GList *anjuta_project_node_all_child (AnjutaProjectNode *node, AnjutaProjectNodeType type);
GList *anjuta_project_node_all (AnjutaProjectNode *node, AnjutaProjectNodeType type);
void anjuta_project_node_all_foreach (AnjutaProjectNode *node, AnjutaProjectNodeFunc func, gpointer data);

AnjutaProjectNodeType anjuta_project_node_get_type (const AnjutaProjectNode *node);

GFile *anjuta_project_group_get_directory (const AnjutaProjectGroup *group);

const gchar *anjuta_project_target_get_name (const AnjutaProjectTarget *target);
AnjutaProjectTargetType anjuta_project_target_get_type (const AnjutaProjectTarget *target);

GFile *anjuta_project_source_get_file (const AnjutaProjectSource *source);

const gchar *anjuta_project_target_type_name (const AnjutaProjectTargetType type);
const gchar *anjuta_project_target_type_mime (const AnjutaProjectTargetType type);
AnjutaProjectTargetClass anjuta_project_target_type_class (const AnjutaProjectTargetType type);

G_END_DECLS

#endif
