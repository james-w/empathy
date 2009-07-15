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

#include <config.h>

#include <gtk/gtk.h>

#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-dispatcher.h>
#include <libempathy/empathy-utils.h>

#include <libempathy-gtk/empathy-conf.h>
#include <libempathy-gtk/empathy-ui-utils.h>

#include "empathy-event-manager.h"
#include "empathy-indicator.h"
#include "empathy-indicator-manager.h"
#include "empathy-misc.h"

#include <libindicate/server.h>

#define INDICATOR_LOGIN_TIMEOUT 15
#define EMPATHY_DESKTOP_PATH "/usr/share/applications/empathy.desktop"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyIndicatorManager)

enum {
  SERVER_ACTIVATE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef struct {
  EmpathyEventManager *event_manager;
  IndicateServer      *indicate_server;
  GSList              *indicator_events;
  GHashTable          *login_timeouts;
} EmpathyIndicatorManagerPriv;

typedef struct {
  EmpathyIndicator   *indicator;
  EmpathyEvent        *event;
} IndicatorEvent;

typedef struct {
  EmpathyIndicatorManager *manager;
  EmpathyIndicator *indicator;
} LoginData;

G_DEFINE_TYPE (EmpathyIndicatorManager, empathy_indicator_manager, G_TYPE_OBJECT);

static EmpathyIndicatorManager * manager_singleton = NULL;


static IndicatorEvent *
indicator_event_new (EmpathyIndicator *indicator,
    EmpathyEvent *event)
{
  IndicatorEvent *indicator_event;

  indicator_event = g_slice_new0 (IndicatorEvent);
  indicator_event->indicator = g_object_ref (indicator);
  indicator_event->event = g_object_ref (event);

  return indicator_event;
}


static void
indicator_event_free (IndicatorEvent *indicator_event)
{
  g_object_unref (indicator_event->indicator);
  g_free (indicator_event);
}


static void
indicate_server_activate (IndicateServer          *server,
    EmpathyIndicatorManager *manager)
{
  g_signal_emit (manager, signals[SERVER_ACTIVATE], 0);
}


static void
indicate_show_cb (EmpathyIndicator *indicator,
    EmpathyEvent     *event)
{
  empathy_event_activate (event);
}


static void
indicator_manager_event_added_cb (EmpathyEventManager *event_manager,
    EmpathyEvent        *event,
    EmpathyIndicatorManager   *manager)
{
  EmpathyIndicator *indicator;
  EmpathyIndicatorManagerPriv *priv;
  IndicatorEvent *indicator_event;

  priv = GET_PRIV (manager);

  if (event->contact == NULL)
    return;
  indicator = empathy_indicator_new (event->contact, event->message, "im");
  empathy_indicator_show (indicator);
  g_signal_connect (G_OBJECT(indicator), "activate",
      G_CALLBACK (indicate_show_cb),
      event);
  indicator_event = indicator_event_new (indicator, event);
  priv->indicator_events = g_slist_prepend (priv->indicator_events,
      indicator_event);
}


static void
indicator_manager_event_removed_cb (EmpathyEventManager *event_manager,
    EmpathyEvent        *event,
    EmpathyIndicatorManager   *manager)
{
  EmpathyIndicatorManagerPriv *priv;
  GSList *l;

  priv = GET_PRIV (manager);

  for (l = priv->indicator_events; l; l = l->next)
  {
    IndicatorEvent *indicator_event;
    indicator_event = l->data;
    if (indicator_event->event == event) {
      priv->indicator_events = g_slist_remove (priv->indicator_events,
          indicator_event);
      empathy_indicator_hide (indicator_event->indicator);
      indicator_event_free (indicator_event);
      return;
    }
  }

}


static void
indicator_manager_event_updated_cb (EmpathyEventManager *event_manager,
    EmpathyEvent        *event,
    EmpathyIndicatorManager   *manager)
{
  EmpathyIndicatorManagerPriv *priv;
  GSList *l;

  priv = GET_PRIV (manager);

  for (l = priv->indicator_events; l; l = l->next)
  {
    IndicatorEvent *indicator_event;
    indicator_event = l->data;
    if (indicator_event->event == event) {
      empathy_indicator_update (indicator_event->indicator,
          event->message);
      return;
    }
  }

}


static gboolean
indicate_login_timeout (gpointer data)
{
  LoginData *login_data;
  EmpathyIndicator *e_indicator;
  EmpathyIndicatorManager *manager;
  EmpathyIndicatorManagerPriv *priv;
  GValue *wrapped_timeout;
  guint login_timeout;

  login_data = (LoginData *)data;
  e_indicator = login_data->indicator;
  manager = login_data->manager;
  priv = GET_PRIV (manager);

  wrapped_timeout = g_hash_table_lookup (priv->login_timeouts, e_indicator);
  if (wrapped_timeout != NULL) {
    login_timeout = g_value_get_uint (wrapped_timeout);
    g_hash_table_remove (priv->login_timeouts, e_indicator);
    empathy_indicator_hide (e_indicator);
    g_object_unref(e_indicator);
    g_slice_free (GValue, wrapped_timeout);
  }
  g_object_unref (e_indicator);
  g_object_unref (manager);
  g_slice_free (LoginData, data);

  return FALSE;
}


static void
indicate_login_cb (EmpathyIndicator *e_indicator,
    EmpathyIndicatorManager *manager)
{
  EmpathyIndicatorManagerPriv *priv;
  GSList *events, *l;
  EmpathyContact *contact;

  priv = GET_PRIV (manager);

  g_hash_table_remove (priv->login_timeouts, e_indicator);
  empathy_indicator_hide (e_indicator);
  g_object_unref (e_indicator);

  contact = empathy_indicator_get_contact (e_indicator);
  /* If the contact has an event activate it, otherwise the
   * default handler of row-activated will be called. */
  events = empathy_event_manager_get_events (priv->event_manager);
  for (l = events; l; l = l->next) {
    EmpathyEvent *event;

    event = l->data;
    if (event->contact == contact) {
      empathy_event_activate (event);
      return;
    }
  }

  /* Else start a new conversation */
  empathy_dispatcher_chat_with_contact (contact, NULL, NULL);
}


EmpathyIndicatorManager *
empathy_indicator_manager_dup_singleton (void)
{
  return g_object_new (EMPATHY_TYPE_INDICATOR_MANAGER, NULL);
}


static void
indicator_manager_finalize (GObject *object)
{
  EmpathyIndicatorManagerPriv *priv;

  priv = GET_PRIV (object);
  g_slist_foreach (priv->indicator_events, (GFunc) indicator_event_free,
          NULL);
  g_slist_free (priv->indicator_events);
  g_object_unref (priv->event_manager);
  g_object_unref (priv->indicate_server);
}


static GObject *
indicator_manager_constructor (GType type,
    guint n_props,
    GObjectConstructParam *props)
{
  GObject *retval;

  if (manager_singleton != NULL) {
    retval = g_object_ref (manager_singleton);
  } else {
    retval = G_OBJECT_CLASS (empathy_indicator_manager_parent_class)->constructor
      (type, n_props, props);

    manager_singleton = EMPATHY_INDICATOR_MANAGER (retval);
    g_object_add_weak_pointer (retval, (gpointer) &manager_singleton);
  }

  return retval;
}


static void
empathy_indicator_manager_class_init (EmpathyIndicatorManagerClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = indicator_manager_finalize;
  object_class->constructor = indicator_manager_constructor;

  signals[SERVER_ACTIVATE] =
      g_signal_new ("server-activate",
          G_TYPE_FROM_CLASS (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__VOID,
          G_TYPE_NONE,
          0);

  g_type_class_add_private (object_class, sizeof (EmpathyIndicatorManagerPriv));
}


static void
empathy_indicator_manager_init (EmpathyIndicatorManager *manager)
{
  EmpathyIndicatorManagerPriv *priv;

  priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
       EMPATHY_TYPE_INDICATOR_MANAGER, EmpathyIndicatorManagerPriv);
  manager->priv = priv;

  priv->event_manager = empathy_event_manager_dup_singleton ();
  priv->login_timeouts = g_hash_table_new (NULL, NULL);
  priv->indicate_server = indicate_server_ref_default ();
  indicate_server_set_type (priv->indicate_server, "message.instant");
  indicate_server_set_desktop_file (priv->indicate_server,
      EMPATHY_DESKTOP_PATH);

  g_signal_connect (priv->indicate_server,
      INDICATE_SERVER_SIGNAL_SERVER_DISPLAY,
      G_CALLBACK (indicate_server_activate),
      manager);

  g_signal_connect (priv->event_manager, "event-added",
      G_CALLBACK (indicator_manager_event_added_cb),
      manager);
  g_signal_connect (priv->event_manager, "event-removed",
      G_CALLBACK (indicator_manager_event_removed_cb),
      manager);
  g_signal_connect (priv->event_manager, "event-updated",
      G_CALLBACK (indicator_manager_event_updated_cb),
      manager);
}


void
empathy_indicator_manager_set_server_visible (EmpathyIndicatorManager *manager,
    gboolean visible)
{
  EmpathyIndicatorManagerPriv *priv;

  priv = GET_PRIV (manager);
  if (visible) {
    indicate_server_show (priv->indicate_server);
  } else {
    /* Causes a crash on next show currently due to object not being
       unregistered from dbus
    indicate_server_hide (priv->indicate_server);
    */
  }
}


EmpathyIndicator *
empathy_indicator_manager_create_indicator (EmpathyIndicatorManager *manager,
    EmpathyContact          *sender,
    const gchar             *body)
{
  return empathy_indicator_new (sender, body, "im");
}


static LoginData *
login_data_new (EmpathyIndicator *e_indicator,
    EmpathyIndicatorManager *manager)
{
    LoginData *login_data;

    login_data = g_slice_new0 (LoginData);
    login_data->indicator = e_indicator;
    login_data->manager = g_object_ref (manager);

    return login_data;
}


/* Add an indicator for someone logging in. This will be displayed for
 * a short period only.
 */
void
empathy_indicator_manager_add_login_indicator (EmpathyIndicatorManager *manager,
    EmpathyContact          *contact)
{
  EmpathyIndicatorManagerPriv *priv;
  guint login_timeout;
  GValue *wrapped_timeout;
  EmpathyIndicator *e_indicator;
  LoginData *login_data;

  priv = GET_PRIV (manager);
  e_indicator = empathy_indicator_new (contact, NULL, "login");
  login_data = login_data_new (e_indicator, manager);

  login_timeout = g_timeout_add_seconds (INDICATOR_LOGIN_TIMEOUT,
      indicate_login_timeout,
      login_data);
  wrapped_timeout = g_slice_new0 (GValue);
  g_value_init (wrapped_timeout, G_TYPE_UINT);
  g_value_set_uint (wrapped_timeout, login_timeout);
  g_hash_table_insert (priv->login_timeouts, e_indicator, wrapped_timeout);
  g_signal_connect (e_indicator, "activate",
      G_CALLBACK (indicate_login_cb), manager);
  empathy_indicator_show (e_indicator);
}
