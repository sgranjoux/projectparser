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
#include "libanjuta/gbf-project.h"

G_BEGIN_DECLS

#define AMP_TYPE_PROJECT		(amp_project_get_type (NULL))
#define AMP_PROJECT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), AMP_TYPE_PROJECT, AmpProject))
#define AMP_PROJECT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), AMP_TYPE_PROJECT, AmpProjectClass))
#define AMP_IS_PROJECT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), AMP_TYPE_PROJECT))
#define AMP_IS_PROJECT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), AMP_TYPE_PROJECT))

typedef struct _AmpProject        AmpProject;
typedef struct _AmpProjectClass   AmpProjectClass;

GType         amp_project_get_type (GTypeModule *plugin);
GbfProject   *amp_project_new      (void);

gboolean amp_project_move (AmpProject *project, const gchar *path);
gboolean amp_project_save (AmpProject *project, GError **error);

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
