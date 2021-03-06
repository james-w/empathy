/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
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
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <sys/types.h>
#include <string.h>
#include <time.h>

#include <glib/gi18n.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtksizegroup.h>
#include <glade/glade.h>

#include <telepathy-glib/util.h>
#include <libmissioncontrol/mc-account.h>

#include <libempathy/empathy-utils.h>

#include "empathy-chat-view.h"
#include "empathy-chat.h"
#include "empathy-conf.h"
#include "empathy-theme-manager.h"
#include "empathy-ui-utils.h"
#include "empathy-smiley-manager.h"

#define DEBUG_FLAG EMPATHY_DEBUG_CHAT
#include <libempathy/empathy-debug.h>

/* Number of seconds between timestamps when using normal mode, 5 minutes. */
#define TIMESTAMP_INTERVAL 300

#define MAX_LINES 800
#define MAX_SCROLL_TIME 0.4 /* seconds */
#define SCROLL_DELAY 33     /* milliseconds */

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyChatView)
typedef struct {
	GtkTextBuffer *buffer;

	EmpathyTheme   *theme;

	time_t         last_timestamp;

	gboolean       allow_scrolling;
	guint          scroll_timeout;
	GTimer        *scroll_time;

	GtkTextMark   *find_mark_previous;
	GtkTextMark   *find_mark_next;
	gboolean       find_wrapped;
	gboolean       find_last_direction;

	/* This is for the group chat so we know if the "other" last contact
	 * changed, so we know whether to insert a header or not.
	 */
	EmpathyContact *last_contact;

	guint          notify_system_fonts_id;
	guint          notify_show_avatars_id;
} EmpathyChatViewPriv;

static void     chat_view_finalize                   (GObject                  *object);
static gboolean chat_view_drag_motion                (GtkWidget                *widget,
						      GdkDragContext           *context,
						      gint                      x,
						      gint                      y,
						      guint                     time);
static void     chat_view_size_allocate              (GtkWidget                *widget,
						      GtkAllocation            *alloc);
static void     chat_view_setup_tags                 (EmpathyChatView           *view);
static void     chat_view_system_font_update         (EmpathyChatView           *view);
static void     chat_view_notify_system_font_cb      (EmpathyConf               *conf,
						      const gchar              *key,
						      gpointer                  user_data);
static void     chat_view_notify_show_avatars_cb     (EmpathyConf               *conf,
						      const gchar              *key,
						      gpointer                  user_data);
static void     chat_view_populate_popup             (EmpathyChatView           *view,
						      GtkMenu                  *menu,
						      gpointer                  user_data);
static gboolean chat_view_event_cb                   (EmpathyChatView           *view,
						      GdkEventMotion           *event,
						      GtkTextTag               *tag);
static gboolean chat_view_url_event_cb               (GtkTextTag               *tag,
						      GObject                  *object,
						      GdkEvent                 *event,
						      GtkTextIter              *iter,
						      GtkTextBuffer            *buffer);
static void     chat_view_open_address_cb            (GtkMenuItem              *menuitem,
						      const gchar              *url);
static void     chat_view_copy_address_cb            (GtkMenuItem              *menuitem,
						      const gchar              *url);
static void     chat_view_clear_view_cb              (GtkMenuItem              *menuitem,
						      EmpathyChatView           *view);
static gboolean chat_view_is_scrolled_down           (EmpathyChatView           *view);
static void     chat_view_theme_changed_cb           (EmpathyThemeManager       *manager,
						      EmpathyChatView           *view);
static void     chat_view_theme_notify_cb            (EmpathyTheme              *theme,
						      GParamSpec                *param,
						      EmpathyChatView           *view);

G_DEFINE_TYPE (EmpathyChatView, empathy_chat_view, GTK_TYPE_TEXT_VIEW);

static void
empathy_chat_view_class_init (EmpathyChatViewClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = chat_view_finalize;
	widget_class->size_allocate = chat_view_size_allocate;
 	widget_class->drag_motion = chat_view_drag_motion; 

	g_type_class_add_private (object_class, sizeof (EmpathyChatViewPriv));
}

static void
empathy_chat_view_init (EmpathyChatView *view)
{
	gboolean             show_avatars;
	EmpathyChatViewPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (view,
		EMPATHY_TYPE_CHAT_VIEW, EmpathyChatViewPriv);

	view->priv = priv;
	priv->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	priv->last_timestamp = 0;
	priv->allow_scrolling = TRUE;

	g_object_set (view,
		      "wrap-mode", GTK_WRAP_WORD_CHAR,
		      "editable", FALSE,
		      "cursor-visible", FALSE,
		      NULL);

	priv->notify_system_fonts_id =
		empathy_conf_notify_add (empathy_conf_get (),
					 "/desktop/gnome/interface/document_font_name",
					 chat_view_notify_system_font_cb,
					 view);
	chat_view_system_font_update (view);

	priv->notify_show_avatars_id =
		empathy_conf_notify_add (empathy_conf_get (),
					 EMPATHY_PREFS_UI_SHOW_AVATARS,
					 chat_view_notify_show_avatars_cb,
					 view);

	chat_view_setup_tags (view);

	empathy_theme_manager_apply_saved (empathy_theme_manager_get (), view);

	show_avatars = FALSE;
	empathy_conf_get_bool (empathy_conf_get (),
			       EMPATHY_PREFS_UI_SHOW_AVATARS,
			       &show_avatars);

	empathy_theme_set_show_avatars (priv->theme, show_avatars);

	g_signal_connect (view,
			  "populate-popup",
			  G_CALLBACK (chat_view_populate_popup),
			  NULL);

	g_signal_connect_object (empathy_theme_manager_get (),
				 "theme-changed",
				 G_CALLBACK (chat_view_theme_changed_cb),
				 view,
				 0);
}

static void
chat_view_finalize (GObject *object)
{
	EmpathyChatView     *view;
	EmpathyChatViewPriv *priv;

	view = EMPATHY_CHAT_VIEW (object);
	priv = GET_PRIV (view);

	DEBUG ("finalize: %p", object);

	empathy_conf_notify_remove (empathy_conf_get (), priv->notify_system_fonts_id);
	empathy_conf_notify_remove (empathy_conf_get (), priv->notify_show_avatars_id);

	if (priv->last_contact) {
		g_object_unref (priv->last_contact);
	}
	if (priv->scroll_time) {
		g_timer_destroy (priv->scroll_time);
	}
	if (priv->scroll_timeout) {
		g_source_remove (priv->scroll_timeout);
	}

	if (priv->theme) {
		g_signal_handlers_disconnect_by_func (priv->theme,
						      chat_view_theme_notify_cb,
						      view);
		g_object_unref (priv->theme);
	}

	G_OBJECT_CLASS (empathy_chat_view_parent_class)->finalize (object);
}

static gboolean
chat_view_drag_motion (GtkWidget        *widget,
		       GdkDragContext   *context,
		       gint              x,
		       gint              y,
		       guint             time)
{
	/* Don't handle drag motion, since we don't want the view to scroll as
	 * the result of dragging something across it.
	 */

	return FALSE;
}

static void
chat_view_size_allocate (GtkWidget     *widget,
			 GtkAllocation *alloc)
{
	gboolean down;

	down = chat_view_is_scrolled_down (EMPATHY_CHAT_VIEW (widget));

	GTK_WIDGET_CLASS (empathy_chat_view_parent_class)->size_allocate (widget, alloc);

	if (down) {
		GtkAdjustment *adj;

		adj = GTK_TEXT_VIEW (widget)->vadjustment;
		gtk_adjustment_set_value (adj, adj->upper - adj->page_size);
	}
}

static void
chat_view_setup_tags (EmpathyChatView *view)
{
	EmpathyChatViewPriv *priv;
	GtkTextTag         *tag;

	priv = GET_PRIV (view);

	gtk_text_buffer_create_tag (priv->buffer,
				    "cut",
				    NULL);

	/* FIXME: Move to the theme and come up with something that looks a bit
	 * nicer.
	 */
	gtk_text_buffer_create_tag (priv->buffer,
				    "highlight",
				    "background", "yellow",
				    NULL);

	tag = gtk_text_buffer_create_tag (priv->buffer,
					  "link",
					  NULL);

	g_signal_connect (tag,
			  "event",
			  G_CALLBACK (chat_view_url_event_cb),
			  priv->buffer);

	g_signal_connect (view,
			  "motion-notify-event",
			  G_CALLBACK (chat_view_event_cb),
			  tag);
}

static void
chat_view_system_font_update (EmpathyChatView *view)
{
	PangoFontDescription *font_description = NULL;
	gchar                *font_name;

	if (empathy_conf_get_string (empathy_conf_get (),
				     "/desktop/gnome/interface/document_font_name",
				     &font_name) && font_name) {
		font_description = pango_font_description_from_string (font_name);
		g_free (font_name);
	} else {
		font_description = NULL;
	}

	gtk_widget_modify_font (GTK_WIDGET (view), font_description);

	if (font_description) {
		pango_font_description_free (font_description);
	}
}

static void
chat_view_notify_system_font_cb (EmpathyConf  *conf,
				 const gchar *key,
				 gpointer     user_data)
{
	EmpathyChatView *view;
	EmpathyChatViewPriv *priv;
	gboolean        show_avatars = FALSE;

	view = user_data;
	priv = GET_PRIV (view);

	chat_view_system_font_update (view);

	/* Ugly, again, to adjust the vertical position of the nick... Will fix
	 * this when reworking the theme manager so that view register
	 * themselves with it instead of the other way around.
	 */
	empathy_conf_get_bool (conf,
			       EMPATHY_PREFS_UI_SHOW_AVATARS,
			       &show_avatars);

	empathy_theme_set_show_avatars (priv->theme, show_avatars);
}

static void
chat_view_notify_show_avatars_cb (EmpathyConf  *conf,
				  const gchar *key,
				  gpointer     user_data)
{
	EmpathyChatView     *view;
	EmpathyChatViewPriv *priv;
	gboolean            show_avatars = FALSE;

	view = user_data;
	priv = GET_PRIV (view);

	empathy_conf_get_bool (conf, key, &show_avatars);

	empathy_theme_set_show_avatars (priv->theme, show_avatars);
}

static void
chat_view_populate_popup (EmpathyChatView *view,
			  GtkMenu        *menu,
			  gpointer        user_data)
{
	EmpathyChatViewPriv *priv;
	GtkTextTagTable    *table;
	GtkTextTag         *tag;
	gint                x, y;
	GtkTextIter         iter, start, end;
	GtkWidget          *item;
	gchar              *str = NULL;

	priv = GET_PRIV (view);

	/* Clear menu item */
	if (gtk_text_buffer_get_char_count (priv->buffer) > 0) {
		item = gtk_menu_item_new ();
		gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);

		item = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLEAR, NULL);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);

		g_signal_connect (item,
				  "activate",
				  G_CALLBACK (chat_view_clear_view_cb),
				  view);
	}

	/* Link context menu items */
	table = gtk_text_buffer_get_tag_table (priv->buffer);
	tag = gtk_text_tag_table_lookup (table, "link");

	gtk_widget_get_pointer (GTK_WIDGET (view), &x, &y);

	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view),
					       GTK_TEXT_WINDOW_WIDGET,
					       x, y,
					       &x, &y);

	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &iter, x, y);

	start = end = iter;

	if (gtk_text_iter_backward_to_tag_toggle (&start, tag) &&
	    gtk_text_iter_forward_to_tag_toggle (&end, tag)) {
		str = gtk_text_buffer_get_text (priv->buffer,
						&start, &end, FALSE);
	}

	if (G_STR_EMPTY (str)) {
		g_free (str);
		return;
	}

	/* NOTE: Set data just to get the string freed when not needed. */
	g_object_set_data_full (G_OBJECT (menu),
				"url", str,
				(GDestroyNotify) g_free);

	item = gtk_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	item = gtk_menu_item_new_with_mnemonic (_("_Copy Link Address"));
	g_signal_connect (item,
			  "activate",
			  G_CALLBACK (chat_view_copy_address_cb),
			  str);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	item = gtk_menu_item_new_with_mnemonic (_("_Open Link"));
	g_signal_connect (item,
			  "activate",
			  G_CALLBACK (chat_view_open_address_cb),
			  str);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
}

static gboolean
chat_view_event_cb (EmpathyChatView *view,
		    GdkEventMotion *event,
		    GtkTextTag     *tag)
{
	static GdkCursor  *hand = NULL;
	static GdkCursor  *beam = NULL;
	GtkTextWindowType  type;
	GtkTextIter        iter;
	GdkWindow         *win;
	gint               x, y, buf_x, buf_y;

	type = gtk_text_view_get_window_type (GTK_TEXT_VIEW (view),
					      event->window);

	if (type != GTK_TEXT_WINDOW_TEXT) {
		return FALSE;
	}

	/* Get where the pointer really is. */
	win = gtk_text_view_get_window (GTK_TEXT_VIEW (view), type);
	if (!win) {
		return FALSE;
	}

	gdk_window_get_pointer (win, &x, &y, NULL);

	/* Get the iter where the cursor is at */
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view), type,
					       x, y,
					       &buf_x, &buf_y);

	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view),
					    &iter,
					    buf_x, buf_y);

	if (gtk_text_iter_has_tag (&iter, tag)) {
		if (!hand) {
			hand = gdk_cursor_new (GDK_HAND2);
			beam = gdk_cursor_new (GDK_XTERM);
		}
		gdk_window_set_cursor (win, hand);
	} else {
		if (!beam) {
			beam = gdk_cursor_new (GDK_XTERM);
		}
		gdk_window_set_cursor (win, beam);
	}

	return FALSE;
}

static gboolean
chat_view_url_event_cb (GtkTextTag    *tag,
			GObject       *object,
			GdkEvent      *event,
			GtkTextIter   *iter,
			GtkTextBuffer *buffer)
{
	GtkTextIter  start, end;
	gchar       *str;

	/* If the link is being selected, don't do anything. */
	gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
	if (gtk_text_iter_get_offset (&start) != gtk_text_iter_get_offset (&end)) {
		return FALSE;
	}

	if (event->type == GDK_BUTTON_RELEASE && event->button.button == 1) {
		start = end = *iter;

		if (gtk_text_iter_backward_to_tag_toggle (&start, tag) &&
		    gtk_text_iter_forward_to_tag_toggle (&end, tag)) {
			str = gtk_text_buffer_get_text (buffer,
							&start,
							&end,
							FALSE);

			empathy_url_show (str);
			g_free (str);
		}
	}

	return FALSE;
}

static void
chat_view_open_address_cb (GtkMenuItem *menuitem, const gchar *url)
{
	empathy_url_show (url);
}

static void
chat_view_copy_address_cb (GtkMenuItem *menuitem, const gchar *url)
{
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clipboard, url, -1);

	clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text (clipboard, url, -1);
}

static void
chat_view_clear_view_cb (GtkMenuItem *menuitem, EmpathyChatView *view)
{
	empathy_chat_view_clear (view);
}

static gboolean
chat_view_is_scrolled_down (EmpathyChatView *view)
{
	GtkWidget *sw;

	sw = gtk_widget_get_parent (GTK_WIDGET (view));
	if (GTK_IS_SCROLLED_WINDOW (sw)) {
		GtkAdjustment *vadj;

		vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (sw));

		if (vadj->value + vadj->page_size / 2 < vadj->upper - vadj->page_size) {
			return FALSE;
		}
	}

	return TRUE;
}

static void
chat_view_maybe_trim_buffer (EmpathyChatView *view)
{
	EmpathyChatViewPriv *priv;
	GtkTextIter         top, bottom;
	gint                line;
	gint                remove;
	GtkTextTagTable    *table;
	GtkTextTag         *tag;

	priv = GET_PRIV (view);

	gtk_text_buffer_get_end_iter (priv->buffer, &bottom);
	line = gtk_text_iter_get_line (&bottom);
	if (line < MAX_LINES) {
		return;
	}

	remove = line - MAX_LINES;
	gtk_text_buffer_get_start_iter (priv->buffer, &top);

	bottom = top;
	if (!gtk_text_iter_forward_lines (&bottom, remove)) {
		return;
	}

	/* Track backwords to a place where we can safely cut, we don't do it in
	 * the middle of a tag.
	 */
	table = gtk_text_buffer_get_tag_table (priv->buffer);
	tag = gtk_text_tag_table_lookup (table, "cut");
	if (!tag) {
		return;
	}

	if (!gtk_text_iter_forward_to_tag_toggle (&bottom, tag)) {
		return;
	}

	if (!gtk_text_iter_equal (&top, &bottom)) {
		gtk_text_buffer_delete (priv->buffer, &top, &bottom);
	}
}

static void
chat_view_theme_changed_cb (EmpathyThemeManager *manager,
			    EmpathyChatView     *view)
{
	EmpathyChatViewPriv *priv;
	gboolean            show_avatars = FALSE;

	priv = GET_PRIV (view);

	empathy_theme_manager_apply_saved (manager, view);

	/* Needed for now to update the "rise" property of the names to get it
	 * vertically centered.
	 */
	empathy_conf_get_bool (empathy_conf_get (),
			       EMPATHY_PREFS_UI_SHOW_AVATARS,
			       &show_avatars);
	empathy_theme_set_show_avatars (priv->theme, show_avatars);
}

/* Pads a pixbuf to the specified size, by centering it in a larger transparent
 * pixbuf. Returns a new ref.
 */
static GdkPixbuf *
chat_view_pad_to_size (GdkPixbuf *pixbuf,
		       gint       width,
		       gint       height,
		       gint       extra_padding_right)
{
	gint       src_width, src_height;
	GdkPixbuf *padded;
	gint       x_offset, y_offset;

	src_width = gdk_pixbuf_get_width (pixbuf);
	src_height = gdk_pixbuf_get_height (pixbuf);

	x_offset = (width - src_width) / 2;
	y_offset = (height - src_height) / 2;

	padded = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (pixbuf),
				 TRUE, /* alpha */
				 gdk_pixbuf_get_bits_per_sample (pixbuf),
				 width + extra_padding_right,
				 height);

	gdk_pixbuf_fill (padded, 0);

	gdk_pixbuf_copy_area (pixbuf,
			      0, /* source coords */
			      0,
			      src_width,
			      src_height,
			      padded,
			      x_offset, /* dest coords */
			      y_offset);

	return padded;
}

typedef struct {
	GdkPixbuf *pixbuf;
	gchar     *token;
} AvatarData;

static void
chat_view_avatar_cache_data_free (gpointer ptr)
{
	AvatarData *data = ptr;

	g_object_unref (data->pixbuf);
	g_free (data->token);
	g_slice_free (AvatarData, data);
}

GdkPixbuf *
empathy_chat_view_get_avatar_pixbuf_with_cache (EmpathyContact *contact)
{
	AvatarData        *data;
	EmpathyAvatar     *avatar;
	GdkPixbuf         *tmp_pixbuf;
	GdkPixbuf         *pixbuf = NULL;

	/* Check if avatar is in cache and if it's up to date */
	avatar = empathy_contact_get_avatar (contact);
	data = g_object_get_data (G_OBJECT (contact), "chat-view-avatar-cache");
	if (data) {
		if (avatar && !tp_strdiff (avatar->token, data->token)) {
			/* We have the avatar in cache */
			return data->pixbuf;
		}
	}

	/* Avatar not in cache, create pixbuf */
	tmp_pixbuf = empathy_pixbuf_avatar_from_contact_scaled (contact, 32, 32);
	if (tmp_pixbuf) {
		pixbuf = chat_view_pad_to_size (tmp_pixbuf, 32, 32, 6);
		g_object_unref (tmp_pixbuf);
	}
	if (!pixbuf) {
		return NULL;
	}

	/* Insert new pixbuf in cache */
	data = g_slice_new0 (AvatarData);
	data->token = g_strdup (avatar->token);
	data->pixbuf = pixbuf;

	g_object_set_data_full (G_OBJECT (contact), "chat-view-avatar-cache",
				data, chat_view_avatar_cache_data_free);

	return data->pixbuf;
}

EmpathyChatView *
empathy_chat_view_new (void)
{
	return g_object_new (EMPATHY_TYPE_CHAT_VIEW, NULL);
}

/* Code stolen from pidgin/gtkimhtml.c */
static gboolean
chat_view_scroll_cb (EmpathyChatView *view)
{
	EmpathyChatViewPriv *priv;
	GtkAdjustment      *adj;
	gdouble             max_val;

	priv = GET_PRIV (view);
	adj = GTK_TEXT_VIEW (view)->vadjustment;
	max_val = adj->upper - adj->page_size;

	g_return_val_if_fail (priv->scroll_time != NULL, FALSE);

	if (g_timer_elapsed (priv->scroll_time, NULL) > MAX_SCROLL_TIME) {
		/* time's up. jump to the end and kill the timer */
		gtk_adjustment_set_value (adj, max_val);
		g_timer_destroy (priv->scroll_time);
		priv->scroll_time = NULL;
		priv->scroll_timeout = 0;
		return FALSE;
	}

	/* scroll by 1/3rd the remaining distance */
	gtk_adjustment_set_value (adj, gtk_adjustment_get_value (adj) + ((max_val - gtk_adjustment_get_value (adj)) / 3));
	return TRUE;
}

void
empathy_chat_view_scroll_down (EmpathyChatView *view)
{
	EmpathyChatViewPriv *priv;

	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));

	priv = GET_PRIV (view);

	if (!priv->allow_scrolling) {
		return;
	}

	DEBUG ("Scrolling down");

	if (priv->scroll_time) {
		g_timer_reset (priv->scroll_time);
	} else {
		priv->scroll_time = g_timer_new();
	}
	if (!priv->scroll_timeout) {
		priv->scroll_timeout = g_timeout_add (SCROLL_DELAY,
						      (GSourceFunc) chat_view_scroll_cb,
						      view);
	}
}

void
empathy_chat_view_append_message (EmpathyChatView *view,
				  EmpathyMessage  *msg)
{
	EmpathyChatViewPriv *priv = GET_PRIV (view);
	gboolean             bottom;

	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	g_return_if_fail (EMPATHY_IS_MESSAGE (msg));

	if (!empathy_message_get_body (msg)) {
		return;
	}

	bottom = chat_view_is_scrolled_down (view);
	
	chat_view_maybe_trim_buffer (view);

	empathy_theme_append_message (priv->theme, view, msg);

	if (bottom) {
		empathy_chat_view_scroll_down (view);
	}

	if (priv->last_contact) {
		g_object_unref (priv->last_contact);
	}
	priv->last_contact = g_object_ref (empathy_message_get_sender (msg));
}

void
empathy_chat_view_append_event (EmpathyChatView *view,
			       const gchar    *str)
{
	EmpathyChatViewPriv *priv;
	gboolean            bottom;

	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	g_return_if_fail (!G_STR_EMPTY (str));

	priv = GET_PRIV (view);

	bottom = chat_view_is_scrolled_down (view);

	chat_view_maybe_trim_buffer (view);

	empathy_theme_append_event (priv->theme, view, str);

	if (bottom) {
		empathy_chat_view_scroll_down (view);
	}

	if (priv->last_contact) {
		g_object_unref (priv->last_contact);
		priv->last_contact = NULL;
	}
}

void
empathy_chat_view_append_button (EmpathyChatView *view,
				const gchar    *message,
				GtkWidget      *button1,
				GtkWidget      *button2)
{
	EmpathyChatViewPriv   *priv;
	GtkTextChildAnchor   *anchor;
	GtkTextIter           iter;
	gboolean              bottom;
	const gchar          *tag;

	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	g_return_if_fail (button1 != NULL);

	priv = GET_PRIV (view);

	tag = "invite";

	bottom = chat_view_is_scrolled_down (view);

	empathy_theme_append_timestamp (priv->theme, view, NULL, TRUE, TRUE);

	if (message) {
		empathy_theme_append_text (priv->theme, view, message, tag, NULL);
	}

	gtk_text_buffer_get_end_iter (priv->buffer, &iter);

	anchor = gtk_text_buffer_create_child_anchor (priv->buffer, &iter);
	gtk_text_view_add_child_at_anchor (GTK_TEXT_VIEW (view), button1, anchor);
	gtk_widget_show (button1);

	gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
						  &iter,
						  " ",
						  1,
						  tag,
						  NULL);

	if (button2) {
		gtk_text_buffer_get_end_iter (priv->buffer, &iter);
		
		anchor = gtk_text_buffer_create_child_anchor (priv->buffer, &iter);
		gtk_text_view_add_child_at_anchor (GTK_TEXT_VIEW (view), button2, anchor);
		gtk_widget_show (button2);
		
		gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
							  &iter,
							  " ",
							  1,
							  tag,
							  NULL);
	}

	gtk_text_buffer_get_end_iter (priv->buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
						  &iter,
						  "\n\n",
						  2,
						  tag,
						  NULL);

	if (bottom) {
		empathy_chat_view_scroll_down (view);
	}

	if (priv->last_contact) {
		g_object_unref (priv->last_contact);
		priv->last_contact = NULL;
	}
}

void
empathy_chat_view_scroll (EmpathyChatView *view,
			 gboolean        allow_scrolling)
{
	EmpathyChatViewPriv *priv = GET_PRIV (view);

	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));

	DEBUG ("Scrolling %s", allow_scrolling ? "enabled" : "disabled");

	priv->allow_scrolling = allow_scrolling;
	if (allow_scrolling) {
		empathy_chat_view_scroll_down (view);
	}
}

gboolean
empathy_chat_view_get_selection_bounds (EmpathyChatView *view,
				       GtkTextIter    *start,
				       GtkTextIter    *end)
{
	GtkTextBuffer *buffer;

	g_return_val_if_fail (EMPATHY_IS_CHAT_VIEW (view), FALSE);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	return gtk_text_buffer_get_selection_bounds (buffer, start, end);
}

void
empathy_chat_view_clear (EmpathyChatView *view)
{
	GtkTextBuffer      *buffer;
	EmpathyChatViewPriv *priv;

	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	gtk_text_buffer_set_text (buffer, "", -1);

	/* We set these back to the initial values so we get
	 * timestamps when clearing the window to know when
	 * conversations start.
	 */
	priv = GET_PRIV (view);

	priv->last_timestamp = 0;
}

gboolean
empathy_chat_view_find_previous (EmpathyChatView *view,
				const gchar    *search_criteria,
				gboolean        new_search)
{
	EmpathyChatViewPriv *priv;
	GtkTextBuffer      *buffer;
	GtkTextIter         iter_at_mark;
	GtkTextIter         iter_match_start;
	GtkTextIter         iter_match_end;
	gboolean            found;
	gboolean            from_start = FALSE;

	g_return_val_if_fail (EMPATHY_IS_CHAT_VIEW (view), FALSE);
	g_return_val_if_fail (search_criteria != NULL, FALSE);

	priv = GET_PRIV (view);

	buffer = priv->buffer;

	if (G_STR_EMPTY (search_criteria)) {
		if (priv->find_mark_previous) {
			gtk_text_buffer_get_start_iter (buffer, &iter_at_mark);

			gtk_text_buffer_move_mark (buffer,
						   priv->find_mark_previous,
						   &iter_at_mark);
			gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
						      priv->find_mark_previous,
						      0.0,
						      TRUE,
						      0.0,
						      0.0);
			gtk_text_buffer_select_range (buffer,
						      &iter_at_mark,
						      &iter_at_mark);
		}

		return FALSE;
	}

	if (new_search) {
		from_start = TRUE;
	}

	if (priv->find_mark_previous) {
		gtk_text_buffer_get_iter_at_mark (buffer,
						  &iter_at_mark,
						  priv->find_mark_previous);
	} else {
		gtk_text_buffer_get_end_iter (buffer, &iter_at_mark);
		from_start = TRUE;
	}

	priv->find_last_direction = FALSE;

	found = empathy_text_iter_backward_search (&iter_at_mark,
						  search_criteria,
						  &iter_match_start,
						  &iter_match_end,
						  NULL);

	if (!found) {
		gboolean result = FALSE;

		if (from_start) {
			return result;
		}

		/* Here we wrap around. */
		if (!new_search && !priv->find_wrapped) {
			priv->find_wrapped = TRUE;
			result = empathy_chat_view_find_previous (view, 
								 search_criteria, 
								 FALSE);
			priv->find_wrapped = FALSE;
		}

		return result;
	}

	/* Set new mark and show on screen */
	if (!priv->find_mark_previous) {
		priv->find_mark_previous = gtk_text_buffer_create_mark (buffer, NULL,
									&iter_match_start,
									TRUE);
	} else {
		gtk_text_buffer_move_mark (buffer,
					   priv->find_mark_previous,
					   &iter_match_start);
	}

	if (!priv->find_mark_next) {
		priv->find_mark_next = gtk_text_buffer_create_mark (buffer, NULL,
								    &iter_match_end,
								    TRUE);
	} else {
		gtk_text_buffer_move_mark (buffer,
					   priv->find_mark_next,
					   &iter_match_end);
	}

	gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
				      priv->find_mark_previous,
				      0.0,
				      TRUE,
				      0.5,
				      0.5);

	gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &iter_match_start);
	gtk_text_buffer_move_mark_by_name (buffer, "insert", &iter_match_end);

	return TRUE;
}

gboolean
empathy_chat_view_find_next (EmpathyChatView *view,
			    const gchar    *search_criteria,
			    gboolean        new_search)
{
	EmpathyChatViewPriv *priv;
	GtkTextBuffer      *buffer;
	GtkTextIter         iter_at_mark;
	GtkTextIter         iter_match_start;
	GtkTextIter         iter_match_end;
	gboolean            found;
	gboolean            from_start = FALSE;

	g_return_val_if_fail (EMPATHY_IS_CHAT_VIEW (view), FALSE);
	g_return_val_if_fail (search_criteria != NULL, FALSE);

	priv = GET_PRIV (view);

	buffer = priv->buffer;

	if (G_STR_EMPTY (search_criteria)) {
		if (priv->find_mark_next) {
			gtk_text_buffer_get_start_iter (buffer, &iter_at_mark);

			gtk_text_buffer_move_mark (buffer,
						   priv->find_mark_next,
						   &iter_at_mark);
			gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
						      priv->find_mark_next,
						      0.0,
						      TRUE,
						      0.0,
						      0.0);
			gtk_text_buffer_select_range (buffer,
						      &iter_at_mark,
						      &iter_at_mark);
		}

		return FALSE;
	}

	if (new_search) {
		from_start = TRUE;
	}

	if (priv->find_mark_next) {
		gtk_text_buffer_get_iter_at_mark (buffer,
						  &iter_at_mark,
						  priv->find_mark_next);
	} else {
		gtk_text_buffer_get_start_iter (buffer, &iter_at_mark);
		from_start = TRUE;
	}

	priv->find_last_direction = TRUE;

	found = empathy_text_iter_forward_search (&iter_at_mark,
						 search_criteria,
						 &iter_match_start,
						 &iter_match_end,
						 NULL);

	if (!found) {
		gboolean result = FALSE;

		if (from_start) {
			return result;
		}

		/* Here we wrap around. */
		if (!new_search && !priv->find_wrapped) {
			priv->find_wrapped = TRUE;
			result = empathy_chat_view_find_next (view, 
							     search_criteria, 
							     FALSE);
			priv->find_wrapped = FALSE;
		}

		return result;
	}

	/* Set new mark and show on screen */
	if (!priv->find_mark_next) {
		priv->find_mark_next = gtk_text_buffer_create_mark (buffer, NULL,
							       &iter_match_end,
							       TRUE);
	} else {
		gtk_text_buffer_move_mark (buffer,
					   priv->find_mark_next,
					   &iter_match_end);
	}

	if (!priv->find_mark_previous) {
		priv->find_mark_previous = gtk_text_buffer_create_mark (buffer, NULL,
									&iter_match_start,
									TRUE);
	} else {
		gtk_text_buffer_move_mark (buffer,
					   priv->find_mark_previous,
					   &iter_match_start);
	}

	gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
				      priv->find_mark_next,
				      0.0,
				      TRUE,
				      0.5,
				      0.5);

	gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &iter_match_start);
	gtk_text_buffer_move_mark_by_name (buffer, "insert", &iter_match_end);

	return TRUE;
}


void
empathy_chat_view_find_abilities (EmpathyChatView *view,
				 const gchar    *search_criteria,
				 gboolean       *can_do_previous,
				 gboolean       *can_do_next)
{
	EmpathyChatViewPriv *priv;
	GtkTextBuffer      *buffer;
	GtkTextIter         iter_at_mark;
	GtkTextIter         iter_match_start;
	GtkTextIter         iter_match_end;

	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	g_return_if_fail (search_criteria != NULL);
	g_return_if_fail (can_do_previous != NULL && can_do_next != NULL);

	priv = GET_PRIV (view);

	buffer = priv->buffer;

	if (can_do_previous) {
		if (priv->find_mark_previous) {
			gtk_text_buffer_get_iter_at_mark (buffer,
							  &iter_at_mark,
							  priv->find_mark_previous);
		} else {
			gtk_text_buffer_get_start_iter (buffer, &iter_at_mark);
		}
		
		*can_do_previous = empathy_text_iter_backward_search (&iter_at_mark,
								     search_criteria,
								     &iter_match_start,
								     &iter_match_end,
								     NULL);
	}

	if (can_do_next) {
		if (priv->find_mark_next) {
			gtk_text_buffer_get_iter_at_mark (buffer,
							  &iter_at_mark,
							  priv->find_mark_next);
		} else {
			gtk_text_buffer_get_start_iter (buffer, &iter_at_mark);
		}
		
		*can_do_next = empathy_text_iter_forward_search (&iter_at_mark,
								search_criteria,
								&iter_match_start,
								&iter_match_end,
								NULL);
	}
}

void
empathy_chat_view_highlight (EmpathyChatView *view,
			     const gchar     *text)
{
	GtkTextBuffer *buffer;
	GtkTextIter    iter;
	GtkTextIter    iter_start;
	GtkTextIter    iter_end;
	GtkTextIter    iter_match_start;
	GtkTextIter    iter_match_end;
	gboolean       found;

	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	gtk_text_buffer_get_start_iter (buffer, &iter);

	gtk_text_buffer_get_bounds (buffer, &iter_start, &iter_end);
	gtk_text_buffer_remove_tag_by_name (buffer, "highlight",
					    &iter_start,
					    &iter_end);

	if (G_STR_EMPTY (text)) {
		return;
	}

	while (1) {
		found = empathy_text_iter_forward_search (&iter,
							 text,
							 &iter_match_start,
							 &iter_match_end,
							 NULL);

		if (!found) {
			break;
		}

		gtk_text_buffer_apply_tag_by_name (buffer, "highlight",
						   &iter_match_start,
						   &iter_match_end);

		iter = iter_match_end;
		gtk_text_iter_forward_char (&iter);
	}
}

void
empathy_chat_view_copy_clipboard (EmpathyChatView *view)
{
	GtkTextBuffer *buffer;
	GtkClipboard  *clipboard;

	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	gtk_text_buffer_copy_clipboard (buffer, clipboard);
}

EmpathyTheme *
empathy_chat_view_get_theme (EmpathyChatView *view)
{
	EmpathyChatViewPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CHAT_VIEW (view), NULL);

	priv = GET_PRIV (view);

	return priv->theme;
}

static void
chat_view_theme_notify_cb (EmpathyTheme    *theme,
			   GParamSpec      *param,
			   EmpathyChatView *view)
{
	empathy_theme_update_view (theme, view);
}

void
empathy_chat_view_set_theme (EmpathyChatView *view, EmpathyTheme *theme)
{
	EmpathyChatViewPriv *priv;

	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	g_return_if_fail (EMPATHY_IS_THEME (theme));

	priv = GET_PRIV (view);

	if (priv->theme) {
		g_signal_handlers_disconnect_by_func (priv->theme,
						      chat_view_theme_notify_cb,
						      view);
		g_object_unref (priv->theme);
	}

	priv->theme = g_object_ref (theme);

	empathy_theme_update_view (theme, view);
	g_signal_connect (priv->theme, "notify",
			  G_CALLBACK (chat_view_theme_notify_cb),
			  view);

	/* FIXME: Redraw all messages using the new theme */
}

void
empathy_chat_view_set_margin (EmpathyChatView *view,
			     gint            margin)
{
	EmpathyChatViewPriv *priv;

	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));

	priv = GET_PRIV (view);

	g_object_set (view,
		      "left-margin", margin,
		      "right-margin", margin,
		      NULL);
}

GtkWidget *
empathy_chat_view_get_smiley_menu (GCallback    callback,
				   gpointer     user_data)
{
	EmpathySmileyManager *smiley_manager;
	GSList               *smileys, *l;
	GtkWidget            *menu;
	gint                  x = 0;
	gint                  y = 0;

	g_return_val_if_fail (callback != NULL, NULL);

	menu = gtk_menu_new ();

	smiley_manager = empathy_smiley_manager_new ();
	smileys = empathy_smiley_manager_get_all (smiley_manager);
	for (l = smileys; l; l = l->next) {
		EmpathySmiley *smiley;
		GtkWidget     *item;
		GtkWidget     *image;

		smiley = l->data;
		image = gtk_image_new_from_pixbuf (smiley->pixbuf);

		item = gtk_image_menu_item_new_with_label ("");
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

		gtk_menu_attach (GTK_MENU (menu), item,
				 x, x + 1, y, y + 1);

		gtk_widget_set_tooltip_text (item, smiley->str);

		g_object_set_data  (G_OBJECT (item), "smiley_text", smiley->str);
		g_signal_connect (item, "activate", callback, user_data);

		if (x > 3) {
			y++;
			x = 0;
		} else {
			x++;
		}
	}
	g_object_unref (smiley_manager);

	gtk_widget_show_all (menu);

	return menu;
}

time_t
empathy_chat_view_get_last_timestamp (EmpathyChatView *view)
{
	EmpathyChatViewPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CHAT_VIEW (view), 0);

	priv = GET_PRIV (view);

	return priv->last_timestamp;
}

void
empathy_chat_view_set_last_timestamp (EmpathyChatView *view,
				     time_t          timestamp)
{
	EmpathyChatViewPriv *priv;

	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));

	priv = GET_PRIV (view);

	priv->last_timestamp = timestamp;
}

EmpathyContact *
empathy_chat_view_get_last_contact (EmpathyChatView *view)
{
	EmpathyChatViewPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CHAT_VIEW (view), NULL);

	priv = GET_PRIV (view);

	return priv->last_contact;
}

