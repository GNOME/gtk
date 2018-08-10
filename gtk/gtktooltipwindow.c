/* GTK - The GIMP Toolkit
 * Copyright 2015  Emmanuele Bassi 
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtktooltipwindowprivate.h"

#include "gtkprivate.h"
#include "gtkintl.h"

#include "gtkaccessible.h"
#include "gtkbox.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtksettings.h"
#include "gtksizerequest.h"
#include "gtkwindowprivate.h"
#include "gtkwidgetprivate.h"

#define MAX_TOOLTIP_LINE_WIDTH  70

struct _GtkTooltipWindow
{
  GtkWindow parent_type;

  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *custom_widget;
};

struct _GtkTooltipWindowClass
{
  GtkWindowClass parent_class;
};

G_DEFINE_TYPE (GtkTooltipWindow, gtk_tooltip_window, GTK_TYPE_WINDOW)

static void
gtk_tooltip_window_class_init (GtkTooltipWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_css_name (widget_class, I_("tooltip"));
  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_TOOL_TIP);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtktooltipwindow.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkTooltipWindow, box);
  gtk_widget_class_bind_template_child (widget_class, GtkTooltipWindow, image);
  gtk_widget_class_bind_template_child (widget_class, GtkTooltipWindow, label);
}

static void
gtk_tooltip_window_init (GtkTooltipWindow *self)
{
  GtkWindow *window = GTK_WINDOW (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  _gtk_window_request_csd (window);
}

GtkWidget *
gtk_tooltip_window_new (void)
{
  return g_object_new (GTK_TYPE_TOOLTIP_WINDOW,
                       "type", GTK_WINDOW_POPUP,
                       NULL);
}

void
gtk_tooltip_window_set_label_markup (GtkTooltipWindow *window,
                                     const char       *markup)
{
  if (markup != NULL)
    {
      gtk_label_set_markup (GTK_LABEL (window->label), markup);
      gtk_widget_show (window->label);
    }
  else
    {
      gtk_widget_hide (window->label);
    }
}

void
gtk_tooltip_window_set_label_text (GtkTooltipWindow *window,
                                   const char       *text)
{
  if (text != NULL)
    {
      gtk_label_set_text (GTK_LABEL (window->label), text);
      gtk_widget_show (window->label);
    }
  else
    {
      gtk_widget_hide (window->label);
    }
}

void
gtk_tooltip_window_set_image_icon (GtkTooltipWindow *window,
                                   GdkPixbuf        *pixbuf)
{

  if (pixbuf != NULL)
    {
      gtk_image_set_from_pixbuf (GTK_IMAGE (window->image), pixbuf);
      gtk_widget_show (window->image);
    }
  else
    {
      gtk_widget_hide (window->image);
    }
}

void
gtk_tooltip_window_set_image_icon_from_stock (GtkTooltipWindow *window,
                                              const char       *stock_id,
                                              GtkIconSize       icon_size)
{
  if (stock_id != NULL)
    {
 G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_image_set_from_stock (GTK_IMAGE (window->image), stock_id, icon_size);
 G_GNUC_END_IGNORE_DEPRECATIONS;

      gtk_widget_show (window->image);
    }
  else
    {
      gtk_widget_hide (window->image);
    }
}

void
gtk_tooltip_window_set_image_icon_from_name (GtkTooltipWindow *window,
                                             const char       *icon_name,
                                             GtkIconSize       icon_size)
{
  if (icon_name)
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (window->image), icon_name, icon_size);
      gtk_widget_show (window->image);
    }
  else
    {
      gtk_widget_hide (window->image);
    }
}

void
gtk_tooltip_window_set_image_icon_from_gicon (GtkTooltipWindow *window,
                                              GIcon            *gicon,
                                              GtkIconSize       icon_size)
{
  if (gicon != NULL)
    {
      gtk_image_set_from_gicon (GTK_IMAGE (window->image), gicon, icon_size);
      gtk_widget_show (window->image);
    }
  else
    {
      gtk_widget_hide (window->image);
    }
}

void
gtk_tooltip_window_set_custom_widget (GtkTooltipWindow *window,
                                      GtkWidget        *custom_widget)
{
  /* No need to do anything if the custom widget stays the same */
  if (window->custom_widget == custom_widget)
    return;

  if (window->custom_widget != NULL)
    {
      GtkWidget *custom = window->custom_widget;

      /* Note: We must reset window->custom_widget first,
       * since gtk_container_remove() will recurse into
       * gtk_tooltip_set_custom()
       */
      window->custom_widget = NULL;
      gtk_container_remove (GTK_CONTAINER (window->box), custom);
      g_object_unref (custom);
    }

  if (custom_widget != NULL)
    {
      window->custom_widget = g_object_ref (custom_widget);

      gtk_container_add (GTK_CONTAINER (window->box), custom_widget);
      gtk_widget_show (custom_widget);
    }
}
