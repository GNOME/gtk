/* gtkentryprivate.h
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_TEXT_PRIVATE_H__
#define __GTK_TEXT_PRIVATE_H__

#include "gtktext.h"

#include "gtkeventcontroller.h"
#include  "gtkimcontext.h"

G_BEGIN_DECLS

#define GTK_TEXT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT, GtkTextClass))
#define GTK_IS_TEXT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT))
#define GTK_TEXT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TEXT, GtkTextClass))

typedef struct _GtkTextClass         GtkTextClass;

/*<private>
 * GtkTextClass:
 * @parent_class: The parent class.
 */
struct _GtkTextClass
{
  GtkWidgetClass parent_class;
};

char *              gtk_text_get_display_text   (GtkText    *entry,
                                                 int         start_pos,
                                                 int         end_pos);
GtkIMContext *      gtk_text_get_im_context     (GtkText    *entry);
void                gtk_text_enter_text         (GtkText    *entry,
                                                 const char *text);
void                gtk_text_set_positions      (GtkText    *entry,
                                                 int         current_pos,
                                                 int         selection_bound);
PangoLayout *       gtk_text_get_layout         (GtkText    *entry);
void                gtk_text_get_layout_offsets (GtkText    *entry,
                                                 int        *x,
                                                 int        *y);
void                gtk_text_reset_im_context   (GtkText    *entry);
GtkEventController *gtk_text_get_key_controller (GtkText    *entry);

G_END_DECLS

#endif /* __GTK_TEXT_PRIVATE_H__ */
