/* GTK - The GIMP Toolkit
 * Copyright (C) 1997 David Mosberger
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "gtkgamma.h"
#include "gtkcurve.h"
#include "gtkdialog.h"
#include "gtkdrawingarea.h"
#include "gtkentry.h"
#include "gtkhbox.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkpixmap.h"
#include "gtkradiobutton.h"
#include "gtksignal.h"
#include "gtktable.h"
#include "gtkvbox.h"
#include "gtkwindow.h"
#include "gtkintl.h"

static GtkVBoxClass *parent_class = NULL;


/* forward declarations: */
static void gtk_gamma_curve_class_init (GtkGammaCurveClass *class);
static void gtk_gamma_curve_init (GtkGammaCurve *curve);
static void gtk_gamma_curve_destroy (GtkObject *object);

static void curve_type_changed_callback (GtkWidget *w, gpointer data);
static void button_realize_callback     (GtkWidget *w);
static void button_toggled_callback     (GtkWidget *w, gpointer data);
static void button_clicked_callback     (GtkWidget *w, gpointer data);

enum
  {
    LINEAR = 0,
    SPLINE,
    FREE,
    GAMMA,
    RESET,
    NUM_XPMS
  };

static const char *xpm[][27] =
  {
    /* spline: */
    {
      /* width height ncolors chars_per_pixel */
      "16 16 4 1",
      /* colors */
      ". c None",
      "B c #000000",
      "+ c #BC2D2D",
      "r c #FF0000",
      /* pixels */
      "..............BB",
      ".........rrrrrrB",
      ".......rr.......",
      ".....B+.........",
      "....BBB.........",
      "....+B..........",
      "....r...........",
      "...r............",
      "...r............",
      "..r.............",
      "..r.............",
      ".r..............",
      ".r..............",
      ".r..............",
      "B+..............",
      "BB.............."
    },
    /* linear: */
    {
      /* width height ncolors chars_per_pixel */
      "16 16 5 1",
      /* colors */
      ". c None", /* transparent */
      "B c #000000",
      "' c #7F7F7F",
      "+ c #824141",
      "r c #FF0000",
      /* pixels */
      "..............BB",
      "..............+B",
      "..............r.",
      ".............r..",
      ".............r..",
      "....'B'.....r...",
      "....BBB.....r...",
      "....+B+....r....",
      "....r.r....r....",
      "...r...r..r.....",
      "...r...r..r.....",
      "..r.....rB+.....",
      "..r.....BBB.....",
      ".r......'B'.....",
      "B+..............",
      "BB.............."
    },
    /* free: */
    {
      /* width height ncolors chars_per_pixel */
      "16 16 2 1",
      /* colors */
      ". c None",
      "r c #FF0000",
      /* pixels */
      "................",
      "................",
      "......r.........",
      "......r.........",
      ".......r........",
      ".......r........",
      ".......r........",
      "........r.......",
      "........r.......",
      "........r.......",
      ".....r...r.rrrrr",
      "....r....r......",
      "...r.....r......",
      "..r.......r.....",
      ".r........r.....",
      "r..............."
    },
    /* gamma: */
    {
      /* width height ncolors chars_per_pixel */
      "16 16 10 1",
      /* colors */
      ". c None",
      "B c #000000",
      "& c #171717",
      "# c #2F2F2F",
      "X c #464646",
      "= c #5E5E5E",
      "/ c #757575",
      "+ c #8C8C8C",
      "- c #A4A4A4",
      "` c #BBBBBB",
      /* pixels */
      "................",
      "................",
      "................",
      "....B=..+B+.....",
      "....X&`./&-.....",
      "...../+.XX......",
      "......B.B+......",
      "......X.X.......",
      "......X&+.......",
      "......-B........",
      "....../=........",
      "......#B........",
      "......BB........",
      "......B#........",
      "................",
      "................"
    },
    /* reset: */
    {
      /* width height ncolors chars_per_pixel */
      "16 16 4 1",
      /* colors */
      ". c None",
      "B c #000000",
      "+ c #824141",
      "r c #FF0000",
      /* pixels */
      "..............BB",
      "..............+B",
      ".............r..",
      "............r...",
      "...........r....",
      "..........r.....",
      ".........r......",
      "........r.......",
      ".......r........",
      "......r.........",
      ".....r..........",
      "....r...........",
      "...r............",
      "..r.............",
      "B+..............",
      "BB.............."
    }
  };

guint
gtk_gamma_curve_get_type (void)
{
  static guint gamma_curve_type = 0;

  if (!gamma_curve_type)
    {
      static const GtkTypeInfo gamma_curve_info =
      {
	"GtkGammaCurve",
	sizeof (GtkGammaCurve),
	sizeof (GtkGammaCurveClass),
	(GtkClassInitFunc) gtk_gamma_curve_class_init,
	(GtkObjectInitFunc) gtk_gamma_curve_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      gamma_curve_type =
	gtk_type_unique (gtk_vbox_get_type (), &gamma_curve_info);
    }
  return gamma_curve_type;
}

static void
gtk_gamma_curve_class_init (GtkGammaCurveClass *class)
{
  GtkObjectClass *object_class;

  parent_class = gtk_type_class (gtk_vbox_get_type ());

  object_class = (GtkObjectClass *) class;
  object_class->destroy = gtk_gamma_curve_destroy;
}

static void
gtk_gamma_curve_init (GtkGammaCurve *curve)
{
  GtkWidget *vbox;
  int i;

  curve->gamma = 1.0;

  curve->table = gtk_table_new (1, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (curve->table), 3);
  gtk_container_add (GTK_CONTAINER (curve), curve->table);

  curve->curve = gtk_curve_new ();
  gtk_signal_connect (GTK_OBJECT (curve->curve), "curve_type_changed",
		      (GtkSignalFunc) curve_type_changed_callback, curve);
  gtk_table_attach_defaults (GTK_TABLE (curve->table), curve->curve, 0, 1, 0, 1);

  vbox = gtk_vbox_new (/* homogeneous */ FALSE, /* spacing */ 3);
  gtk_table_attach (GTK_TABLE (curve->table), vbox, 1, 2, 0, 1, 0, 0, 0, 0);

  /* toggle buttons: */
  for (i = 0; i < 3; ++i)
    {
      curve->button[i] = gtk_toggle_button_new ();
      gtk_object_set_data (GTK_OBJECT (curve->button[i]), "_GtkGammaCurveIndex",
			   GINT_TO_POINTER (i));
      gtk_container_add (GTK_CONTAINER (vbox), curve->button[i]);
      gtk_signal_connect (GTK_OBJECT (curve->button[i]), "realize",
			  (GtkSignalFunc) button_realize_callback, 0);
      gtk_signal_connect (GTK_OBJECT (curve->button[i]), "toggled",
			  (GtkSignalFunc) button_toggled_callback, curve);
      gtk_widget_show (curve->button[i]);
    }

  /* push buttons: */
  for (i = 3; i < 5; ++i)
    {
      curve->button[i] = gtk_button_new ();
      gtk_object_set_data (GTK_OBJECT (curve->button[i]), "_GtkGammaCurveIndex",
			   GINT_TO_POINTER (i));
      gtk_container_add (GTK_CONTAINER (vbox), curve->button[i]);
      gtk_signal_connect (GTK_OBJECT (curve->button[i]), "realize",
			  (GtkSignalFunc) button_realize_callback, 0);
      gtk_signal_connect (GTK_OBJECT (curve->button[i]), "clicked",
			  (GtkSignalFunc) button_clicked_callback, curve);
      gtk_widget_show (curve->button[i]);
    }

  gtk_widget_show (vbox);
  gtk_widget_show (curve->table);
  gtk_widget_show (curve->curve);
}

static void
button_realize_callback (GtkWidget *w)
{
  GtkWidget *pixmap;
  GdkBitmap *mask;
  GdkPixmap *pm;
  int i;

  i = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (w), "_GtkGammaCurveIndex"));
  pm = gdk_pixmap_create_from_xpm_d (w->window, &mask,
				     &w->style->bg[GTK_STATE_NORMAL], (gchar **)xpm[i]);

  pixmap = gtk_pixmap_new (pm, mask);
  gtk_container_add (GTK_CONTAINER (w), pixmap);
  gtk_widget_show (pixmap);

  gdk_pixmap_unref (pm);
  gdk_bitmap_unref (mask);   /* a bitmap is really just a special pixmap */
}

static void
button_toggled_callback (GtkWidget *w, gpointer data)
{
  GtkGammaCurve *c = data;
  GtkCurveType type;
  int active, i;

  if (!GTK_TOGGLE_BUTTON (w)->active)
    return;

  active = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (w), "_GtkGammaCurveIndex"));

  for (i = 0; i < 3; ++i)
    if ((i != active) && GTK_TOGGLE_BUTTON (c->button[i])->active)
      break;

  if (i < 3)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (c->button[i]), FALSE);

  switch (active)
    {
    case 0:  type = GTK_CURVE_TYPE_SPLINE; break;
    case 1:  type = GTK_CURVE_TYPE_LINEAR; break;
    default: type = GTK_CURVE_TYPE_FREE; break;
    }
  gtk_curve_set_curve_type (GTK_CURVE (c->curve), type);
}

static void
gamma_cancel_callback (GtkWidget *w, gpointer data)
{
  GtkGammaCurve *c = data;

  gtk_widget_destroy (c->gamma_dialog);
  c->gamma_dialog = 0;
}

static void
gamma_ok_callback (GtkWidget *w, gpointer data)
{
  GtkGammaCurve *c = data;
  gchar *start, *end;
  gfloat v;

  start = gtk_entry_get_text (GTK_ENTRY (c->gamma_text));
  if (start)
    {
      v = strtod (start, &end);
      if (end > start && v > 0.0)
	c->gamma = v;
    }
  gtk_curve_set_gamma (GTK_CURVE (c->curve), c->gamma);
  gamma_cancel_callback (w, data);
}

static void
button_clicked_callback (GtkWidget *w, gpointer data)
{
  GtkGammaCurve *c = data;
  int active;

  active = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (w), "_GtkGammaCurveIndex"));
  if (active == 3)
    {
      /* set gamma */
      if (c->gamma_dialog)
	return;
      else
	{
	  GtkWidget *vbox, *hbox, *label, *button;
	  gchar buf[64];
	  
	  c->gamma_dialog = gtk_dialog_new ();
	  gtk_window_set_title (GTK_WINDOW (c->gamma_dialog), _("Gamma"));
	  vbox = GTK_DIALOG (c->gamma_dialog)->vbox;
	  
	  hbox = gtk_hbox_new (/* homogeneous */ FALSE, 0);
	  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 2);
	  gtk_widget_show (hbox);
	  
	  label = gtk_label_new (_("Gamma value"));
	  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
	  gtk_widget_show (label);
	  
	  sprintf (buf, "%g", c->gamma);
	  c->gamma_text = gtk_entry_new ();
	  gtk_entry_set_text (GTK_ENTRY (c->gamma_text), buf);
	  gtk_box_pack_start (GTK_BOX (hbox), c->gamma_text, TRUE, TRUE, 2);
	  gtk_widget_show (c->gamma_text);
	  
	  /* fill in action area: */
	  hbox = GTK_DIALOG (c->gamma_dialog)->action_area;
	  
	  button = gtk_button_new_with_label (_("OK"));
	  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	  gtk_signal_connect (GTK_OBJECT (button), "clicked",
			      (GtkSignalFunc) gamma_ok_callback, c);
	  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	  gtk_widget_grab_default (button);
	  gtk_widget_show (button);
	  
	  button = gtk_button_new_with_label (_("Cancel"));
	  gtk_signal_connect (GTK_OBJECT (button), "clicked",
			      (GtkSignalFunc) gamma_cancel_callback, c);
	  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	  gtk_widget_show (button);
	  
	  gtk_widget_show (c->gamma_dialog);
	}
    }
  else
    {
      /* reset */
      gtk_curve_reset (GTK_CURVE (c->curve));
    }
}

static void
curve_type_changed_callback (GtkWidget *w, gpointer data)
{
  GtkGammaCurve *c = data;
  GtkCurveType new_type;
  int active;

  new_type = GTK_CURVE (w)->curve_type;
  switch (new_type)
    {
    case GTK_CURVE_TYPE_SPLINE: active = 0; break;
    case GTK_CURVE_TYPE_LINEAR: active = 1; break;
    default:		        active = 2; break;
    }
  if (!GTK_TOGGLE_BUTTON (c->button[active])->active)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (c->button[active]), TRUE);
}

GtkWidget*
gtk_gamma_curve_new (void)
{
  return gtk_type_new (gtk_gamma_curve_get_type ());
}

static void
gtk_gamma_curve_destroy (GtkObject *object)
{
  GtkGammaCurve *c;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_GAMMA_CURVE (object));

  c = GTK_GAMMA_CURVE (object);

  if (c->gamma_dialog)
    gtk_widget_destroy (c->gamma_dialog);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

