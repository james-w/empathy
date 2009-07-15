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

#include <gtk/gtk.h>

#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-utils.h>

#include "empathy-indicator.h"
#include "empathy-misc.h"

#include <libindicate/indicator-message.h>

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyIndicator)

enum {
  ACTIVATE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef struct {
  IndicateIndicator *indicator;
  EmpathyContact    *contact;
} EmpathyIndicatorPriv;

G_DEFINE_TYPE(EmpathyIndicator, empathy_indicator, G_TYPE_OBJECT)


static void
empathy_indicator_finalize (GObject *object)
{
  EmpathyIndicatorPriv *priv;

  priv = GET_PRIV (object);
  g_object_unref (priv->indicator);
  g_object_unref (priv->contact);
}


static void
empathy_indicator_class_init (EmpathyIndicatorClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = empathy_indicator_dispose;

  signals[ACTIVATE] =
      g_signal_new ("activate",
          G_TYPE_FROM_CLASS (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__VOID,
          G_TYPE_NONE,
          0);

  g_type_class_add_private (object_class, sizeof (EmpathyIndicatorPriv));
}


static void
indicate_show_cb (IndicateIndicator *indicator,
    EmpathyIndicator *e_indicator)
{
  g_signal_emit (e_indicator, signals[ACTIVATE], 0);
}


static void
empathy_indicator_init (EmpathyIndicator *e_indicator)
{
  GdkPixbuf *pixbuf;
  GTimeVal time;
  EmpathyIndicatorPriv *priv;

  priv = G_TYPE_INSTANCE_GET_PRIVATE (e_indicator,
      EMPATHY_TYPE_INDICATOR,
      EmpathyIndicatorPriv);

  e_indicator->priv = priv;

  priv->indicator = g_object_ref(INDICATE_INDICATOR (indicate_indicator_message_new ()));
  g_assert (priv->indicator);
  indicate_indicator_set_property (priv->indicator, "subtype",
      subtype);
  indicate_indicator_set_property (priv->indicator, "sender",
      empathy_contact_get_name(sender));
  priv->contact = g_object_ref(sender);
  pixbuf = empathy_misc_get_pixbuf_for_indicator (sender);
  if (pixbuf) {
      indicate_indicator_set_property_icon (priv->indicator, "icon",
          gdk_pixbuf_copy (pixbuf));
      g_object_unref (pixbuf);
  }
  if (body) {
      indicate_indicator_set_property (priv->indicator, "body",
          g_strdup (body));
  }
  g_get_current_time(&time);
  indicate_indicator_set_property_time (priv->indicator, "time", &time);
  g_signal_connect(G_OBJECT(priv->indicator),
      INDICATE_INDICATOR_SIGNAL_DISPLAY,
      G_CALLBACK(indicate_show_cb),
      e_indicator);

  return e_indicator;
}


EmpathyIndicator *
empathy_indicator_new (EmpathyContact *sender,
    const gchar *body,
    const gchar *subtype)
{
  EmpathyIndicator *e_indicator;

  e_indicator = g_object_new (EMPATHY_TYPE_INDICATOR, NULL);

  return e_indicator;
}


void
empathy_indicator_show (EmpathyIndicator *e_indicator)
{
  EmpathyIndicatorPriv *priv = GET_PRIV (e_indicator);

  indicate_indicator_show (priv->indicator);
}


void
empathy_indicator_hide (EmpathyIndicator *e_indicator)
{
  EmpathyIndicatorPriv *priv = GET_PRIV (e_indicator);

  indicate_indicator_hide (priv->indicator);
}


void
empathy_indicator_update (EmpathyIndicator *e_indicator,
    const gchar *body)
{
  EmpathyIndicatorPriv *priv;
  GTimeVal time;

  priv = GET_PRIV (e_indicator);
  g_get_current_time(&time);
  indicate_indicator_set_property_time(priv->indicator, "time", &time);
  if (body) {
    indicate_indicator_set_property (priv->indicator, "body",
        g_strdup(body));
  }
}


EmpathyContact *
empathy_indicator_get_contact (EmpathyIndicator *e_indicator)
{
  EmpathyIndicatorPriv *priv;
  priv = GET_PRIV (e_indicator);
  return priv->contact;
}
