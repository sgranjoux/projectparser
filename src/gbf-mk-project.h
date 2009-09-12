/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4; coding: utf-8 -*- */
/* gbf-mk-project.h
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

#ifndef _GBF_MK_PROJECT_H_
#define _GBF_MK_PROJECT_H_

#include "libanjuta/gbf-project.h"

G_BEGIN_DECLS

#define GBF_TYPE_MAKE_PROJECT		(gbf_make_project_get_type (NULL))
#define GBF_MAKE_PROJECT(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GBF_TYPE_MAKE_PROJECT, GbfMakeProject))
#define GBF_MAKE_PROJECT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GBF_TYPE_MAKE_PROJECT, GbfMakeProjectClass))
#define GBF_IS_MAKE_PROJECT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GBF_TYPE_MAKE_PROJECT))
#define GBF_IS_MAKE_PROJECT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), GBF_TYPE_MAKE_PROJECT))

typedef struct _GbfMakeProject       	GbfMakeProject;
typedef struct _GbfMakeProjectClass   GbfMakeProjectClass;

GType         gbf_make_project_get_type (GTypeModule *plugin);

G_END_DECLS

#endif /* _GBF_MK_PROJECT_H_ */
