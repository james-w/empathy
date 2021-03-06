/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007-2008 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#include <string.h>

#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include <telepathy-glib/dbus.h>
#include <libmissioncontrol/mc-enum-types.h>

#include "empathy-idle.h"
#include "empathy-utils.h" 

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include "empathy-debug.h"

/* Number of seconds before entering extended autoaway. */
#define EXT_AWAY_TIME (30*60)

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyIdle)
typedef struct {
	MissionControl *mc;
	DBusGProxy     *gs_proxy;
	DBusGProxy     *nm_proxy;

	McPresence      state;
	gchar          *status;
	McPresence      flash_state;
	gboolean        auto_away;
	gboolean        use_nm;

	gboolean        away_reset_status;
	McPresence      away_saved_state;
	gboolean        nm_reset_status;
	McPresence      nm_saved_state;

	gboolean        is_idle;
	gboolean        nm_connected;
	guint           ext_away_timeout;
} EmpathyIdlePriv;

typedef enum {
	NM_STATE_UNKNOWN,
	NM_STATE_ASLEEP,
	NM_STATE_CONNECTING,
	NM_STATE_CONNECTED,
	NM_STATE_DISCONNECTED
} NMState;

static void     idle_finalize                (GObject          *object);
static void     idle_get_property            (GObject          *object,
					      guint             param_id,
					      GValue           *value,
					      GParamSpec       *pspec);
static void     idle_set_property            (GObject          *object,
					      guint             param_id,
					      const GValue     *value,
					      GParamSpec       *pspec);
static void     idle_presence_changed_cb     (MissionControl   *mc,
					      McPresence        state,
					      gchar            *status,
					      EmpathyIdle      *idle);
static void     idle_session_idle_changed_cb (DBusGProxy       *gs_proxy,
					      gboolean          is_idle,
					      EmpathyIdle      *idle);
static void     idle_nm_state_change_cb      (DBusGProxy       *proxy,
					      guint             state,
					      EmpathyIdle      *idle);
static void     idle_ext_away_start          (EmpathyIdle      *idle);
static void     idle_ext_away_stop           (EmpathyIdle      *idle);
static gboolean idle_ext_away_cb             (EmpathyIdle      *idle);

enum {
	PROP_0,
	PROP_STATE,
	PROP_STATUS,
	PROP_FLASH_STATE,
	PROP_AUTO_AWAY,
	PROP_USE_NM
};

G_DEFINE_TYPE (EmpathyIdle, empathy_idle, G_TYPE_OBJECT)

static void
empathy_idle_class_init (EmpathyIdleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = idle_finalize;
	object_class->get_property = idle_get_property;
	object_class->set_property = idle_set_property;

	g_object_class_install_property (object_class,
					 PROP_STATE,
					 g_param_spec_enum ("state",
							    "state",
							    "state",
							    MC_TYPE_PRESENCE,
							    MC_PRESENCE_AVAILABLE,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_STATUS,
					 g_param_spec_string ("status",
							      "status",
							      "status",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_FLASH_STATE,
					 g_param_spec_enum ("flash-state",
							    "flash-state",
							    "flash-state",
							    MC_TYPE_PRESENCE,
							    MC_PRESENCE_UNSET,
							    G_PARAM_READWRITE));

	 g_object_class_install_property (object_class,
					  PROP_AUTO_AWAY,
					  g_param_spec_boolean ("auto-away",
								"Automatic set presence to away",
								"Should it set presence to away if inactive",
								FALSE,
								G_PARAM_READWRITE));

	 g_object_class_install_property (object_class,
					  PROP_USE_NM,
					  g_param_spec_boolean ("use-nm",
								"Use Network Manager",
								"Set presence according to Network Manager",
								FALSE,
								G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EmpathyIdlePriv));
}

static void
empathy_idle_init (EmpathyIdle *idle)
{
	DBusGConnection *system_bus;
	GError          *error = NULL;
	EmpathyIdlePriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (idle,
		EMPATHY_TYPE_IDLE, EmpathyIdlePriv);

	idle->priv = priv;
	priv->is_idle = FALSE;
	priv->mc = empathy_mission_control_new ();
	priv->state = mission_control_get_presence_actual (priv->mc, NULL);
	priv->status = mission_control_get_presence_message_actual (priv->mc, NULL);
	if (G_STR_EMPTY (priv->status)) {
		g_free (priv->status);
		priv->status = NULL;
	}

	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->mc),
				     "PresenceChanged",
				     G_CALLBACK (idle_presence_changed_cb),
				     idle, NULL);

	priv->gs_proxy = dbus_g_proxy_new_for_name (tp_get_bus (),
						    "org.gnome.ScreenSaver",
						    "/org/gnome/ScreenSaver",
						    "org.gnome.ScreenSaver");
	if (priv->gs_proxy) {
		dbus_g_proxy_add_signal (priv->gs_proxy, "SessionIdleChanged",
					 G_TYPE_BOOLEAN,
					 G_TYPE_INVALID);
		dbus_g_proxy_connect_signal (priv->gs_proxy, "SessionIdleChanged",
					     G_CALLBACK (idle_session_idle_changed_cb),
					     idle, NULL);
	} else {
		DEBUG ("Failed to get gs proxy");
	}


	system_bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (!system_bus) {
		DEBUG ("Failed to get system bus: %s",
			error ? error->message : "No error given");
	} else {
		priv->nm_proxy = dbus_g_proxy_new_for_name (system_bus,
							    "org.freedesktop.NetworkManager",
							    "/org/freedesktop/NetworkManager",
							    "org.freedesktop.NetworkManager");
	}
	if (priv->nm_proxy) {
		dbus_g_proxy_add_signal (priv->nm_proxy, "StateChange",
					 G_TYPE_UINT, G_TYPE_INVALID);
		dbus_g_proxy_connect_signal (priv->nm_proxy, "StateChange",
					     G_CALLBACK (idle_nm_state_change_cb),
					     idle, NULL);
	} else {
		DEBUG ("Failed to get nm proxy");
	}

	priv->nm_connected = TRUE;
}

static void
idle_finalize (GObject *object)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->status);
	g_object_unref (priv->mc);

	if (priv->gs_proxy) {
		g_object_unref (priv->gs_proxy);
	}

	idle_ext_away_stop (EMPATHY_IDLE (object));
}

static void
idle_get_property (GObject    *object,
		   guint       param_id,
		   GValue     *value,
		   GParamSpec *pspec)
{
	EmpathyIdlePriv *priv;
	EmpathyIdle     *idle;

	priv = GET_PRIV (object);
	idle = EMPATHY_IDLE (object);

	switch (param_id) {
	case PROP_STATE:
		g_value_set_enum (value, empathy_idle_get_state (idle));
		break;
	case PROP_STATUS:
		g_value_set_string (value, empathy_idle_get_status (idle));
		break;
	case PROP_FLASH_STATE:
		g_value_set_enum (value, empathy_idle_get_flash_state (idle));
		break;
	case PROP_AUTO_AWAY:
		g_value_set_boolean (value, empathy_idle_get_auto_away (idle));
		break;
	case PROP_USE_NM:
		g_value_set_boolean (value, empathy_idle_get_use_nm (idle));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
idle_set_property (GObject      *object,
		   guint         param_id,
		   const GValue *value,
		   GParamSpec   *pspec)
{
	EmpathyIdlePriv *priv;
	EmpathyIdle     *idle;

	priv = GET_PRIV (object);
	idle = EMPATHY_IDLE (object);

	switch (param_id) {
	case PROP_STATE:
		empathy_idle_set_state (idle, g_value_get_enum (value));
		break;
	case PROP_STATUS:
		empathy_idle_set_status (idle, g_value_get_string (value));
		break;
	case PROP_FLASH_STATE:
		empathy_idle_set_flash_state (idle, g_value_get_enum (value));
		break;
	case PROP_AUTO_AWAY:
		empathy_idle_set_auto_away (idle, g_value_get_boolean (value));
		break;
	case PROP_USE_NM:
		empathy_idle_set_use_nm (idle, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

EmpathyIdle *
empathy_idle_new (void)
{
	static EmpathyIdle *idle = NULL;

	if (!idle) {
		idle = g_object_new (EMPATHY_TYPE_IDLE, NULL);
		g_object_add_weak_pointer (G_OBJECT (idle), (gpointer) &idle);
	} else {
		g_object_ref (idle);
	}

	return idle;
}

McPresence
empathy_idle_get_state (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	return priv->state;
}

void
empathy_idle_set_state (EmpathyIdle *idle,
			McPresence   state)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	empathy_idle_set_presence (idle, state, priv->status);
}

const gchar *
empathy_idle_get_status (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	if (!priv->status) {
		return empathy_presence_get_default_message (priv->state);
	}

	return priv->status;
}

void
empathy_idle_set_status (EmpathyIdle *idle,
			 const gchar *status)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	empathy_idle_set_presence (idle, priv->state, status);
}

McPresence
empathy_idle_get_flash_state (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	return priv->flash_state;
}

void
empathy_idle_set_flash_state (EmpathyIdle *idle,
			      McPresence   state)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	priv->flash_state = state;

	if (state == MC_PRESENCE_UNSET) {
	}

	g_object_notify (G_OBJECT (idle), "flash-state");
}

void
empathy_idle_set_presence (EmpathyIdle *idle,
			   McPresence   state,
			   const gchar *status)
{
	EmpathyIdlePriv *priv;
	const gchar     *default_status;

	priv = GET_PRIV (idle);

	DEBUG ("Changing presence to %s (%d)",
		       status, state);

	if (!priv->nm_connected) {
		DEBUG ("NM not connected");
		return;
	}

	/* Do not set translated default messages */
	default_status = empathy_presence_get_default_message (state);
	if (status && strcmp (status, default_status) == 0) {
		status = NULL;
	}

	mission_control_set_presence (priv->mc,
				      state,
				      status,
				      NULL, NULL);
}

gboolean
empathy_idle_get_auto_away (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv = GET_PRIV (idle);

	return priv->auto_away;
}

void
empathy_idle_set_auto_away (EmpathyIdle *idle,
			    gboolean     auto_away)
{
	EmpathyIdlePriv *priv = GET_PRIV (idle);

	priv->auto_away = auto_away;

	g_object_notify (G_OBJECT (idle), "auto-away");
}

gboolean
empathy_idle_get_use_nm (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv = GET_PRIV (idle);

	return priv->use_nm;
}

void
empathy_idle_set_use_nm (EmpathyIdle *idle,
			 gboolean     use_nm)
{
	EmpathyIdlePriv *priv = GET_PRIV (idle);

	if (!priv->nm_proxy || use_nm == priv->use_nm) {
		return;
	}

	priv->use_nm = use_nm;

	if (use_nm) {
		guint   nm_status;
		GError *error = NULL;

		dbus_g_proxy_call (priv->nm_proxy, "state",
				   &error,
				   G_TYPE_INVALID,
				   G_TYPE_UINT, &nm_status,
				   G_TYPE_INVALID);

		if (error) {
			DEBUG ("Couldn't get NM state: %s", error->message);
			g_clear_error (&error);
			nm_status = NM_STATE_ASLEEP;
		}
		
		idle_nm_state_change_cb (priv->nm_proxy, nm_status, idle);
	} else {
		if (!priv->nm_connected) {
			empathy_idle_set_state (idle, priv->nm_saved_state);
		}
		priv->nm_connected = TRUE;
		priv->nm_saved_state = MC_PRESENCE_UNSET;
	}

	g_object_notify (G_OBJECT (idle), "use-nm");
}

static void
idle_presence_changed_cb (MissionControl *mc,
			  McPresence      state,
			  gchar          *status,
			  EmpathyIdle    *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	DEBUG ("Presence changed to '%s' (%d)", status, state);

	g_free (priv->status);
	priv->state = state;
	priv->status = NULL;
	if (!G_STR_EMPTY (status)) {
		priv->status = g_strdup (status);
	}

	g_object_notify (G_OBJECT (idle), "state");
	g_object_notify (G_OBJECT (idle), "status");
}

static void
idle_session_idle_changed_cb (DBusGProxy  *gs_proxy,
			      gboolean     is_idle,
			      EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	DEBUG ("Session idle state changed, %s -> %s",
		priv->is_idle ? "yes" : "no",
		is_idle ? "yes" : "no");

	if (priv->state <= MC_PRESENCE_OFFLINE ||
	    priv->state == MC_PRESENCE_HIDDEN ||
	    !priv->auto_away) {
		/* We are not online or we don't want to go auto away,
		 * nothing to do here */
		priv->is_idle = is_idle;
		return;
	}

	if (is_idle && !priv->is_idle) {
		McPresence new_state;
		/* We are now idle */

		if (priv->state == MC_PRESENCE_AWAY ||
		    priv->state == MC_PRESENCE_EXTENDED_AWAY) {
			/* User set away manually, when coming back we restore
			 * default presence. */
			new_state = priv->state;
			priv->away_saved_state = MC_PRESENCE_AVAILABLE;
			priv->away_reset_status = TRUE;
		} else {
			new_state = MC_PRESENCE_AWAY;
			priv->away_saved_state = priv->state;
			priv->away_reset_status = FALSE;
		}

		DEBUG ("Going to autoaway");
		empathy_idle_set_state (idle, new_state);

		idle_ext_away_start (idle);
	} else if (!is_idle && priv->is_idle) {
		/* We are no more idle, restore state */
		idle_ext_away_stop (idle);

		DEBUG ("Restoring state to %d, reset status: %s",
			priv->away_saved_state,
			priv->away_reset_status ? "Yes" : "No");

		if (priv->nm_connected) {
			empathy_idle_set_presence (idle,
						   priv->away_saved_state,
						   priv->away_reset_status ? NULL : priv->status);
		} else {
			/* We can't restore state now, will do when NM gets
			 * connected. */
			priv->nm_saved_state = priv->away_saved_state;
			priv->nm_reset_status = priv->away_reset_status;
		}

		priv->away_saved_state = MC_PRESENCE_UNSET;
		priv->away_reset_status = FALSE;
	}

	priv->is_idle = is_idle;
}

static void
idle_nm_state_change_cb (DBusGProxy  *proxy,
			 guint        state,
			 EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;
	gboolean         old_nm_connected;
	gboolean         new_nm_connected;

	priv = GET_PRIV (idle);

	DEBUG ("New network state (%d), in use = %s",
		state, priv->use_nm ? "Yes" : "No");

	if (!priv->use_nm) {
		return;
	}

	old_nm_connected = priv->nm_connected;
	new_nm_connected = !(state == NM_STATE_CONNECTING ||
			     state == NM_STATE_DISCONNECTED);
	priv->nm_connected = TRUE; /* To be sure _set_state will work */

	if (old_nm_connected && !new_nm_connected) {
		/* We are no more connected */
		idle_ext_away_stop (idle);

		priv->nm_saved_state = priv->state;
		empathy_idle_set_state (idle, MC_PRESENCE_OFFLINE);
	}
	else if (!old_nm_connected && new_nm_connected) {
		/* We are now connected */
		empathy_idle_set_presence (idle,
					   priv->nm_saved_state,
					   priv->nm_reset_status ? NULL : priv->status);
		priv->nm_saved_state = MC_PRESENCE_UNSET;
		priv->nm_reset_status = FALSE;
	}

	priv->nm_connected = new_nm_connected;
}

static void
idle_ext_away_start (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	idle_ext_away_stop (idle);
	priv->ext_away_timeout = g_timeout_add_seconds (EXT_AWAY_TIME,
							(GSourceFunc) idle_ext_away_cb,
							idle);
}

static void
idle_ext_away_stop (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	if (priv->ext_away_timeout) {
		g_source_remove (priv->ext_away_timeout);
		priv->ext_away_timeout = 0;
	}
}

static gboolean
idle_ext_away_cb (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	DEBUG ("Going to extended autoaway");
	empathy_idle_set_state (idle, MC_PRESENCE_EXTENDED_AWAY);
	priv->ext_away_timeout = 0;

	return FALSE;
}

