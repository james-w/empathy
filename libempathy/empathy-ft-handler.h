/*
 * empathy-ft-handler.h - Header for EmpathyFTHandler
 * Copyright (C) 2009 Collabora Ltd.
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
 * Author: Cosimo Cecchi <cosimo.cecchi@collabora.co.uk>
 */

/* empathy-ft-handler.h */

#ifndef __EMPATHY_FT_HANDLER_H__
#define __EMPATHY_FT_HANDLER_H__

#include <glib-object.h>
#include <gio/gio.h>

#include "empathy-tp-file.h"
#include "empathy-contact.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_FT_HANDLER empathy_ft_handler_get_type()
#define EMPATHY_FT_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), EMPATHY_TYPE_FT_HANDLER, EmpathyFTHandler))
#define EMPATHY_FT_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), EMPATHY_TYPE_FT_HANDLER, EmpathyFTHandlerClass))
#define EMPATHY_IS_FT_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EMPATHY_TYPE_FT_HANDLER))
#define EMPATHY_IS_FT_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), EMPATHY_TYPE_FT_HANDLER))
#define EMPATHY_FT_HANDLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EMPATHY_TYPE_FT_HANDLER, EmpathyFTHandlerClass))

typedef struct {
  GObject parent;
  gpointer priv;
} EmpathyFTHandler;

typedef struct {
  GObjectClass parent_class;
} EmpathyFTHandlerClass;

GType empathy_ft_handler_get_type (void);

/* public methods */
EmpathyFTHandler * empathy_ft_handler_new_outgoing (EmpathyContact *contact,
    GFile *source);
EmpathyFTHandler * empathy_ft_handler_new_incoming (EmpathyTpFile *tp_file,
    GFile *destination);
void empathy_ft_handler_start_transfer (EmpathyFTHandler *handler,
    GCancellable *cancellable);

G_END_DECLS

#endif /* __EMPATHY_FT_HANDLER_H__ */
