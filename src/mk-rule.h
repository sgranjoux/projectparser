/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4; coding: utf-8 -*- */
/* mk-rule.h
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

#ifndef _MK_RULE_H_
#define _MK_RULE_H_

#include <glib-object.h>

#include "mk-project.h"


G_BEGIN_DECLS

void mkp_project_init_rules (MkpProject *project);
void mkp_project_free_rules (MkpProject *project);
void mkp_project_enumerate_targets (MkpProject *project, AnjutaProjectGroup *parent);

G_END_DECLS

#endif /* _MK_RULE_H_ */
