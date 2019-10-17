/*
 * gtkimmodulequartz
 * Copyright (C) 2011 Hiroyuki Yamamoto
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * $Id:$
 */

#include "config.h"
#include <string.h>

#include "gtk/gtkimcontextquartz.h"
#include "gtk/gtkintl.h"
#include "gtk/gtkimmoduleprivate.h"

#include "gdk/quartz/gdkquartz.h"
#include "gdk/quartz/GdkQuartzView.h"

#define GTK_IM_CONTEXT_TYPE_QUARTZ (type_quartz)
#define GTK_IM_CONTEXT_QUARTZ(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_IM_CONTEXT_TYPE_QUARTZ, GtkIMContextQuartz))
#define GTK_IM_CONTEXT_QUARTZ_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_IM_CONTEXT_TYPE_QUARTZ, GtkIMContextQuartzClass))


typedef struct _GtkIMContextQuartz
{
  GtkIMContext parent;
  GtkIMContext *slave;
  GdkSurface *client_surface;
  gchar *preedit_str;
  unsigned int cursor_index;
  unsigned int selected_len;
  GdkRectangle *cursor_rect;
  gboolean focused;
} GtkIMContextQuartz;

typedef struct _GtkIMContextQuartzClass
{
  GtkIMContextClass parent_class;
} GtkIMContextQuartzClass;

G_DEFINE_TYPE_WITH_CODE (GtkIMContextQuartz, gtk_im_context_quartz, GTK_TYPE_IM_CONTEXT,
			 gtk_im_module_ensure_extension_point ();
                         g_io_extension_point_implement (GTK_IM_MODULE_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "quartz",
                                                         0))

static void
quartz_get_preedit_string (GtkIMContext *context,
                           gchar **str,
                           PangoAttrList **attrs,
                           gint *cursor_pos)
{
  GtkIMContextQuartz *qc = GTK_IM_CONTEXT_QUARTZ (context);

  GTK_NOTE (MISC, g_print ("quartz_get_preedit_string\n"));

  if (str)
    *str = qc->preedit_str ? g_strdup (qc->preedit_str) : g_strdup ("");

  if (attrs)
    {
      *attrs = pango_attr_list_new ();
      int len = g_utf8_strlen (*str, -1);
      gchar *ch = *str;
      if (len > 0)
        {
          PangoAttribute *attr;
          int i = 0;
          for (;;)
            {
              gchar *s = ch;
              ch = g_utf8_next_char (ch);

              if (i >= qc->cursor_index &&
		  i < qc->cursor_index + qc->selected_len)
                attr = pango_attr_underline_new (PANGO_UNDERLINE_DOUBLE);
              else
                attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);

              attr->start_index = s - *str;
              if (!*ch)
                attr->end_index = attr->start_index + strlen (s);
              else
                attr->end_index = ch - *str;

              pango_attr_list_change (*attrs, attr);

              if (!*ch)
                break;
              i++;
            }
        }
    }
  if (cursor_pos)
    *cursor_pos = qc->cursor_index;
}

static gboolean
output_result (GtkIMContext *context,
               GdkSurface *surface)
{
  GtkIMContextQuartz *qc = GTK_IM_CONTEXT_QUARTZ (context);
  gboolean retval = FALSE;
  gchar *fixed_str, *marked_str;

  fixed_str = g_strdup (g_object_get_data (G_OBJECT (surface), TIC_INSERT_TEXT));
  marked_str = g_strdup (g_object_get_data (G_OBJECT (surface), TIC_MARKED_TEXT));
  if (fixed_str)
    {
      GTK_NOTE (MISC, g_print ("tic-insert-text: %s\n", fixed_str));
      g_free (qc->preedit_str);
      qc->preedit_str = NULL;
      g_object_set_data (G_OBJECT (surface), TIC_INSERT_TEXT, NULL);
      g_signal_emit_by_name (context, "commit", fixed_str);
      g_signal_emit_by_name (context, "preedit_changed");

      unsigned int filtered =
	   GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (surface),
						GIC_FILTER_KEY));
      GTK_NOTE (MISC, g_print ("filtered, %d\n", filtered));
      if (filtered)
        retval = TRUE;
      else
        retval = FALSE;
    }
  if (marked_str)
    {
      GTK_NOTE (MISC, g_print ("tic-marked-text: %s\n", marked_str));
      qc->cursor_index =
	   GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (surface),
						TIC_SELECTED_POS));
      qc->selected_len =
	   GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (surface),
						TIC_SELECTED_LEN));
      g_free (qc->preedit_str);
      qc->preedit_str = g_strdup (marked_str);
      g_object_set_data (G_OBJECT (surface), TIC_MARKED_TEXT, NULL);
      g_signal_emit_by_name (context, "preedit_changed");
      retval = TRUE;
    }
  if (!fixed_str && !marked_str)
    {
      if (qc->preedit_str && strlen (qc->preedit_str) > 0)
        retval = TRUE;
    }
  g_free (fixed_str);
  g_free (marked_str);
  return retval;
}

static gboolean
quartz_filter_keypress (GtkIMContext *context,
                        GdkEventKey *event)
{
  GtkIMContextQuartz *qc = GTK_IM_CONTEXT_QUARTZ (context);
  gboolean retval;
  NSView *nsview;
  GdkSurface *surface;

  GTK_NOTE (MISC, g_print ("quartz_filter_keypress\n"));

  if (!GDK_IS_QUARTZ_SURFACE (qc->client_surface))
    return FALSE;

  nsview = gdk_quartz_surface_get_nsview (qc->client_surface);
  surface = (GdkSurface *)[ (GdkQuartzView *)nsview gdkSurface];
  GTK_NOTE (MISC, g_print ("client_surface: %p, win: %p, nsview: %p\n",
			   qc->client_surface, surface, nsview));

  NSEvent *nsevent = gdk_quartz_event_get_nsevent ((GdkEvent *)event);

  if (!nsevent)
    {
      if (event->hardware_keycode == 0 && event->keyval == 0xffffff)
        /* update text input changes by mouse events */
        return output_result (context, surface);
      else
        return gtk_im_context_filter_keypress (qc->slave, event);
    }

  if (event->type == GDK_KEY_RELEASE)
    return FALSE;

  if (event->hardware_keycode == 55)	/* Command */
    return FALSE;

  NSEventType etype = [nsevent type];
  if (etype == NSKeyDown)
    {
       g_object_set_data (G_OBJECT (surface), TIC_IN_KEY_DOWN,
                                          GUINT_TO_POINTER (TRUE));
       [nsview keyDown: nsevent];
    }
  /* JIS_Eisu || JIS_Kana */
  if (event->hardware_keycode == 102 || event->hardware_keycode == 104)
    return FALSE;

  retval = output_result(context, surface);
  g_object_set_data (G_OBJECT (surface), TIC_IN_KEY_DOWN,
                                     GUINT_TO_POINTER (FALSE));
  GTK_NOTE (MISC, g_print ("quartz_filter_keypress done\n"));

  return retval;
}

static void
discard_preedit (GtkIMContext *context)
{
  GtkIMContextQuartz *qc = GTK_IM_CONTEXT_QUARTZ (context);

  if (!qc->client_surface)
    return;

  if (!GDK_IS_QUARTZ_SURFACE (qc->client_surface))
    return;

  NSView *nsview = gdk_quartz_surface_get_nsview (qc->client_surface);
  if (!nsview)
    return;

  /* reset any partial input for this NSView */
  [(GdkQuartzView *)nsview unmarkText];
  NSInputManager *currentInputManager = [NSInputManager currentInputManager];
  [currentInputManager markedTextAbandoned:nsview];

  if (qc->preedit_str && strlen (qc->preedit_str) > 0)
    {
      g_signal_emit_by_name (context, "commit", qc->preedit_str);

      g_free (qc->preedit_str);
      qc->preedit_str = NULL;
      g_signal_emit_by_name (context, "preedit_changed");
    }
}

static void
quartz_reset (GtkIMContext *context)
{
  GTK_NOTE (MISC, g_print ("quartz_reset\n"));
  discard_preedit (context);
}

static void
quartz_set_client_surface (GtkIMContext *context, GtkWidget *widget)
{
  GtkIMContextQuartz *qc = GTK_IM_CONTEXT_QUARTZ (context);

  GTK_NOTE (MISC, g_print ("quartz_set_client_surface: %p\n", widget));

  qc->client_surface = gtk_widget_get_parent_surface (widget);
}

static void
quartz_focus_in (GtkIMContext *context)
{
  GTK_NOTE (MISC, g_print ("quartz_focus_in\n"));

  GtkIMContextQuartz *qc = GTK_IM_CONTEXT_QUARTZ (context);
  qc->focused = TRUE;
}

static void
quartz_focus_out (GtkIMContext *context)
{
  GTK_NOTE (MISC, g_print ("quartz_focus_out\n"));

  GtkIMContextQuartz *qc = GTK_IM_CONTEXT_QUARTZ (context);
  qc->focused = FALSE;

  /* Commit any partially built strings or it'll mess up other GTK+ widgets in the window */
  discard_preedit (context);
}

static void
quartz_set_cursor_location (GtkIMContext *context, GdkRectangle *area)
{
  GtkIMContextQuartz *qc = GTK_IM_CONTEXT_QUARTZ (context);
  gint x, y;
  NSView *nsview;
  GdkSurface *surface;

  GTK_NOTE (MISC, g_print ("quartz_set_cursor_location\n"));

  if (!qc->client_surface)
    return;

  if (!qc->focused)
    return;

  qc->cursor_rect->x = area->x;
  qc->cursor_rect->y = area->y;
  qc->cursor_rect->width = area->width;
  qc->cursor_rect->height = area->height;

  gdk_surface_get_origin (qc->client_surface, &x, &y);

  qc->cursor_rect->x = area->x + x;
  qc->cursor_rect->y = area->y + y;

  if (!GDK_IS_QUARTZ_SURFACE (qc->client_surface))
    return;

  nsview = gdk_quartz_surface_get_nsview (qc->client_surface);
  surface = (GdkSurface *)[ (GdkQuartzView*)nsview gdkSurface];
  g_object_set_data (G_OBJECT (surface), GIC_CURSOR_RECT, qc->cursor_rect);
}

static void
quartz_set_use_preedit (GtkIMContext *context, gboolean use_preedit)
{
  GTK_NOTE (MISC, g_print ("quartz_set_use_preedit: %d\n", use_preedit));
}

static void
commit_cb (GtkIMContext *context, const gchar *str, GtkIMContextQuartz *qc)
{
  g_signal_emit_by_name (qc, "commit", str);
}

static void
imquartz_finalize (GObject *obj)
{
  GTK_NOTE (MISC, g_print ("imquartz_finalize\n"));

  GtkIMContextQuartz *qc = GTK_IM_CONTEXT_QUARTZ (obj);
  g_free (qc->preedit_str);
  qc->preedit_str = NULL;
  g_free (qc->cursor_rect);
  qc->cursor_rect = NULL;

  g_signal_handlers_disconnect_by_func (qc->slave, (gpointer)commit_cb, qc);
  g_object_unref (qc->slave);

  gtk_im_context_quartz_parent_class->finalize (obj);
}

static void
gtk_im_context_quartz_class_init (GtkIMContextQuartzClass *class)
{
  GTK_NOTE (MISC, g_print ("gtk_im_context_quartz_class_init\n"));

  GtkIMContextClass *klass = GTK_IM_CONTEXT_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  klass->get_preedit_string = quartz_get_preedit_string;
  klass->filter_keypress = quartz_filter_keypress;
  klass->reset = quartz_reset;
  klass->set_client_widget = quartz_set_client_surface;
  klass->focus_in = quartz_focus_in;
  klass->focus_out = quartz_focus_out;
  klass->set_cursor_location = quartz_set_cursor_location;
  klass->set_use_preedit = quartz_set_use_preedit;

  object_class->finalize = imquartz_finalize;
}

static void
gtk_im_context_quartz_init (GtkIMContextQuartz *qc)
{
  GTK_NOTE (MISC, g_print ("gtk_im_context_quartz_init\n"));

  qc->preedit_str = g_strdup ("");
  qc->cursor_index = 0;
  qc->selected_len = 0;
  qc->cursor_rect = g_malloc (sizeof (GdkRectangle));
  qc->focused = FALSE;

  qc->slave = g_object_new (GTK_TYPE_IM_CONTEXT_SIMPLE, NULL);
  g_signal_connect (G_OBJECT (qc->slave), "commit", G_CALLBACK (commit_cb), qc);
}
