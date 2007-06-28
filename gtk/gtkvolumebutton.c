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

#include <config.h>

#include <glib/gi18n.h>
#include <atk/atk.h>

#include "gtkvolumebutton.h"
#include "gtktooltips.h"
#include "gtkstock.h"

#include "gtkalias.h"

#define EPSILON (1e-10)

struct _GtkVolumeButton
{
  GtkScaleButton  parent;
  GtkTooltips    *tooltips;
};

static void	gtk_volume_button_class_init	(GtkVolumeButtonClass *klass);
static void	gtk_volume_button_init		(GtkVolumeButton      *button);
static void	gtk_volume_button_dispose	(GObject *object);
static void	gtk_volume_button_update_tooltip(GtkVolumeButton *button);
static void	cb_value_changed		(GtkVolumeButton      *button,
						 gdouble               value,
						 gpointer              user_data);

G_DEFINE_TYPE (GtkVolumeButton, gtk_volume_button, GTK_TYPE_SCALE_BUTTON)

static void
gtk_volume_button_class_init (GtkVolumeButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_volume_button_dispose;
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
  atk_object_set_name (gtk_widget_get_accessible (sbutton->minus_button),
		       _("Volume Down"));
  atk_object_set_name (gtk_widget_get_accessible (sbutton->plus_button),
		       _("Volume Up"));

  gtk_scale_button_set_icons (sbutton, icons);

  adj = gtk_adjustment_new (0., 0., 1., 0.02, 0.2, 0.);
  g_object_set (G_OBJECT (button),
		"adjustment", adj,
		"size", GTK_ICON_SIZE_SMALL_TOOLBAR,
	       	NULL);

  button->tooltips = gtk_tooltips_new ();
  g_object_ref_sink (button->tooltips);
  gtk_volume_button_update_tooltip (button);

  g_signal_connect (G_OBJECT (button), "value-changed",
		    G_CALLBACK (cb_value_changed), NULL);
}

static void
gtk_volume_button_dispose (GObject *object)
{
  GtkVolumeButton *button;

  button = GTK_VOLUME_BUTTON (object);

  if (button->tooltips)
    g_object_unref (button->tooltips);
  button->tooltips = NULL;

  G_OBJECT_CLASS (gtk_volume_button_parent_class)->dispose (object);
}

/**
 * gtk_volume_button_new
 *
 * Creates a #GtkVolumeButton, with a range between 0 and 100, with
 * a stepping of 2. Volume values can be obtained and modified using
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

static void
gtk_volume_button_update_tooltip (GtkVolumeButton *button)
{
  GtkScaleButton *scale_button = GTK_SCALE_BUTTON (button);
  GtkAdjustment *adj;
  gdouble val;
  char *str;

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
       * Do not translate and do not include the "volume percentage|"
       * part in the translation!
       */
      str = g_strdup_printf (Q_("volume percentage|%d %%"), percent);
    }

  gtk_tooltips_set_tip (button->tooltips,
		       	GTK_WIDGET (button),
			str, NULL);
  g_free (str);
}

static void
cb_value_changed (GtkVolumeButton *button, gdouble value, gpointer user_data)
{
  gtk_volume_button_update_tooltip (button);
}


#define __GTK_VOLUME_BUTTON_C__
#include "gtkaliasdef.c"
