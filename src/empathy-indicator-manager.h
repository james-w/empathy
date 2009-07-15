/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Canonical Ltd.
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors: James Westby <james.westby@ubuntu.com>
 *
 */

#ifndef _EMPATHY_INDICATOR_MANAGER_H_
#define _EMPATHY_INDICATOR_MANAGER_H_

#include <glib.h>

#include <libempathy/empathy-contact.h>
#include "empathy-indicator.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_INDICATOR_MANAGER         (empathy_indicator_manager_get_type ())
#define EMPATHY_INDICATOR_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_INDICATOR_MANAGER, EmpathyIndicatorManager))
#define EMPATHY_INDICATOR_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_INDICATOR_MANAGER, EmpathyIndicatorManagerClass))
#define EMPATHY_IS_INDICATOR_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_INDICATOR_MANAGER))
#define EMPATHY_IS_INDICATOR_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_INDICATOR_MANAGER))
#define EMPATHY_INDICATOR_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_INDICATOR_MANAGER, EmpathyIndicatorManagerClass))

typedef struct _EmpathyIndicatorManager      EmpathyIndicatorManager;
typedef struct _EmpathyIndicatorManagerClass EmpathyIndicatorManagerClass;

struct _EmpathyIndicatorManager {
  GObject parent;
  gpointer priv;
};

struct _EmpathyIndicatorManagerClass {
  GObjectClass parent_class;
};

GType              empathy_indicator_manager_get_type (void) G_GNUC_CONST;
EmpathyIndicatorManager *empathy_indicator_manager_dup_singleton (void);
void               empathy_indicator_manager_set_server_visible (EmpathyIndicatorManager *manager,
    gboolean visible);
EmpathyIndicator * empathy_indicator_manager_create_indicator (
    EmpathyIndicatorManager *manager,
    EmpathyContact          *sender,
    const gchar             *body);
void               empathy_indicator_manager_add_login_indicator (
    EmpathyIndicatorManager *manager,
    EmpathyContact          *contact);

G_END_DECLS

#endif /* _EMPATHY_INDICATOR_MANAGER_H_ */
