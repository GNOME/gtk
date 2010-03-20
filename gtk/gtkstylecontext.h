/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_STYLE_CONTEXT_H__
#define __GTK_STYLE_CONTEXT_H__

#include <glib-object.h>
#include <gtk/gtkstyleprovider.h>
#include <gtk/gtkwidgetpath.h>

G_BEGIN_DECLS

#define GTK_TYPE_STYLE_CONTEXT         (gtk_style_context_get_type ())
#define GTK_STYLE_CONTEXT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_STYLE_CONTEXT, GtkStyleContext))
#define GTK_STYLE_CONTEXT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_STYLE_CONTEXT, GtkStyleContextClass))
#define GTK_IS_STYLE_CONTEXT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_STYLE_CONTEXT))
#define GTK_IS_STYLE_CONTEXT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_STYLE_CONTEXT))
#define GTK_STYLE_CONTEXT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_STYLE_CONTEXT, GtkStyleContextClass))

typedef struct GtkStyleContext GtkStyleContext;
typedef struct GtkStyleContextClass GtkStyleContextClass;

typedef enum {
  GTK_CHILD_CLASS_EVEN    = 1 << 0,
  GTK_CHILD_CLASS_ODD     = 1 << 1,
  GTK_CHILD_CLASS_FIRST   = 1 << 2,
  GTK_CHILD_CLASS_LAST    = 1 << 3,
  GTK_CHILD_CLASS_DEFAULT = 1 << 4,
  GTK_CHILD_CLASS_SORTED  = 1 << 5
} GtkChildClassFlags;

struct GtkStyleContext
{
  GObject parent_object;
};

struct GtkStyleContextClass
{
  GObjectClass parent_class;
};

GType gtk_style_context_get_type (void) G_GNUC_CONST;

void gtk_style_context_add_provider    (GtkStyleContext  *context,
                                        GtkStyleProvider *provider,
                                        guint             priority);

void gtk_style_context_remove_provider (GtkStyleContext  *context,
                                        GtkStyleProvider *provider);

void gtk_style_context_get_property (GtkStyleContext *context,
                                     const gchar     *property,
                                     GtkStateType     state,
                                     GValue          *value);
void gtk_style_context_get_valist   (GtkStyleContext *context,
                                     GtkStateType     state,
                                     va_list          args);
void gtk_style_context_get          (GtkStyleContext *context,
                                     GtkStateType     state,
                                     ...) G_GNUC_NULL_TERMINATED;

void          gtk_style_context_set_state    (GtkStyleContext *context,
                                              GtkStateFlags    flags);
GtkStateFlags gtk_style_context_get_state    (GtkStyleContext *context);

gboolean      gtk_style_context_is_state_set (GtkStyleContext *context,
                                              GtkStateType     state);

void          gtk_style_context_set_path     (GtkStyleContext *context,
                                              GtkWidgetPath   *path);
G_CONST_RETURN GtkWidgetPath * gtk_style_context_get_path (GtkStyleContext *context);

void     gtk_style_context_set_class   (GtkStyleContext *context,
                                        const gchar     *class_name);
void     gtk_style_context_unset_class (GtkStyleContext *context,
                                        const gchar     *class_name);
gboolean gtk_style_context_has_class   (GtkStyleContext *context,
                                        const gchar     *class_name);

void     gtk_style_context_set_child_class   (GtkStyleContext    *context,
                                              const gchar        *class_name,
                                              GtkChildClassFlags  flags);
void     gtk_style_context_unset_child_class (GtkStyleContext    *context,
                                              const gchar        *class_name);
gboolean gtk_style_context_has_child_class   (GtkStyleContext    *context,
                                              const gchar        *class_name,
                                              GtkChildClassFlags *flags_return);

G_END_DECLS

#endif /* __GTK_STYLE_CONTEXT_H__ */
