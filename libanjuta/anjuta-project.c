/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta-project.c
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

#include "anjuta-project.h"

#include "anjuta-debug.h"

/* convenient shortcut macro the get the AnjutaProjectNode from a GNode */
#define NODE_DATA(node)  ((node) != NULL ? (AnjutaProjectNodeData *)((node)->data) : NULL)
#define GROUP_DATA(node)  ((node) != NULL ? (AnjutaProjectGroupData *)((node)->data) : NULL)
#define TARGET_DATA(node)  ((node) != NULL ? (AnjutaProjectTargetData *)((node)->data) : NULL)
#define SOURCE_DATA(node)  ((node) != NULL ? (AnjutaProjectSourceData *)((node)->data) : NULL)


/* Node access functions
 *---------------------------------------------------------------------------*/

AnjutaProjectNode *
anjuta_project_node_parent(AnjutaProjectNode *node)
{
	return node->parent;
}

AnjutaProjectNode *
anjuta_project_node_first_child(AnjutaProjectNode *node)
{
	return g_node_first_child (node);
}

AnjutaProjectNode *
anjuta_project_node_last_child(AnjutaProjectNode *node)
{
	return g_node_last_child (node);
}

AnjutaProjectNode *
anjuta_project_node_next_sibling (AnjutaProjectNode *node)
{
	return g_node_next_sibling (node);
}

AnjutaProjectNode *
anjuta_project_node_prev_sibling (AnjutaProjectNode *node)
{
	return g_node_prev_sibling (node);
}

AnjutaProjectNode *anjuta_project_node_nth_child (AnjutaProjectNode *node, guint n)
{
	return g_node_nth_child (node, n);
}

void
anjuta_project_node_all_foreach (AnjutaProjectNode *node, AnjutaProjectNodeFunc func, gpointer data)
{
	g_node_traverse (node, G_PRE_ORDER, G_TRAVERSE_ALL, -1, func, data);
}

AnjutaProjectNodeType
anjuta_project_node_get_type (AnjutaProjectNode *node)
{
	return NODE_DATA (node)->type;
}

/* Group access functions
 *---------------------------------------------------------------------------*/

GFile*
anjuta_project_group_get_directory (AnjutaProjectGroup *group)
{
	return GROUP_DATA (group)->directory;
}

/* Target access functions
 *---------------------------------------------------------------------------*/

const gchar *
anjuta_project_target_get_name (AnjutaProjectTarget *target)
{
	return TARGET_DATA (target)->name;
}

AnjutaProjectTargetType
anjuta_project_target_get_type (AnjutaProjectTarget *target)
{
	return TARGET_DATA (target)->type;
}

/* Source access functions
 *---------------------------------------------------------------------------*/

GFile*
anjuta_project_source_get_file (AnjutaProjectSource *source)
{
	return SOURCE_DATA (source)->file;
}

/* Target type functions
 *---------------------------------------------------------------------------*/

const gchar *
anjuta_project_target_type_name (AnjutaProjectTargetType type)
{
	return type->name;
}

const gchar *
anjuta_project_target_type_mime (AnjutaProjectTargetType type)
{
	return type->mime_type;
}

AnjutaProjectTargetClass
anjuta_project_target_type_class (AnjutaProjectTargetType type)
{
	return type->base;
}
