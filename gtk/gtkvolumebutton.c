/* GTK - The GIMP Toolkit
 * Copyright (C) 2007 Red Hat, Inc.
 *
 * Authors:
 * - Bastien Nocera <bnocera@redhat.com>
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

/*
 * Modified by the GTK+ Team and others 2007.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkvolumebutton.h"
#include "gtkstock.h"
#include "gtktooltip.h"
#include "gtkintl.h"

#include "gtkalias.h"

/**
 * SECTION:gtkvolumebutton
 * @Short_description: A button which pops up a volume control
 * @Title: GtkVolumeButton
 *
 * #GtkVolumeButton is a subclass of #GtkScaleButton that has
 * been tailored for use as a volume control widget with suitable
 * icons, tooltips and accessible labels.
 */

#define EPSILON (1e-10)


static gboolean	cb_query_tooltip (GtkWidget       *button,
                                  gint             x,
                                  gint             y,
                                  gboolean         keyboard_mode,
                                  GtkTooltip      *tooltip,
                                  gpointer         user_data);
static void	cb_value_changed (GtkVolumeButton *button,
                                  gdouble          value,
                                  gpointer         user_data);

G_DEFINE_TYPE (GtkVolumeButton, gtk_volume_button, GTK_TYPE_SCALE_BUTTON)

static void
gtk_volume_button_class_init (GtkVolumeButtonClass *klass)
{
}

static void
gtk_volume_button_init (GtkVolumeButton *button)
{
  GtkScaleButton *sbutton = GTK_SCALE_BUTTON (button);
  GtkObject *adj;
  const char *icons[] = {
	"audio-volume-muted",
	"audio-volume-high",
	"audio-volume-low",
	"audio-volume-medium",
	NULL
  };

  atk_object_set_name (gtk_widget_get_accessible (GTK_WIDGET (button)),
		       _("Volume"));
  atk_object_set_description (gtk_widget_get_accessible (GTK_WIDGET (button)),
		       _("Turns volume down or up"));
  atk_action_set_description (ATK_ACTION (gtk_widget_get_accessible (GTK_WIDGET (button))),
			      1,
			      _("Adjusts the volume"));

  atk_object_set_name (gtk_widget_get_accessible (sbutton->minus_button),
		       _("Volume Down"));
  atk_object_set_description (gtk_widget_get_accessible (sbutton->minus_button),
		       _("Decreases the volume"));
  gtk_widget_set_tooltip_text (sbutton->minus_button, _("Volume Down"));

  atk_object_set_name (gtk_widget_get_accessible (sbutton->plus_button),
		       _("Volume Up"));
  atk_object_set_description (gtk_widget_get_accessible (sbutton->plus_button),
		       _("Increases the volume"));
  gtk_widget_set_tooltip_text (sbutton->plus_button, _("Volume Up"));

  gtk_scale_button_set_icons (sbutton, icons);

  adj = gtk_adjustment_new (0., 0., 1., 0.02, 0.2, 0.);
  g_object_set (G_OBJECT (button),
		"adjustment", adj,
		"size", GTK_ICON_SIZE_SMALL_TOOLBAR,
		"has-tooltip", TRUE,
	       	NULL);

  g_signal_connect (G_OBJECT (button), "query-tooltip",
		    G_CALLBACK (cb_query_tooltip), NULL);
  g_signal_connect (G_OBJECT (button), "value-changed",
		    G_CALLBACK (cb_value_changed), NULL);
}

/**
 * gtk_volume_button_new
 *
 * Creates a #GtkVolumeButton, with a range between 0.0 and 1.0, with
 * a stepping of 0.02. Volume values can be obtained and modified using
 * the functions from #GtkScaleButton.
 *
 * Return value: a new #GtkVolumeButton
 *
 * Since: 2.12
 */
GtkWidget *
gtk_volume_button_new (void)
{
  GObject *button;
  button = g_object_new (GTK_TYPE_VOLUME_BUTTON, NULL);
  return GTK_WIDGET (button);
}

static gboolean
cb_query_tooltip (GtkWidget  *button,
		  gint        x,
		  gint        y,
		  gboolean    keyboard_mode,
		  GtkTooltip *tooltip,
		  gpointer    user_data)
{
  GtkScaleButton *scale_button = GTK_SCALE_BUTTON (button);
  GtkAdjustment *adj;
  gdouble val;
  char *str;
  AtkImage *image;

  image = ATK_IMAGE (gtk_widget_get_accessible (button));

  adj = gtk_scale_button_get_adjustment (scale_button);
  val = gtk_scale_button_get_value (scale_button);

  if (val < (adj->lower + EPSILON))
    {
      str = g_strdup (_("Muted"));
    }
  else if (val >= (adj->upper - EPSILON))
    {
      str = g_strdup (_("Full Volume"));
    }
  else
    {
      int percent;

      percent = (int) (100. * val / (adj->upper - adj->lower) + .5);

      /* Translators: this is the percentage of the current volume,
       * as used in the tooltip, eg. "49 %".
       * Translate the "%d" to "%Id" if you want to use localised digits,
       * or otherwise translate the "%d" to "%d".
       */
      str = g_strdup_printf (C_("volume percentage", "%d %%"), percent);
    }

  gtk_tooltip_set_text (tooltip, str);
  atk_image_set_image_description (image, str);
  g_free (str);

  return TRUE;
}

static void
cb_value_changed (GtkVolumeButton *button, gdouble value, gpointer user_data)
{
  gtk_widget_trigger_tooltip_query (GTK_WIDGET (button));
}

#define __GTK_VOLUME_BUTTON_C__
#include "gtkaliasdef.c"
