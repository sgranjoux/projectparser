/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4; coding: utf-8 -*- */
/* gbf-am-project.h
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

#ifndef _GBF_AM_PROJECT_H_
#define _GBF_AM_PROJECT_H_

#include "libanjuta/gbf-project.h"

G_BEGIN_DECLS

#define GBF_TYPE_AM_PROJECT		(gbf_am_project_get_type (NULL))
#define GBF_AM_PROJECT(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GBF_TYPE_AM_PROJECT, GbfAmProject))
#define GBF_AM_PROJECT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GBF_TYPE_AM_PROJECT, GbfAmProjectClass))
#define GBF_IS_AM_PROJECT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GBF_TYPE_AM_PROJECT))
#define GBF_IS_AM_PROJECT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), GBF_TYPE_AM_PROJECT))

typedef struct _GbfAmProject       	GbfAmProject;
typedef struct _GbfAmProjectClass   GbfAmProjectClass;

GType         gbf_am_project_get_type (GTypeModule *plugin);

G_END_DECLS

#endif /* _GBF_AM_PROJECT_H_ */
