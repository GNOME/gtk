/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat Software
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>


G_BEGIN_DECLS

#define GTK_TYPE_IM_CONTEXT              (gtk_im_context_get_type ())
#define GTK_IM_CONTEXT(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_IM_CONTEXT, GtkIMContext))
#define GTK_IM_CONTEXT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_IM_CONTEXT, GtkIMContextClass))
#define GTK_IS_IM_CONTEXT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_IM_CONTEXT))
#define GTK_IS_IM_CONTEXT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IM_CONTEXT))
#define GTK_IM_CONTEXT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_IM_CONTEXT, GtkIMContextClass))


typedef struct _GtkIMContext       GtkIMContext;
typedef struct _GtkIMContextClass  GtkIMContextClass;

struct _GtkIMContext
{
  GObject parent_instance;
};

struct _GtkIMContextClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  /* Signals */
  void     (*preedit_start)        (GtkIMContext *context);
  void     (*preedit_end)          (GtkIMContext *context);
  void     (*preedit_changed)      (GtkIMContext *context);
  void     (*commit)               (GtkIMContext *context, const char *str);
  gboolean (*retrieve_surrounding) (GtkIMContext *context);
  gboolean (*delete_surrounding)   (GtkIMContext *context,
				    int           offset,
				    int           n_chars);

  /* Virtual functions */
  void     (*set_client_widget)   (GtkIMContext   *context,
				   GtkWidget      *widget);
  void     (*get_preedit_string)  (GtkIMContext   *context,
				   char          **str,
				   PangoAttrList **attrs,
				   int            *cursor_pos);
  gboolean (*filter_keypress)     (GtkIMContext   *context,
			           GdkEvent       *event);
  void     (*focus_in)            (GtkIMContext   *context);
  void     (*focus_out)           (GtkIMContext   *context);
  void     (*reset)               (GtkIMContext   *context);
  void     (*set_cursor_location) (GtkIMContext   *context,
				   GdkRectangle   *area);
  void     (*set_use_preedit)     (GtkIMContext   *context,
				   gboolean        use_preedit);
  void     (*set_surrounding)     (GtkIMContext   *context,
				   const char     *text,
				   int             len,
				   int             cursor_index);
  gboolean (*get_surrounding)     (GtkIMContext   *context,
				   char          **text,
				   int            *cursor_index);
  void     (*set_surrounding_with_selection)
                                  (GtkIMContext   *context,
				   const char     *text,
				   int             len,
				   int             cursor_index,
                                   int             anchor_index);
  gboolean (*get_surrounding_with_selection)
                                  (GtkIMContext   *context,
				   char          **text,
				   int            *cursor_index,
                                   int            *anchor_index);

  /*< private >*/
  void (* activate_osk) (GtkIMContext *context);
  gboolean (* activate_osk_with_event) (GtkIMContext *context,
                                        GdkEvent     *event);

  /* another signal */
  gboolean (*invalid_composition)  (GtkIMContext *context,
				    const char *str);

  /* Padding for future expansion */
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GDK_AVAILABLE_IN_ALL
GType    gtk_im_context_get_type            (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
void     gtk_im_context_set_client_widget   (GtkIMContext       *context,
                                             GtkWidget          *widget);
GDK_AVAILABLE_IN_ALL
void     gtk_im_context_get_preedit_string  (GtkIMContext       *context,
					     char              **str,
					     PangoAttrList     **attrs,
					     int                *cursor_pos);
GDK_AVAILABLE_IN_ALL
gboolean gtk_im_context_filter_keypress     (GtkIMContext       *context,
                                             GdkEvent           *event);

GDK_AVAILABLE_IN_ALL
gboolean gtk_im_context_filter_key          (GtkIMContext       *context,
                                             gboolean            press,
                                             GdkSurface         *surface,
                                             GdkDevice          *device,
                                             guint32             time,
                                             guint               keycode,
                                             GdkModifierType     state,
                                             int                 group);

GDK_AVAILABLE_IN_ALL
void     gtk_im_context_focus_in            (GtkIMContext       *context);
GDK_AVAILABLE_IN_ALL
void     gtk_im_context_focus_out           (GtkIMContext       *context);
GDK_AVAILABLE_IN_ALL
void     gtk_im_context_reset               (GtkIMContext       *context);
GDK_AVAILABLE_IN_ALL
void     gtk_im_context_set_cursor_location (GtkIMContext       *context,
					     const GdkRectangle *area);
GDK_AVAILABLE_IN_ALL
void     gtk_im_context_set_use_preedit     (GtkIMContext       *context,
					     gboolean            use_preedit);
GDK_DEPRECATED_IN_4_2_FOR(gtk_im_context_set_surrounding_with_selection)
void     gtk_im_context_set_surrounding     (GtkIMContext       *context,
                                             const char         *text,
                                             int                 len,
                                             int                 cursor_index);
GDK_DEPRECATED_IN_4_2_FOR(gtk_im_context_get_surrounding_with_selection)
gboolean gtk_im_context_get_surrounding     (GtkIMContext       *context,
                                             char              **text,
                                             int                *cursor_index);
GDK_AVAILABLE_IN_4_2
void     gtk_im_context_set_surrounding_with_selection
                                            (GtkIMContext       *context,
                                             const char         *text,
                                             int                 len,
                                             int                 cursor_index,
                                             int                 anchor_index);
GDK_AVAILABLE_IN_4_2
gboolean gtk_im_context_get_surrounding_with_selection
                                            (GtkIMContext       *context,
                                             char              **text,
                                             int                *cursor_index,
                                             int                *anchor_index);
GDK_AVAILABLE_IN_ALL
gboolean gtk_im_context_delete_surrounding  (GtkIMContext       *context,
					     int                 offset,
					     int                 n_chars);

GDK_AVAILABLE_IN_4_14
gboolean gtk_im_context_activate_osk (GtkIMContext *context,
                                      GdkEvent     *event);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtkIMContext, g_object_unref)

G_END_DECLS

