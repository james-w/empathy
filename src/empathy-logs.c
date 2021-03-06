/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Collabora Ltd.
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
 * 
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <libempathy/empathy-debug.h>
#include <libempathy-gtk/empathy-log-window.h>

static void
destroy_cb (GtkWidget *dialog,
	    gpointer   user_data)
{
	gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
	GtkWidget *window;

	gtk_init (&argc, &argv);

	if (g_getenv ("EMPATHY_TIMING") != NULL) {
		g_log_set_default_handler (tp_debug_timestamped_log_handler, NULL);
	}
	empathy_debug_set_flags (g_getenv ("EMPATHY_DEBUG"));
	tp_debug_divert_messages (g_getenv ("EMPATHY_LOGFILE"));

	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   PKGDATADIR G_DIR_SEPARATOR_S "icons");
	window = empathy_log_window_show (NULL, NULL, FALSE, NULL);

	g_signal_connect (window, "destroy",
			  G_CALLBACK (destroy_cb),
			  NULL);

	gtk_main ();

	return EXIT_SUCCESS;
}

