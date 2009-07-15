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

#ifndef _EMPATHY_INDICATOR_H_
#define _EMPATHY_INDICATOR_H_

#include <glib.h>

#include <libempathy/empathy-contact.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_INDICATOR         (empathy_indicator_get_type ())
#define EMPATHY_INDICATOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_INDICATOR, EmpathyIndicator))
#define EMPATHY_INDICATOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_INDICATOR, EmpathyIndicatorClass))
#define EMPATHY_IS_INDICATOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_INDICATOR))
#define EMPATHY_IS_INDICATOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_INDICATOR))
#define EMPATHY_INDICATOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_INDICATOR, EmpathyIndicatorClass))

typedef struct _EmpathyIndicator      EmpathyIndicator;
typedef struct _EmpathyIndicatorClass EmpathyIndicatorClass;

struct _EmpathyIndicator {
  GObject parent;
  gpointer priv;
};

struct _EmpathyIndicatorClass {
  GObjectClass parent_class;
};

GType              empathy_indicator_get_type (void) G_GNUC_CONST;
EmpathyIndicator  *empathy_indicator_new (EmpathyContact *sender,
    const gchar *body,
    const gchar *type);
void               empathy_indicator_show (EmpathyIndicator *e_indicator);
void               empathy_indicator_hide (EmpathyIndicator *e_indicator);
void               empathy_indicator_update (EmpathyIndicator *e_indicator,
    const gchar *body);
EmpathyContact    *empathy_indicator_get_contact (EmpathyIndicator *e_indicator);

G_END_DECLS


#endif /* _EMPATHY-INDICATOR_H_ */
