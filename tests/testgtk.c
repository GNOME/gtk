/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#undef GTK_DISABLE_DEPRECATED

#undef GDK_DISABLE_DEPRECATED

#include "config.h"

#undef	G_LOG_DOMAIN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define GTK_ENABLE_BROKEN
#include "gtk/gtk.h"
#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"

#ifdef G_OS_WIN32
#define sleep(n) _sleep(n)
#endif

#include "prop-editor.h"

#include "circles.xbm"
#include "test.xpm"

gboolean
file_exists (const char *filename)
{
  struct stat statbuf;

  return stat (filename, &statbuf) == 0;
}

GtkWidget *
shape_create_icon (GdkScreen *screen,
		   char      *xpm_file,
		   gint       x,
		   gint       y,
		   gint       px,
		   gint       py,
		   gint       window_type);

static GtkWidget *
build_option_menu (gchar           *items[],
		   gint             num_items,
		   gint             history,
		   void           (*func) (GtkWidget *widget, gpointer data),
		   gpointer         data);

/* macro, structure and variables used by tree window demos */
#define DEFAULT_NUMBER_OF_ITEM  3
#define DEFAULT_RECURSION_LEVEL 3

struct {
  GSList* selection_mode_group;
  GtkWidget* single_button;
  GtkWidget* browse_button;
  GtkWidget* multiple_button;
  GtkWidget* draw_line_button;
  GtkWidget* view_line_button;
  GtkWidget* no_root_item_button;
  GtkWidget* nb_item_spinner;
  GtkWidget* recursion_spinner;
} sTreeSampleSelection;

typedef struct sTreeButtons {
  guint nb_item_add;
  GtkWidget* add_button;
  GtkWidget* remove_button;
  GtkWidget* subtree_button;
} sTreeButtons;
/* end of tree section */

static GtkWidget *
build_option_menu (gchar           *items[],
		   gint             num_items,
		   gint             history,
		   void           (*func)(GtkWidget *widget, gpointer data),
		   gpointer         data)
{
  GtkWidget *omenu;
  GtkWidget *menu;
  GSList *group;
  gint i;

  omenu = gtk_combo_box_text_new ();
  g_signal_connect (omenu, "changed",
		    G_CALLBACK (func), data);
      
  menu = gtk_menu_new ();
  group = NULL;
  
  for (i = 0; i < num_items; i++)
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (omenu), items[i]);

  gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), history);
  
  return omenu;
}

static void
destroy_tooltips (GtkWidget *widget, GtkWindow **window)
{
  GtkTooltips *tt = g_object_get_data (G_OBJECT (*window), "tooltips");
  g_object_unref (tt);
  *window = NULL;
}


/*
 * Windows with an alpha channel
 */


static gboolean
on_alpha_window_expose (GtkWidget      *widget,
			GdkEventExpose *expose)
{
  cairo_t *cr;
  cairo_pattern_t *pattern;
  int radius;

  cr = gdk_cairo_create (widget->window);

  radius = MIN (widget->allocation.width, widget->allocation.height) / 2;
  pattern = cairo_pattern_create_radial (widget->allocation.width / 2,
					 widget->allocation.height / 2,
					 0.0,
					 widget->allocation.width / 2,
					 widget->allocation.height / 2,
					 radius * 1.33);

  if (gdk_screen_get_rgba_colormap (gtk_widget_get_screen (widget)) &&
      gtk_widget_is_composited (widget))
    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0); /* transparent */
  else
    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0); /* opaque white */
    
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);
  
  cairo_pattern_add_color_stop_rgba (pattern, 0.0,
				     1.0, 0.75, 0.0, 1.0); /* solid orange */
  cairo_pattern_add_color_stop_rgba (pattern, 1.0,
				     1.0, 0.75, 0.0, 0.0); /* transparent orange */

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);
  
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_paint (cr);

  cairo_destroy (cr);

  return FALSE;
}

static GtkWidget *
build_alpha_widgets (void)
{
  GtkWidget *table;
  GtkWidget *radio_button;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *entry;

  table = gtk_table_new (1, 1, FALSE);

  radio_button = gtk_radio_button_new_with_label (NULL, "Red");
  gtk_table_attach (GTK_TABLE (table),
		    radio_button,
		    0, 1,                  0, 1,
		    GTK_EXPAND | GTK_FILL, 0,
		    0,                     0);

  radio_button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio_button), "Green");
  gtk_table_attach (GTK_TABLE (table),
		    radio_button,
		    0, 1,                  1, 2,
		    GTK_EXPAND | GTK_FILL, 0,
		    0,                     0);

  radio_button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio_button), "Blue"),
  gtk_table_attach (GTK_TABLE (table),
		    radio_button,
		    0, 1,                  2, 3,
		    GTK_EXPAND | GTK_FILL, 0,
		    0,                     0);

  gtk_table_attach (GTK_TABLE (table),
		    gtk_check_button_new_with_label ("Sedentary"),
		    1, 2,                  0, 1,
		    GTK_EXPAND | GTK_FILL, 0,
		    0,                     0);
  gtk_table_attach (GTK_TABLE (table),
		    gtk_check_button_new_with_label ("Nocturnal"),
		    1, 2,                  1, 2,
		    GTK_EXPAND | GTK_FILL, 0,
		    0,                     0);
  gtk_table_attach (GTK_TABLE (table),
		    gtk_check_button_new_with_label ("Compulsive"),
		    1, 2,                  2, 3,
		    GTK_EXPAND | GTK_FILL, 0,
		    0,                     0);

  radio_button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio_button), "Green");
  gtk_table_attach (GTK_TABLE (table),
		    radio_button,
		    0, 1,                  1, 2,
		    GTK_EXPAND | GTK_FILL, 0,
		    0,                     0);

  radio_button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio_button), "Blue"),
  gtk_table_attach (GTK_TABLE (table),
		    radio_button,
		    0, 1,                  2, 3,
		    GTK_EXPAND | GTK_FILL, 0,
		    0,                     0);
  
  hbox = gtk_hbox_new (FALSE, 0);
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), "<i>Entry: </i>");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
  gtk_table_attach (GTK_TABLE (table),
		    hbox,
		    0, 1,                  3, 4,
		    GTK_EXPAND | GTK_FILL, 0,
		    0,                     0);
  
  return table;
}

static void
on_alpha_screen_changed (GtkWidget *widget,
			 GdkScreen *old_screen,
			 GtkWidget *label)
{
  GdkScreen *screen = gtk_widget_get_screen (widget);
  GdkColormap *colormap = gdk_screen_get_rgba_colormap (screen);

  if (!colormap)
    {
      colormap = gdk_screen_get_default_colormap (screen);
      gtk_label_set_markup (GTK_LABEL (label), "<b>Screen doesn't support alpha</b>");
    }
  else
    {
      gtk_label_set_markup (GTK_LABEL (label), "<b>Screen supports alpha</b>");
    }

  gtk_widget_set_colormap (widget, colormap);
}

static void
on_composited_changed (GtkWidget *window,
		      GtkLabel *label)
{
  gboolean is_composited = gtk_widget_is_composited (window);

  if (is_composited)
    gtk_label_set_text (label, "Composited");
  else
    gtk_label_set_text (label, "Not composited");
}

void
create_alpha_window (GtkWidget *widget)
{
  static GtkWidget *window;

  if (!window)
    {
      GtkWidget *vbox;
      GtkWidget *label;
      
      window = gtk_dialog_new_with_buttons ("Alpha Window",
					    GTK_WINDOW (gtk_widget_get_toplevel (widget)), 0,
					    GTK_STOCK_CLOSE, 0,
					    NULL);

      gtk_widget_set_app_paintable (window, TRUE);
      g_signal_connect (window, "expose-event",
			G_CALLBACK (on_alpha_window_expose), NULL);
      
      vbox = gtk_vbox_new (FALSE, 8);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), vbox,
			  TRUE, TRUE, 0);

      label = gtk_label_new (NULL);
      gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
      on_alpha_screen_changed (window, NULL, label);
      g_signal_connect (window, "screen-changed",
			G_CALLBACK (on_alpha_screen_changed), label);
      
      label = gtk_label_new (NULL);
      gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
      on_composited_changed (window, GTK_LABEL (label));
      g_signal_connect (window, "composited_changed", G_CALLBACK (on_composited_changed), label);
      
      gtk_box_pack_start (GTK_BOX (vbox), build_alpha_widgets (), TRUE, TRUE, 0);

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);
      
      g_signal_connect (window, "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL); 
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Composited non-toplevel window
 */

/* The expose event handler for the event box.
 *
 * This function simply draws a transparency onto a widget on the area
 * for which it receives expose events.  This is intended to give the
 * event box a "transparent" background.
 *
 * In order for this to work properly, the widget must have an RGBA
 * colourmap.  The widget should also be set as app-paintable since it
 * doesn't make sense for GTK to draw a background if we are drawing it
 * (and because GTK might actually replace our transparency with its
 * default background colour).
 */
static gboolean
transparent_expose (GtkWidget *widget,
                    GdkEventExpose *event)
{
  cairo_t *cr;

  cr = gdk_cairo_create (widget->window);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  gdk_cairo_region (cr, event->region);
  cairo_fill (cr);
  cairo_destroy (cr);

  return FALSE;
}

/* The expose event handler for the window.
 *
 * This function performs the actual compositing of the event box onto
 * the already-existing background of the window at 50% normal opacity.
 *
 * In this case we do not want app-paintable to be set on the widget
 * since we want it to draw its own (red) background.  Because of this,
 * however, we must ensure that we use g_signal_register_after so that
 * this handler is called after the red has been drawn.  If it was
 * called before then GTK would just blindly paint over our work.
 */
static gboolean
window_expose_event (GtkWidget *widget,
                     GdkEventExpose *event)
{
  GdkRegion *region;
  GtkWidget *child;
  cairo_t *cr;

  /* get our child (in this case, the event box) */ 
  child = gtk_bin_get_child (GTK_BIN (widget));

  /* create a cairo context to draw to the window */
  cr = gdk_cairo_create (widget->window);

  /* the source data is the (composited) event box */
  gdk_cairo_set_source_pixmap (cr, child->window,
                               child->allocation.x,
                               child->allocation.y);

  /* draw no more than our expose event intersects our child */
  region = gdk_region_rectangle (&child->allocation);
  gdk_region_intersect (region, event->region);
  gdk_cairo_region (cr, region);
  cairo_clip (cr);

  /* composite, with a 50% opacity */
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_paint_with_alpha (cr, 0.5);

  /* we're done */
  cairo_destroy (cr);

  return FALSE;
}

void
create_composited_window (GtkWidget *widget)
{
  static GtkWidget *window;

  if (!window)
    {
      GtkWidget *event, *button;
      GdkScreen *screen;
      GdkColormap *rgba;
      GdkColor red;

      /* make the widgets */
      button = gtk_button_new_with_label ("A Button");
      event = gtk_event_box_new ();
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &window);

      /* put a red background on the window */
      gdk_color_parse ("red", &red);
      gtk_widget_modify_bg (window, GTK_STATE_NORMAL, &red);

      /* set the colourmap for the event box.
       * must be done before the event box is realised.
       */
      screen = gtk_widget_get_screen (event);
      rgba = gdk_screen_get_rgba_colormap (screen);
      gtk_widget_set_colormap (event, rgba);

      /* set our event box to have a fully-transparent background
       * drawn on it.  currently there is no way to simply tell gtk
       * that "transparency" is the background colour for a widget.
       */
      gtk_widget_set_app_paintable (GTK_WIDGET (event), TRUE);
      g_signal_connect (event, "expose-event",
                        G_CALLBACK (transparent_expose), NULL);

      /* put them inside one another */
      gtk_container_set_border_width (GTK_CONTAINER (window), 10);
      gtk_container_add (GTK_CONTAINER (window), event);
      gtk_container_add (GTK_CONTAINER (event), button);

      /* realise and show everything */
      gtk_widget_realize (button);

      /* set the event box GdkWindow to be composited.
       * obviously must be performed after event box is realised.
       */
      gdk_window_set_composited (event->window, TRUE);

      /* set up the compositing handler.
       * note that we do _after so that the normal (red) background is drawn
       * by gtk before our compositing occurs.
       */
      g_signal_connect_after (window, "expose-event",
                              G_CALLBACK (window_expose_event), NULL);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Big windows and guffaw scrolling
 */

static gboolean
pattern_expose (GtkWidget      *widget,
		GdkEventExpose *event,
		gpointer        data)
{
  GdkColor *color;
  GdkWindow *window = event->window;

  color = g_object_get_data (G_OBJECT (window), "pattern-color");
  if (color)
    {
      cairo_t *cr = gdk_cairo_create (window);

      gdk_cairo_set_source_color (cr, color);
      gdk_cairo_rectangle (cr, &event->area);
      cairo_fill (cr);

      cairo_destroy (cr);
    }

  return FALSE;
}

static void
pattern_set_bg (GtkWidget   *widget,
		GdkWindow   *child,
		gint         level)
{
  static const GdkColor colors[] = {
    { 0, 0x4444, 0x4444, 0xffff },
    { 0, 0x8888, 0x8888, 0xffff },
    { 0, 0xaaaa, 0xaaaa, 0xffff }
  };
    
  g_object_set_data (G_OBJECT (child), "pattern-color", (gpointer) &colors[level]);
  gdk_window_set_user_data (child, widget);
}

static void
create_pattern (GtkWidget   *widget,
		GdkWindow   *parent,
		gint         level,
		gint         width,
		gint         height)
{
  gint h = 1;
  gint i = 0;
    
  GdkWindow *child;

  while (2 * h <= height)
    {
      gint w = 1;
      gint j = 0;
      
      while (2 * w <= width)
	{
	  if ((i + j) % 2 == 0)
	    {
	      gint x = w  - 1;
	      gint y = h - 1;
	      
	      GdkWindowAttr attributes;

	      attributes.window_type = GDK_WINDOW_CHILD;
	      attributes.x = x;
	      attributes.y = y;
	      attributes.width = w;
	      attributes.height = h;
	      attributes.wclass = GDK_INPUT_OUTPUT;
	      attributes.event_mask = GDK_EXPOSURE_MASK;
	      attributes.visual = gtk_widget_get_visual (widget);
	      attributes.colormap = gtk_widget_get_colormap (widget);
	      
	      child = gdk_window_new (parent, &attributes,
				      GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP);

	      pattern_set_bg (widget, child, level);

	      if (level < 2)
		create_pattern (widget, child, level + 1, w, h);

	      gdk_window_show (child);
	    }
	  j++;
	  w *= 2;
	}
      i++;
      h *= 2;
    }
}

#define PATTERN_SIZE (1 << 18)

static void
pattern_hadj_changed (GtkAdjustment *adj,
		      GtkWidget     *darea)
{
  gint *old_value = g_object_get_data (G_OBJECT (adj), "old-value");
  gint new_value = adj->value;

  if (gtk_widget_get_realized (darea))
    {
      gdk_window_scroll (darea->window, *old_value - new_value, 0);
      *old_value = new_value;
    }
}

static void
pattern_vadj_changed (GtkAdjustment *adj,
		      GtkWidget *darea)
{
  gint *old_value = g_object_get_data (G_OBJECT (adj), "old-value");
  gint new_value = adj->value;

  if (gtk_widget_get_realized (darea))
    {
      gdk_window_scroll (darea->window, 0, *old_value - new_value);
      *old_value = new_value;
    }
}

static void
pattern_realize (GtkWidget *widget,
		 gpointer   data)
{
  pattern_set_bg (widget, widget->window, 0);
  create_pattern (widget, widget->window, 1, PATTERN_SIZE, PATTERN_SIZE);
}

static void 
create_big_windows (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *darea, *table, *scrollbar;
  GtkWidget *eventbox;
  GtkAdjustment *hadj;
  GtkAdjustment *vadj;
  static gint current_x;
  static gint current_y;
 
  if (!window)
    {
      current_x = 0;
      current_y = 0;
      
      window = gtk_dialog_new_with_buttons ("Big Windows",
                                            NULL, 0,
                                            GTK_STOCK_CLOSE,
                                            GTK_RESPONSE_NONE,
                                            NULL);
 
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      gtk_window_set_default_size (GTK_WINDOW (window), 200, 300);

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      g_signal_connect (window, "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);

      table = gtk_table_new (2, 2, FALSE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox),
			  table, TRUE, TRUE, 0);

      darea = gtk_drawing_area_new ();

      hadj = (GtkAdjustment *)gtk_adjustment_new (0, 0, PATTERN_SIZE, 10, 100, 100);
      g_signal_connect (hadj, "value_changed",
			G_CALLBACK (pattern_hadj_changed), darea);
      g_object_set_data (G_OBJECT (hadj), "old-value", &current_x);
      
      vadj = (GtkAdjustment *)gtk_adjustment_new (0, 0, PATTERN_SIZE, 10, 100, 100);
      g_signal_connect (vadj, "value_changed",
			G_CALLBACK (pattern_vadj_changed), darea);
      g_object_set_data (G_OBJECT (vadj), "old-value", &current_y);
      
      g_signal_connect (darea, "realize",
                        G_CALLBACK (pattern_realize),
                        NULL);
      g_signal_connect (darea, "expose_event",
                        G_CALLBACK (pattern_expose),
                        NULL);

      eventbox = gtk_event_box_new ();
      gtk_table_attach (GTK_TABLE (table), eventbox,
			0, 1,                  0, 1,
			GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
			0,                     0);

      gtk_container_add (GTK_CONTAINER (eventbox), darea);

      scrollbar = gtk_hscrollbar_new (hadj);
      gtk_table_attach (GTK_TABLE (table), scrollbar,
			0, 1,                  1, 2,
			GTK_FILL | GTK_EXPAND, GTK_FILL,
			0,                     0);

      scrollbar = gtk_vscrollbar_new (vadj);
      gtk_table_attach (GTK_TABLE (table), scrollbar,
			1, 2,                  0, 1,
			GTK_FILL,              GTK_EXPAND | GTK_FILL,
			0,                     0);

    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_hide (window);
}

/*
 * GtkButton
 */

static void
button_window (GtkWidget *widget,
	       GtkWidget *button)
{
  if (!gtk_widget_get_visible (button))
    gtk_widget_show (button);
  else
    gtk_widget_hide (button);
}

static void
create_buttons (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *table;
  GtkWidget *button[10];
  GtkWidget *separator;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkButton");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      table = gtk_table_new (3, 3, FALSE);
      gtk_table_set_row_spacings (GTK_TABLE (table), 5);
      gtk_table_set_col_spacings (GTK_TABLE (table), 5);
      gtk_container_set_border_width (GTK_CONTAINER (table), 10);
      gtk_box_pack_start (GTK_BOX (box1), table, TRUE, TRUE, 0);

      button[0] = gtk_button_new_with_label ("button1");
      button[1] = gtk_button_new_with_mnemonic ("_button2");
      button[2] = gtk_button_new_with_mnemonic ("_button3");
      button[3] = gtk_button_new_from_stock (GTK_STOCK_OK);
      button[4] = gtk_button_new_with_label ("button5");
      button[5] = gtk_button_new_with_label ("button6");
      button[6] = gtk_button_new_with_label ("button7");
      button[7] = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
      button[8] = gtk_button_new_with_label ("button9");
      
      g_signal_connect (button[0], "clicked",
			G_CALLBACK (button_window),
			button[1]);

      gtk_table_attach (GTK_TABLE (table), button[0], 0, 1, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      g_signal_connect (button[1], "clicked",
			G_CALLBACK (button_window),
			button[2]);

      gtk_table_attach (GTK_TABLE (table), button[1], 1, 2, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      g_signal_connect (button[2], "clicked",
			G_CALLBACK (button_window),
			button[3]);
      gtk_table_attach (GTK_TABLE (table), button[2], 2, 3, 2, 3,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      g_signal_connect (button[3], "clicked",
			G_CALLBACK (button_window),
			button[4]);
      gtk_table_attach (GTK_TABLE (table), button[3], 0, 1, 2, 3,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      g_signal_connect (button[4], "clicked",
			G_CALLBACK (button_window),
			button[5]);
      gtk_table_attach (GTK_TABLE (table), button[4], 2, 3, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      g_signal_connect (button[5], "clicked",
			G_CALLBACK (button_window),
			button[6]);
      gtk_table_attach (GTK_TABLE (table), button[5], 1, 2, 2, 3,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      g_signal_connect (button[6], "clicked",
			G_CALLBACK (button_window),
			button[7]);
      gtk_table_attach (GTK_TABLE (table), button[6], 1, 2, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      g_signal_connect (button[7], "clicked",
			G_CALLBACK (button_window),
			button[8]);
      gtk_table_attach (GTK_TABLE (table), button[7], 2, 3, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      g_signal_connect (button[8], "clicked",
			G_CALLBACK (button_window),
			button[0]);
      gtk_table_attach (GTK_TABLE (table), button[8], 0, 1, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button[9] = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button[9], "clicked",
				G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_box_pack_start (GTK_BOX (box2), button[9], TRUE, TRUE, 0);
      gtk_widget_set_can_default (button[9], TRUE);
      gtk_widget_grab_default (button[9]);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkToggleButton
 */

static void
create_toggle_buttons (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkToggleButton");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      button = gtk_toggle_button_new_with_label ("button1");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_toggle_button_new_with_label ("button2");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_toggle_button_new_with_label ("button3");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_toggle_button_new_with_label ("inconsistent");
      gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (button), TRUE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

static GtkWidget *
create_widget_grid (GType widget_type)
{
  GtkWidget *table;
  GtkWidget *group_widget = NULL;
  gint i, j;
  
  table = gtk_table_new (FALSE, 3, 3);
  
  for (i = 0; i < 5; i++)
    {
      for (j = 0; j < 5; j++)
	{
	  GtkWidget *widget;
	  char *tmp;
	  
	  if (i == 0 && j == 0)
	    {
	      widget = NULL;
	    }
	  else if (i == 0)
	    {
	      tmp = g_strdup_printf ("%d", j);
	      widget = gtk_label_new (tmp);
	      g_free (tmp);
	    }
	  else if (j == 0)
	    {
	      tmp = g_strdup_printf ("%c", 'A' + i - 1);
	      widget = gtk_label_new (tmp);
	      g_free (tmp);
	    }
	  else
	    {
	      widget = g_object_new (widget_type, NULL);
	      
	      if (g_type_is_a (widget_type, GTK_TYPE_RADIO_BUTTON))
		{
		  if (!group_widget)
		    group_widget = widget;
		  else
		    g_object_set (widget, "group", group_widget, NULL);
		}
	    }
	  
	  if (widget)
	    gtk_table_attach (GTK_TABLE (table), widget,
			      i, i + 1, j, j + 1,
			      0,        0,
			      0,        0);
	}
    }

  return table;
}

/*
 * GtkCheckButton
 */

static void
create_check_buttons (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *table;
  
  if (!window)
    {
      window = gtk_dialog_new_with_buttons ("Check Buttons",
                                            NULL, 0,
                                            GTK_STOCK_CLOSE,
                                            GTK_RESPONSE_NONE,
                                            NULL);

      gtk_window_set_screen (GTK_WINDOW (window), 
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);
      g_signal_connect (window, "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);

      box1 = GTK_DIALOG (window)->vbox;
      
      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      button = gtk_check_button_new_with_mnemonic ("_button1");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_check_button_new_with_label ("button2");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_check_button_new_with_label ("button3");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_check_button_new_with_label ("inconsistent");
      gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (button), TRUE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      table = create_widget_grid (GTK_TYPE_CHECK_BUTTON);
      gtk_container_set_border_width (GTK_CONTAINER (table), 10);
      gtk_box_pack_start (GTK_BOX (box1), table, TRUE, TRUE, 0);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkRadioButton
 */

static void
create_radio_buttons (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *table;

  if (!window)
    {
      window = gtk_dialog_new_with_buttons ("Radio Buttons",
                                            NULL, 0,
                                            GTK_STOCK_CLOSE,
                                            GTK_RESPONSE_NONE,
                                            NULL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);
      g_signal_connect (window, "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);

      box1 = GTK_DIALOG (window)->vbox;

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      button = gtk_radio_button_new_with_label (NULL, "button1");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_radio_button_new_with_label (
	         gtk_radio_button_get_group (GTK_RADIO_BUTTON (button)),
		 "button2");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_radio_button_new_with_label (
                 gtk_radio_button_get_group (GTK_RADIO_BUTTON (button)),
		 "button3");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_radio_button_new_with_label (
                 gtk_radio_button_get_group (GTK_RADIO_BUTTON (button)),
		 "inconsistent");
      gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (button), TRUE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      button = gtk_radio_button_new_with_label (NULL, "button4");
      gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_radio_button_new_with_label (
	         gtk_radio_button_get_group (GTK_RADIO_BUTTON (button)),
		 "button5");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
      gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_radio_button_new_with_label (
                 gtk_radio_button_get_group (GTK_RADIO_BUTTON (button)),
		 "button6");
      gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      table = create_widget_grid (GTK_TYPE_RADIO_BUTTON);
      gtk_container_set_border_width (GTK_CONTAINER (table), 10);
      gtk_box_pack_start (GTK_BOX (box1), table, TRUE, TRUE, 0);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkButtonBox
 */

static GtkWidget *
create_bbox (gint  horizontal,
	     char* title, 
	     gint  spacing,
	     gint  child_w,
	     gint  child_h,
	     gint  layout)
{
  GtkWidget *frame;
  GtkWidget *bbox;
  GtkWidget *button;
	
  frame = gtk_frame_new (title);

  if (horizontal)
    bbox = gtk_hbutton_box_new ();
  else
    bbox = gtk_vbutton_box_new ();

  gtk_container_set_border_width (GTK_CONTAINER (bbox), 5);
  gtk_container_add (GTK_CONTAINER (frame), bbox);

  gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), layout);
  gtk_box_set_spacing (GTK_BOX (bbox), spacing);
  gtk_button_box_set_child_size (GTK_BUTTON_BOX (bbox), child_w, child_h);
  
  button = gtk_button_new_with_label ("OK");
  gtk_container_add (GTK_CONTAINER (bbox), button);
  
  button = gtk_button_new_with_label ("Cancel");
  gtk_container_add (GTK_CONTAINER (bbox), button);
  
  button = gtk_button_new_with_label ("Help");
  gtk_container_add (GTK_CONTAINER (bbox), button);

  return frame;
}

static void
create_button_box (GtkWidget *widget)
{
  static GtkWidget* window = NULL;
  GtkWidget *main_vbox;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *frame_horz;
  GtkWidget *frame_vert;

  if (!window)
  {
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_screen (GTK_WINDOW (window), gtk_widget_get_screen (widget));
    gtk_window_set_title (GTK_WINDOW (window), "Button Boxes");
    
    g_signal_connect (window, "destroy",
		      G_CALLBACK (gtk_widget_destroyed),
		      &window);

    gtk_container_set_border_width (GTK_CONTAINER (window), 10);

    main_vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (window), main_vbox);
    
    frame_horz = gtk_frame_new ("Horizontal Button Boxes");
    gtk_box_pack_start (GTK_BOX (main_vbox), frame_horz, TRUE, TRUE, 10);
    
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
    gtk_container_add (GTK_CONTAINER (frame_horz), vbox);
    
    gtk_box_pack_start (GTK_BOX (vbox), 
                        create_bbox (TRUE, "Spread", 40, 85, 20, GTK_BUTTONBOX_SPREAD),
			TRUE, TRUE, 0);
    
    gtk_box_pack_start (GTK_BOX (vbox), 
                        create_bbox (TRUE, "Edge", 40, 85, 20, GTK_BUTTONBOX_EDGE),
			TRUE, TRUE, 5);
    
    gtk_box_pack_start (GTK_BOX (vbox), 
                        create_bbox (TRUE, "Start", 40, 85, 20, GTK_BUTTONBOX_START),
			TRUE, TRUE, 5);
    
    gtk_box_pack_start (GTK_BOX (vbox), 
                        create_bbox (TRUE, "End", 40, 85, 20, GTK_BUTTONBOX_END),
			TRUE, TRUE, 5);
    
    gtk_box_pack_start (GTK_BOX (vbox),
                        create_bbox (TRUE, "Center", 40, 85, 20, GTK_BUTTONBOX_CENTER),
			TRUE, TRUE, 5);
    
    frame_vert = gtk_frame_new ("Vertical Button Boxes");
    gtk_box_pack_start (GTK_BOX (main_vbox), frame_vert, TRUE, TRUE, 10);
    
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
    gtk_container_add (GTK_CONTAINER (frame_vert), hbox);

    gtk_box_pack_start (GTK_BOX (hbox), 
                        create_bbox (FALSE, "Spread", 30, 85, 20, GTK_BUTTONBOX_SPREAD),
			TRUE, TRUE, 0);
    
    gtk_box_pack_start (GTK_BOX (hbox), 
                        create_bbox (FALSE, "Edge", 30, 85, 20, GTK_BUTTONBOX_EDGE),
			TRUE, TRUE, 5);
    
    gtk_box_pack_start (GTK_BOX (hbox), 
                        create_bbox (FALSE, "Start", 30, 85, 20, GTK_BUTTONBOX_START),
			TRUE, TRUE, 5);
    
    gtk_box_pack_start (GTK_BOX (hbox), 
                        create_bbox (FALSE, "End", 30, 85, 20, GTK_BUTTONBOX_END),
			TRUE, TRUE, 5);
    
    gtk_box_pack_start (GTK_BOX (hbox),
                        create_bbox (FALSE, "Center", 30, 85, 20, GTK_BUTTONBOX_CENTER),
			TRUE, TRUE, 5);
  }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkToolBar
 */

static GtkWidget*
new_pixmap (char      *filename,
	    GdkWindow *window,
	    GdkColor  *background)
{
  GtkWidget *wpixmap;
  GdkPixmap *pixmap;
  GdkBitmap *mask;

  if (strcmp (filename, "test.xpm") == 0 ||
      !file_exists (filename))
    {
      pixmap = gdk_pixmap_create_from_xpm_d (window, &mask,
					     background,
					     openfile);
    }
  else
    pixmap = gdk_pixmap_create_from_xpm (window, &mask,
					 background,
					 filename);
  
  wpixmap = gtk_image_new_from_pixmap (pixmap, mask);

  return wpixmap;
}


static void
set_toolbar_small_stock (GtkWidget *widget,
			 gpointer   data)
{
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (data), GTK_ICON_SIZE_SMALL_TOOLBAR);
}

static void
set_toolbar_large_stock (GtkWidget *widget,
			 gpointer   data)
{
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (data), GTK_ICON_SIZE_LARGE_TOOLBAR);
}

static void
set_toolbar_horizontal (GtkWidget *widget,
			gpointer   data)
{
  gtk_toolbar_set_orientation (GTK_TOOLBAR (data), GTK_ORIENTATION_HORIZONTAL);
}

static void
set_toolbar_vertical (GtkWidget *widget,
		      gpointer   data)
{
  gtk_toolbar_set_orientation (GTK_TOOLBAR (data), GTK_ORIENTATION_VERTICAL);
}

static void
set_toolbar_icons (GtkWidget *widget,
		   gpointer   data)
{
  gtk_toolbar_set_style (GTK_TOOLBAR (data), GTK_TOOLBAR_ICONS);
}

static void
set_toolbar_text (GtkWidget *widget,
	          gpointer   data)
{
  gtk_toolbar_set_style (GTK_TOOLBAR (data), GTK_TOOLBAR_TEXT);
}

static void
set_toolbar_both (GtkWidget *widget,
		  gpointer   data)
{
  gtk_toolbar_set_style (GTK_TOOLBAR (data), GTK_TOOLBAR_BOTH);
}

static void
set_toolbar_both_horiz (GtkWidget *widget,
			gpointer   data)
{
  gtk_toolbar_set_style (GTK_TOOLBAR (data), GTK_TOOLBAR_BOTH_HORIZ);
}

static void
set_toolbar_enable (GtkWidget *widget,
		    gpointer   data)
{
  gtk_toolbar_set_tooltips (GTK_TOOLBAR (data), TRUE);
}

static void
set_toolbar_disable (GtkWidget *widget,
		     gpointer   data)
{
  gtk_toolbar_set_tooltips (GTK_TOOLBAR (data), FALSE);
}

static void
create_toolbar (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *toolbar;
  GtkWidget *entry;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));
      
      gtk_window_set_title (GTK_WINDOW (window), "Toolbar test");

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_container_set_border_width (GTK_CONTAINER (window), 0);
      gtk_widget_realize (window);

      toolbar = gtk_toolbar_new ();

      gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
				GTK_STOCK_NEW,
				"Stock icon: New", "Toolbar/New",
				G_CALLBACK (set_toolbar_small_stock), toolbar, -1);
      
      gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
				GTK_STOCK_OPEN,
				"Stock icon: Open", "Toolbar/Open",
				G_CALLBACK (set_toolbar_large_stock), toolbar, -1);
      
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Horizontal", "Horizontal toolbar layout", "Toolbar/Horizontal",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       G_CALLBACK (set_toolbar_horizontal), toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Vertical", "Vertical toolbar layout", "Toolbar/Vertical",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       G_CALLBACK (set_toolbar_vertical), toolbar);

      gtk_toolbar_append_space (GTK_TOOLBAR(toolbar));

      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Icons", "Only show toolbar icons", "Toolbar/IconsOnly",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       G_CALLBACK (set_toolbar_icons), toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Text", "Only show toolbar text", "Toolbar/TextOnly",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       G_CALLBACK (set_toolbar_text), toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Both", "Show toolbar icons and text", "Toolbar/Both",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       G_CALLBACK (set_toolbar_both), toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Both (horizontal)",
			       "Show toolbar icons and text in a horizontal fashion",
			       "Toolbar/BothHoriz",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       G_CALLBACK (set_toolbar_both_horiz), toolbar);
			       
      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

      entry = gtk_entry_new ();

      gtk_toolbar_append_widget (GTK_TOOLBAR (toolbar), entry, "This is an unusable GtkEntry ;)", "Hey don't click me!!!");

      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));


      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Enable", "Enable tooltips", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       G_CALLBACK (set_toolbar_enable), toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Disable", "Disable tooltips", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       G_CALLBACK (set_toolbar_disable), toolbar);

      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Frobate", "Frobate tooltip", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       NULL, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Baz", "Baz tooltip", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       NULL, toolbar);

      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));
      
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Blah", "Blah tooltip", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       NULL, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Bar", "Bar tooltip", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       NULL, toolbar);

      gtk_container_add (GTK_CONTAINER (window), toolbar);

      gtk_widget_set_size_request (toolbar, 200, -1);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

static GtkWidget*
make_toolbar (GtkWidget *window)
{
  GtkWidget *toolbar;

  if (!gtk_widget_get_realized (window))
    gtk_widget_realize (window);

  toolbar = gtk_toolbar_new ();

  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Horizontal", "Horizontal toolbar layout", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   G_CALLBACK (set_toolbar_horizontal), toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Vertical", "Vertical toolbar layout", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   G_CALLBACK (set_toolbar_vertical), toolbar);

  gtk_toolbar_append_space (GTK_TOOLBAR(toolbar));

  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Icons", "Only show toolbar icons", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   G_CALLBACK (set_toolbar_icons), toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Text", "Only show toolbar text", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   G_CALLBACK (set_toolbar_text), toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Both", "Show toolbar icons and text", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   G_CALLBACK (set_toolbar_both), toolbar);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Woot", "Woot woot woot", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   NULL, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Blah", "Blah blah blah", "Toolbar/Big",
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   NULL, toolbar);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Enable", "Enable tooltips", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   G_CALLBACK (set_toolbar_enable), toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Disable", "Disable tooltips", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   G_CALLBACK (set_toolbar_disable), toolbar);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));
  
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Hoo", "Hoo tooltip", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   NULL, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Woo", "Woo tooltip", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   NULL, toolbar);

  return toolbar;
}

/*
 * GtkStatusBar
 */

static guint statusbar_counter = 1;

static void
statusbar_push (GtkWidget *button,
		GtkStatusbar *statusbar)
{
  gchar text[1024];

  sprintf (text, "something %d", statusbar_counter++);

  gtk_statusbar_push (statusbar, 1, text);
}

static void
statusbar_push_long (GtkWidget *button,
                     GtkStatusbar *statusbar)
{
  gchar text[1024];

  sprintf (text, "Just because a system has menu choices written with English words, phrases or sentences, that is no guarantee, that it is comprehensible. Individual words may not be familiar to some users (for example, \"repaginate\"), and two menu items may appear to satisfy the users's needs, whereas only one does (for example, \"put away\" or \"eject\").");

  gtk_statusbar_push (statusbar, 1, text);
}

static void
statusbar_pop (GtkWidget *button,
	       GtkStatusbar *statusbar)
{
  gtk_statusbar_pop (statusbar, 1);
}

static void
statusbar_steal (GtkWidget *button,
	         GtkStatusbar *statusbar)
{
  gtk_statusbar_remove (statusbar, 1, 4);
}

static void
statusbar_popped (GtkStatusbar  *statusbar,
		  guint          context_id,
		  const gchar	*text)
{
  if (!statusbar->messages)
    statusbar_counter = 1;
}

static void
statusbar_contexts (GtkStatusbar *statusbar)
{
  gchar *string;

  string = "any context";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));
  
  string = "idle messages";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));
  
  string = "some text";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));

  string = "hit the mouse";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));

  string = "hit the mouse2";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));
}

static void
create_statusbar (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *statusbar;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "statusbar");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      statusbar = gtk_statusbar_new ();
      gtk_box_pack_end (GTK_BOX (box1), statusbar, TRUE, TRUE, 0);
      g_signal_connect (statusbar,
			"text_popped",
			G_CALLBACK (statusbar_popped),
			NULL);

      button = g_object_new (gtk_button_get_type (),
			       "label", "push something",
			       "visible", TRUE,
			       "parent", box2,
			       NULL);
      g_object_connect (button,
			"signal::clicked", statusbar_push, statusbar,
			NULL);

      button = g_object_connect (g_object_new (gtk_button_get_type (),
						 "label", "pop",
						 "visible", TRUE,
						 "parent", box2,
						 NULL),
				 "signal_after::clicked", statusbar_pop, statusbar,
				 NULL);

      button = g_object_connect (g_object_new (gtk_button_get_type (),
						 "label", "steal #4",
						 "visible", TRUE,
						 "parent", box2,
						 NULL),
				 "signal_after::clicked", statusbar_steal, statusbar,
				 NULL);

      button = g_object_connect (g_object_new (gtk_button_get_type (),
						 "label", "test contexts",
						 "visible", TRUE,
						 "parent", box2,
						 NULL),
				 "swapped_signal_after::clicked", statusbar_contexts, statusbar,
				 NULL);

      button = g_object_connect (g_object_new (gtk_button_get_type (),
						 "label", "push something long",
						 "visible", TRUE,
						 "parent", box2,
						 NULL),
				 "signal_after::clicked", statusbar_push_long, statusbar,
				 NULL);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkTree
 */

static void
cb_tree_destroy_event(GtkWidget* w)
{
  sTreeButtons* tree_buttons;

  /* free buttons structure associate at this tree */
  tree_buttons = g_object_get_data (G_OBJECT (w), "user_data");
  g_free (tree_buttons);
}

static void
cb_add_new_item(GtkWidget* w, GtkTree* tree)
{
  sTreeButtons* tree_buttons;
  GList* selected_list;
  GtkWidget* selected_item;
  GtkWidget* subtree;
  GtkWidget* item_new;
  char buffer[255];

  tree_buttons = g_object_get_data (G_OBJECT (tree), "user_data");

  selected_list = GTK_TREE_SELECTION_OLD(tree);

  if(selected_list == NULL)
    {
      /* there is no item in tree */
      subtree = GTK_WIDGET(tree);
    }
  else
    {
      /* list can have only one element */
      selected_item = GTK_WIDGET(selected_list->data);
      
      subtree = GTK_TREE_ITEM_SUBTREE(selected_item);

      if(subtree == NULL)
	{
	  /* current selected item have not subtree ... create it */
	  subtree = gtk_tree_new();
	  gtk_tree_item_set_subtree(GTK_TREE_ITEM(selected_item), 
				    subtree);
	}
    }

  /* at this point, we know which subtree will be used to add new item */
  /* create a new item */
  sprintf(buffer, "item add %d", tree_buttons->nb_item_add);
  item_new = gtk_tree_item_new_with_label(buffer);
  gtk_tree_append(GTK_TREE(subtree), item_new);
  gtk_widget_show(item_new);

  tree_buttons->nb_item_add++;
}

static void
cb_remove_item(GtkWidget*w, GtkTree* tree)
{
  GList* selected_list;
  GList* clear_list;
  
  selected_list = GTK_TREE_SELECTION_OLD(tree);

  clear_list = NULL;
    
  while (selected_list) 
    {
      clear_list = g_list_prepend (clear_list, selected_list->data);
      selected_list = selected_list->next;
    }
  
  clear_list = g_list_reverse (clear_list);
  gtk_tree_remove_items(tree, clear_list);

  g_list_free (clear_list);
}

static void
cb_remove_subtree(GtkWidget*w, GtkTree* tree)
{
  GList* selected_list;
  GtkTreeItem *item;
  
  selected_list = GTK_TREE_SELECTION_OLD(tree);

  if (selected_list)
    {
      item = GTK_TREE_ITEM (selected_list->data);
      if (item->subtree)
	gtk_tree_item_remove_subtree (item);
    }
}

static void
cb_tree_changed(GtkTree* tree)
{
  sTreeButtons* tree_buttons;
  GList* selected_list;
  guint nb_selected;

  tree_buttons = g_object_get_data (G_OBJECT (tree), "user_data");

  selected_list = GTK_TREE_SELECTION_OLD(tree);
  nb_selected = g_list_length(selected_list);

  if(nb_selected == 0) 
    {
      if(tree->children == NULL)
	gtk_widget_set_sensitive(tree_buttons->add_button, TRUE);
      else
	gtk_widget_set_sensitive(tree_buttons->add_button, FALSE);
      gtk_widget_set_sensitive(tree_buttons->remove_button, FALSE);
      gtk_widget_set_sensitive(tree_buttons->subtree_button, FALSE);
    } 
  else 
    {
      gtk_widget_set_sensitive(tree_buttons->remove_button, TRUE);
      gtk_widget_set_sensitive(tree_buttons->add_button, (nb_selected == 1));
      gtk_widget_set_sensitive(tree_buttons->subtree_button, (nb_selected == 1));
    }  
}

static void 
create_subtree(GtkWidget* item, guint level, guint nb_item_max, guint recursion_level_max)
{
  GtkWidget* item_subtree;
  GtkWidget* item_new;
  guint nb_item;
  char buffer[255];
  int no_root_item;

  if(level == recursion_level_max) return;

  if(level == -1)
    {
      /* query with no root item */
      level = 0;
      item_subtree = item;
      no_root_item = 1;
    }
  else
    {
      /* query with no root item */
      /* create subtree and associate it with current item */
      item_subtree = gtk_tree_new();
      no_root_item = 0;
    }
  
  for(nb_item = 0; nb_item < nb_item_max; nb_item++)
    {
      sprintf(buffer, "item %d-%d", level, nb_item);
      item_new = gtk_tree_item_new_with_label(buffer);
      gtk_tree_append(GTK_TREE(item_subtree), item_new);
      create_subtree(item_new, level+1, nb_item_max, recursion_level_max);
      gtk_widget_show(item_new);
    }

  if(!no_root_item)
    gtk_tree_item_set_subtree(GTK_TREE_ITEM(item), item_subtree);
}

static void
create_tree_sample(GdkScreen *screen, guint selection_mode, 
		   guint draw_line, guint view_line, guint no_root_item,
		   guint nb_item_max, guint recursion_level_max) 
{
  GtkWidget* window;
  GtkWidget* box1;
  GtkWidget* box2;
  GtkWidget* separator;
  GtkWidget* button;
  GtkWidget* scrolled_win;
  GtkWidget* root_tree;
  GtkWidget* root_item;
  sTreeButtons* tree_buttons;

  /* create tree buttons struct */
  if ((tree_buttons = g_malloc (sizeof (sTreeButtons))) == NULL)
    {
      g_error("can't allocate memory for tree structure !\n");
      return;
    }
  tree_buttons->nb_item_add = 0;

  /* create top level window */
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_screen (GTK_WINDOW (window), screen);
  gtk_window_set_title(GTK_WINDOW(window), "Tree Sample");
  g_signal_connect (window, "destroy",
		    G_CALLBACK (cb_tree_destroy_event), NULL);
  g_object_set_data (G_OBJECT (window), "user_data", tree_buttons);

  box1 = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(window), box1);
  gtk_widget_show(box1);

  /* create tree box */
  box2 = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box1), box2, TRUE, TRUE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(box2), 5);
  gtk_widget_show(box2);

  /* create scrolled window */
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (box2), scrolled_win, TRUE, TRUE, 0);
  gtk_widget_set_size_request (scrolled_win, 200, 200);
  gtk_widget_show (scrolled_win);
  
  /* create root tree widget */
  root_tree = gtk_tree_new();
  g_signal_connect (root_tree, "selection_changed",
		    G_CALLBACK (cb_tree_changed),
		    NULL);
  g_object_set_data (G_OBJECT (root_tree), "user_data", tree_buttons);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_win), root_tree);
  gtk_tree_set_selection_mode(GTK_TREE(root_tree), selection_mode);
  gtk_tree_set_view_lines(GTK_TREE(root_tree), draw_line);
  gtk_tree_set_view_mode(GTK_TREE(root_tree), !view_line);
  gtk_widget_show(root_tree);

  if ( no_root_item )
    {
      /* set root tree to subtree function with root item variable */
      root_item = GTK_WIDGET(root_tree);
    }
  else
    {
      /* create root tree item widget */
      root_item = gtk_tree_item_new_with_label("root item");
      gtk_tree_append(GTK_TREE(root_tree), root_item);
      gtk_widget_show(root_item);
     }
  create_subtree(root_item, -no_root_item, nb_item_max, recursion_level_max);

  box2 = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(box2), 5);
  gtk_widget_show(box2);

  button = gtk_button_new_with_label("Add Item");
  gtk_widget_set_sensitive(button, FALSE);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (cb_add_new_item),
		    root_tree);
  gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
  tree_buttons->add_button = button;

  button = gtk_button_new_with_label("Remove Item(s)");
  gtk_widget_set_sensitive(button, FALSE);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (cb_remove_item),
		    root_tree);
  gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
  tree_buttons->remove_button = button;

  button = gtk_button_new_with_label("Remove Subtree");
  gtk_widget_set_sensitive(button, FALSE);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (cb_remove_subtree),
		    root_tree);
  gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
  tree_buttons->subtree_button = button;

  /* create separator */
  separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(box1), separator, FALSE, FALSE, 0);
  gtk_widget_show(separator);

  /* create button box */
  box2 = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(box2), 5);
  gtk_widget_show(box2);

  button = gtk_button_new_with_label("Close");
  gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (gtk_widget_destroy),
			    window);
  gtk_widget_show(button);

  gtk_widget_show(window);
}

static void
cb_create_tree(GtkWidget* w)
{
  guint selection_mode = GTK_SELECTION_SINGLE;
  guint view_line;
  guint draw_line;
  guint no_root_item;
  guint nb_item;
  guint recursion_level;

  /* get selection mode choice */
  if(GTK_TOGGLE_BUTTON(sTreeSampleSelection.single_button)->active)
    selection_mode = GTK_SELECTION_SINGLE;
  else
    if(GTK_TOGGLE_BUTTON(sTreeSampleSelection.browse_button)->active)
      selection_mode = GTK_SELECTION_BROWSE;
    else
      selection_mode = GTK_SELECTION_MULTIPLE;

  /* get options choice */
  draw_line = GTK_TOGGLE_BUTTON(sTreeSampleSelection.draw_line_button)->active;
  view_line = GTK_TOGGLE_BUTTON(sTreeSampleSelection.view_line_button)->active;
  no_root_item = GTK_TOGGLE_BUTTON(sTreeSampleSelection.no_root_item_button)->active;
    
  /* get levels */
  nb_item = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(sTreeSampleSelection.nb_item_spinner));
  recursion_level = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(sTreeSampleSelection.recursion_spinner));

  if (pow (nb_item, recursion_level) > 10000)
    {
      g_print ("%g total items? That will take a very long time. Try less\n",
	       pow (nb_item, recursion_level));
      return;
    }

  create_tree_sample(gtk_widget_get_screen (w),
		     selection_mode, draw_line, 
		     view_line, no_root_item, nb_item, recursion_level);
}

void 
create_tree_mode_window(GtkWidget *widget)
{
  static GtkWidget* window;
  GtkWidget* box1;
  GtkWidget* box2;
  GtkWidget* box3;
  GtkWidget* box4;
  GtkWidget* box5;
  GtkWidget* button;
  GtkWidget* frame;
  GtkWidget* separator;
  GtkWidget* label;
  GtkWidget* spinner;
  GtkAdjustment *adj;

  if (!window)
    {
      /* create toplevel window  */
      window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));
      gtk_window_set_title(GTK_WINDOW(window), "Set Tree Parameters");
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);
      box1 = gtk_vbox_new(FALSE, 0);
      gtk_container_add(GTK_CONTAINER(window), box1);

      /* create upper box - selection box */
      box2 = gtk_vbox_new(FALSE, 5);
      gtk_box_pack_start(GTK_BOX(box1), box2, TRUE, TRUE, 0);
      gtk_container_set_border_width(GTK_CONTAINER(box2), 5);

      box3 = gtk_hbox_new(FALSE, 5);
      gtk_box_pack_start(GTK_BOX(box2), box3, TRUE, TRUE, 0);

      /* create selection mode frame */
      frame = gtk_frame_new("Selection Mode");
      gtk_box_pack_start(GTK_BOX(box3), frame, TRUE, TRUE, 0);

      box4 = gtk_vbox_new(FALSE, 0);
      gtk_container_add(GTK_CONTAINER(frame), box4);
      gtk_container_set_border_width(GTK_CONTAINER(box4), 5);

      /* create radio button */  
      button = gtk_radio_button_new_with_label(NULL, "SINGLE");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      sTreeSampleSelection.single_button = button;

      button = gtk_radio_button_new_with_label(gtk_radio_button_get_group (GTK_RADIO_BUTTON (button)),
					       "BROWSE");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      sTreeSampleSelection.browse_button = button;

      button = gtk_radio_button_new_with_label(gtk_radio_button_get_group (GTK_RADIO_BUTTON (button)),
					       "MULTIPLE");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      sTreeSampleSelection.multiple_button = button;

      sTreeSampleSelection.selection_mode_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));

      /* create option mode frame */
      frame = gtk_frame_new("Options");
      gtk_box_pack_start(GTK_BOX(box3), frame, TRUE, TRUE, 0);

      box4 = gtk_vbox_new(FALSE, 0);
      gtk_container_add(GTK_CONTAINER(frame), box4);
      gtk_container_set_border_width(GTK_CONTAINER(box4), 5);

      /* create check button */
      button = gtk_check_button_new_with_label("Draw line");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
      sTreeSampleSelection.draw_line_button = button;
  
      button = gtk_check_button_new_with_label("View Line mode");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
      sTreeSampleSelection.view_line_button = button;
  
      button = gtk_check_button_new_with_label("Without Root item");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      sTreeSampleSelection.no_root_item_button = button;

      /* create recursion parameter */
      frame = gtk_frame_new("Size Parameters");
      gtk_box_pack_start(GTK_BOX(box2), frame, TRUE, TRUE, 0);

      box4 = gtk_hbox_new(FALSE, 5);
      gtk_container_add(GTK_CONTAINER(frame), box4);
      gtk_container_set_border_width(GTK_CONTAINER(box4), 5);

      /* create number of item spin button */
      box5 = gtk_hbox_new(FALSE, 5);
      gtk_box_pack_start(GTK_BOX(box4), box5, FALSE, FALSE, 0);

      label = gtk_label_new("Number of items : ");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (box5), label, FALSE, TRUE, 0);

      adj = (GtkAdjustment *) gtk_adjustment_new (DEFAULT_NUMBER_OF_ITEM, 1.0, 255.0, 1.0,
						  5.0, 0.0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (box5), spinner, FALSE, TRUE, 0);
      sTreeSampleSelection.nb_item_spinner = spinner;
  
      /* create recursion level spin button */
      box5 = gtk_hbox_new(FALSE, 5);
      gtk_box_pack_start(GTK_BOX(box4), box5, FALSE, FALSE, 0);

      label = gtk_label_new("Depth : ");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (box5), label, FALSE, TRUE, 0);

      adj = (GtkAdjustment *) gtk_adjustment_new (DEFAULT_RECURSION_LEVEL, 0.0, 255.0, 1.0,
						  5.0, 0.0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (box5), spinner, FALSE, TRUE, 0);
      sTreeSampleSelection.recursion_spinner = spinner;
  
      /* create horizontal separator */
      separator = gtk_hseparator_new();
      gtk_box_pack_start(GTK_BOX(box1), separator, FALSE, FALSE, 0);

      /* create bottom button box */
      box2 = gtk_hbox_new(TRUE, 10);
      gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, FALSE, 0);
      gtk_container_set_border_width(GTK_CONTAINER(box2), 5);

      button = gtk_button_new_with_label("Create Tree");
      gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (cb_create_tree), NULL);

      button = gtk_button_new_with_label("Close");
      gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
    }
  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Gridded geometry
 */
#define GRID_SIZE 20
#define DEFAULT_GEOMETRY "10x10"

static gboolean
gridded_geometry_expose (GtkWidget      *widget,
			 GdkEventExpose *event)
{
  int i, j;
  cairo_t *cr;

  cr = gdk_cairo_create (widget->window);

  cairo_rectangle (cr, 0, 0, widget->allocation.width, widget->allocation.height);
  gdk_cairo_set_source_color (cr, &widget->style->base[widget->state]);
  cairo_fill (cr);
  
  for (i = 0 ; i * GRID_SIZE < widget->allocation.width; i++)
    for (j = 0 ; j * GRID_SIZE < widget->allocation.height; j++)
      {
	if ((i + j) % 2 == 0)
	  cairo_rectangle (cr, i * GRID_SIZE, j * GRID_SIZE, GRID_SIZE, GRID_SIZE);
      }

  gdk_cairo_set_source_color (cr, &widget->style->text[widget->state]);
  cairo_fill (cr);

  cairo_destroy (cr);

  return FALSE;
}

static void
gridded_geometry_subresponse (GtkDialog *dialog,
			      gint       response_id,
			      gchar     *geometry_string)
{
  if (response_id == GTK_RESPONSE_NONE)
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
    }
  else
    {
      if (!gtk_window_parse_geometry (GTK_WINDOW (dialog), geometry_string))
	{
	  g_print ("Can't parse geometry string %s\n", geometry_string);
	  gtk_window_parse_geometry (GTK_WINDOW (dialog), DEFAULT_GEOMETRY);
	}
    }
}

static void
gridded_geometry_response (GtkDialog *dialog,
			   gint       response_id,
			   GtkEntry  *entry)
{
  if (response_id == GTK_RESPONSE_NONE)
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
    }
  else
    {
      gchar *geometry_string = g_strdup (gtk_entry_get_text (entry));
      gchar *title = g_strdup_printf ("Gridded window at: %s", geometry_string);
      GtkWidget *window;
      GtkWidget *drawing_area;
      GtkWidget *box;
      GdkGeometry geometry;
      
      window = gtk_dialog_new_with_buttons (title,
                                            NULL, 0,
                                            "Reset", 1,
                                            GTK_STOCK_CLOSE, GTK_RESPONSE_NONE,
                                            NULL);

      gtk_window_set_screen (GTK_WINDOW (window), 
			     gtk_widget_get_screen (GTK_WIDGET (dialog)));
      g_free (title);
      g_signal_connect (window, "response",
			G_CALLBACK (gridded_geometry_subresponse), geometry_string);

      box = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), box, TRUE, TRUE, 0);
      
      gtk_container_set_border_width (GTK_CONTAINER (box), 7);
      
      drawing_area = gtk_drawing_area_new ();
      g_signal_connect (drawing_area, "expose_event",
			G_CALLBACK (gridded_geometry_expose), NULL);
      gtk_box_pack_start (GTK_BOX (box), drawing_area, TRUE, TRUE, 0);

      /* Gross hack to work around bug 68668... if we set the size request
       * large enough, then  the current
       *
       *   request_of_window - request_of_geometry_widget
       *
       * method of getting the base size works more or less works.
       */
      gtk_widget_set_size_request (drawing_area, 2000, 2000);

      geometry.base_width = 0;
      geometry.base_height = 0;
      geometry.min_width = 2 * GRID_SIZE;
      geometry.min_height = 2 * GRID_SIZE;
      geometry.width_inc = GRID_SIZE;
      geometry.height_inc = GRID_SIZE;

      gtk_window_set_geometry_hints (GTK_WINDOW (window), drawing_area,
				     &geometry,
				     GDK_HINT_BASE_SIZE | GDK_HINT_MIN_SIZE | GDK_HINT_RESIZE_INC);

      if (!gtk_window_parse_geometry (GTK_WINDOW (window), geometry_string))
	{
	  g_print ("Can't parse geometry string %s\n", geometry_string);
	  gtk_window_parse_geometry (GTK_WINDOW (window), DEFAULT_GEOMETRY);
	}

      gtk_widget_show_all (window);
    }
}

static void 
create_gridded_geometry (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  gpointer window_ptr;
  GtkWidget *entry;
  GtkWidget *label;

  if (!window)
    {
      window = gtk_dialog_new_with_buttons ("Gridded Geometry",
                                            NULL, 0,
					    "Create", 1,
                                            GTK_STOCK_CLOSE, GTK_RESPONSE_NONE,
                                            NULL);
      
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      label = gtk_label_new ("Geometry string:");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), label, FALSE, FALSE, 0);

      entry = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (entry), DEFAULT_GEOMETRY);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), entry, FALSE, FALSE, 0);

      g_signal_connect (window, "response",
			G_CALLBACK (gridded_geometry_response), entry);
      window_ptr = &window;
      g_object_add_weak_pointer (G_OBJECT (window), window_ptr);

      gtk_widget_show_all (window);
    }
  else
    gtk_widget_destroy (window);
}

/*
 * GtkHandleBox
 */

static void
handle_box_child_signal (GtkHandleBox *hb,
			 GtkWidget    *child,
			 const gchar  *action)
{
  printf ("%s: child <%s> %sed\n",
	  g_type_name (G_OBJECT_TYPE (hb)),
	  g_type_name (G_OBJECT_TYPE (child)),
	  action);
}

static void
create_handle_box (GtkWidget *widget)
{
  static GtkWidget* window = NULL;
  GtkWidget *handle_box;
  GtkWidget *handle_box2;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *toolbar;
  GtkWidget *label;
  GtkWidget *separator;

  if (!window)
  {
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    
    gtk_window_set_screen (GTK_WINDOW (window),
			   gtk_widget_get_screen (widget));
    gtk_window_set_modal (GTK_WINDOW (window), FALSE);
    gtk_window_set_title (GTK_WINDOW (window),
			  "Handle Box Test");
    gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
    
    g_signal_connect (window, "destroy",
		      G_CALLBACK (gtk_widget_destroyed),
		      &window);
    
    gtk_container_set_border_width (GTK_CONTAINER (window), 20);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (window), vbox);
    gtk_widget_show (vbox);

    label = gtk_label_new ("Above");
    gtk_container_add (GTK_CONTAINER (vbox), label);
    gtk_widget_show (label);

    separator = gtk_hseparator_new ();
    gtk_container_add (GTK_CONTAINER (vbox), separator);
    gtk_widget_show (separator);
    
    hbox = gtk_hbox_new (FALSE, 10);
    gtk_container_add (GTK_CONTAINER (vbox), hbox);
    gtk_widget_show (hbox);

    separator = gtk_hseparator_new ();
    gtk_container_add (GTK_CONTAINER (vbox), separator);
    gtk_widget_show (separator);

    label = gtk_label_new ("Below");
    gtk_container_add (GTK_CONTAINER (vbox), label);
    gtk_widget_show (label);

    handle_box = gtk_handle_box_new ();
    gtk_box_pack_start (GTK_BOX (hbox), handle_box, FALSE, FALSE, 0);
    g_signal_connect (handle_box,
		      "child_attached",
		      G_CALLBACK (handle_box_child_signal),
		      "attached");
    g_signal_connect (handle_box,
		      "child_detached",
		      G_CALLBACK (handle_box_child_signal),
		      "detached");
    gtk_widget_show (handle_box);

    toolbar = make_toolbar (window);
    
    gtk_container_add (GTK_CONTAINER (handle_box), toolbar);
    gtk_widget_show (toolbar);

    handle_box = gtk_handle_box_new ();
    gtk_box_pack_start (GTK_BOX (hbox), handle_box, FALSE, FALSE, 0);
    g_signal_connect (handle_box,
		      "child_attached",
		      G_CALLBACK (handle_box_child_signal),
		      "attached");
    g_signal_connect (handle_box,
		      "child_detached",
		      G_CALLBACK (handle_box_child_signal),
		      "detached");
    gtk_widget_show (handle_box);

    handle_box2 = gtk_handle_box_new ();
    gtk_container_add (GTK_CONTAINER (handle_box), handle_box2);
    g_signal_connect (handle_box2,
		      "child_attached",
		      G_CALLBACK (handle_box_child_signal),
		      "attached");
    g_signal_connect (handle_box2,
		      "child_detached",
		      G_CALLBACK (handle_box_child_signal),
		      "detached");
    gtk_widget_show (handle_box2);

    hbox = g_object_new (GTK_TYPE_HBOX, "visible", 1, "parent", handle_box2, NULL);
    label = gtk_label_new ("Fooo!");
    gtk_container_add (GTK_CONTAINER (hbox), label);
    gtk_widget_show (label);
    g_object_new (GTK_TYPE_ARROW, "visible", 1, "parent", hbox, NULL);
  }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/* 
 * Label Demo
 */
static void
sensitivity_toggled (GtkWidget *toggle,
                     GtkWidget *widget)
{
  gtk_widget_set_sensitive (widget,  GTK_TOGGLE_BUTTON (toggle)->active);  
}

static GtkWidget*
create_sensitivity_control (GtkWidget *widget)
{
  GtkWidget *button;

  button = gtk_toggle_button_new_with_label ("Sensitive");  

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                gtk_widget_is_sensitive (widget));
  
  g_signal_connect (button,
                    "toggled",
                    G_CALLBACK (sensitivity_toggled),
                    widget);
  
  gtk_widget_show_all (button);

  return button;
}

static void
set_selectable_recursive (GtkWidget *widget,
                          gboolean   setting)
{
  if (GTK_IS_CONTAINER (widget))
    {
      GList *children;
      GList *tmp;
      
      children = gtk_container_get_children (GTK_CONTAINER (widget));
      tmp = children;
      while (tmp)
        {
          set_selectable_recursive (tmp->data, setting);

          tmp = tmp->next;
        }
      g_list_free (children);
    }
  else if (GTK_IS_LABEL (widget))
    {
      gtk_label_set_selectable (GTK_LABEL (widget), setting);
    }
}

static void
selectable_toggled (GtkWidget *toggle,
                    GtkWidget *widget)
{
  set_selectable_recursive (widget,
                            GTK_TOGGLE_BUTTON (toggle)->active);
}

static GtkWidget*
create_selectable_control (GtkWidget *widget)
{
  GtkWidget *button;

  button = gtk_toggle_button_new_with_label ("Selectable");  

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                FALSE);
  
  g_signal_connect (button,
                    "toggled",
                    G_CALLBACK (selectable_toggled),
                    widget);
  
  gtk_widget_show_all (button);

  return button;
}

static void
dialog_response (GtkWidget *dialog, gint response_id, GtkLabel *label)
{
  const gchar *text;

  gtk_widget_destroy (dialog);

  text = "Some <a href=\"http://en.wikipedia.org/wiki/Text\" title=\"plain text\">text</a> may be marked up\n"
         "as hyperlinks, which can be clicked\n"
         "or activated via <a href=\"keynav\">keynav</a>.\n"
         "The links remain the same.";
  gtk_label_set_markup (label, text);
}

static gboolean
activate_link (GtkWidget *label, const gchar *uri, gpointer data)
{
  if (g_strcmp0 (uri, "keynav") == 0)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (gtk_widget_get_toplevel (label)),
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_INFO,
                                       GTK_BUTTONS_OK,
                                       "The term <i>keynav</i> is a shorthand for "
                                       "keyboard navigation and refers to the process of using a program "
                                       "(exclusively) via keyboard input.");

      gtk_window_present (GTK_WINDOW (dialog));

      g_signal_connect (dialog, "response", G_CALLBACK (dialog_response), label);

      return TRUE;
    }

  return FALSE;
}

void create_labels (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *frame;
  GtkWidget *label;
  GtkWidget *button;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "Label");

      vbox = gtk_vbox_new (FALSE, 5);
      
      hbox = gtk_hbox_new (FALSE, 5);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      button = create_sensitivity_control (hbox);

      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

      button = create_selectable_control (hbox);

      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      
      vbox = gtk_vbox_new (FALSE, 5);
      
      gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (window), 5);

      frame = gtk_frame_new ("Normal Label");
      label = gtk_label_new ("This is a Normal label");
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_START);
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Multi-line Label");
      label = gtk_label_new ("This is a Multi-line label.\nSecond line\nThird line");
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Left Justified Label");
      label = gtk_label_new ("This is a Left-Justified\nMulti-line label.\nThird      line");
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Right Justified Label");
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_START);
      label = gtk_label_new ("This is a Right-Justified\nMulti-line label.\nFourth line, (j/k)");
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Internationalized Label");
      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label),
			    "French (Fran\303\247ais) Bonjour, Salut\n"
			    "Korean (\355\225\234\352\270\200)   \354\225\210\353\205\225\355\225\230\354\204\270\354\232\224, \354\225\210\353\205\225\355\225\230\354\213\255\353\213\210\352\271\214\n"
			    "Russian (\320\240\321\203\321\201\321\201\320\272\320\270\320\271) \320\227\320\264\321\200\320\260\320\262\321\201\321\202\320\262\321\203\320\271\321\202\320\265!\n"
			    "Chinese (Simplified) <span lang=\"zh-cn\">\345\205\203\346\260\224	\345\274\200\345\217\221</span>\n"
			    "Chinese (Traditional) <span lang=\"zh-tw\">\345\205\203\346\260\243	\351\226\213\347\231\274</span>\n"
			    "Japanese <span lang=\"ja\">\345\205\203\346\260\227	\351\226\213\347\231\272</span>");
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Bidirection Label");
      label = gtk_label_new ("\342\200\217Arabic	\330\247\331\204\330\263\331\204\330\247\331\205 \330\271\331\204\331\212\331\203\331\205\n"
			     "\342\200\217Hebrew	\327\251\327\234\327\225\327\235");
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Links in a label");
      label = gtk_label_new ("Some <a href=\"http://en.wikipedia.org/wiki/Text\" title=\"plain text\">text</a> may be marked up\n"
                             "as hyperlinks, which can be clicked\n"
                             "or activated via <a href=\"keynav\">keynav</a>");
      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
      g_signal_connect (label, "activate-link", G_CALLBACK (activate_link), NULL);

      vbox = gtk_vbox_new (FALSE, 5);
      gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
      frame = gtk_frame_new ("Line wrapped label");
      label = gtk_label_new ("This is an example of a line-wrapped label.  It should not be taking "\
			     "up the entire             "/* big space to test spacing */\
			     "width allocated to it, but automatically wraps the words to fit.  "\
			     "The time has come, for all good men, to come to the aid of their party.  "\
			     "The sixth sheik's six sheep's sick.\n"\
			     "     It supports multiple paragraphs correctly, and  correctly   adds "\
			     "many          extra  spaces. ");

      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Filled, wrapped label");
      label = gtk_label_new ("This is an example of a line-wrapped, filled label.  It should be taking "\
			     "up the entire              width allocated to it.  Here is a seneance to prove "\
			     "my point.  Here is another sentence. "\
			     "Here comes the sun, do de do de do.\n"\
			     "    This is a new paragraph.\n"\
			     "    This is another newer, longer, better paragraph.  It is coming to an end, "\
			     "unfortunately.");
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_FILL);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Underlined label");
      label = gtk_label_new ("This label is underlined!\n"
			     "This one is underlined (\343\201\223\343\202\223\343\201\253\343\201\241\343\201\257) in quite a funky fashion");
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
      gtk_label_set_pattern (GTK_LABEL (label), "_________________________ _ _________ _ _____ _ __ __  ___ ____ _____");
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Markup label");
      label = gtk_label_new (NULL);

      /* There's also a gtk_label_set_markup() without accel if you
       * don't have an accelerator key
       */
      gtk_label_set_markup_with_mnemonic (GTK_LABEL (label),
					  "This <span foreground=\"blue\" background=\"orange\">label</span> has "
					  "<b>markup</b> _such as "
					  "<big><i>Big Italics</i></big>\n"
					  "<tt>Monospace font</tt>\n"
					  "<u>Underline!</u>\n"
					  "foo\n"
					  "<span foreground=\"green\" background=\"red\">Ugly colors</span>\n"
					  "and nothing on this line,\n"
					  "or this.\n"
					  "or this either\n"
					  "or even on this one\n"
					  "la <big>la <big>la <big>la <big>la</big></big></big></big>\n"
					  "but this _word is <span foreground=\"purple\"><big>purple</big></span>\n"
					  "<span underline=\"double\">We like <sup>superscript</sup> and <sub>subscript</sub> too</span>");

      g_assert (gtk_label_get_mnemonic_keyval (GTK_LABEL (label)) == GDK_s);
      
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
    }
  
  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

static void
on_angle_scale_changed (GtkRange *range,
			GtkLabel *label)
{
  gtk_label_set_angle (GTK_LABEL (label), gtk_range_get_value (range));
}

static void
create_rotated_label (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *vbox;
  GtkWidget *hscale;
  GtkWidget *label;  
  GtkWidget *scale_label;  
  GtkWidget *scale_hbox;  

  if (!window)
    {
      window = gtk_dialog_new_with_buttons ("Rotated Label",
					    GTK_WINDOW (gtk_widget_get_toplevel (widget)), 0,
					    GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					    NULL);

      gtk_window_set_resizable (GTK_WINDOW (window), TRUE);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "response",
			G_CALLBACK (gtk_object_destroy), NULL);
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed), &window);

      vbox = gtk_vbox_new (FALSE, 5);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), vbox, TRUE, TRUE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label), "Hello World\n<i>Rotate</i> <span underline='single' foreground='blue'>me</span>");
      gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

      scale_hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), scale_hbox, FALSE, FALSE, 0);
      
      scale_label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (scale_label), "<i>Angle: </i>");
      gtk_box_pack_start (GTK_BOX (scale_hbox), scale_label, FALSE, FALSE, 0);

      hscale = gtk_hscale_new_with_range (0, 360, 5);
      g_signal_connect (hscale, "value-changed",
			G_CALLBACK (on_angle_scale_changed), label);
      
      gtk_range_set_value (GTK_RANGE (hscale), 45);
      gtk_widget_set_usize (hscale, 200, -1);
      gtk_box_pack_start (GTK_BOX (scale_hbox), hscale, TRUE, TRUE, 0);
    }
  
  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

#define DEFAULT_TEXT_RADIUS 200

static void
on_rotated_text_unrealize (GtkWidget *widget)
{
  g_object_set_data (G_OBJECT (widget), "text-gc", NULL);
}

static gboolean
on_rotated_text_expose (GtkWidget      *widget,
			GdkEventExpose *event,
			GdkPixbuf      *tile_pixbuf)
{
  static const gchar *words[] = { "The", "grand", "old", "Duke", "of", "York",
                                  "had", "10,000", "men" };
  int n_words;
  int i;
  double radius;
  PangoLayout *layout;
  PangoContext *context;
  PangoFontDescription *desc;
  cairo_t *cr;

  cr = gdk_cairo_create (event->window);

  if (tile_pixbuf)
    {
      gdk_cairo_set_source_pixbuf (cr, tile_pixbuf, 0, 0);
      cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);
    }
  else
    cairo_set_source_rgb (cr, 0, 0, 0);

  radius = MIN (widget->allocation.width, widget->allocation.height) / 2.;

  cairo_translate (cr,
                   radius + (widget->allocation.width - 2 * radius) / 2,
                   radius + (widget->allocation.height - 2 * radius) / 2);
  cairo_scale (cr, radius / DEFAULT_TEXT_RADIUS, radius / DEFAULT_TEXT_RADIUS);

  context = gtk_widget_get_pango_context (widget);
  layout = pango_layout_new (context);
  desc = pango_font_description_from_string ("Sans Bold 30");
  pango_layout_set_font_description (layout, desc);
  pango_font_description_free (desc);
    
  n_words = G_N_ELEMENTS (words);
  for (i = 0; i < n_words; i++)
    {
      int width, height;

      cairo_save (cr);

      cairo_rotate (cr, 2 * G_PI * i / n_words);
      pango_cairo_update_layout (cr, layout);

      pango_layout_set_text (layout, words[i], -1);
      pango_layout_get_size (layout, &width, &height);

      cairo_move_to (cr, - width / 2 / PANGO_SCALE, - DEFAULT_TEXT_RADIUS);
      pango_cairo_show_layout (cr, layout);

      cairo_restore (cr);
    }
  
  g_object_unref (layout);
  cairo_destroy (cr);

  return FALSE;
}

static void
create_rotated_text (GtkWidget *widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      const GdkColor white = { 0, 0xffff, 0xffff, 0xffff };
      GtkRequisition requisition;
      GtkWidget *drawing_area;
      GdkPixbuf *tile_pixbuf;

      window = gtk_dialog_new_with_buttons ("Rotated Text",
					    GTK_WINDOW (gtk_widget_get_toplevel (widget)), 0,
					    GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					    NULL);

      gtk_window_set_resizable (GTK_WINDOW (window), TRUE);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "response",
			G_CALLBACK (gtk_object_destroy), NULL);
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed), &window);

      drawing_area = gtk_drawing_area_new ();
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), drawing_area, TRUE, TRUE, 0);
      gtk_widget_modify_bg (drawing_area, GTK_STATE_NORMAL, &white);

      tile_pixbuf = gdk_pixbuf_new_from_file ("marble.xpm", NULL);
      
      g_signal_connect (drawing_area, "expose-event",
			G_CALLBACK (on_rotated_text_expose), tile_pixbuf);
      g_signal_connect (drawing_area, "unrealize",
			G_CALLBACK (on_rotated_text_unrealize), NULL);

      gtk_widget_show_all (GTK_BIN (window)->child);
      
      gtk_widget_set_size_request (drawing_area, DEFAULT_TEXT_RADIUS * 2, DEFAULT_TEXT_RADIUS * 2);
      gtk_widget_size_request (window, &requisition);
      gtk_widget_set_size_request (drawing_area, -1, -1);
      gtk_window_resize (GTK_WINDOW (window), requisition.width, requisition.height);
    }
  
  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Reparent demo
 */

static void
reparent_label (GtkWidget *widget,
		GtkWidget *new_parent)
{
  GtkWidget *label;

  label = g_object_get_data (G_OBJECT (widget), "user_data");

  gtk_widget_reparent (label, new_parent);
}

static void
set_parent_signal (GtkWidget *child,
		   GtkWidget *old_parent,
		   gpointer   func_data)
{
  g_message ("set_parent for \"%s\": new parent: \"%s\", old parent: \"%s\", data: %d\n",
             g_type_name (G_OBJECT_TYPE (child)),
             child->parent ? g_type_name (G_OBJECT_TYPE (child->parent)) : "NULL",
             old_parent ? g_type_name (G_OBJECT_TYPE (old_parent)) : "NULL",
             GPOINTER_TO_INT (func_data));
}

static void
create_reparent (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *box3;
  GtkWidget *frame;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *separator;
  GtkWidget *event_box;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "reparent");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      label = gtk_label_new ("Hello World");

      frame = gtk_frame_new ("Frame 1");
      gtk_box_pack_start (GTK_BOX (box2), frame, TRUE, TRUE, 0);

      box3 = gtk_vbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (box3), 5);
      gtk_container_add (GTK_CONTAINER (frame), box3);

      button = gtk_button_new_with_label ("switch");
      g_object_set_data (G_OBJECT (button), "user_data", label);
      gtk_box_pack_start (GTK_BOX (box3), button, FALSE, TRUE, 0);

      event_box = gtk_event_box_new ();
      gtk_box_pack_start (GTK_BOX (box3), event_box, FALSE, TRUE, 0);
      gtk_container_add (GTK_CONTAINER (event_box), label);
			 
      g_signal_connect (button, "clicked",
			G_CALLBACK (reparent_label),
			event_box);
      
      g_signal_connect (label, "parent_set",
			G_CALLBACK (set_parent_signal),
			GINT_TO_POINTER (42));

      frame = gtk_frame_new ("Frame 2");
      gtk_box_pack_start (GTK_BOX (box2), frame, TRUE, TRUE, 0);

      box3 = gtk_vbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (box3), 5);
      gtk_container_add (GTK_CONTAINER (frame), box3);

      button = gtk_button_new_with_label ("switch");
      g_object_set_data (G_OBJECT (button), "user_data", label);
      gtk_box_pack_start (GTK_BOX (box3), button, FALSE, TRUE, 0);
      
      event_box = gtk_event_box_new ();
      gtk_box_pack_start (GTK_BOX (box3), event_box, FALSE, TRUE, 0);

      g_signal_connect (button, "clicked",
			G_CALLBACK (reparent_label),
			event_box);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy), window);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Resize Grips
 */
static gboolean
grippy_button_press (GtkWidget *area, GdkEventButton *event, GdkWindowEdge edge)
{
  if (event->type == GDK_BUTTON_PRESS) 
    {
      if (event->button == 1)
	gtk_window_begin_resize_drag (GTK_WINDOW (gtk_widget_get_toplevel (area)), edge,
				      event->button, event->x_root, event->y_root,
				      event->time);
      else if (event->button == 2)
	gtk_window_begin_move_drag (GTK_WINDOW (gtk_widget_get_toplevel (area)), 
				    event->button, event->x_root, event->y_root,
				    event->time);
    }
  return TRUE;
}

static gboolean
grippy_expose (GtkWidget *area, GdkEventExpose *event, GdkWindowEdge edge)
{
  gtk_paint_resize_grip (area->style,
			 area->window,
			 gtk_widget_get_state (area),
			 &event->area,
			 area,
			 "statusbar",
			 edge,
			 0, 0,
			 area->allocation.width,
			 area->allocation.height);

  return TRUE;
}

static void
create_resize_grips (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *area;
  GtkWidget *hbox, *vbox;
  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      gtk_window_set_title (GTK_WINDOW (window), "resize grips");
      
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);
      
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

      /* North west */
      area = gtk_drawing_area_new ();
      gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK);
      gtk_box_pack_start (GTK_BOX (hbox), area, TRUE, TRUE, 0);
      g_signal_connect (area, "expose_event", G_CALLBACK (grippy_expose),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_NORTH_WEST));
      g_signal_connect (area, "button_press_event", G_CALLBACK (grippy_button_press),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_NORTH_WEST));
      
      /* North */
      area = gtk_drawing_area_new ();
      gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK);
      gtk_box_pack_start (GTK_BOX (hbox), area, TRUE, TRUE, 0);
      g_signal_connect (area, "expose_event", G_CALLBACK (grippy_expose),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_NORTH));
      g_signal_connect (area, "button_press_event", G_CALLBACK (grippy_button_press),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_NORTH));

      /* North east */
      area = gtk_drawing_area_new ();
      gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK);
      gtk_box_pack_start (GTK_BOX (hbox), area, TRUE, TRUE, 0);
      g_signal_connect (area, "expose_event", G_CALLBACK (grippy_expose),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_NORTH_EAST));
      g_signal_connect (area, "button_press_event", G_CALLBACK (grippy_button_press),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_NORTH_EAST));

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

      /* West */
      area = gtk_drawing_area_new ();
      gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK);
      gtk_box_pack_start (GTK_BOX (hbox), area, TRUE, TRUE, 0);
      g_signal_connect (area, "expose_event", G_CALLBACK (grippy_expose),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_WEST));
      g_signal_connect (area, "button_press_event", G_CALLBACK (grippy_button_press),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_WEST));

      /* Middle */
      area = gtk_drawing_area_new ();
      gtk_box_pack_start (GTK_BOX (hbox), area, TRUE, TRUE, 0);

      /* East */
      area = gtk_drawing_area_new ();
      gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK);
      gtk_box_pack_start (GTK_BOX (hbox), area, TRUE, TRUE, 0);
      g_signal_connect (area, "expose_event", G_CALLBACK (grippy_expose),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_EAST));
      g_signal_connect (area, "button_press_event", G_CALLBACK (grippy_button_press),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_EAST));


      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

      /* South west */
      area = gtk_drawing_area_new ();
      gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK);
      gtk_box_pack_start (GTK_BOX (hbox), area, TRUE, TRUE, 0);
      g_signal_connect (area, "expose_event", G_CALLBACK (grippy_expose),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_SOUTH_WEST));
      g_signal_connect (area, "button_press_event", G_CALLBACK (grippy_button_press),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_SOUTH_WEST));
      /* South */
      area = gtk_drawing_area_new ();
      gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK);
      gtk_box_pack_start (GTK_BOX (hbox), area, TRUE, TRUE, 0);
      g_signal_connect (area, "expose_event", G_CALLBACK (grippy_expose),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_SOUTH));
      g_signal_connect (area, "button_press_event", G_CALLBACK (grippy_button_press),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_SOUTH));
      
      /* South east */
      area = gtk_drawing_area_new ();
      gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK);
      gtk_box_pack_start (GTK_BOX (hbox), area, TRUE, TRUE, 0);
      g_signal_connect (area, "expose_event", G_CALLBACK (grippy_expose),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_SOUTH_EAST));
      g_signal_connect (area, "button_press_event", G_CALLBACK (grippy_button_press),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_SOUTH_EAST));
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Saved Position
 */
gint upositionx = 0;
gint upositiony = 0;

static gint
uposition_configure (GtkWidget *window)
{
  GtkLabel *lx;
  GtkLabel *ly;
  gchar buffer[64];

  lx = g_object_get_data (G_OBJECT (window), "x");
  ly = g_object_get_data (G_OBJECT (window), "y");

  gdk_window_get_root_origin (window->window, &upositionx, &upositiony);
  sprintf (buffer, "%d", upositionx);
  gtk_label_set_text (lx, buffer);
  sprintf (buffer, "%d", upositiony);
  gtk_label_set_text (ly, buffer);

  return FALSE;
}

static void
uposition_stop_configure (GtkToggleButton *toggle,
			  GtkObject       *window)
{
  if (toggle->active)
    g_signal_handlers_block_by_func (window, G_CALLBACK (uposition_configure), NULL);
  else
    g_signal_handlers_unblock_by_func (window, G_CALLBACK (uposition_configure), NULL);
}

static void
create_saved_position (GtkWidget *widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *hbox;
      GtkWidget *main_vbox;
      GtkWidget *vbox;
      GtkWidget *x_label;
      GtkWidget *y_label;
      GtkWidget *button;
      GtkWidget *label;
      GtkWidget *any;

      window = g_object_connect (g_object_new (GTK_TYPE_WINDOW,
						 "type", GTK_WINDOW_TOPLEVEL,
						 "title", "Saved Position",
						 NULL),
				 "signal::configure_event", uposition_configure, NULL,
				 NULL);

      gtk_window_move (GTK_WINDOW (window), upositionx, upositiony);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));
      

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      main_vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 0);
      gtk_container_add (GTK_CONTAINER (window), main_vbox);

      vbox =
	g_object_new (gtk_vbox_get_type (),
			"GtkBox::homogeneous", FALSE,
			"GtkBox::spacing", 5,
			"GtkContainer::border_width", 10,
			"GtkWidget::parent", main_vbox,
			"GtkWidget::visible", TRUE,
			"child", g_object_connect (g_object_new (GTK_TYPE_TOGGLE_BUTTON,
								   "label", "Stop Events",
								   "active", FALSE,
								   "visible", TRUE,
								   NULL),
						   "signal::clicked", uposition_stop_configure, window,
						   NULL),
			NULL);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

      label = gtk_label_new ("X Origin : ");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

      x_label = gtk_label_new ("");
      gtk_box_pack_start (GTK_BOX (hbox), x_label, TRUE, TRUE, 0);
      g_object_set_data (G_OBJECT (window), "x", x_label);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

      label = gtk_label_new ("Y Origin : ");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

      y_label = gtk_label_new ("");
      gtk_box_pack_start (GTK_BOX (hbox), y_label, TRUE, TRUE, 0);
      g_object_set_data (G_OBJECT (window), "y", y_label);

      any =
	g_object_new (gtk_hseparator_get_type (),
			"GtkWidget::visible", TRUE,
			NULL);
      gtk_box_pack_start (GTK_BOX (main_vbox), any, FALSE, TRUE, 0);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
      gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("Close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);
      
      gtk_widget_show_all (window);
    }
  else
    gtk_widget_destroy (window);
}

/*
 * GtkPixmap
 */

static void
create_pixmap (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *box3;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *separator;
  GtkWidget *pixmapwid;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkPixmap");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);
      gtk_widget_realize(window);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      button = gtk_button_new ();
      gtk_box_pack_start (GTK_BOX (box2), button, FALSE, FALSE, 0);

      pixmapwid = new_pixmap ("test.xpm", window->window, NULL);

      label = gtk_label_new ("Pixmap\ntest");
      box3 = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (box3), 2);
      gtk_container_add (GTK_CONTAINER (box3), pixmapwid);
      gtk_container_add (GTK_CONTAINER (box3), label);
      gtk_container_add (GTK_CONTAINER (button), box3);

      button = gtk_button_new ();
      gtk_box_pack_start (GTK_BOX (box2), button, FALSE, FALSE, 0);
      
      pixmapwid = new_pixmap ("test.xpm", window->window, NULL);

      label = gtk_label_new ("Pixmap\ntest");
      box3 = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (box3), 2);
      gtk_container_add (GTK_CONTAINER (box3), pixmapwid);
      gtk_container_add (GTK_CONTAINER (box3), label);
      gtk_container_add (GTK_CONTAINER (button), box3);

      gtk_widget_set_sensitive (button, FALSE);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

static void
tips_query_widget_entered (GtkTipsQuery   *tips_query,
			   GtkWidget      *widget,
			   const gchar    *tip_text,
			   const gchar    *tip_private,
			   GtkWidget	  *toggle)
{
  if (GTK_TOGGLE_BUTTON (toggle)->active)
    {
      gtk_label_set_text (GTK_LABEL (tips_query), tip_text ? "There is a Tip!" : "There is no Tip!");
      /* don't let GtkTipsQuery reset its label */
      g_signal_stop_emission_by_name (tips_query, "widget_entered");
    }
}

static gint
tips_query_widget_selected (GtkWidget      *tips_query,
			    GtkWidget      *widget,
			    const gchar    *tip_text,
			    const gchar    *tip_private,
			    GdkEventButton *event,
			    gpointer        func_data)
{
  if (widget)
    g_print ("Help \"%s\" requested for <%s>\n",
	     tip_private ? tip_private : "None",
	     g_type_name (G_OBJECT_TYPE (widget)));
  return TRUE;
}

static void
create_tooltips (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *box3;
  GtkWidget *button;
  GtkWidget *toggle;
  GtkWidget *frame;
  GtkWidget *tips_query;
  GtkWidget *separator;
  GtkTooltips *tooltips;

  if (!window)
    {
      window =
	g_object_new (gtk_window_get_type (),
			"GtkWindow::type", GTK_WINDOW_TOPLEVEL,
			"GtkContainer::border_width", 0,
			"GtkWindow::title", "Tooltips",
			"GtkWindow::allow_shrink", TRUE,
			"GtkWindow::allow_grow", FALSE,
			NULL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
                        G_CALLBACK (destroy_tooltips),
                        &window);

      tooltips=gtk_tooltips_new();
      g_object_ref (tooltips);
      gtk_object_sink (GTK_OBJECT (tooltips));
      g_object_set_data (G_OBJECT (window), "tooltips", tooltips);
      
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      button = gtk_toggle_button_new_with_label ("button1");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      gtk_tooltips_set_tip (tooltips,
			    button,
			    "This is button 1",
			    "ContextHelp/buttons/1");

      button = gtk_toggle_button_new_with_label ("button2");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      gtk_tooltips_set_tip (tooltips,
			    button,
			    "This is button 2. This is also a really long tooltip which probably won't fit on a single line and will therefore need to be wrapped. Hopefully the wrapping will work correctly.",
			    "ContextHelp/buttons/2_long");

      toggle = gtk_toggle_button_new_with_label ("Override TipsQuery Label");
      gtk_box_pack_start (GTK_BOX (box2), toggle, TRUE, TRUE, 0);

      gtk_tooltips_set_tip (tooltips,
			    toggle,
			    "Toggle TipsQuery view.",
			    "Hi msw! ;)");

      box3 =
	g_object_new (gtk_vbox_get_type (),
			"homogeneous", FALSE,
			"spacing", 5,
			"border_width", 5,
			"visible", TRUE,
			NULL);

      tips_query = gtk_tips_query_new ();

      button =
	g_object_new (gtk_button_get_type (),
			"label", "[?]",
			"visible", TRUE,
			"parent", box3,
			NULL);
      g_object_connect (button,
			"swapped_signal::clicked", gtk_tips_query_start_query, tips_query,
			NULL);
      gtk_box_set_child_packing (GTK_BOX (box3), button, FALSE, FALSE, 0, GTK_PACK_START);
      gtk_tooltips_set_tip (tooltips,
			    button,
			    "Start the Tooltips Inspector",
			    "ContextHelp/buttons/?");
      
      
      g_object_set (g_object_connect (tips_query,
				      "signal::widget_entered", tips_query_widget_entered, toggle,
				      "signal::widget_selected", tips_query_widget_selected, NULL,
				      NULL),
		    "visible", TRUE,
		    "parent", box3,
		    "caller", button,
		    NULL);
      
      frame = g_object_new (gtk_frame_get_type (),
			      "label", "ToolTips Inspector",
			      "label_xalign", (double) 0.5,
			      "border_width", 0,
			      "visible", TRUE,
			      "parent", box2,
			      "child", box3,
			      NULL);
      gtk_box_set_child_packing (GTK_BOX (box2), frame, TRUE, TRUE, 10, GTK_PACK_START);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);

      gtk_tooltips_set_tip (tooltips, button, "Push this button to close window", "ContextHelp/buttons/Close");
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkImage
 */

static void
pack_image (GtkWidget *box,
            const gchar *text,
            GtkWidget *image)
{
  gtk_box_pack_start (GTK_BOX (box),
                      gtk_label_new (text),
                      FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (box),
                      image,
                      TRUE, TRUE, 0);  
}

static void
create_image (GtkWidget *widget)
{
  static GtkWidget *window = NULL;

  if (window == NULL)
    {
      GtkWidget *vbox;
      GdkPixmap *pixmap;
      GdkBitmap *mask;
        
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      /* this is bogus for testing drawing when allocation < request,
       * don't copy into real code
       */
      g_object_set (window, "allow_shrink", TRUE, "allow_grow", TRUE, NULL);
      
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      vbox = gtk_vbox_new (FALSE, 5);

      gtk_container_add (GTK_CONTAINER (window), vbox);

      pack_image (vbox, "Stock Warning Dialog",
                  gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING,
                                            GTK_ICON_SIZE_DIALOG));

      pixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL,
                                                      gtk_widget_get_colormap (window),
                                                      &mask,
                                                      NULL,
                                                      openfile);
      
      pack_image (vbox, "Pixmap",
                  gtk_image_new_from_pixmap (pixmap, mask));
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}
     
/*
 * Menu demo
 */

static GtkWidget*
create_menu (GdkScreen *screen, gint depth, gint length, gboolean tearoff)
{
  GtkWidget *menu;
  GtkWidget *menuitem;
  GtkWidget *image;
  GSList *group;
  char buf[32];
  int i, j;

  if (depth < 1)
    return NULL;

  menu = gtk_menu_new ();
  gtk_menu_set_screen (GTK_MENU (menu), screen);

  group = NULL;

  if (tearoff)
    {
      menuitem = gtk_tearoff_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
      gtk_widget_show (menuitem);
    }

  image = gtk_image_new_from_stock (GTK_STOCK_OPEN,
                                    GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  menuitem = gtk_image_menu_item_new_with_label ("Image item");
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);
  
  for (i = 0, j = 1; i < length; i++, j++)
    {
      sprintf (buf, "item %2d - %d", depth, j);

      menuitem = gtk_radio_menu_item_new_with_label (group, buf);
      group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menuitem));

#if 0
      if (depth % 2)
	gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (menuitem), TRUE);
#endif

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
      gtk_widget_show (menuitem);
      if (i == 3)
	gtk_widget_set_sensitive (menuitem, FALSE);

      if (i == 5)
        gtk_check_menu_item_set_inconsistent (GTK_CHECK_MENU_ITEM (menuitem),
                                              TRUE);

      if (i < 5)
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), 
				   create_menu (screen, depth - 1, 5,  TRUE));
    }

  return menu;
}

static GtkWidget*
create_table_menu (GdkScreen *screen, gint cols, gint rows, gboolean tearoff)
{
  GtkWidget *menu;
  GtkWidget *menuitem;
  GtkWidget *submenu;
  GtkWidget *image;
  char buf[32];
  int i, j;

  menu = gtk_menu_new ();
  gtk_menu_set_screen (GTK_MENU (menu), screen);

  j = 0;
  if (tearoff)
    {
      menuitem = gtk_tearoff_menu_item_new ();
      gtk_menu_attach (GTK_MENU (menu), menuitem, 0, cols, j, j + 1);
      gtk_widget_show (menuitem);
      j++;
    }
  
  menuitem = gtk_menu_item_new_with_label ("items");
  gtk_menu_attach (GTK_MENU (menu), menuitem, 0, cols, j, j + 1);

  submenu = gtk_menu_new ();
  gtk_menu_set_screen (GTK_MENU (submenu), screen);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
  gtk_widget_show (menuitem);
  j++;

  /* now fill the items submenu */
  image = gtk_image_new_from_stock (GTK_STOCK_HELP,
				    GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  menuitem = gtk_image_menu_item_new_with_label ("Image");
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 0, 1, 0, 1);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("x");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 1, 2, 0, 1);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("x");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 0, 1, 1, 2);
  gtk_widget_show (menuitem);
  
  image = gtk_image_new_from_stock (GTK_STOCK_HELP,
				    GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  menuitem = gtk_image_menu_item_new_with_label ("Image");
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 1, 2, 1, 2);
  gtk_widget_show (menuitem);

  menuitem = gtk_radio_menu_item_new_with_label (NULL, "Radio");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 0, 1, 2, 3);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("x");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 1, 2, 2, 3);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("x");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 0, 1, 3, 4);
  gtk_widget_show (menuitem);
  
  menuitem = gtk_radio_menu_item_new_with_label (NULL, "Radio");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 1, 2, 3, 4);
  gtk_widget_show (menuitem);

  menuitem = gtk_check_menu_item_new_with_label ("Check");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 0, 1, 4, 5);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("x");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 1, 2, 4, 5);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("x");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 0, 1, 5, 6);
  gtk_widget_show (menuitem);
  
  menuitem = gtk_check_menu_item_new_with_label ("Check");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 1, 2, 5, 6);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("1. Inserted normally (8)");
  gtk_widget_show (menuitem);
  gtk_menu_shell_insert (GTK_MENU_SHELL (submenu), menuitem, 8);

  menuitem = gtk_menu_item_new_with_label ("2. Inserted normally (2)");
  gtk_widget_show (menuitem);
  gtk_menu_shell_insert (GTK_MENU_SHELL (submenu), menuitem, 2);

  menuitem = gtk_menu_item_new_with_label ("3. Inserted normally (0)");
  gtk_widget_show (menuitem);
  gtk_menu_shell_insert (GTK_MENU_SHELL (submenu), menuitem, 0);

  menuitem = gtk_menu_item_new_with_label ("4. Inserted normally (-1)");
  gtk_widget_show (menuitem);
  gtk_menu_shell_insert (GTK_MENU_SHELL (submenu), menuitem, -1);
  
  /* end of items submenu */

  menuitem = gtk_menu_item_new_with_label ("spanning");
  gtk_menu_attach (GTK_MENU (menu), menuitem, 0, cols, j, j + 1);

  submenu = gtk_menu_new ();
  gtk_menu_set_screen (GTK_MENU (submenu), screen);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
  gtk_widget_show (menuitem);
  j++;

  /* now fill the spanning submenu */
  menuitem = gtk_menu_item_new_with_label ("a");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 0, 2, 0, 1);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("b");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 2, 3, 0, 2);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("c");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 0, 1, 1, 3);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("d");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 1, 2, 1, 2);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("e");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 1, 3, 2, 3);
  gtk_widget_show (menuitem);
  /* end of spanning submenu */
  
  menuitem = gtk_menu_item_new_with_label ("left");
  gtk_menu_attach (GTK_MENU (menu), menuitem, 0, 1, j, j + 1);
  submenu = gtk_menu_new ();
  gtk_menu_set_screen (GTK_MENU (submenu), screen);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("Empty");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 0, 1, 0, 1);
  submenu = gtk_menu_new ();
  gtk_menu_set_screen (GTK_MENU (submenu), screen);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("right");
  gtk_menu_attach (GTK_MENU (menu), menuitem, 1, 2, j, j + 1);
  submenu = gtk_menu_new ();
  gtk_menu_set_screen (GTK_MENU (submenu), screen);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("Empty");
  gtk_menu_attach (GTK_MENU (submenu), menuitem, 0, 1, 0, 1);
  gtk_widget_show (menuitem);

  j++;

  for (; j < rows; j++)
      for (i = 0; i < cols; i++)
      {
	sprintf (buf, "(%d %d)", i, j);
	menuitem = gtk_menu_item_new_with_label (buf);
	gtk_menu_attach (GTK_MENU (menu), menuitem, i, i + 1, j, j + 1);
	gtk_widget_show (menuitem);
      }
  
  menuitem = gtk_menu_item_new_with_label ("1. Inserted normally (8)");
  gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, 8);
  gtk_widget_show (menuitem);
  menuitem = gtk_menu_item_new_with_label ("2. Inserted normally (2)");
  gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, 2);
  gtk_widget_show (menuitem);
  menuitem = gtk_menu_item_new_with_label ("3. Inserted normally (0)");
  gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, 0);
  gtk_widget_show (menuitem);
  menuitem = gtk_menu_item_new_with_label ("4. Inserted normally (-1)");
  gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, -1);
  gtk_widget_show (menuitem);
  
  return menu;
}

static void
create_menus (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *optionmenu;
  GtkWidget *separator;
  
  if (!window)
    {
      GtkWidget *menubar;
      GtkWidget *menu;
      GtkWidget *menuitem;
      GtkAccelGroup *accel_group;
      GtkWidget *image;
      GdkScreen *screen = gtk_widget_get_screen (widget);
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (window), screen);
      
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);
      g_signal_connect (window, "delete-event",
			G_CALLBACK (gtk_true),
			NULL);
      
      accel_group = gtk_accel_group_new ();
      gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

      gtk_window_set_title (GTK_WINDOW (window), "menus");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);
      
      
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);
      
      menubar = gtk_menu_bar_new ();
      gtk_box_pack_start (GTK_BOX (box1), menubar, FALSE, TRUE, 0);
      gtk_widget_show (menubar);
      
      menu = create_menu (screen, 2, 50, TRUE);
      
      menuitem = gtk_menu_item_new_with_label ("test\nline2");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
      gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
      gtk_widget_show (menuitem);

      menu = create_table_menu (screen, 2, 50, TRUE);
      
      menuitem = gtk_menu_item_new_with_label ("table");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
      gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
      gtk_widget_show (menuitem);
      
      menuitem = gtk_menu_item_new_with_label ("foo");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_menu (screen, 3, 5, TRUE));
      gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
      gtk_widget_show (menuitem);

      image = gtk_image_new_from_stock (GTK_STOCK_HELP,
                                        GTK_ICON_SIZE_MENU);
      gtk_widget_show (image);
      menuitem = gtk_image_menu_item_new_with_label ("Help");
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_menu (screen, 4, 5, TRUE));
      gtk_menu_item_set_right_justified (GTK_MENU_ITEM (menuitem), TRUE);
      gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
      gtk_widget_show (menuitem);
      
      menubar = gtk_menu_bar_new ();
      gtk_box_pack_start (GTK_BOX (box1), menubar, FALSE, TRUE, 0);
      gtk_widget_show (menubar);
      
      menu = create_menu (screen, 2, 10, TRUE);
      
      menuitem = gtk_menu_item_new_with_label ("Second menu bar");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
      gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
      gtk_widget_show (menuitem);
      
      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);
      
      menu = create_menu (screen, 1, 5, FALSE);
      gtk_menu_set_accel_group (GTK_MENU (menu), accel_group);

      menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_NEW, accel_group);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
      gtk_widget_show (menuitem);
      
      menuitem = gtk_check_menu_item_new_with_label ("Accelerate Me");
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
      gtk_widget_show (menuitem);
      gtk_widget_add_accelerator (menuitem,
				  "activate",
				  accel_group,
				  GDK_F1,
				  0,
				  GTK_ACCEL_VISIBLE);
      menuitem = gtk_check_menu_item_new_with_label ("Accelerator Locked");
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
      gtk_widget_show (menuitem);
      gtk_widget_add_accelerator (menuitem,
				  "activate",
				  accel_group,
				  GDK_F2,
				  0,
				  GTK_ACCEL_VISIBLE | GTK_ACCEL_LOCKED);
      menuitem = gtk_check_menu_item_new_with_label ("Accelerators Frozen");
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
      gtk_widget_show (menuitem);
      gtk_widget_add_accelerator (menuitem,
				  "activate",
				  accel_group,
				  GDK_F2,
				  0,
				  GTK_ACCEL_VISIBLE);
      gtk_widget_add_accelerator (menuitem,
				  "activate",
				  accel_group,
				  GDK_F3,
				  0,
				  GTK_ACCEL_VISIBLE);
      
      optionmenu = gtk_option_menu_new ();
      gtk_option_menu_set_menu (GTK_OPTION_MENU (optionmenu), menu);
      gtk_option_menu_set_history (GTK_OPTION_MENU (optionmenu), 3);
      gtk_box_pack_start (GTK_BOX (box2), optionmenu, TRUE, TRUE, 0);
      gtk_widget_show (optionmenu);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

static void
gtk_ifactory_cb (gpointer             callback_data,
		 guint                callback_action,
		 GtkWidget           *widget)
{
  g_message ("ItemFactory: activated \"%s\"", gtk_item_factory_path_from_widget (widget));
}

/* GdkPixbuf RGBA C-Source image dump */

static const guint8 apple[] = 
{ ""
  /* Pixbuf magic (0x47646b50) */
  "GdkP"
  /* length: header (24) + pixel_data (2304) */
  "\0\0\11\30"
  /* pixdata_type (0x1010002) */
  "\1\1\0\2"
  /* rowstride (96) */
  "\0\0\0`"
  /* width (24) */
  "\0\0\0\30"
  /* height (24) */
  "\0\0\0\30"
  /* pixel_data: */
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\26\24"
  "\17\11\0\0\0\2\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0`m"
  "[pn{a\344hv_\345_k[`\0\0\0\0\0\0\0\0\0\0\0\0D>/\305\0\0\0_\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0`l[Blza\373s\202d\354w\206g\372p~c"
  "\374`l[y\0\0\0\0[S\77/\27\25\17\335\0\0\0\20\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0`l\\\20iw_\356y\211h\373x\207g\364~\216i\364u\204e\366gt"
  "_\374^jX\241A;-_\0\0\0~\0\0\0\0SM4)SM21B9&\22\320\270\204\1\320\270\204"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0eq"
  "]\212r\200c\366v\205f\371jx_\323_kY\232_kZH^jY\26]iW\211@G9\272:6%j\220"
  "\211]\320\221\211`\377\212\203Z\377~xP\377mkE\331]^;|/0\37\21\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0ly`\40p~b\360lz`\353^kY\246["
  "eT<\216\200Z\203\227\211_\354\234\217c\377\232\217b\362\232\220c\337"
  "\243\233k\377\252\241p\377\250\236p\377\241\225h\377\231\214_\377\210"
  "\202U\377srI\377[]:\355KO0U\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0i"
  "v^\200`lY\211^jY\"\0\0\0\0\221\204\\\273\250\233r\377\302\267\224\377"
  "\311\300\237\377\272\256\204\377\271\256\177\377\271\257\200\377\267"
  "\260\177\377\260\251x\377\250\236l\377\242\225e\377\226\213]\377~zP\377"
  "ff@\377QT5\377LR2d\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0]iW(\0\0\0\0\0\0\0"
  "\0\213\203[v\253\240t\377\334\326\301\377\344\340\317\377\321\312\253"
  "\377\303\271\217\377\300\270\213\377\277\267\210\377\272\264\203\377"
  "\261\255z\377\250\242n\377\243\232h\377\232\220`\377\210\202V\377nnE"
  "\377SW6\377RX6\364Za<\34\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0b]@\20"
  "\234\222e\362\304\274\232\377\337\333\306\377\332\325\273\377\311\302"
  "\232\377\312\303\236\377\301\273\216\377\300\271\212\377\270\264\200"
  "\377\256\253v\377\246\243n\377\236\232h\377\230\220`\377\213\203V\377"
  "wvL\377X]:\377KR0\377NU5v\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\212"
  "\203Zl\242\234l\377\321\315\260\377\331\324\271\377\320\313\251\377\307"
  "\301\232\377\303\276\224\377\300\272\214\377\274\267\206\377\264\260"
  "|\377\253\251s\377\244\243n\377\232\230e\377\223\216^\377\207\200U\377"
  "ttJ\377[_<\377HO/\377GN0\200\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\210\204Y\240\245\237o\377\316\310\253\377\310\303\237\377\304\300\230"
  "\377\303\277\225\377\277\272\216\377\274\270\210\377\266\263\200\377"
  "\256\254v\377\247\246p\377\237\236j\377\227\226d\377\215\212[\377\203"
  "\177T\377qsH\377X]8\377FN.\377DK-\200\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\207\204X\257\244\240o\377\300\275\231\377\301\275\226\377\274"
  "\270\213\377\274\270\214\377\267\264\205\377\264\262\200\377\260\256"
  "z\377\251\251s\377\243\244n\377\231\232g\377\220\222`\377\210\211Y\377"
  "|}Q\377hlC\377PU3\377CK,\377DL/Y\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\205\204X\220\232\230h\377\261\260\204\377\266\264\212\377\261\260"
  "\201\377\263\260\200\377\260\257}\377\256\256x\377\253\254t\377\244\246"
  "o\377\233\236i\377\221\224b\377\211\214\\\377\202\204V\377txM\377]b>"
  "\377HP0\377@H+\373CJ-\25\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0xxO>"
  "\215\215_\377\237\237r\377\247\247x\377\247\247t\377\252\252w\377\252"
  "\252u\377\252\253t\377\243\246o\377\235\240j\377\223\230c\377\213\217"
  "]\377\201\206V\377x}P\377gkD\377RY5\377BI,\377AI,\262\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\202\205W\312\216\220`\377\230"
  "\232g\377\234\236i\377\236\241l\377\241\244n\377\240\244m\377\232\237"
  "i\377\223\230c\377\212\221]\377\200\210W\377v|P\377jnG\377Za>\377HP2"
  "\377=D)\377HQ1:\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0wzQ6\177\201U\371\206\211Z\377\216\222`\377\220\225a\377\220\225b\377"
  "\220\226a\377\213\221_\377\204\213Z\377{\203R\377ryN\377iqH\377^fA\377"
  "R[;\377BJ-\3778@'\317\0\0\0>\0\0\0\36\0\0\0\7\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0ptJTw|Q\371z\177R\377}\202T\377|\203T\377z\200"
  "R\377v|O\377pwL\377jpF\377dlB\377`hB\377Yb@\377LT6\377<C*\377\11\12\6"
  "\376\0\0\0\347\0\0\0\262\0\0\0Y\0\0\0\32\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\\`=UgnE\370hnG\377gmE\377djB\377]d>\377[c<\377Y"
  "b<\377Zc>\377V_>\377OW8\377BK/\377\16\20\12\377\0\0\0\377\0\0\0\377\0"
  "\0\0\374\0\0\0\320\0\0\0I\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\1\0\0\0\40\22\24\15\260@D+\377W`;\377OV5\377.3\36\377.3\37\377IP0"
  "\377RZ7\377PZ8\3776=&\377\14\15\10\377\0\0\0\377\0\0\0\377\0\0\0\377"
  "\0\0\0\347\0\0\0\217\0\0\0""4\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\20\0\0\0P\0\0\0\252\7\10\5\346\7\7\5\375\0\0\0\377\0\0"
  "\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\374\0"
  "\0\0\336\0\0\0\254\0\0\0i\0\0\0""2\0\0\0\10\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\11\0\0\0\40\0\0\0D\0\0\0m\0\0\0"
  "\226\0\0\0\234\0\0\0\234\0\0\0\244\0\0\0\246\0\0\0\232\0\0\0\202\0\0"
  "\0i\0\0\0T\0\0\0,\0\0\0\15\0\0\0\2\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\2\0\0\0\6\0\0\0"
  "\16\0\0\0\22\0\0\0\24\0\0\0\23\0\0\0\17\0\0\0\14\0\0\0\13\0\0\0\10\0"
  "\0\0\5\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"};


static void
dump_accels (gpointer             callback_data,
	     guint                callback_action,
	     GtkWidget           *widget)
{
  gtk_accel_map_save_fd (1 /* stdout */);
}
    
static GtkItemFactoryEntry menu_items[] =
{
  { "/_File",                  NULL,         NULL,                  0, "<Branch>" },
  { "/File/tearoff1",          NULL,         gtk_ifactory_cb,       0, "<Tearoff>" },
  { "/File/_New",              NULL,         gtk_ifactory_cb,       0, "<StockItem>", GTK_STOCK_NEW },
  { "/File/_Open",             NULL,         gtk_ifactory_cb,       0, "<StockItem>", GTK_STOCK_OPEN },
  { "/File/_Save",             NULL,         gtk_ifactory_cb,       0, "<StockItem>", GTK_STOCK_SAVE },
  { "/File/Save _As...",       "<control>A", gtk_ifactory_cb,       0, "<StockItem>", GTK_STOCK_SAVE },
  { "/File/_Dump \"_Accels\"",  NULL,        dump_accels,           0 },
  { "/File/\\/Test__Escaping/And\\/\n\tWei\\\\rdly",
                                NULL,        gtk_ifactory_cb,       0 },
  { "/File/sep1",        NULL,               gtk_ifactory_cb,       0, "<Separator>" },
  { "/File/_Quit",       NULL,               gtk_ifactory_cb,       0, "<StockItem>", GTK_STOCK_QUIT },

  { "/_Preferences",     		NULL, 0,               0, "<Branch>" },
  { "/_Preferences/_Color", 		NULL, 0,               0, "<Branch>" },
  { "/_Preferences/Color/_Red",      	NULL, gtk_ifactory_cb, 0, "<RadioItem>" },
  { "/_Preferences/Color/_Green",   	NULL, gtk_ifactory_cb, 0, "/Preferences/Color/Red" },
  { "/_Preferences/Color/_Blue",        NULL, gtk_ifactory_cb, 0, "/Preferences/Color/Red" },
  { "/_Preferences/_Shape", 		NULL, 0,               0, "<Branch>" },
  { "/_Preferences/Shape/_Square",      NULL, gtk_ifactory_cb, 0, "<RadioItem>" },
  { "/_Preferences/Shape/_Rectangle",   NULL, gtk_ifactory_cb, 0, "/Preferences/Shape/Square" },
  { "/_Preferences/Shape/_Oval",        NULL, gtk_ifactory_cb, 0, "/Preferences/Shape/Rectangle" },
  { "/_Preferences/Shape/_Rectangle",   NULL, gtk_ifactory_cb, 0, "/Preferences/Shape/Square" },
  { "/_Preferences/Shape/_Oval",        NULL, gtk_ifactory_cb, 0, "/Preferences/Shape/Rectangle" },
  { "/_Preferences/Shape/_Image",       NULL, gtk_ifactory_cb, 0, "<ImageItem>", apple },
  { "/_Preferences/Coffee",                  NULL, gtk_ifactory_cb, 0, "<CheckItem>" },
  { "/_Preferences/Toast",                   NULL, gtk_ifactory_cb, 0, "<CheckItem>" },
  { "/_Preferences/Marshmallow Froot Loops", NULL, gtk_ifactory_cb, 0, "<CheckItem>" },

  /* For testing deletion of menus */
  { "/_Preferences/Should_NotAppear",          NULL, 0,               0, "<Branch>" },
  { "/Preferences/ShouldNotAppear/SubItem1",   NULL, gtk_ifactory_cb, 0 },
  { "/Preferences/ShouldNotAppear/SubItem2",   NULL, gtk_ifactory_cb, 0 },

  { "/_Help",            NULL,         0,                     0, "<LastBranch>" },
  { "/Help/_Help",       NULL,         gtk_ifactory_cb,       0, "<StockItem>", GTK_STOCK_HELP},
  { "/Help/_About",      NULL,         gtk_ifactory_cb,       0 },
};


static int nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);

static void
create_item_factory (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  
  if (!window)
    {
      GtkWidget *box1;
      GtkWidget *box2;
      GtkWidget *separator;
      GtkWidget *label;
      GtkWidget *button;
      GtkAccelGroup *accel_group;
      GtkItemFactory *item_factory;
      GtkTooltips *tooltips;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));
      
      g_signal_connect (window, "destroy",
			G_CALLBACK(gtk_widget_destroyed),
			&window);
      g_signal_connect (window, "delete-event",
			G_CALLBACK (gtk_true),
			NULL);
      
      accel_group = gtk_accel_group_new ();
      item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group);
      g_object_set_data_full (G_OBJECT (window),
			      "<main>",
			      item_factory,
			      g_object_unref);
      gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
      gtk_window_set_title (GTK_WINDOW (window), "Item Factory");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);
      gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, NULL);

      /* preselect /Preferences/Shape/Oval over the other radios
       */
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory,
										      "/Preferences/Shape/Oval")),
				      TRUE);

      /* preselect /Preferences/Coffee
       */
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory,
										      "/Preferences/Coffee")),
				      TRUE);

      /* preselect /Preferences/Marshmallow Froot Loops and set it insensitive
       */
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory,
										      "/Preferences/Marshmallow Froot Loops")),
				      TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (gtk_item_factory_get_item (item_factory,
								       "/Preferences/Marshmallow Froot Loops")),
				FALSE);
       
      /* Test how tooltips (ugh) work on menu items
       */
      tooltips = gtk_tooltips_new ();
      g_object_ref (tooltips);
      gtk_object_sink (GTK_OBJECT (tooltips));
      g_object_set_data_full (G_OBJECT (window), "testgtk-tooltips",
			      tooltips, (GDestroyNotify)g_object_unref);
      
      gtk_tooltips_set_tip (tooltips, gtk_item_factory_get_item (item_factory, "/File/New"),
			    "Create a new file", NULL);
      gtk_tooltips_set_tip (tooltips, gtk_item_factory_get_item (item_factory, "/File/Open"),
			    "Open a file", NULL);
      gtk_tooltips_set_tip (tooltips, gtk_item_factory_get_item (item_factory, "/File/Save"),
			    "Safe file", NULL);
      gtk_tooltips_set_tip (tooltips, gtk_item_factory_get_item (item_factory, "/Preferences/Color"),
			    "Modify color", NULL);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      
      gtk_box_pack_start (GTK_BOX (box1),
			  gtk_item_factory_get_widget (item_factory, "<main>"),
			  FALSE, FALSE, 0);

      label = gtk_label_new ("Type\n<alt>\nto start");
      gtk_widget_set_size_request (label, 200, 200);
      gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
      gtk_box_pack_start (GTK_BOX (box1), label, TRUE, TRUE, 0);


      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);

      gtk_item_factory_delete_item (item_factory, "/Preferences/ShouldNotAppear");
      
      gtk_widget_show_all (window);
    }
  else
    gtk_widget_destroy (window);
}

static GtkWidget *
accel_button_new (GtkAccelGroup *accel_group,
		  const gchar   *text,
		  const gchar   *accel)
{
  guint keyval;
  GdkModifierType modifiers;
  GtkWidget *button;
  GtkWidget *label;

  gtk_accelerator_parse (accel, &keyval, &modifiers);
  g_assert (keyval);

  button = gtk_button_new ();
  gtk_widget_add_accelerator (button, "activate", accel_group,
			      keyval, modifiers, GTK_ACCEL_VISIBLE | GTK_ACCEL_LOCKED);

  label = gtk_accel_label_new (text);
  gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (label), button);
  gtk_widget_show (label);
  
  gtk_container_add (GTK_CONTAINER (button), label);

  return button;
}

static void
create_key_lookup (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  gpointer window_ptr;

  if (!window)
    {
      GtkAccelGroup *accel_group = gtk_accel_group_new ();
      GtkWidget *button;
      
      window = gtk_dialog_new_with_buttons ("Key Lookup", NULL, 0,
					    GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					    NULL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      /* We have to expand it so the accel labels will draw their labels
       */
      gtk_window_set_default_size (GTK_WINDOW (window), 300, -1);
      
      gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
      
      button = gtk_button_new_with_mnemonic ("Button 1 (_a)");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), button, FALSE, FALSE, 0);
      button = gtk_button_new_with_mnemonic ("Button 2 (_A)");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), button, FALSE, FALSE, 0);
      button = gtk_button_new_with_mnemonic ("Button 3 (_\321\204)");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), button, FALSE, FALSE, 0);
      button = gtk_button_new_with_mnemonic ("Button 4 (_\320\244)");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), button, FALSE, FALSE, 0);
      button = gtk_button_new_with_mnemonic ("Button 6 (_b)");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), button, FALSE, FALSE, 0);
      button = accel_button_new (accel_group, "Button 7", "<Alt><Shift>b");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), button, FALSE, FALSE, 0);
      button = accel_button_new (accel_group, "Button 8", "<Alt>d");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), button, FALSE, FALSE, 0);
      button = accel_button_new (accel_group, "Button 9", "<Alt>Cyrillic_ve");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), button, FALSE, FALSE, 0);
      button = gtk_button_new_with_mnemonic ("Button 10 (_1)");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), button, FALSE, FALSE, 0);
      button = gtk_button_new_with_mnemonic ("Button 11 (_!)");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), button, FALSE, FALSE, 0);
      button = accel_button_new (accel_group, "Button 12", "<Super>a");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), button, FALSE, FALSE, 0);
      button = accel_button_new (accel_group, "Button 13", "<Hyper>a");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), button, FALSE, FALSE, 0);
      button = accel_button_new (accel_group, "Button 14", "<Meta>a");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), button, FALSE, FALSE, 0);
      button = accel_button_new (accel_group, "Button 15", "<Shift><Mod4>b");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), button, FALSE, FALSE, 0);

      window_ptr = &window;
      g_object_add_weak_pointer (G_OBJECT (window), window_ptr);
      g_signal_connect (window, "response", G_CALLBACK (gtk_object_destroy), NULL);

      gtk_widget_show_all (window);
    }
  else
    gtk_widget_destroy (window);
}


/*
 create_modal_window
 */

static gboolean
cmw_destroy_cb(GtkWidget *widget)
{
  /* This is needed to get out of gtk_main */
  gtk_main_quit ();

  return FALSE;
}

static void
cmw_color (GtkWidget *widget, GtkWidget *parent)
{
    GtkWidget *csd;

    csd = gtk_color_selection_dialog_new ("This is a modal color selection dialog");

    gtk_window_set_screen (GTK_WINDOW (csd), gtk_widget_get_screen (parent));

    gtk_color_selection_set_has_palette (GTK_COLOR_SELECTION (GTK_COLOR_SELECTION_DIALOG (csd)->colorsel),
                                         TRUE);
    
    /* Set as modal */
    gtk_window_set_modal (GTK_WINDOW(csd),TRUE);

    /* And mark it as a transient dialog */
    gtk_window_set_transient_for (GTK_WINDOW (csd), GTK_WINDOW (parent));
    
    g_signal_connect (csd, "destroy",
		      G_CALLBACK (cmw_destroy_cb), NULL);

    g_signal_connect_swapped (GTK_COLOR_SELECTION_DIALOG (csd)->ok_button,
			     "clicked", G_CALLBACK (gtk_widget_destroy), csd);
    g_signal_connect_swapped (GTK_COLOR_SELECTION_DIALOG (csd)->cancel_button,
			     "clicked", G_CALLBACK (gtk_widget_destroy), csd);
    
    /* wait until destroy calls gtk_main_quit */
    gtk_widget_show (csd);    
    gtk_main ();
}

static void
cmw_file (GtkWidget *widget, GtkWidget *parent)
{
    GtkWidget *fs;

    fs = gtk_file_selection_new("This is a modal file selection dialog");

    gtk_window_set_screen (GTK_WINDOW (fs), gtk_widget_get_screen (parent));

    /* Set as modal */
    gtk_window_set_modal (GTK_WINDOW(fs),TRUE);

    /* And mark it as a transient dialog */
    gtk_window_set_transient_for (GTK_WINDOW (fs), GTK_WINDOW (parent));

    g_signal_connect (fs, "destroy",
                      G_CALLBACK (cmw_destroy_cb), NULL);

    g_signal_connect_swapped (GTK_FILE_SELECTION (fs)->ok_button,
			      "clicked", G_CALLBACK (gtk_widget_destroy), fs);
    g_signal_connect_swapped (GTK_FILE_SELECTION (fs)->cancel_button,
			      "clicked", G_CALLBACK (gtk_widget_destroy), fs);
    
    /* wait until destroy calls gtk_main_quit */
    gtk_widget_show (fs);
    
    gtk_main();
}


static void
create_modal_window (GtkWidget *widget)
{
  GtkWidget *window = NULL;
  GtkWidget *box1,*box2;
  GtkWidget *frame1;
  GtkWidget *btnColor,*btnFile,*btnClose;

  /* Create modal window (Here you can use any window descendent )*/
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_screen (GTK_WINDOW (window),
			 gtk_widget_get_screen (widget));

  gtk_window_set_title (GTK_WINDOW(window),"This window is modal");

  /* Set window as modal */
  gtk_window_set_modal (GTK_WINDOW(window),TRUE);

  /* Create widgets */
  box1 = gtk_vbox_new (FALSE,5);
  frame1 = gtk_frame_new ("Standard dialogs in modal form");
  box2 = gtk_vbox_new (TRUE,5);
  btnColor = gtk_button_new_with_label ("Color");
  btnFile = gtk_button_new_with_label ("File Selection");
  btnClose = gtk_button_new_with_label ("Close");

  /* Init widgets */
  gtk_container_set_border_width (GTK_CONTAINER (box1), 3);
  gtk_container_set_border_width (GTK_CONTAINER (box2), 3);
    
  /* Pack widgets */
  gtk_container_add (GTK_CONTAINER (window), box1);
  gtk_box_pack_start (GTK_BOX (box1), frame1, TRUE, TRUE, 4);
  gtk_container_add (GTK_CONTAINER (frame1), box2);
  gtk_box_pack_start (GTK_BOX (box2), btnColor, FALSE, FALSE, 4);
  gtk_box_pack_start (GTK_BOX (box2), btnFile, FALSE, FALSE, 4);
  gtk_box_pack_start (GTK_BOX (box1), gtk_hseparator_new (), FALSE, FALSE, 4);
  gtk_box_pack_start (GTK_BOX (box1), btnClose, FALSE, FALSE, 4);
   
  /* connect signals */
  g_signal_connect_swapped (btnClose, "clicked",
			    G_CALLBACK (gtk_widget_destroy), window);

  g_signal_connect (window, "destroy",
                    G_CALLBACK (cmw_destroy_cb), NULL);
  
  g_signal_connect (btnColor, "clicked",
                    G_CALLBACK (cmw_color), window);
  g_signal_connect (btnFile, "clicked",
                    G_CALLBACK (cmw_file), window);

  /* Show widgets */
  gtk_widget_show_all (window);

  /* wait until dialog get destroyed */
  gtk_main();
}

/*
 * GtkMessageDialog
 */

static void
make_message_dialog (GdkScreen *screen,
		     GtkWidget **dialog,
                     GtkMessageType  type,
                     GtkButtonsType  buttons,
		     guint           default_response)
{
  if (*dialog)
    {
      gtk_widget_destroy (*dialog);

      return;
    }

  *dialog = gtk_message_dialog_new (NULL, 0, type, buttons,
                                    "This is a message dialog; it can wrap long lines. This is a long line. La la la. Look this line is wrapped. Blah blah blah blah blah blah. (Note: testgtk has a nonstandard gtkrc that changes some of the message dialog icons.)");

  gtk_window_set_screen (GTK_WINDOW (*dialog), screen);

  g_signal_connect_swapped (*dialog,
			    "response",
			    G_CALLBACK (gtk_widget_destroy),
			    *dialog);
  
  g_signal_connect (*dialog,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    dialog);

  gtk_dialog_set_default_response (GTK_DIALOG (*dialog), default_response);

  gtk_widget_show (*dialog);
}

static void
create_message_dialog (GtkWidget *widget)
{
  static GtkWidget *info = NULL;
  static GtkWidget *warning = NULL;
  static GtkWidget *error = NULL;
  static GtkWidget *question = NULL;
  GdkScreen *screen = gtk_widget_get_screen (widget);

  make_message_dialog (screen, &info, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, GTK_RESPONSE_OK);
  make_message_dialog (screen, &warning, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, GTK_RESPONSE_CLOSE);
  make_message_dialog (screen, &error, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK_CANCEL, GTK_RESPONSE_OK);
  make_message_dialog (screen, &question, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_RESPONSE_NO);
}

/*
 * GtkScrolledWindow
 */

static GtkWidget *sw_parent = NULL;
static GtkWidget *sw_float_parent;
static guint sw_destroyed_handler = 0;

static gboolean
scrolled_windows_delete_cb (GtkWidget *widget, GdkEventAny *event, GtkWidget *scrollwin)
{
  gtk_widget_reparent (scrollwin, sw_parent);
  
  g_signal_handler_disconnect (sw_parent, sw_destroyed_handler);
  sw_float_parent = NULL;
  sw_parent = NULL;
  sw_destroyed_handler = 0;

  return FALSE;
}

static void
scrolled_windows_destroy_cb (GtkWidget *widget, GtkWidget *scrollwin)
{
  gtk_widget_destroy (sw_float_parent);

  sw_float_parent = NULL;
  sw_parent = NULL;
  sw_destroyed_handler = 0;
}

static void
scrolled_windows_remove (GtkWidget *widget, GtkWidget *scrollwin)
{
  if (sw_parent)
    {
      gtk_widget_reparent (scrollwin, sw_parent);
      gtk_widget_destroy (sw_float_parent);

      g_signal_handler_disconnect (sw_parent, sw_destroyed_handler);
      sw_float_parent = NULL;
      sw_parent = NULL;
      sw_destroyed_handler = 0;
    }
  else
    {
      sw_parent = scrollwin->parent;
      sw_float_parent = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (sw_float_parent),
			     gtk_widget_get_screen (widget));
      
      gtk_window_set_default_size (GTK_WINDOW (sw_float_parent), 200, 200);
      
      gtk_widget_reparent (scrollwin, sw_float_parent);
      gtk_widget_show (sw_float_parent);

      sw_destroyed_handler =
	g_signal_connect (sw_parent, "destroy",
			  G_CALLBACK (scrolled_windows_destroy_cb), scrollwin);
      g_signal_connect (sw_float_parent, "delete_event",
			G_CALLBACK (scrolled_windows_delete_cb), scrollwin);
    }
}

static void
create_scrolled_windows (GtkWidget *widget)
{
  static GtkWidget *window;
  GtkWidget *scrolled_window;
  GtkWidget *table;
  GtkWidget *button;
  char buffer[32];
  int i, j;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "dialog");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);


      scrolled_window = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 10);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  scrolled_window, TRUE, TRUE, 0);
      gtk_widget_show (scrolled_window);

      table = gtk_table_new (20, 20, FALSE);
      gtk_table_set_row_spacings (GTK_TABLE (table), 10);
      gtk_table_set_col_spacings (GTK_TABLE (table), 10);
      gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), table);
      gtk_container_set_focus_hadjustment (GTK_CONTAINER (table),
					   gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scrolled_window)));
      gtk_container_set_focus_vadjustment (GTK_CONTAINER (table),
					   gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window)));
      gtk_widget_show (table);

      for (i = 0; i < 20; i++)
	for (j = 0; j < 20; j++)
	  {
	    sprintf (buffer, "button (%d,%d)\n", i, j);
	    button = gtk_toggle_button_new_with_label (buffer);
	    gtk_table_attach_defaults (GTK_TABLE (table), button,
				       i, i+1, j, j+1);
	    gtk_widget_show (button);
	  }


      button = gtk_button_new_with_label ("Close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Reparent Out");
      g_signal_connect (button, "clicked",
			G_CALLBACK (scrolled_windows_remove),
			scrolled_window);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkEntry
 */

static void
entry_toggle_frame (GtkWidget *checkbutton,
                    GtkWidget *entry)
{
   gtk_entry_set_has_frame (GTK_ENTRY(entry),
                            GTK_TOGGLE_BUTTON(checkbutton)->active);
}

static void
entry_toggle_sensitive (GtkWidget *checkbutton,
			GtkWidget *entry)
{
   gtk_widget_set_sensitive (entry, GTK_TOGGLE_BUTTON(checkbutton)->active);
}

static gboolean
entry_progress_timeout (gpointer data)
{
  if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (data), "progress-pulse")))
    {
      gtk_entry_progress_pulse (GTK_ENTRY (data));
    }
  else
    {
      gdouble fraction;

      fraction = gtk_entry_get_progress_fraction (GTK_ENTRY (data));

      fraction += 0.05;
      if (fraction > 1.0001)
        fraction = 0.0;

      gtk_entry_set_progress_fraction (GTK_ENTRY (data), fraction);
    }

  return TRUE;
}

static void
entry_remove_timeout (gpointer data)
{
  g_source_remove (GPOINTER_TO_UINT (data));
}

static void
entry_toggle_progress (GtkWidget *checkbutton,
                       GtkWidget *entry)
{
  if (GTK_TOGGLE_BUTTON (checkbutton)->active)
    {
      guint timeout = gdk_threads_add_timeout (100,
                                               entry_progress_timeout,
                                               entry);
      g_object_set_data_full (G_OBJECT (entry), "timeout-id",
                              GUINT_TO_POINTER (timeout),
                              entry_remove_timeout);
    }
  else
    {
      g_object_set_data (G_OBJECT (entry), "timeout-id",
                         GUINT_TO_POINTER (0));

      gtk_entry_set_progress_fraction (GTK_ENTRY (entry), 0.0);
    }
}

static void
entry_toggle_pulse (GtkWidget *checkbutton,
                    GtkWidget *entry)
{
  g_object_set_data (G_OBJECT (entry), "progress-pulse",
                     GUINT_TO_POINTER ((guint) GTK_TOGGLE_BUTTON (checkbutton)->active));
}

static void
props_clicked (GtkWidget *button,
               GObject   *object)
{
  GtkWidget *window = create_prop_editor (object, 0);

  gtk_window_set_title (GTK_WINDOW (window), "Object Properties");
}

static void
create_entry (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *hbox;
  GtkWidget *has_frame_check;
  GtkWidget *sensitive_check;
  GtkWidget *progress_check;
  GtkWidget *entry, *cb;
  GtkWidget *button;
  GtkWidget *separator;
  GList *cbitems = NULL;

  if (!window)
    {
      cbitems = g_list_append(cbitems, "item0");
      cbitems = g_list_append(cbitems, "item1 item1");
      cbitems = g_list_append(cbitems, "item2 item2 item2");
      cbitems = g_list_append(cbitems, "item3 item3 item3 item3");
      cbitems = g_list_append(cbitems, "item4 item4 item4 item4 item4");
      cbitems = g_list_append(cbitems, "item5 item5 item5 item5 item5 item5");
      cbitems = g_list_append(cbitems, "item6 item6 item6 item6 item6");
      cbitems = g_list_append(cbitems, "item7 item7 item7 item7");
      cbitems = g_list_append(cbitems, "item8 item8 item8");
      cbitems = g_list_append(cbitems, "item9 item9");

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "entry");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      hbox = gtk_hbox_new (FALSE, 5);
      gtk_box_pack_start (GTK_BOX (box2), hbox, TRUE, TRUE, 0);
      
      entry = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (entry), "hello world \330\247\331\204\330\263\331\204\330\247\331\205 \330\271\331\204\331\212\331\203\331\205");
      gtk_editable_select_region (GTK_EDITABLE (entry), 0, 5);
      gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

      button = gtk_button_new_with_mnemonic ("_Props");
      gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (props_clicked),
			entry);

      cb = gtk_combo_new ();
      gtk_combo_set_popdown_strings (GTK_COMBO (cb), cbitems);
      gtk_entry_set_text (GTK_ENTRY (GTK_COMBO(cb)->entry), "hello world \n\n\n foo");
      gtk_editable_select_region (GTK_EDITABLE (GTK_COMBO(cb)->entry),
				  0, -1);
      gtk_box_pack_start (GTK_BOX (box2), cb, TRUE, TRUE, 0);

      sensitive_check = gtk_check_button_new_with_label("Sensitive");
      gtk_box_pack_start (GTK_BOX (box2), sensitive_check, FALSE, TRUE, 0);
      g_signal_connect (sensitive_check, "toggled",
			G_CALLBACK (entry_toggle_sensitive), entry);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sensitive_check), TRUE);

      has_frame_check = gtk_check_button_new_with_label("Has Frame");
      gtk_box_pack_start (GTK_BOX (box2), has_frame_check, FALSE, TRUE, 0);
      g_signal_connect (has_frame_check, "toggled",
			G_CALLBACK (entry_toggle_frame), entry);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (has_frame_check), TRUE);

      progress_check = gtk_check_button_new_with_label("Show Progress");
      gtk_box_pack_start (GTK_BOX (box2), progress_check, FALSE, TRUE, 0);
      g_signal_connect (progress_check, "toggled",
			G_CALLBACK (entry_toggle_progress), entry);

      progress_check = gtk_check_button_new_with_label("Pulse Progress");
      gtk_box_pack_start (GTK_BOX (box2), progress_check, FALSE, TRUE, 0);
      g_signal_connect (progress_check, "toggled",
			G_CALLBACK (entry_toggle_pulse), entry);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

static void
create_expander (GtkWidget *widget)
{
  GtkWidget *box1;
  GtkWidget *expander;
  GtkWidget *hidden;
  static GtkWidget *window = NULL;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));
      
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);
      
      gtk_window_set_title (GTK_WINDOW (window), "expander");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);
      
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      
      expander = gtk_expander_new ("The Hidden");
      
      gtk_box_pack_start (GTK_BOX (box1), expander, TRUE, TRUE, 0);
      
      hidden = gtk_label_new ("Revealed!");
      
      gtk_container_add (GTK_CONTAINER (expander), hidden);
    }
  
  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}


/* GtkEventBox */


static void
event_box_label_pressed (GtkWidget        *widget,
			 GdkEventButton   *event,
			 gpointer user_data)
{
  g_print ("clicked on event box\n");
}

static void
event_box_button_clicked (GtkWidget *widget,
			  GtkWidget *button,
			  gpointer user_data)
{
  g_print ("pushed button\n");
}

static void
event_box_toggle_visible_window (GtkWidget *checkbutton,
				 GtkEventBox *event_box)
{
  gtk_event_box_set_visible_window (event_box,
				    GTK_TOGGLE_BUTTON(checkbutton)->active);
}

static void
event_box_toggle_above_child (GtkWidget *checkbutton,
			      GtkEventBox *event_box)
{
  gtk_event_box_set_above_child (event_box,
				 GTK_TOGGLE_BUTTON(checkbutton)->active);
}

static void
create_event_box (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *event_box;
  GtkWidget *label;
  GtkWidget *visible_window_check;
  GtkWidget *above_child_check;
  GdkColor color;

  if (!window)
    {
      color.red = 0;
      color.blue = 65535;
      color.green = 0;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "event box");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_modify_bg (window, GTK_STATE_NORMAL, &color);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (box1), hbox, TRUE, FALSE, 0);
      
      event_box = gtk_event_box_new ();
      gtk_box_pack_start (GTK_BOX (hbox), event_box, TRUE, FALSE, 0);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (event_box), vbox);
      g_signal_connect (event_box, "button_press_event",
			G_CALLBACK (event_box_label_pressed),
			NULL);
      
      label = gtk_label_new ("Click on this label");
      gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);

      button = gtk_button_new_with_label ("button in eventbox");
      gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, FALSE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (event_box_button_clicked),
			NULL);
      

      visible_window_check = gtk_check_button_new_with_label("Visible Window");
      gtk_box_pack_start (GTK_BOX (box1), visible_window_check, FALSE, TRUE, 0);
      g_signal_connect (visible_window_check, "toggled",
			G_CALLBACK (event_box_toggle_visible_window), event_box);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (visible_window_check), FALSE);
      
      above_child_check = gtk_check_button_new_with_label("Above Child");
      gtk_box_pack_start (GTK_BOX (box1), above_child_check, FALSE, TRUE, 0);
      g_signal_connect (above_child_check, "toggled",
			G_CALLBACK (event_box_toggle_above_child), event_box);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (above_child_check), FALSE);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}


/*
 * GtkSizeGroup
 */

#define SIZE_GROUP_INITIAL_SIZE 50

static void
size_group_hsize_changed (GtkSpinButton *spin_button,
			  GtkWidget     *button)
{
  gtk_widget_set_size_request (GTK_BIN (button)->child,
			       gtk_spin_button_get_value_as_int (spin_button),
			       -1);
}

static void
size_group_vsize_changed (GtkSpinButton *spin_button,
			  GtkWidget     *button)
{
  gtk_widget_set_size_request (GTK_BIN (button)->child,
			       -1,
			       gtk_spin_button_get_value_as_int (spin_button));
}

static GtkWidget *
create_size_group_window (GdkScreen    *screen,
			  GtkSizeGroup *master_size_group)
{
  GtkWidget *window;
  GtkWidget *table;
  GtkWidget *main_button;
  GtkWidget *button;
  GtkWidget *spin_button;
  GtkWidget *hbox;
  GtkSizeGroup *hgroup1;
  GtkSizeGroup *hgroup2;
  GtkSizeGroup *vgroup1;
  GtkSizeGroup *vgroup2;

  window = gtk_dialog_new_with_buttons ("GtkSizeGroup",
					NULL, 0,
					GTK_STOCK_CLOSE,
					GTK_RESPONSE_NONE,
					NULL);

  gtk_window_set_screen (GTK_WINDOW (window), screen);

  gtk_window_set_resizable (GTK_WINDOW (window), TRUE);

  g_signal_connect (window, "response",
		    G_CALLBACK (gtk_widget_destroy),
		    NULL);

  table = gtk_table_new (2, 2, FALSE);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), table, TRUE, TRUE, 0);

  gtk_table_set_row_spacings (GTK_TABLE (table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table), 5);
  gtk_container_set_border_width (GTK_CONTAINER (table), 5);
  gtk_widget_set_size_request (table, 250, 250);

  hgroup1 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  hgroup2 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  vgroup1 = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
  vgroup2 = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

  main_button = gtk_button_new_with_label ("X");
  
  gtk_table_attach (GTK_TABLE (table), main_button,
		    0, 1,       0, 1,
		    GTK_EXPAND, GTK_EXPAND,
		    0,          0);
  gtk_size_group_add_widget (master_size_group, main_button);
  gtk_size_group_add_widget (hgroup1, main_button);
  gtk_size_group_add_widget (vgroup1, main_button);
  gtk_widget_set_size_request (GTK_BIN (main_button)->child,
			       SIZE_GROUP_INITIAL_SIZE,
			       SIZE_GROUP_INITIAL_SIZE);

  button = gtk_button_new ();
  gtk_table_attach (GTK_TABLE (table), button,
		    1, 2,       0, 1,
		    GTK_EXPAND, GTK_EXPAND,
		    0,          0);
  gtk_size_group_add_widget (vgroup1, button);
  gtk_size_group_add_widget (vgroup2, button);

  button = gtk_button_new ();
  gtk_table_attach (GTK_TABLE (table), button,
		    0, 1,       1, 2,
		    GTK_EXPAND, GTK_EXPAND,
		    0,          0);
  gtk_size_group_add_widget (hgroup1, button);
  gtk_size_group_add_widget (hgroup2, button);

  button = gtk_button_new ();
  gtk_table_attach (GTK_TABLE (table), button,
		    1, 2,       1, 2,
		    GTK_EXPAND, GTK_EXPAND,
		    0,          0);
  gtk_size_group_add_widget (hgroup2, button);
  gtk_size_group_add_widget (vgroup2, button);

  g_object_unref (hgroup1);
  g_object_unref (hgroup2);
  g_object_unref (vgroup1);
  g_object_unref (vgroup2);
  
  hbox = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), hbox, FALSE, FALSE, 0);
  
  spin_button = gtk_spin_button_new_with_range (1, 100, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_button), SIZE_GROUP_INITIAL_SIZE);
  gtk_box_pack_start (GTK_BOX (hbox), spin_button, TRUE, TRUE, 0);
  g_signal_connect (spin_button, "value_changed",
		    G_CALLBACK (size_group_hsize_changed), main_button);

  spin_button = gtk_spin_button_new_with_range (1, 100, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_button), SIZE_GROUP_INITIAL_SIZE);
  gtk_box_pack_start (GTK_BOX (hbox), spin_button, TRUE, TRUE, 0);
  g_signal_connect (spin_button, "value_changed",
		    G_CALLBACK (size_group_vsize_changed), main_button);

  return window;
}

static void
create_size_groups (GtkWidget *widget)
{
  static GtkWidget *window1 = NULL;
  static GtkWidget *window2 = NULL;
  static GtkSizeGroup *master_size_group;

  if (!master_size_group)
    master_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  if (!window1)
    {
      window1 = create_size_group_window (gtk_widget_get_screen (widget),
					  master_size_group);

      g_signal_connect (window1, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window1);
    }

  if (!window2)
    {
      window2 = create_size_group_window (gtk_widget_get_screen (widget),
					  master_size_group);

      g_signal_connect (window2, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window2);
    }

  if (gtk_widget_get_visible (window1) && gtk_widget_get_visible (window2))
    {
      gtk_widget_destroy (window1);
      gtk_widget_destroy (window2);
    }
  else
    {
      if (!gtk_widget_get_visible (window1))
	gtk_widget_show_all (window1);
      if (!gtk_widget_get_visible (window2))
	gtk_widget_show_all (window2);
    }
}

/*
 * GtkSpinButton
 */

static GtkWidget *spinner1;

static void
toggle_snap (GtkWidget *widget, GtkSpinButton *spin)
{
  gtk_spin_button_set_snap_to_ticks (spin, GTK_TOGGLE_BUTTON (widget)->active);
}

static void
toggle_numeric (GtkWidget *widget, GtkSpinButton *spin)
{
  gtk_spin_button_set_numeric (spin, GTK_TOGGLE_BUTTON (widget)->active);
}

static void
change_digits (GtkWidget *widget, GtkSpinButton *spin)
{
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spinner1),
			      gtk_spin_button_get_value_as_int (spin));
}

static void
get_value (GtkWidget *widget, gpointer data)
{
  gchar buf[32];
  GtkLabel *label;
  GtkSpinButton *spin;

  spin = GTK_SPIN_BUTTON (spinner1);
  label = GTK_LABEL (g_object_get_data (G_OBJECT (widget), "user_data"));
  if (GPOINTER_TO_INT (data) == 1)
    sprintf (buf, "%d", gtk_spin_button_get_value_as_int (spin));
  else
    sprintf (buf, "%0.*f", spin->digits, gtk_spin_button_get_value (spin));
  gtk_label_set_text (label, buf);
}

static void
get_spin_value (GtkWidget *widget, gpointer data)
{
  gchar *buffer;
  GtkLabel *label;
  GtkSpinButton *spin;

  spin = GTK_SPIN_BUTTON (widget);
  label = GTK_LABEL (data);

  buffer = g_strdup_printf ("%0.*f", spin->digits,
			    gtk_spin_button_get_value (spin));
  gtk_label_set_text (label, buffer);

  g_free (buffer);
}

static gint
spin_button_time_output_func (GtkSpinButton *spin_button)
{
  static gchar buf[6];
  gdouble hours;
  gdouble minutes;

  hours = spin_button->adjustment->value / 60.0;
  minutes = (fabs(floor (hours) - hours) < 1e-5) ? 0.0 : 30;
  sprintf (buf, "%02.0f:%02.0f", floor (hours), minutes);
  if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin_button))))
    gtk_entry_set_text (GTK_ENTRY (spin_button), buf);
  return TRUE;
}

static gint
spin_button_month_input_func (GtkSpinButton *spin_button,
			      gdouble       *new_val)
{
  gint i;
  static gchar *month[12] = { "January", "February", "March", "April",
			      "May", "June", "July", "August",
			      "September", "October", "November", "December" };
  gchar *tmp1, *tmp2;
  gboolean found = FALSE;

  for (i = 1; i <= 12; i++)
    {
      tmp1 = g_ascii_strup (month[i - 1], -1);
      tmp2 = g_ascii_strup (gtk_entry_get_text (GTK_ENTRY (spin_button)), -1);
      if (strstr (tmp1, tmp2) == tmp1)
	found = TRUE;
      g_free (tmp1);
      g_free (tmp2);
      if (found)
	break;
    }
  if (!found)
    {
      *new_val = 0.0;
      return GTK_INPUT_ERROR;
    }
  *new_val = (gdouble) i;
  return TRUE;
}

static gint
spin_button_month_output_func (GtkSpinButton *spin_button)
{
  gint i;
  static gchar *month[12] = { "January", "February", "March", "April",
			      "May", "June", "July", "August", "September",
			      "October", "November", "December" };

  for (i = 1; i <= 12; i++)
    if (fabs (spin_button->adjustment->value - (double)i) < 1e-5)
      {
	if (strcmp (month[i-1], gtk_entry_get_text (GTK_ENTRY (spin_button))))
	  gtk_entry_set_text (GTK_ENTRY (spin_button), month[i-1]);
      }
  return TRUE;
}

static gint
spin_button_hex_input_func (GtkSpinButton *spin_button,
			    gdouble       *new_val)
{
  const gchar *buf;
  gchar *err;
  gdouble res;

  buf = gtk_entry_get_text (GTK_ENTRY (spin_button));
  res = strtol(buf, &err, 16);
  *new_val = res;
  if (*err)
    return GTK_INPUT_ERROR;
  else
    return TRUE;
}

static gint
spin_button_hex_output_func (GtkSpinButton *spin_button)
{
  static gchar buf[7];
  gint val;

  val = (gint) spin_button->adjustment->value;
  if (fabs (val) < 1e-5)
    sprintf (buf, "0x00");
  else
    sprintf (buf, "0x%.2X", val);
  if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin_button))))
    gtk_entry_set_text (GTK_ENTRY (spin_button), buf);
  return TRUE;
}

static void
create_spins (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *frame;
  GtkWidget *hbox;
  GtkWidget *main_vbox;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *spinner2;
  GtkWidget *spinner;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *val_label;
  GtkAdjustment *adj;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));
      
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);
      
      gtk_window_set_title (GTK_WINDOW (window), "GtkSpinButton");
      
      main_vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 10);
      gtk_container_add (GTK_CONTAINER (window), main_vbox);
      
      frame = gtk_frame_new ("Not accelerated");
      gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, TRUE, 0);
      
      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
      gtk_container_add (GTK_CONTAINER (frame), vbox);
      
      /* Time, month, hex spinners */
      
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 5);
      
      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);
      
      label = gtk_label_new ("Time :");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (0, 0, 1410, 30, 60, 0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_editable_set_editable (GTK_EDITABLE (spinner), FALSE);
      g_signal_connect (spinner,
			"output",
			G_CALLBACK (spin_button_time_output_func),
			NULL);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner), TRUE);
      gtk_entry_set_width_chars (GTK_ENTRY (spinner), 5);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner, FALSE, TRUE, 0);

      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);
      
      label = gtk_label_new ("Month :");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (1.0, 1.0, 12.0, 1.0,
						  5.0, 0.0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (spinner),
					 GTK_UPDATE_IF_VALID);
      g_signal_connect (spinner,
			"input",
			G_CALLBACK (spin_button_month_input_func),
			NULL);
      g_signal_connect (spinner,
			"output",
			G_CALLBACK (spin_button_month_output_func),
			NULL);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner), TRUE);
      gtk_entry_set_width_chars (GTK_ENTRY (spinner), 9);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner, FALSE, TRUE, 0);
      
      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);

      label = gtk_label_new ("Hex :");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);

      adj = (GtkAdjustment *) gtk_adjustment_new (0, 0, 255, 1, 16, 0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_editable_set_editable (GTK_EDITABLE (spinner), TRUE);
      g_signal_connect (spinner,
			"input",
			G_CALLBACK (spin_button_hex_input_func),
			NULL);
      g_signal_connect (spinner,
			"output",
			G_CALLBACK (spin_button_hex_output_func),
			NULL);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner), TRUE);
      gtk_entry_set_width_chars (GTK_ENTRY (spinner), 4);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner, FALSE, TRUE, 0);

      frame = gtk_frame_new ("Accelerated");
      gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, TRUE, 0);
  
      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
      gtk_container_add (GTK_CONTAINER (frame), vbox);
      
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 5);
      
      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 5);
      
      label = gtk_label_new ("Value :");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);

      adj = (GtkAdjustment *) gtk_adjustment_new (0.0, -10000.0, 10000.0,
						  0.5, 100.0, 0.0);
      spinner1 = gtk_spin_button_new (adj, 1.0, 2);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner1), TRUE);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner1, FALSE, TRUE, 0);

      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 5);

      label = gtk_label_new ("Digits :");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);

      adj = (GtkAdjustment *) gtk_adjustment_new (2, 1, 15, 1, 1, 0);
      spinner2 = gtk_spin_button_new (adj, 0.0, 0);
      g_signal_connect (adj, "value_changed",
			G_CALLBACK (change_digits),
			spinner2);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner2, FALSE, TRUE, 0);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);

      button = gtk_check_button_new_with_label ("Snap to 0.5-ticks");
      g_signal_connect (button, "clicked",
			G_CALLBACK (toggle_snap),
			spinner1);
      gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

      button = gtk_check_button_new_with_label ("Numeric only input mode");
      g_signal_connect (button, "clicked",
			G_CALLBACK (toggle_numeric),
			spinner1);
      gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

      val_label = gtk_label_new ("");

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 5);

      button = gtk_button_new_with_label ("Value as Int");
      g_object_set_data (G_OBJECT (button), "user_data", val_label);
      g_signal_connect (button, "clicked",
			G_CALLBACK (get_value),
			GINT_TO_POINTER (1));
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);

      button = gtk_button_new_with_label ("Value as Float");
      g_object_set_data (G_OBJECT (button), "user_data", val_label);
      g_signal_connect (button, "clicked",
			G_CALLBACK (get_value),
			GINT_TO_POINTER (2));
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);

      gtk_box_pack_start (GTK_BOX (vbox), val_label, TRUE, TRUE, 0);
      gtk_label_set_text (GTK_LABEL (val_label), "0");

      frame = gtk_frame_new ("Using Convenience Constructor");
      gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, TRUE, 0);
  
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_container_add (GTK_CONTAINER (frame), hbox);
      
      val_label = gtk_label_new ("0.0");

      spinner = gtk_spin_button_new_with_range (0.0, 10.0, 0.009);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (spinner), 0.0);
      g_signal_connect (spinner, "value_changed",
			G_CALLBACK (get_spin_value), val_label);
      gtk_box_pack_start (GTK_BOX (hbox), spinner, TRUE, TRUE, 5);
      gtk_box_pack_start (GTK_BOX (hbox), val_label, TRUE, TRUE, 5);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, TRUE, 0);
  
      button = gtk_button_new_with_label ("Close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}


/*
 * Cursors
 */

static gint
cursor_expose_event (GtkWidget *widget,
		     GdkEvent  *event,
		     gpointer   user_data)
{
  GtkDrawingArea *darea;
  GdkDrawable *drawable;
  guint max_width;
  guint max_height;
  cairo_t *cr;

  g_return_val_if_fail (widget != NULL, TRUE);
  g_return_val_if_fail (GTK_IS_DRAWING_AREA (widget), TRUE);

  darea = GTK_DRAWING_AREA (widget);
  drawable = widget->window;
  max_width = widget->allocation.width;
  max_height = widget->allocation.height;

  cr = gdk_cairo_create (drawable);

  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_rectangle (cr, 0, 0, max_width, max_height / 2);
  cairo_fill (cr);

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_rectangle (cr, 0, max_height / 2, max_width, max_height / 2);
  cairo_fill (cr);

  gdk_cairo_set_source_color (cr, &widget->style->bg[GTK_STATE_NORMAL]);
  cairo_rectangle (cr, max_width / 3, max_height / 3, max_width / 3, max_height / 3);
  cairo_fill (cr);

  cairo_destroy (cr);

  return TRUE;
}

static void
set_cursor (GtkWidget *spinner,
	    GtkWidget *widget)
{
  guint c;
  GdkCursor *cursor;
  GtkWidget *label;
  GEnumClass *class;
  GEnumValue *vals;

  c = CLAMP (gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spinner)), 0, 152);
  c &= 0xfe;

  label = g_object_get_data (G_OBJECT (spinner), "user_data");
  
  class = g_type_class_ref (GDK_TYPE_CURSOR_TYPE);
  vals = class->values;

  while (vals && vals->value != c)
    vals++;
  if (vals)
    gtk_label_set_text (GTK_LABEL (label), vals->value_nick);
  else
    gtk_label_set_text (GTK_LABEL (label), "<unknown>");

  g_type_class_unref (class);

  cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget), c);
  gdk_window_set_cursor (widget->window, cursor);
  gdk_cursor_unref (cursor);
}

static gint
cursor_event (GtkWidget          *widget,
	      GdkEvent           *event,
	      GtkSpinButton	 *spinner)
{
  if ((event->type == GDK_BUTTON_PRESS) &&
      ((event->button.button == 1) ||
       (event->button.button == 3)))
    {
      gtk_spin_button_spin (spinner, event->button.button == 1 ?
			    GTK_SPIN_STEP_FORWARD : GTK_SPIN_STEP_BACKWARD, 0);
      return TRUE;
    }

  return FALSE;
}

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"

static void
change_cursor_theme (GtkWidget *widget,
		     gpointer   data)
{
  const gchar *theme;
  gint size;
  GList *children;

  children = gtk_container_get_children (GTK_CONTAINER (data));

  theme = gtk_entry_get_text (GTK_ENTRY (children->next->data));
  size = (gint) gtk_spin_button_get_value (GTK_SPIN_BUTTON (children->next->next->data));

  g_list_free (children);

  gdk_x11_display_set_cursor_theme (gtk_widget_get_display (widget),
				    theme, size);
}
#endif


static void
create_cursors (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *frame;
  GtkWidget *hbox;
  GtkWidget *main_vbox;
  GtkWidget *vbox;
  GtkWidget *darea;
  GtkWidget *spinner;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *any;
  GtkAdjustment *adj;
  GtkWidget *entry;
  GtkWidget *size;  

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window), 
			     gtk_widget_get_screen (widget));
      
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);
      
      gtk_window_set_title (GTK_WINDOW (window), "Cursors");
      
      main_vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 0);
      gtk_container_add (GTK_CONTAINER (window), main_vbox);

      vbox =
	g_object_new (gtk_vbox_get_type (),
			"GtkBox::homogeneous", FALSE,
			"GtkBox::spacing", 5,
			"GtkContainer::border_width", 10,
			"GtkWidget::parent", main_vbox,
			"GtkWidget::visible", TRUE,
			NULL);

#ifdef GDK_WINDOWING_X11
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

      label = gtk_label_new ("Cursor Theme : ");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

      entry = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (entry), "default");
      gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, TRUE, 0);

      size = gtk_spin_button_new_with_range (1.0, 64.0, 1.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (size), 24.0);
      gtk_box_pack_start (GTK_BOX (hbox), size, TRUE, TRUE, 0);
      
      g_signal_connect (entry, "changed", 
			G_CALLBACK (change_cursor_theme), hbox);
      g_signal_connect (size, "changed", 
			G_CALLBACK (change_cursor_theme), hbox);
#endif

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

      label = gtk_label_new ("Cursor Value : ");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (0,
						  0, 152,
						  2,
						  10, 0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (hbox), spinner, TRUE, TRUE, 0);

      frame =
	g_object_new (gtk_frame_get_type (),
			"GtkFrame::shadow", GTK_SHADOW_ETCHED_IN,
			"GtkFrame::label_xalign", 0.5,
			"GtkFrame::label", "Cursor Area",
			"GtkContainer::border_width", 10,
			"GtkWidget::parent", vbox,
			"GtkWidget::visible", TRUE,
			NULL);

      darea = gtk_drawing_area_new ();
      gtk_widget_set_size_request (darea, 80, 80);
      gtk_container_add (GTK_CONTAINER (frame), darea);
      g_signal_connect (darea,
			"expose_event",
			G_CALLBACK (cursor_expose_event),
			NULL);
      gtk_widget_set_events (darea, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK);
      g_signal_connect (darea,
			"button_press_event",
			G_CALLBACK (cursor_event),
			spinner);
      gtk_widget_show (darea);

      g_signal_connect (spinner, "changed",
			G_CALLBACK (set_cursor),
			darea);

      label = g_object_new (GTK_TYPE_LABEL,
			      "visible", TRUE,
			      "label", "XXX",
			      "parent", vbox,
			      NULL);
      gtk_container_child_set (GTK_CONTAINER (vbox), label,
			       "expand", FALSE,
			       NULL);
      g_object_set_data (G_OBJECT (spinner), "user_data", label);

      any =
	g_object_new (gtk_hseparator_get_type (),
			"GtkWidget::visible", TRUE,
			NULL);
      gtk_box_pack_start (GTK_BOX (main_vbox), any, FALSE, TRUE, 0);
  
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
      gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("Close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);

      gtk_widget_show_all (window);

      set_cursor (spinner, darea);
    }
  else
    gtk_widget_destroy (window);
}

/*
 * GtkList
 */

static void
list_add (GtkWidget *widget,
	  GtkWidget *list)
{
  static int i = 1;
  gchar buffer[64];
  GtkWidget *list_item;
  GtkContainer *container;

  container = GTK_CONTAINER (list);

  sprintf (buffer, "added item %d", i++);
  list_item = gtk_list_item_new_with_label (buffer);
  gtk_widget_show (list_item);

  gtk_container_add (container, list_item);
}

static void
list_remove (GtkWidget *widget,
	     GtkList   *list)
{
  GList *clear_list = NULL;
  GList *sel_row = NULL;
  GList *work = NULL;

  if (list->selection_mode == GTK_SELECTION_EXTENDED)
    {
      GtkWidget *item;

      item = GTK_CONTAINER (list)->focus_child;
      if (!item && list->selection)
	item = list->selection->data;

      if (item)
	{
	  work = g_list_find (list->children, item);
	  for (sel_row = work; sel_row; sel_row = sel_row->next)
	    if (GTK_WIDGET (sel_row->data)->state != GTK_STATE_SELECTED)
	      break;

	  if (!sel_row)
	    {
	      for (sel_row = work; sel_row; sel_row = sel_row->prev)
		if (GTK_WIDGET (sel_row->data)->state != GTK_STATE_SELECTED)
		  break;
	    }
	}
    }

  for (work = list->selection; work; work = work->next)
    clear_list = g_list_prepend (clear_list, work->data);

  clear_list = g_list_reverse (clear_list);
  gtk_list_remove_items (GTK_LIST (list), clear_list);
  g_list_free (clear_list);

  if (list->selection_mode == GTK_SELECTION_EXTENDED && sel_row)
    gtk_list_select_child (list, GTK_WIDGET(sel_row->data));
}

static gchar *selection_mode_items[] =
{
  "Single",
  "Browse",
  "Multiple"
};

static const GtkSelectionMode selection_modes[] = {
  GTK_SELECTION_SINGLE,
  GTK_SELECTION_BROWSE,
  GTK_SELECTION_MULTIPLE
};

static GtkWidget *list_omenu;

static void 
list_toggle_sel_mode (GtkWidget *widget, gpointer data)
{
  GtkList *list;
  gint i;

  list = GTK_LIST (data);

  if (!gtk_widget_get_mapped (widget))
    return;

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));

  gtk_list_set_selection_mode (list, selection_modes[i]);
}

static void
create_list (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkComboBoxText *cb;
  GtkWidget *cb_entry;

  if (!window)
    {
      GtkWidget *cbox;
      GtkWidget *vbox;
      GtkWidget *hbox;
      GtkWidget *label;
      GtkWidget *scrolled_win;
      GtkWidget *list;
      GtkWidget *button;
      GtkWidget *separator;
      FILE *infile;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "list");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      scrolled_win = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_set_border_width (GTK_CONTAINER (scrolled_win), 5);
      gtk_widget_set_size_request (scrolled_win, -1, 300);
      gtk_box_pack_start (GTK_BOX (vbox), scrolled_win, TRUE, TRUE, 0);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);

      list = gtk_list_new ();
      gtk_list_set_selection_mode (GTK_LIST (list), GTK_SELECTION_SINGLE);
      gtk_scrolled_window_add_with_viewport
	(GTK_SCROLLED_WINDOW (scrolled_win), list);
      gtk_container_set_focus_vadjustment
	(GTK_CONTAINER (list),
	 gtk_scrolled_window_get_vadjustment
	 (GTK_SCROLLED_WINDOW (scrolled_win)));
      gtk_container_set_focus_hadjustment
	(GTK_CONTAINER (list),
	 gtk_scrolled_window_get_hadjustment
	 (GTK_SCROLLED_WINDOW (scrolled_win)));

      if ((infile = fopen("../gtk/gtkenums.h", "r")))
	{
	  char buffer[256];
	  char *pos;
	  GtkWidget *item;

	  while (fgets (buffer, 256, infile))
	    {
	      if ((pos = strchr (buffer, '\n')))
		*pos = 0;
	      item = gtk_list_item_new_with_label (buffer);
	      gtk_container_add (GTK_CONTAINER (list), item);
	    }
	  
	  fclose (infile);
	}


      hbox = gtk_hbox_new (TRUE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("Insert Row");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (list_add),
			list);

      cb = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new_with_entry ());

      gtk_combo_box_text_append_text (cb, "item0");
      gtk_combo_box_text_append_text (cb, "item0");
      gtk_combo_box_text_append_text (cb, "item1 item1");
      gtk_combo_box_text_append_text (cb, "item2 item2 item2");
      gtk_combo_box_text_append_text (cb, "item3 item3 item3 item3");
      gtk_combo_box_text_append_text (cb, "item4 item4 item4 item4 item4");
      gtk_combo_box_text_append_text (cb, "item5 item5 item5 item5 item5 item5");
      gtk_combo_box_text_append_text (cb, "item6 item6 item6 item6 item6");
      gtk_combo_box_text_append_text (cb, "item7 item7 item7 item7");
      gtk_combo_box_text_append_text (cb, "item8 item8 item8");
      gtk_combo_box_text_append_text (cb, "item9 item9");

      cb_entry = gtk_bin_get_child (GTK_BIN (cb));
      gtk_entry_set_text (GTK_ENTRY (cb_entry), "hello world \n\n\n foo");
      gtk_editable_select_region (GTK_EDITABLE (cb_entry), 0, -1);
      gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (cb), TRUE, TRUE, 0);

      button = gtk_button_new_with_label ("Remove Selection");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (list_remove),
			list);

      cbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), cbox, FALSE, TRUE, 0);

      hbox = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (cbox), hbox, TRUE, FALSE, 0);

      label = gtk_label_new ("Selection Mode :");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

      list_omenu = build_option_menu (selection_mode_items, 3, 3, 
				      list_toggle_sel_mode,
				      list);
      gtk_box_pack_start (GTK_BOX (hbox), list_omenu, FALSE, TRUE, 0);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 0);

      cbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), cbox, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      gtk_container_set_border_width (GTK_CONTAINER (button), 10);
      gtk_box_pack_start (GTK_BOX (cbox), button, TRUE, TRUE, 0);
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);

      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkCList
 */

static char * book_open_xpm[] = {
"16 16 4 1",
"       c None s None",
".      c black",
"X      c #808080",
"o      c white",
"                ",
"  ..            ",
" .Xo.    ...    ",
" .Xoo. ..oo.    ",
" .Xooo.Xooo...  ",
" .Xooo.oooo.X.  ",
" .Xooo.Xooo.X.  ",
" .Xooo.oooo.X.  ",
" .Xooo.Xooo.X.  ",
" .Xooo.oooo.X.  ",
"  .Xoo.Xoo..X.  ",
"   .Xo.o..ooX.  ",
"    .X..XXXXX.  ",
"    ..X.......  ",
"     ..         ",
"                "};

static char * book_closed_xpm[] = {
"16 16 6 1",
"       c None s None",
".      c black",
"X      c red",
"o      c yellow",
"O      c #808080",
"#      c white",
"                ",
"       ..       ",
"     ..XX.      ",
"   ..XXXXX.     ",
" ..XXXXXXXX.    ",
".ooXXXXXXXXX.   ",
"..ooXXXXXXXXX.  ",
".X.ooXXXXXXXXX. ",
".XX.ooXXXXXX..  ",
" .XX.ooXXX..#O  ",
"  .XX.oo..##OO. ",
"   .XX..##OO..  ",
"    .X.#OO..    ",
"     ..O..      ",
"      ..        ",
"                "};

static char * mini_page_xpm[] = {
"16 16 4 1",
"       c None s None",
".      c black",
"X      c white",
"o      c #808080",
"                ",
"   .......      ",
"   .XXXXX..     ",
"   .XoooX.X.    ",
"   .XXXXX....   ",
"   .XooooXoo.o  ",
"   .XXXXXXXX.o  ",
"   .XooooooX.o  ",
"   .XXXXXXXX.o  ",
"   .XooooooX.o  ",
"   .XXXXXXXX.o  ",
"   .XooooooX.o  ",
"   .XXXXXXXX.o  ",
"   ..........o  ",
"    oooooooooo  ",
"                "};

static char * gtk_mini_xpm[] = {
"15 20 17 1",
"       c None",
".      c #14121F",
"+      c #278828",
"@      c #9B3334",
"#      c #284C72",
"$      c #24692A",
"%      c #69282E",
"&      c #37C539",
"*      c #1D2F4D",
"=      c #6D7076",
"-      c #7D8482",
";      c #E24A49",
">      c #515357",
",      c #9B9C9B",
"'      c #2FA232",
")      c #3CE23D",
"!      c #3B6CCB",
"               ",
"      ***>     ",
"    >.*!!!*    ",
"   ***....#*=  ",
"  *!*.!!!**!!# ",
" .!!#*!#*!!!!# ",
" @%#!.##.*!!$& ",
" @;%*!*.#!#')) ",
" @;;@%!!*$&)'' ",
" @%.%@%$'&)$+' ",
" @;...@$'*'*)+ ",
" @;%..@$+*.')$ ",
" @;%%;;$+..$)# ",
" @;%%;@$$$'.$# ",
" %;@@;;$$+))&* ",
"  %;;;@+$&)&*  ",
"   %;;@'))+>   ",
"    %;@'&#     ",
"     >%$$      ",
"      >=       "};

#define TESTGTK_CLIST_COLUMNS 12
static gint clist_rows = 0;
static GtkWidget *clist_omenu;

static void
add1000_clist (GtkWidget *widget, gpointer data)
{
  gint i, row;
  char text[TESTGTK_CLIST_COLUMNS][50];
  char *texts[TESTGTK_CLIST_COLUMNS];
  GdkBitmap *mask;
  GdkPixmap *pixmap;
  GtkCList  *clist;

  clist = GTK_CLIST (data);

  pixmap = gdk_pixmap_create_from_xpm_d (clist->clist_window,
					 &mask, 
					 &GTK_WIDGET (data)->style->white,
					 gtk_mini_xpm);

  for (i = 0; i < TESTGTK_CLIST_COLUMNS; i++)
    {
      texts[i] = text[i];
      sprintf (text[i], "Column %d", i);
    }
  
  texts[3] = NULL;
  sprintf (text[1], "Right");
  sprintf (text[2], "Center");
  
  gtk_clist_freeze (GTK_CLIST (data));
  for (i = 0; i < 1000; i++)
    {
      sprintf (text[0], "CListRow %d", rand() % 10000);
      row = gtk_clist_append (clist, texts);
      gtk_clist_set_pixtext (clist, row, 3, "gtk+", 5, pixmap, mask);
    }

  gtk_clist_thaw (GTK_CLIST (data));

  g_object_unref (pixmap);
  g_object_unref (mask);
}

static void
add10000_clist (GtkWidget *widget, gpointer data)
{
  gint i;
  char text[TESTGTK_CLIST_COLUMNS][50];
  char *texts[TESTGTK_CLIST_COLUMNS];

  for (i = 0; i < TESTGTK_CLIST_COLUMNS; i++)
    {
      texts[i] = text[i];
      sprintf (text[i], "Column %d", i);
    }
  
  sprintf (text[1], "Right");
  sprintf (text[2], "Center");
  
  gtk_clist_freeze (GTK_CLIST (data));
  for (i = 0; i < 10000; i++)
    {
      sprintf (text[0], "CListRow %d", rand() % 10000);
      gtk_clist_append (GTK_CLIST (data), texts);
    }
  gtk_clist_thaw (GTK_CLIST (data));
}

void
clear_clist (GtkWidget *widget, gpointer data)
{
  gtk_clist_clear (GTK_CLIST (data));
  clist_rows = 0;
}

void clist_remove_selection (GtkWidget *widget, GtkCList *clist)
{
  gtk_clist_freeze (clist);

  while (clist->selection)
    {
      gint row;

      clist_rows--;
      row = GPOINTER_TO_INT (clist->selection->data);

      gtk_clist_remove (clist, row);

      if (clist->selection_mode == GTK_SELECTION_BROWSE)
	break;
    }

  if (clist->selection_mode == GTK_SELECTION_EXTENDED && !clist->selection &&
      clist->focus_row >= 0)
    gtk_clist_select_row (clist, clist->focus_row, -1);

  gtk_clist_thaw (clist);
}

void toggle_title_buttons (GtkWidget *widget, GtkCList *clist)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
    gtk_clist_column_titles_show (clist);
  else
    gtk_clist_column_titles_hide (clist);
}

void toggle_reorderable (GtkWidget *widget, GtkCList *clist)
{
  gtk_clist_set_reorderable (clist, GTK_TOGGLE_BUTTON (widget)->active);
}

static void
insert_row_clist (GtkWidget *widget, gpointer data)
{
  static char *text[] =
  {
    "This", "is an", "inserted", "row.",
    "This", "is an", "inserted", "row.",
    "This", "is an", "inserted", "row."
  };

  static GtkStyle *style1 = NULL;
  static GtkStyle *style2 = NULL;
  static GtkStyle *style3 = NULL;
  gint row;
  
  if (GTK_CLIST (data)->focus_row >= 0)
    row = gtk_clist_insert (GTK_CLIST (data), GTK_CLIST (data)->focus_row,
			    text);
  else
    row = gtk_clist_prepend (GTK_CLIST (data), text);

  if (!style1)
    {
      GdkColor col1 = { 0, 0, 56000, 0};
      GdkColor col2 = { 0, 32000, 0, 56000};

      style1 = gtk_style_copy (GTK_WIDGET (data)->style);
      style1->base[GTK_STATE_NORMAL] = col1;
      style1->base[GTK_STATE_SELECTED] = col2;

      style2 = gtk_style_copy (GTK_WIDGET (data)->style);
      style2->fg[GTK_STATE_NORMAL] = col1;
      style2->fg[GTK_STATE_SELECTED] = col2;

      style3 = gtk_style_copy (GTK_WIDGET (data)->style);
      style3->fg[GTK_STATE_NORMAL] = col1;
      style3->base[GTK_STATE_NORMAL] = col2;
      pango_font_description_free (style3->font_desc);
      style3->font_desc = pango_font_description_from_string ("courier 12");
    }

  gtk_clist_set_cell_style (GTK_CLIST (data), row, 3, style1);
  gtk_clist_set_cell_style (GTK_CLIST (data), row, 4, style2);
  gtk_clist_set_cell_style (GTK_CLIST (data), row, 0, style3);

  clist_rows++;
}

static void
clist_warning_test (GtkWidget *button,
		    GtkWidget *clist)
{
  GtkWidget *child;
  static gboolean add_remove = FALSE;

  add_remove = !add_remove;

  child = gtk_label_new ("Test");
  g_object_ref (child);
  gtk_object_sink (GTK_OBJECT (child));

  if (add_remove)
    gtk_container_add (GTK_CONTAINER (clist), child);
  else
    {
      child->parent = clist;
      gtk_container_remove (GTK_CONTAINER (clist), child);
      child->parent = NULL;
    }

  gtk_widget_destroy (child);
  g_object_unref (child);
}

static void
undo_selection (GtkWidget *button, GtkCList *clist)
{
  gtk_clist_undo_selection (clist);
}

static void 
clist_toggle_sel_mode (GtkWidget *widget, gpointer data)
{
  GtkCList *clist;
  gint i;

  clist = GTK_CLIST (data);

  if (!gtk_widget_get_mapped (widget))
    return;

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));

  gtk_clist_set_selection_mode (clist, selection_modes[i]);
}

static void 
clist_click_column (GtkCList *clist, gint column, gpointer data)
{
  if (column == 4)
    gtk_clist_set_column_visibility (clist, column, FALSE);
  else if (column == clist->sort_column)
    {
      if (clist->sort_type == GTK_SORT_ASCENDING)
	clist->sort_type = GTK_SORT_DESCENDING;
      else
	clist->sort_type = GTK_SORT_ASCENDING;
    }
  else
    gtk_clist_set_sort_column (clist, column);

  gtk_clist_sort (clist);
}

static void
create_clist (GtkWidget *widget)
{
  gint i;
  static GtkWidget *window = NULL;

  static char *titles[] =
  {
    "auto resize", "not resizeable", "max width 100", "min width 50",
    "hide column", "Title 5", "Title 6", "Title 7",
    "Title 8",  "Title 9",  "Title 10", "Title 11"
  };

  char text[TESTGTK_CLIST_COLUMNS][50];
  char *texts[TESTGTK_CLIST_COLUMNS];

  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *clist;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *scrolled_win;
  GtkWidget *check;

  GtkWidget *undo_button;
  GtkWidget *label;

  GtkStyle *style;
  GdkColor red_col = { 0, 56000, 0, 0};
  GdkColor light_green_col = { 0, 0, 56000, 32000};

  if (!window)
    {
      clist_rows = 0;
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window), 
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_window_set_title (GTK_WINDOW (window), "clist");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      scrolled_win = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_set_border_width (GTK_CONTAINER (scrolled_win), 5);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				      GTK_POLICY_AUTOMATIC, 
				      GTK_POLICY_AUTOMATIC);

      /* create GtkCList here so we have a pointer to throw at the 
       * button callbacks -- more is done with it later */
      clist = gtk_clist_new_with_titles (TESTGTK_CLIST_COLUMNS, titles);
      gtk_container_add (GTK_CONTAINER (scrolled_win), clist);
      g_signal_connect (clist, "click_column",
			G_CALLBACK (clist_click_column), NULL);

      /* control buttons */
      hbox = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Insert Row");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (insert_row_clist), clist);

      button = gtk_button_new_with_label ("Add 1,000 Rows With Pixmaps");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (add1000_clist), clist);

      button = gtk_button_new_with_label ("Add 10,000 Rows");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (add10000_clist), clist);

      /* second layer of buttons */
      hbox = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Clear List");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (clear_clist), clist);

      button = gtk_button_new_with_label ("Remove Selection");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (clist_remove_selection), clist);

      undo_button = gtk_button_new_with_label ("Undo Selection");
      gtk_box_pack_start (GTK_BOX (hbox), undo_button, TRUE, TRUE, 0);
      g_signal_connect (undo_button, "clicked",
			G_CALLBACK (undo_selection), clist);

      button = gtk_button_new_with_label ("Warning Test");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (clist_warning_test), clist);

      /* third layer of buttons */
      hbox = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      check = gtk_check_button_new_with_label ("Show Title Buttons");
      gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, TRUE, 0);
      g_signal_connect (check, "clicked",
			G_CALLBACK (toggle_title_buttons), clist);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);

      check = gtk_check_button_new_with_label ("Reorderable");
      gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, TRUE, 0);
      g_signal_connect (check, "clicked",
			G_CALLBACK (toggle_reorderable), clist);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);

      label = gtk_label_new ("Selection Mode :");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

      clist_omenu = build_option_menu (selection_mode_items, 3, 3, 
				       clist_toggle_sel_mode,
				       clist);
      gtk_box_pack_start (GTK_BOX (hbox), clist_omenu, FALSE, TRUE, 0);

      /* 
       * the rest of the clist configuration
       */

      gtk_box_pack_start (GTK_BOX (vbox), scrolled_win, TRUE, TRUE, 0);
      gtk_clist_set_row_height (GTK_CLIST (clist), 18);
      gtk_widget_set_size_request (clist, -1, 300);

      for (i = 1; i < TESTGTK_CLIST_COLUMNS; i++)
	gtk_clist_set_column_width (GTK_CLIST (clist), i, 80);

      gtk_clist_set_column_auto_resize (GTK_CLIST (clist), 0, TRUE);
      gtk_clist_set_column_resizeable (GTK_CLIST (clist), 1, FALSE);
      gtk_clist_set_column_max_width (GTK_CLIST (clist), 2, 100);
      gtk_clist_set_column_min_width (GTK_CLIST (clist), 3, 50);
      gtk_clist_set_selection_mode (GTK_CLIST (clist), GTK_SELECTION_EXTENDED);
      gtk_clist_set_column_justification (GTK_CLIST (clist), 1,
					  GTK_JUSTIFY_RIGHT);
      gtk_clist_set_column_justification (GTK_CLIST (clist), 2,
					  GTK_JUSTIFY_CENTER);
      
      for (i = 0; i < TESTGTK_CLIST_COLUMNS; i++)
	{
	  texts[i] = text[i];
	  sprintf (text[i], "Column %d", i);
	}

      sprintf (text[1], "Right");
      sprintf (text[2], "Center");

      style = gtk_style_new ();
      style->fg[GTK_STATE_NORMAL] = red_col;
      style->base[GTK_STATE_NORMAL] = light_green_col;

      pango_font_description_set_size (style->font_desc, 14 * PANGO_SCALE);
      pango_font_description_set_weight (style->font_desc, PANGO_WEIGHT_BOLD);

      for (i = 0; i < 10; i++)
	{
	  sprintf (text[0], "CListRow %d", clist_rows++);
	  gtk_clist_append (GTK_CLIST (clist), texts);

	  switch (i % 4)
	    {
	    case 2:
	      gtk_clist_set_row_style (GTK_CLIST (clist), i, style);
	      break;
	    default:
	      gtk_clist_set_cell_style (GTK_CLIST (clist), i, i % 4, style);
	      break;
	    }
	}

      g_object_unref (style);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 0);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      gtk_container_set_border_width (GTK_CONTAINER (button), 10);
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);

      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    {
      clist_rows = 0;
      gtk_widget_destroy (window);
    }
}

/*
 * GtkCTree
 */

typedef struct 
{
  GdkPixmap *pixmap1;
  GdkPixmap *pixmap2;
  GdkPixmap *pixmap3;
  GdkBitmap *mask1;
  GdkBitmap *mask2;
  GdkBitmap *mask3;
} CTreePixmaps;

static gint books = 0;
static gint pages = 0;

static GtkWidget *book_label;
static GtkWidget *page_label;
static GtkWidget *sel_label;
static GtkWidget *vis_label;
static GtkWidget *omenu1;
static GtkWidget *omenu2;
static GtkWidget *omenu3;
static GtkWidget *omenu4;
static GtkWidget *spin1;
static GtkWidget *spin2;
static GtkWidget *spin3;
static gint line_style;


static CTreePixmaps *
get_ctree_pixmaps (GtkCTree *ctree)
{
  GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (ctree));
  CTreePixmaps *pixmaps = g_object_get_data (G_OBJECT (screen), "ctree-pixmaps");

  if (!pixmaps)
    {
      GdkColormap *colormap = gdk_screen_get_rgb_colormap (screen);
      pixmaps = g_new (CTreePixmaps, 1);
      
      pixmaps->pixmap1 = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap,
								&pixmaps->mask1, 
								NULL, book_closed_xpm);
      pixmaps->pixmap2 = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap,
								&pixmaps->mask2, 
								NULL, book_open_xpm);
      pixmaps->pixmap3 = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap,
								&pixmaps->mask3,
								NULL, mini_page_xpm);
      
      g_object_set_data (G_OBJECT (screen), "ctree-pixmaps", pixmaps);
    }

  return pixmaps;
}

void after_press (GtkCTree *ctree, gpointer data)
{
  char buf[80];

  sprintf (buf, "%d", g_list_length (GTK_CLIST (ctree)->selection));
  gtk_label_set_text (GTK_LABEL (sel_label), buf);

  sprintf (buf, "%d", g_list_length (GTK_CLIST (ctree)->row_list));
  gtk_label_set_text (GTK_LABEL (vis_label), buf);

  sprintf (buf, "%d", books);
  gtk_label_set_text (GTK_LABEL (book_label), buf);

  sprintf (buf, "%d", pages);
  gtk_label_set_text (GTK_LABEL (page_label), buf);
}

void after_move (GtkCTree *ctree, GtkCTreeNode *child, GtkCTreeNode *parent, 
		 GtkCTreeNode *sibling, gpointer data)
{
  char *source;
  char *target1;
  char *target2;

  gtk_ctree_get_node_info (ctree, child, &source, 
			   NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  if (parent)
    gtk_ctree_get_node_info (ctree, parent, &target1, 
			     NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  if (sibling)
    gtk_ctree_get_node_info (ctree, sibling, &target2, 
			     NULL, NULL, NULL, NULL, NULL, NULL, NULL);

  g_print ("Moving \"%s\" to \"%s\" with sibling \"%s\".\n", source,
	   (parent) ? target1 : "nil", (sibling) ? target2 : "nil");
}

void count_items (GtkCTree *ctree, GtkCTreeNode *list)
{
  if (GTK_CTREE_ROW (list)->is_leaf)
    pages--;
  else
    books--;
}

void expand_all (GtkWidget *widget, GtkCTree *ctree)
{
  gtk_ctree_expand_recursive (ctree, NULL);
  after_press (ctree, NULL);
}

void collapse_all (GtkWidget *widget, GtkCTree *ctree)
{
  gtk_ctree_collapse_recursive (ctree, NULL);
  after_press (ctree, NULL);
}

void select_all (GtkWidget *widget, GtkCTree *ctree)
{
  gtk_ctree_select_recursive (ctree, NULL);
  after_press (ctree, NULL);
}

void change_style (GtkWidget *widget, GtkCTree *ctree)
{
  static GtkStyle *style1 = NULL;
  static GtkStyle *style2 = NULL;

  GtkCTreeNode *node;
  GdkColor green_col = { 0, 0, 56000, 0};
  GdkColor purple_col = { 0, 32000, 0, 56000};

  if (GTK_CLIST (ctree)->focus_row >= 0)
    node = GTK_CTREE_NODE
      (g_list_nth (GTK_CLIST (ctree)->row_list,GTK_CLIST (ctree)->focus_row));
  else
    node = GTK_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  if (!node)
    return;

  if (!style1)
    {
      style1 = gtk_style_new ();
      style1->base[GTK_STATE_NORMAL] = green_col;
      style1->fg[GTK_STATE_SELECTED] = purple_col;

      style2 = gtk_style_new ();
      style2->base[GTK_STATE_SELECTED] = purple_col;
      style2->fg[GTK_STATE_NORMAL] = green_col;
      style2->base[GTK_STATE_NORMAL] = purple_col;
      pango_font_description_free (style2->font_desc);
      style2->font_desc = pango_font_description_from_string ("courier 30");
    }

  gtk_ctree_node_set_cell_style (ctree, node, 1, style1);
  gtk_ctree_node_set_cell_style (ctree, node, 0, style2);

  if (GTK_CTREE_ROW (node)->children)
    gtk_ctree_node_set_row_style (ctree, GTK_CTREE_ROW (node)->children,
				  style2);
}

void unselect_all (GtkWidget *widget, GtkCTree *ctree)
{
  gtk_ctree_unselect_recursive (ctree, NULL);
  after_press (ctree, NULL);
}

void remove_selection (GtkWidget *widget, GtkCTree *ctree)
{
  GtkCList *clist;
  GtkCTreeNode *node;

  clist = GTK_CLIST (ctree);

  gtk_clist_freeze (clist);

  while (clist->selection)
    {
      node = clist->selection->data;

      if (GTK_CTREE_ROW (node)->is_leaf)
	pages--;
      else
	gtk_ctree_post_recursive (ctree, node,
				  (GtkCTreeFunc) count_items, NULL);

      gtk_ctree_remove_node (ctree, node);

      if (clist->selection_mode == GTK_SELECTION_BROWSE)
	break;
    }

  if (clist->selection_mode == GTK_SELECTION_EXTENDED && !clist->selection &&
      clist->focus_row >= 0)
    {
      node = gtk_ctree_node_nth (ctree, clist->focus_row);

      if (node)
	gtk_ctree_select (ctree, node);
    }
    
  gtk_clist_thaw (clist);
  after_press (ctree, NULL);
}

struct _ExportStruct {
  gchar *tree;
  gchar *info;
  gboolean is_leaf;
};

typedef struct _ExportStruct ExportStruct;

gboolean
gnode2ctree (GtkCTree   *ctree,
	     guint       depth,
	     GNode        *gnode,
	     GtkCTreeNode *cnode,
	     gpointer    data)
{
  ExportStruct *es;
  GdkPixmap *pixmap_closed;
  GdkBitmap *mask_closed;
  GdkPixmap *pixmap_opened;
  GdkBitmap *mask_opened;
  CTreePixmaps *pixmaps;

  if (!cnode || !gnode || (!(es = gnode->data)))
    return FALSE;

  pixmaps = get_ctree_pixmaps (ctree);

  if (es->is_leaf)
    {
      pixmap_closed = pixmaps->pixmap3;
      mask_closed = pixmaps->mask3;
      pixmap_opened = NULL;
      mask_opened = NULL;
    }
  else
    {
      pixmap_closed = pixmaps->pixmap1;
      mask_closed = pixmaps->mask1;
      pixmap_opened = pixmaps->pixmap2;
      mask_opened = pixmaps->mask2;
    }

  gtk_ctree_set_node_info (ctree, cnode, es->tree, 2, pixmap_closed,
			   mask_closed, pixmap_opened, mask_opened,
			   es->is_leaf, (depth < 3));
  gtk_ctree_node_set_text (ctree, cnode, 1, es->info);
  g_free (es);
  gnode->data = NULL;

  return TRUE;
}

gboolean
ctree2gnode (GtkCTree   *ctree,
	     guint       depth,
	     GNode        *gnode,
	     GtkCTreeNode *cnode,
	     gpointer    data)
{
  ExportStruct *es;

  if (!cnode || !gnode)
    return FALSE;
  
  es = g_new (ExportStruct, 1);
  gnode->data = es;
  es->is_leaf = GTK_CTREE_ROW (cnode)->is_leaf;
  es->tree = GTK_CELL_PIXTEXT (GTK_CTREE_ROW (cnode)->row.cell[0])->text;
  es->info = GTK_CELL_PIXTEXT (GTK_CTREE_ROW (cnode)->row.cell[1])->text;
  return TRUE;
}

void export_ctree (GtkWidget *widget, GtkCTree *ctree)
{
  char *title[] = { "Tree" , "Info" };
  static GtkWidget *export_window = NULL;
  static GtkCTree *export_ctree;
  GtkWidget *vbox;
  GtkWidget *scrolled_win;
  GtkWidget *button;
  GtkWidget *sep;
  GNode *gnode;
  GtkCTreeNode *node;

  if (!export_window)
    {
      export_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (export_window),
			     gtk_widget_get_screen (widget));
  
      g_signal_connect (export_window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&export_window);

      gtk_window_set_title (GTK_WINDOW (export_window), "exported ctree");
      gtk_container_set_border_width (GTK_CONTAINER (export_window), 5);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (export_window), vbox);
      
      button = gtk_button_new_with_label ("Close");
      gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, TRUE, 0);

      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				export_window);

      sep = gtk_hseparator_new ();
      gtk_box_pack_end (GTK_BOX (vbox), sep, FALSE, TRUE, 10);

      export_ctree = GTK_CTREE (gtk_ctree_new_with_titles (2, 0, title));
      gtk_ctree_set_line_style (export_ctree, GTK_CTREE_LINES_DOTTED);

      scrolled_win = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_add (GTK_CONTAINER (scrolled_win),
			 GTK_WIDGET (export_ctree));
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (vbox), scrolled_win, TRUE, TRUE, 0);
      gtk_clist_set_selection_mode (GTK_CLIST (export_ctree),
				    GTK_SELECTION_EXTENDED);
      gtk_clist_set_column_width (GTK_CLIST (export_ctree), 0, 200);
      gtk_clist_set_column_width (GTK_CLIST (export_ctree), 1, 200);
      gtk_widget_set_size_request (GTK_WIDGET (export_ctree), 300, 200);
    }

  if (!gtk_widget_get_visible (export_window))
    gtk_widget_show_all (export_window);
      
  gtk_clist_clear (GTK_CLIST (export_ctree));

  node = GTK_CTREE_NODE (g_list_nth (GTK_CLIST (ctree)->row_list,
				     GTK_CLIST (ctree)->focus_row));
  if (!node)
    return;

  gnode = gtk_ctree_export_to_gnode (ctree, NULL, NULL, node,
				     ctree2gnode, NULL);
  if (gnode)
    {
      gtk_ctree_insert_gnode (export_ctree, NULL, NULL, gnode,
			      gnode2ctree, NULL);
      g_node_destroy (gnode);
    }
}

void change_indent (GtkWidget *widget, GtkCTree *ctree)
{
  gtk_ctree_set_indent (ctree, GTK_ADJUSTMENT (widget)->value);
}

void change_spacing (GtkWidget *widget, GtkCTree *ctree)
{
  gtk_ctree_set_spacing (ctree, GTK_ADJUSTMENT (widget)->value);
}

void change_row_height (GtkWidget *widget, GtkCList *clist)
{
  gtk_clist_set_row_height (clist, GTK_ADJUSTMENT (widget)->value);
}

void set_background (GtkCTree *ctree, GtkCTreeNode *node, gpointer data)
{
  GtkStyle *style = NULL;
  
  if (!node)
    return;
  
  if (ctree->line_style != GTK_CTREE_LINES_TABBED)
    {
      if (!GTK_CTREE_ROW (node)->is_leaf)
	style = GTK_CTREE_ROW (node)->row.data;
      else if (GTK_CTREE_ROW (node)->parent)
	style = GTK_CTREE_ROW (GTK_CTREE_ROW (node)->parent)->row.data;
    }

  gtk_ctree_node_set_row_style (ctree, node, style);
}

void 
ctree_toggle_line_style (GtkWidget *widget, gpointer data)
{
  GtkCTree *ctree;
  gint i;

  ctree = GTK_CTREE (data);

  if (!gtk_widget_get_mapped (widget))
    return;

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));

  if ((ctree->line_style == GTK_CTREE_LINES_TABBED && 
       ((GtkCTreeLineStyle) i) != GTK_CTREE_LINES_TABBED) ||
      (ctree->line_style != GTK_CTREE_LINES_TABBED && 
       ((GtkCTreeLineStyle) i) == GTK_CTREE_LINES_TABBED))
    gtk_ctree_pre_recursive (ctree, NULL, set_background, NULL);
  gtk_ctree_set_line_style (ctree, i);
  line_style = i;
}

void 
ctree_toggle_expander_style (GtkWidget *widget, gpointer data)
{
  GtkCTree *ctree;
  gint i;

  ctree = GTK_CTREE (data);

  if (!gtk_widget_get_mapped (widget))
    return;
  
  i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));
  
  gtk_ctree_set_expander_style (ctree, (GtkCTreeExpanderStyle) i);
}

void 
ctree_toggle_justify (GtkWidget *widget, gpointer data)
{
  GtkCTree *ctree;
  gint i;

  ctree = GTK_CTREE (data);

  if (!gtk_widget_get_mapped (widget))
    return;

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));

  gtk_clist_set_column_justification (GTK_CLIST (ctree), ctree->tree_column, 
				      (GtkJustification) i);
}

void 
ctree_toggle_sel_mode (GtkWidget *widget, gpointer data)
{
  GtkCTree *ctree;
  gint i;

  ctree = GTK_CTREE (data);

  if (!gtk_widget_get_mapped (widget))
    return;

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));

  gtk_clist_set_selection_mode (GTK_CLIST (ctree), selection_modes[i]);
  after_press (ctree, NULL);
}
    
void build_recursive (GtkCTree *ctree, gint cur_depth, gint depth, 
		      gint num_books, gint num_pages, GtkCTreeNode *parent)
{
  gchar *text[2];
  gchar buf1[60];
  gchar buf2[60];
  GtkCTreeNode *sibling;
  CTreePixmaps *pixmaps;
  gint i;

  text[0] = buf1;
  text[1] = buf2;
  sibling = NULL;

  pixmaps = get_ctree_pixmaps (ctree);

  for (i = num_pages + num_books; i > num_books; i--)
    {
      pages++;
      sprintf (buf1, "Page %02d", (gint) rand() % 100);
      sprintf (buf2, "Item %d-%d", cur_depth, i);
      sibling = gtk_ctree_insert_node (ctree, parent, sibling, text, 5,
				       pixmaps->pixmap3, pixmaps->mask3, NULL, NULL,
				       TRUE, FALSE);

      if (parent && ctree->line_style == GTK_CTREE_LINES_TABBED)
	gtk_ctree_node_set_row_style (ctree, sibling,
				      GTK_CTREE_ROW (parent)->row.style);
    }

  if (cur_depth == depth)
    return;

  for (i = num_books; i > 0; i--)
    {
      GtkStyle *style;

      books++;
      sprintf (buf1, "Book %02d", (gint) rand() % 100);
      sprintf (buf2, "Item %d-%d", cur_depth, i);
      sibling = gtk_ctree_insert_node (ctree, parent, sibling, text, 5,
				       pixmaps->pixmap1, pixmaps->mask1, pixmaps->pixmap2, pixmaps->mask2,
				       FALSE, FALSE);

      style = gtk_style_new ();
      switch (cur_depth % 3)
	{
	case 0:
	  style->base[GTK_STATE_NORMAL].red   = 10000 * (cur_depth % 6);
	  style->base[GTK_STATE_NORMAL].green = 0;
	  style->base[GTK_STATE_NORMAL].blue  = 65535 - ((i * 10000) % 65535);
	  break;
	case 1:
	  style->base[GTK_STATE_NORMAL].red   = 10000 * (cur_depth % 6);
	  style->base[GTK_STATE_NORMAL].green = 65535 - ((i * 10000) % 65535);
	  style->base[GTK_STATE_NORMAL].blue  = 0;
	  break;
	default:
	  style->base[GTK_STATE_NORMAL].red   = 65535 - ((i * 10000) % 65535);
	  style->base[GTK_STATE_NORMAL].green = 0;
	  style->base[GTK_STATE_NORMAL].blue  = 10000 * (cur_depth % 6);
	  break;
	}
      gtk_ctree_node_set_row_data_full (ctree, sibling, style,
					(GDestroyNotify) g_object_unref);

      if (ctree->line_style == GTK_CTREE_LINES_TABBED)
	gtk_ctree_node_set_row_style (ctree, sibling, style);

      build_recursive (ctree, cur_depth + 1, depth, num_books, num_pages,
		       sibling);
    }
}

void rebuild_tree (GtkWidget *widget, GtkCTree *ctree)
{
  gchar *text [2];
  gchar label1[] = "Root";
  gchar label2[] = "";
  GtkCTreeNode *parent;
  GtkStyle *style;
  guint b, d, p, n;
  CTreePixmaps *pixmaps;

  pixmaps = get_ctree_pixmaps (ctree);

  text[0] = label1;
  text[1] = label2;
  
  d = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin1)); 
  b = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin2));
  p = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin3));

  n = ((pow (b, d) - 1) / (b - 1)) * (p + 1);

  if (n > 100000)
    {
      g_print ("%d total items? Try less\n",n);
      return;
    }

  gtk_clist_freeze (GTK_CLIST (ctree));
  gtk_clist_clear (GTK_CLIST (ctree));

  books = 1;
  pages = 0;

  parent = gtk_ctree_insert_node (ctree, NULL, NULL, text, 5, pixmaps->pixmap1,
				  pixmaps->mask1, pixmaps->pixmap2, pixmaps->mask2, FALSE, TRUE);

  style = gtk_style_new ();
  style->base[GTK_STATE_NORMAL].red   = 0;
  style->base[GTK_STATE_NORMAL].green = 45000;
  style->base[GTK_STATE_NORMAL].blue  = 55000;
  gtk_ctree_node_set_row_data_full (ctree, parent, style,
				    (GDestroyNotify) g_object_unref);

  if (ctree->line_style == GTK_CTREE_LINES_TABBED)
    gtk_ctree_node_set_row_style (ctree, parent, style);

  build_recursive (ctree, 1, d, b, p, parent);
  gtk_clist_thaw (GTK_CLIST (ctree));
  after_press (ctree, NULL);
}

static void 
ctree_click_column (GtkCTree *ctree, gint column, gpointer data)
{
  GtkCList *clist;

  clist = GTK_CLIST (ctree);

  if (column == clist->sort_column)
    {
      if (clist->sort_type == GTK_SORT_ASCENDING)
	clist->sort_type = GTK_SORT_DESCENDING;
      else
	clist->sort_type = GTK_SORT_ASCENDING;
    }
  else
    gtk_clist_set_sort_column (clist, column);

  gtk_ctree_sort_recursive (ctree, NULL);
}

void create_ctree (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkTooltips *tooltips;
  GtkCTree *ctree;
  GtkWidget *scrolled_win;
  GtkWidget *vbox;
  GtkWidget *bbox;
  GtkWidget *mbox;
  GtkWidget *hbox;
  GtkWidget *hbox2;
  GtkWidget *frame;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *check;
  GtkAdjustment *adj;
  GtkWidget *spinner;

  char *title[] = { "Tree" , "Info" };
  char buf[80];

  static gchar *items1[] =
  {
    "No lines",
    "Solid",
    "Dotted",
    "Tabbed"
  };

  static gchar *items2[] =
  {
    "None",
    "Square",
    "Triangle",
    "Circular"
  };

  static gchar *items3[] =
  {
    "Left",
    "Right"
  };
  
  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window), 
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkCTree");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      tooltips = gtk_tooltips_new ();
      g_object_ref (tooltips);
      gtk_object_sink (GTK_OBJECT (tooltips));

      g_object_set_data_full (G_OBJECT (window), "tooltips", tooltips,
			      g_object_unref);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      hbox = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
      
      label = gtk_label_new ("Depth :");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (4, 1, 10, 1, 5, 0);
      spin1 = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (hbox), spin1, FALSE, TRUE, 5);
  
      label = gtk_label_new ("Books :");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (3, 1, 20, 1, 5, 0);
      spin2 = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (hbox), spin2, FALSE, TRUE, 5);

      label = gtk_label_new ("Pages :");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (5, 1, 20, 1, 5, 0);
      spin3 = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (hbox), spin3, FALSE, TRUE, 5);

      button = gtk_button_new_with_label ("Close");
      gtk_box_pack_end (GTK_BOX (hbox), button, TRUE, TRUE, 0);

      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);

      button = gtk_button_new_with_label ("Rebuild Tree");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

      scrolled_win = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_set_border_width (GTK_CONTAINER (scrolled_win), 5);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_ALWAYS);
      gtk_box_pack_start (GTK_BOX (vbox), scrolled_win, TRUE, TRUE, 0);

      ctree = GTK_CTREE (gtk_ctree_new_with_titles (2, 0, title));
      gtk_container_add (GTK_CONTAINER (scrolled_win), GTK_WIDGET (ctree));

      gtk_clist_set_column_auto_resize (GTK_CLIST (ctree), 0, TRUE);
      gtk_clist_set_column_width (GTK_CLIST (ctree), 1, 200);
      gtk_clist_set_selection_mode (GTK_CLIST (ctree), GTK_SELECTION_EXTENDED);
      gtk_ctree_set_line_style (ctree, GTK_CTREE_LINES_DOTTED);
      line_style = GTK_CTREE_LINES_DOTTED;

      g_signal_connect (button, "clicked",
			G_CALLBACK (rebuild_tree), ctree);
      g_signal_connect (ctree, "click_column",
			G_CALLBACK (ctree_click_column), NULL);

      g_signal_connect_after (ctree, "button_press_event",
			      G_CALLBACK (after_press), NULL);
      g_signal_connect_after (ctree, "button_release_event",
			      G_CALLBACK (after_press), NULL);
      g_signal_connect_after (ctree, "tree_move",
			      G_CALLBACK (after_move), NULL);
      g_signal_connect_after (ctree, "end_selection",
			      G_CALLBACK (after_press), NULL);
      g_signal_connect_after (ctree, "toggle_focus_row",
			      G_CALLBACK (after_press), NULL);
      g_signal_connect_after (ctree, "select_all",
			      G_CALLBACK (after_press), NULL);
      g_signal_connect_after (ctree, "unselect_all",
			      G_CALLBACK (after_press), NULL);
      g_signal_connect_after (ctree, "scroll_vertical",
			      G_CALLBACK (after_press), NULL);

      bbox = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (bbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, TRUE, 0);

      mbox = gtk_vbox_new (TRUE, 5);
      gtk_box_pack_start (GTK_BOX (bbox), mbox, FALSE, TRUE, 0);

      label = gtk_label_new ("Row Height :");
      gtk_box_pack_start (GTK_BOX (mbox), label, FALSE, FALSE, 0);

      label = gtk_label_new ("Indent :");
      gtk_box_pack_start (GTK_BOX (mbox), label, FALSE, FALSE, 0);

      label = gtk_label_new ("Spacing :");
      gtk_box_pack_start (GTK_BOX (mbox), label, FALSE, FALSE, 0);

      mbox = gtk_vbox_new (TRUE, 5);
      gtk_box_pack_start (GTK_BOX (bbox), mbox, FALSE, TRUE, 0);

      adj = (GtkAdjustment *) gtk_adjustment_new (20, 12, 100, 1, 10, 0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (mbox), spinner, FALSE, FALSE, 5);
      gtk_tooltips_set_tip (tooltips, spinner,
			    "Row height of list items", NULL);
      g_signal_connect (adj, "value_changed",
			G_CALLBACK (change_row_height), ctree);
      gtk_clist_set_row_height ( GTK_CLIST (ctree), adj->value);

      adj = (GtkAdjustment *) gtk_adjustment_new (20, 0, 60, 1, 10, 0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (mbox), spinner, FALSE, FALSE, 5);
      gtk_tooltips_set_tip (tooltips, spinner, "Tree Indentation.", NULL);
      g_signal_connect (adj, "value_changed",
			G_CALLBACK (change_indent), ctree);

      adj = (GtkAdjustment *) gtk_adjustment_new (5, 0, 60, 1, 10, 0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (mbox), spinner, FALSE, FALSE, 5);
      gtk_tooltips_set_tip (tooltips, spinner, "Tree Spacing.", NULL);
      g_signal_connect (adj, "value_changed",
			G_CALLBACK (change_spacing), ctree);

      mbox = gtk_vbox_new (TRUE, 5);
      gtk_box_pack_start (GTK_BOX (bbox), mbox, FALSE, TRUE, 0);

      hbox = gtk_hbox_new (FALSE, 5);
      gtk_box_pack_start (GTK_BOX (mbox), hbox, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Expand All");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (expand_all), ctree);

      button = gtk_button_new_with_label ("Collapse All");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (collapse_all), ctree);

      button = gtk_button_new_with_label ("Change Style");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (change_style), ctree);

      button = gtk_button_new_with_label ("Export Tree");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (export_ctree), ctree);

      hbox = gtk_hbox_new (FALSE, 5);
      gtk_box_pack_start (GTK_BOX (mbox), hbox, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Select All");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (select_all), ctree);

      button = gtk_button_new_with_label ("Unselect All");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (unselect_all), ctree);

      button = gtk_button_new_with_label ("Remove Selection");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (remove_selection), ctree);

      check = gtk_check_button_new_with_label ("Reorderable");
      gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, TRUE, 0);
      gtk_tooltips_set_tip (tooltips, check,
			    "Tree items can be reordered by dragging.", NULL);
      g_signal_connect (check, "clicked",
			G_CALLBACK (toggle_reorderable), ctree);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);

      hbox = gtk_hbox_new (TRUE, 5);
      gtk_box_pack_start (GTK_BOX (mbox), hbox, FALSE, FALSE, 0);

      omenu1 = build_option_menu (items1, 4, 2, 
				  ctree_toggle_line_style,
				  ctree);
      gtk_box_pack_start (GTK_BOX (hbox), omenu1, FALSE, TRUE, 0);
      gtk_tooltips_set_tip (tooltips, omenu1, "The tree's line style.", NULL);

      omenu2 = build_option_menu (items2, 4, 1, 
				  ctree_toggle_expander_style,
				  ctree);
      gtk_box_pack_start (GTK_BOX (hbox), omenu2, FALSE, TRUE, 0);
      gtk_tooltips_set_tip (tooltips, omenu2, "The tree's expander style.",
			    NULL);

      omenu3 = build_option_menu (items3, 2, 0, 
				  ctree_toggle_justify, ctree);
      gtk_box_pack_start (GTK_BOX (hbox), omenu3, FALSE, TRUE, 0);
      gtk_tooltips_set_tip (tooltips, omenu3, "The tree's justification.",
			    NULL);

      omenu4 = build_option_menu (selection_mode_items, 3, 3, 
				  ctree_toggle_sel_mode, ctree);
      gtk_box_pack_start (GTK_BOX (hbox), omenu4, FALSE, TRUE, 0);
      gtk_tooltips_set_tip (tooltips, omenu4, "The list's selection mode.",
			    NULL);

      gtk_widget_realize (window);
      
      gtk_widget_set_size_request (GTK_WIDGET (ctree), -1, 300);

      frame = gtk_frame_new (NULL);
      gtk_container_set_border_width (GTK_CONTAINER (frame), 0);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);

      hbox = gtk_hbox_new (TRUE, 2);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
      gtk_container_add (GTK_CONTAINER (frame), hbox);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, TRUE, 0);

      hbox2 = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox2), 2);
      gtk_container_add (GTK_CONTAINER (frame), hbox2);

      label = gtk_label_new ("Books :");
      gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, TRUE, 0);

      sprintf (buf, "%d", books);
      book_label = gtk_label_new (buf);
      gtk_box_pack_end (GTK_BOX (hbox2), book_label, FALSE, TRUE, 5);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, TRUE, 0);

      hbox2 = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox2), 2);
      gtk_container_add (GTK_CONTAINER (frame), hbox2);

      label = gtk_label_new ("Pages :");
      gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, TRUE, 0);

      sprintf (buf, "%d", pages);
      page_label = gtk_label_new (buf);
      gtk_box_pack_end (GTK_BOX (hbox2), page_label, FALSE, TRUE, 5);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, TRUE, 0);

      hbox2 = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox2), 2);
      gtk_container_add (GTK_CONTAINER (frame), hbox2);

      label = gtk_label_new ("Selected :");
      gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, TRUE, 0);

      sprintf (buf, "%d", g_list_length (GTK_CLIST (ctree)->selection));
      sel_label = gtk_label_new (buf);
      gtk_box_pack_end (GTK_BOX (hbox2), sel_label, FALSE, TRUE, 5);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, TRUE, 0);

      hbox2 = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox2), 2);
      gtk_container_add (GTK_CONTAINER (frame), hbox2);

      label = gtk_label_new ("Visible :");
      gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, TRUE, 0);

      sprintf (buf, "%d", g_list_length (GTK_CLIST (ctree)->row_list));
      vis_label = gtk_label_new (buf);
      gtk_box_pack_end (GTK_BOX (hbox2), vis_label, FALSE, TRUE, 5);

      rebuild_tree (NULL, ctree);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkColorSelection
 */

void
color_selection_ok (GtkWidget               *w,
                    GtkColorSelectionDialog *cs)
{
  GtkColorSelection *colorsel;
  gdouble color[4];

  colorsel=GTK_COLOR_SELECTION(cs->colorsel);

  gtk_color_selection_get_color(colorsel,color);
  gtk_color_selection_set_color(colorsel,color);
}

void
color_selection_changed (GtkWidget *w,
                         GtkColorSelectionDialog *cs)
{
  GtkColorSelection *colorsel;
  gdouble color[4];

  colorsel=GTK_COLOR_SELECTION(cs->colorsel);
  gtk_color_selection_get_color(colorsel,color);
}

#if 0 /* unused */
static void
opacity_toggled_cb (GtkWidget *w,
		    GtkColorSelectionDialog *cs)
{
  GtkColorSelection *colorsel;

  colorsel = GTK_COLOR_SELECTION (cs->colorsel);
  gtk_color_selection_set_has_opacity_control (colorsel,
					       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)));
}

static void
palette_toggled_cb (GtkWidget *w,
		    GtkColorSelectionDialog *cs)
{
  GtkColorSelection *colorsel;

  colorsel = GTK_COLOR_SELECTION (cs->colorsel);
  gtk_color_selection_set_has_palette (colorsel,
				       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)));
}
#endif

void
create_color_selection (GtkWidget *widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *picker;
      GtkWidget *hbox;
      GtkWidget *label;
      GtkWidget *button;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window), 
			     gtk_widget_get_screen (widget));
			     
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
                        &window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkColorButton");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      hbox = gtk_hbox_new (FALSE, 8);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
      gtk_container_add (GTK_CONTAINER (window), hbox);
      
      label = gtk_label_new ("Pick a color");
      gtk_container_add (GTK_CONTAINER (hbox), label);

      picker = gtk_color_button_new ();
      gtk_color_button_set_use_alpha (GTK_COLOR_BUTTON (picker), TRUE);
      gtk_container_add (GTK_CONTAINER (hbox), picker);

      button = gtk_button_new_with_mnemonic ("_Props");
      gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (props_clicked),
			picker);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkFileSelection
 */

void
show_fileops (GtkWidget        *widget,
	      GtkFileSelection *fs)
{
  gboolean show_ops;

  show_ops = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

  if (show_ops)
    gtk_file_selection_show_fileop_buttons (fs);
  else
    gtk_file_selection_hide_fileop_buttons (fs);
}

void
select_multiple (GtkWidget        *widget,
		 GtkFileSelection *fs)
{
  gboolean select_multiple;

  select_multiple = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  gtk_file_selection_set_select_multiple (fs, select_multiple);
}

void
file_selection_ok (GtkFileSelection *fs)
{
  int i;
  gchar **selections;

  selections = gtk_file_selection_get_selections (fs);

  for (i = 0; selections[i] != NULL; i++)
    g_print ("%s\n", selections[i]);

  g_strfreev (selections);

  gtk_widget_destroy (GTK_WIDGET (fs));
}

void
create_file_selection (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *button;

  if (!window)
    {
      window = gtk_file_selection_new ("file selection dialog");
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION (window));

      gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_MOUSE);

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      g_signal_connect_swapped (GTK_FILE_SELECTION (window)->ok_button,
				"clicked",
				G_CALLBACK (file_selection_ok),
				window);
      g_signal_connect_swapped (GTK_FILE_SELECTION (window)->cancel_button,
			        "clicked",
				G_CALLBACK (gtk_widget_destroy),
				window);
      
      button = gtk_check_button_new_with_label ("Show Fileops");
      g_signal_connect (button, "toggled",
			G_CALLBACK (show_fileops),
			window);
      gtk_box_pack_start (GTK_BOX (GTK_FILE_SELECTION (window)->action_area), 
			  button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      button = gtk_check_button_new_with_label ("Select Multiple");
      g_signal_connect (button, "clicked",
			G_CALLBACK (select_multiple),
			window);
      gtk_box_pack_start (GTK_BOX (GTK_FILE_SELECTION (window)->action_area), 
			  button, FALSE, FALSE, 0);
      gtk_widget_show (button);
    }
  
  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

void
flipping_toggled_cb (GtkWidget *widget, gpointer data)
{
  int state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  int new_direction = state ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR;

  gtk_widget_set_default_direction (new_direction);
}

static void
orientable_toggle_orientation (GtkOrientable *orientable)
{
  GtkOrientation orientation;

  orientation = gtk_orientable_get_orientation (orientable);
  gtk_orientable_set_orientation (orientable,
                                  orientation == GTK_ORIENTATION_HORIZONTAL ?
                                  GTK_ORIENTATION_VERTICAL :
                                  GTK_ORIENTATION_HORIZONTAL);

  if (GTK_IS_CONTAINER (orientable))
    {
      GList *children;
      GList *child;

      children = gtk_container_get_children (GTK_CONTAINER (orientable));

      for (child = children; child; child = child->next)
        {
          if (GTK_IS_ORIENTABLE (child->data))
            orientable_toggle_orientation (child->data);
        }

      g_list_free (children);
    }
}

void
flipping_orientation_toggled_cb (GtkWidget *widget, gpointer data)
{
  orientable_toggle_orientation (GTK_ORIENTABLE (GTK_DIALOG (gtk_widget_get_toplevel (widget))->vbox));
}

static void
set_direction_recurse (GtkWidget *widget,
		       gpointer   data)
{
  GtkTextDirection *dir = data;
  
  gtk_widget_set_direction (widget, *dir);
  if (GTK_IS_CONTAINER (widget))
    gtk_container_foreach (GTK_CONTAINER (widget),
			   set_direction_recurse,
			   data);
}

static GtkWidget *
create_forward_back (const char       *title,
		     GtkTextDirection  text_dir)
{
  GtkWidget *frame = gtk_frame_new (title);
  GtkWidget *bbox = gtk_hbutton_box_new ();
  GtkWidget *back_button = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
  GtkWidget *forward_button = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);

  gtk_container_set_border_width (GTK_CONTAINER (bbox), 5);
  
  gtk_container_add (GTK_CONTAINER (frame), bbox);
  gtk_container_add (GTK_CONTAINER (bbox), back_button);
  gtk_container_add (GTK_CONTAINER (bbox), forward_button);

  set_direction_recurse (frame, &text_dir);

  return frame;
}

void
create_flipping (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *check_button, *button;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "Bidirectional Flipping");

      check_button = gtk_check_button_new_with_label ("Right-to-left global direction");
      gtk_container_set_border_width (GTK_CONTAINER (check_button), 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox),
			  check_button, TRUE, TRUE, 0);

      if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button), TRUE);

      g_signal_connect (check_button, "toggled",
			G_CALLBACK (flipping_toggled_cb), NULL);

      check_button = gtk_check_button_new_with_label ("Toggle orientation of all boxes");
      gtk_container_set_border_width (GTK_CONTAINER (check_button), 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  check_button, TRUE, TRUE, 0);

      g_signal_connect (check_button, "toggled",
			G_CALLBACK (flipping_orientation_toggled_cb), NULL);

      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  create_forward_back ("Default", GTK_TEXT_DIR_NONE),
			  TRUE, TRUE, 0);

      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  create_forward_back ("Left-to-Right", GTK_TEXT_DIR_LTR),
			  TRUE, TRUE, 0);

      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  create_forward_back ("Right-to-Left", GTK_TEXT_DIR_RTL),
			  TRUE, TRUE, 0);

      button = gtk_button_new_with_label ("Close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy), window);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
    }
  
  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Focus test
 */

static GtkWidget*
make_focus_table (GList **list)
{
  GtkWidget *table;
  gint i, j;
  
  table = gtk_table_new (5, 5, FALSE);

  i = 0;
  j = 0;

  while (i < 5)
    {
      j = 0;
      while (j < 5)
        {
          GtkWidget *widget;
          
          if ((i + j) % 2)
            widget = gtk_entry_new ();
          else
            widget = gtk_button_new_with_label ("Foo");

          *list = g_list_prepend (*list, widget);
          
          gtk_table_attach (GTK_TABLE (table),
                            widget,
                            i, i + 1,
                            j, j + 1,
                            GTK_EXPAND | GTK_FILL,
                            GTK_EXPAND | GTK_FILL,
                            5, 5);
          
          ++j;
        }

      ++i;
    }

  *list = g_list_reverse (*list);
  
  return table;
}

static void
create_focus (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  
  if (!window)
    {
      GtkWidget *table;
      GtkWidget *frame;
      GList *list = NULL;
      
      window = gtk_dialog_new_with_buttons ("Keyboard focus navigation",
                                            NULL, 0,
                                            GTK_STOCK_CLOSE,
                                            GTK_RESPONSE_NONE,
                                            NULL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      g_signal_connect (window, "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);
      
      gtk_window_set_title (GTK_WINDOW (window), "Keyboard Focus Navigation");

      frame = gtk_frame_new ("Weird tab focus chain");

      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  frame, TRUE, TRUE, 0);
      
      table = make_focus_table (&list);

      gtk_container_add (GTK_CONTAINER (frame), table);

      gtk_container_set_focus_chain (GTK_CONTAINER (table),
                                     list);

      g_list_free (list);
      
      frame = gtk_frame_new ("Default tab focus chain");

      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  frame, TRUE, TRUE, 0);

      list = NULL;
      table = make_focus_table (&list);

      g_list_free (list);
      
      gtk_container_add (GTK_CONTAINER (frame), table);      
    }
  
  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkFontSelection
 */

void
font_selection_ok (GtkWidget              *w,
		   GtkFontSelectionDialog *fs)
{
  gchar *s = gtk_font_selection_dialog_get_font_name (fs);

  g_print ("%s\n", s);
  g_free (s);
  gtk_widget_destroy (GTK_WIDGET (fs));
}

void
create_font_selection (GtkWidget *widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *picker;
      GtkWidget *hbox;
      GtkWidget *label;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkFontButton");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      hbox = gtk_hbox_new (FALSE, 8);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
      gtk_container_add (GTK_CONTAINER (window), hbox);
      
      label = gtk_label_new ("Pick a font");
      gtk_container_add (GTK_CONTAINER (hbox), label);

      picker = gtk_font_button_new ();
      gtk_font_button_set_use_font (GTK_FONT_BUTTON (picker), TRUE);
      gtk_container_add (GTK_CONTAINER (hbox), picker);
    }
  
  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkDialog
 */

static GtkWidget *dialog_window = NULL;

static void
label_toggle (GtkWidget  *widget,
	      GtkWidget **label)
{
  if (!(*label))
    {
      *label = gtk_label_new ("Dialog Test");
      g_signal_connect (*label,
			"destroy",
			G_CALLBACK (gtk_widget_destroyed),
			label);
      gtk_misc_set_padding (GTK_MISC (*label), 10, 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_window)->vbox), 
			  *label, TRUE, TRUE, 0);
      gtk_widget_show (*label);
    }
  else
    gtk_widget_destroy (*label);
}

#define RESPONSE_TOGGLE_SEPARATOR 1

static void
print_response (GtkWidget *dialog,
                gint       response_id,
                gpointer   data)
{
  g_print ("response signal received (%d)\n", response_id);

  if (response_id == RESPONSE_TOGGLE_SEPARATOR)
    {
      gtk_dialog_set_has_separator (GTK_DIALOG (dialog),
                                    !gtk_dialog_get_has_separator (GTK_DIALOG (dialog)));
    }
}

static void
create_dialog (GtkWidget *widget)
{
  static GtkWidget *label;
  GtkWidget *button;

  if (!dialog_window)
    {
      /* This is a terrible example; it's much simpler to create
       * dialogs than this. Don't use testgtk for example code,
       * use gtk-demo ;-)
       */
      
      dialog_window = gtk_dialog_new ();
      gtk_window_set_screen (GTK_WINDOW (dialog_window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (dialog_window,
                        "response",
                        G_CALLBACK (print_response),
                        NULL);
      
      g_signal_connect (dialog_window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&dialog_window);

      gtk_window_set_title (GTK_WINDOW (dialog_window), "GtkDialog");
      gtk_container_set_border_width (GTK_CONTAINER (dialog_window), 0);

      button = gtk_button_new_with_label ("OK");
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Toggle");
      g_signal_connect (button, "clicked",
			G_CALLBACK (label_toggle),
			&label);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_window)->action_area),
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      label = NULL;
      
      button = gtk_button_new_with_label ("Separator");

      gtk_widget_set_can_default (button, TRUE);

      gtk_dialog_add_action_widget (GTK_DIALOG (dialog_window),
                                    button,
                                    RESPONSE_TOGGLE_SEPARATOR);
      gtk_widget_show (button);
    }

  if (!gtk_widget_get_visible (dialog_window))
    gtk_widget_show (dialog_window);
  else
    gtk_widget_destroy (dialog_window);
}

/* Display & Screen test 
 */

typedef struct 
{ 
  GtkEntry *entry;
  GtkWidget *radio_dpy;
  GtkWidget *toplevel; 
  GtkWidget *dialog_window;
  GtkWidget *combo;
  GList *valid_display_list;
} ScreenDisplaySelection;

static void
screen_display_check (GtkWidget *widget, ScreenDisplaySelection *data)
{
  char *display_name;
  GdkDisplay *display = gtk_widget_get_display (widget);
  GtkWidget *dialog;
  GdkScreen *new_screen = NULL;
  GdkScreen *current_screen = gtk_widget_get_screen (widget);
  
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data->radio_dpy)))
    {
      display_name = g_strdup (gtk_entry_get_text (data->entry));
      display = gdk_display_open (display_name);
      
      if (!display)
	{
	  dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (widget)),
					   GTK_DIALOG_DESTROY_WITH_PARENT,
					   GTK_MESSAGE_ERROR,
					   GTK_BUTTONS_OK,
					   "The display :\n%s\ncannot be opened",
					   display_name);
	  gtk_window_set_screen (GTK_WINDOW (dialog), current_screen);
	  gtk_widget_show (dialog);
	  g_signal_connect (dialog, "response",
			    G_CALLBACK (gtk_widget_destroy),
			    NULL);
	}
      else
        {
          GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (data->combo));
          gint i = 0;
          GtkTreeIter iter;
          gboolean found = FALSE;
          while (gtk_tree_model_iter_nth_child (model, &iter, NULL, i++))
            {
              gchar *name;
              gtk_tree_model_get (model, &iter, 0, &name, -1);
              found = !g_ascii_strcasecmp (display_name, name);
              g_free (name);

              if (found)
                break;
            }
          if (!found)
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (data->combo), display_name);
          new_screen = gdk_display_get_default_screen (display);
        }
    }
  else
    {
      gint number_of_screens = gdk_display_get_n_screens (display);
      gint screen_num = gdk_screen_get_number (current_screen);
      if ((screen_num +1) < number_of_screens)
	new_screen = gdk_display_get_screen (display, screen_num + 1);
      else
	new_screen = gdk_display_get_screen (display, 0);
    }
  
  if (new_screen) 
    {
      gtk_window_set_screen (GTK_WINDOW (data->toplevel), new_screen);
      gtk_widget_destroy (data->dialog_window);
    }
}

void
screen_display_destroy_diag (GtkWidget *widget, GtkWidget *data)
{
  gtk_widget_destroy (data);
}

void
create_display_screen (GtkWidget *widget)
{
  GtkWidget *table, *frame, *window, *combo_dpy, *vbox;
  GtkWidget *radio_dpy, *radio_scr, *applyb, *cancelb;
  GtkWidget *bbox;
  ScreenDisplaySelection *scr_dpy_data;
  GdkScreen *screen = gtk_widget_get_screen (widget);
  static GList *valid_display_list = NULL;
  
  GdkDisplay *display = gdk_screen_get_display (screen);

  window = g_object_new (gtk_window_get_type (),
			   "screen", screen,
			   "user_data", NULL,
			   "type", GTK_WINDOW_TOPLEVEL,
			   "title",
			   "Screen or Display selection",
			   "border_width", 10, NULL);
  g_signal_connect (window, "destroy", 
		    G_CALLBACK (gtk_widget_destroy), NULL);

  vbox = gtk_vbox_new (FALSE, 3);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  
  frame = gtk_frame_new ("Select screen or display");
  gtk_container_add (GTK_CONTAINER (vbox), frame);
  
  table = gtk_table_new (2, 2, TRUE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 3);
  gtk_table_set_col_spacings (GTK_TABLE (table), 3);

  gtk_container_add (GTK_CONTAINER (frame), table);

  radio_dpy = gtk_radio_button_new_with_label (NULL, "move to another X display");
  if (gdk_display_get_n_screens(display) > 1)
    radio_scr = gtk_radio_button_new_with_label 
    (gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio_dpy)), "move to next screen");
  else
    {    
      radio_scr = gtk_radio_button_new_with_label 
	(gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio_dpy)), 
	 "only one screen on the current display");
      gtk_widget_set_sensitive (radio_scr, FALSE);
    }
  combo_dpy = gtk_combo_box_text_new_with_entry ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_dpy), "diabolo:0.0");
  gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (combo_dpy))),
                      "<hostname>:<X Server Num>.<Screen Num>");

  gtk_table_attach_defaults (GTK_TABLE (table), radio_dpy, 0, 1, 0, 1);
  gtk_table_attach_defaults (GTK_TABLE (table), radio_scr, 0, 1, 1, 2);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_dpy, 1, 2, 0, 1);

  bbox = gtk_hbutton_box_new ();
  applyb = gtk_button_new_from_stock (GTK_STOCK_APPLY);
  cancelb = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  
  gtk_container_add (GTK_CONTAINER (vbox), bbox);

  gtk_container_add (GTK_CONTAINER (bbox), applyb);
  gtk_container_add (GTK_CONTAINER (bbox), cancelb);

  scr_dpy_data = g_new0 (ScreenDisplaySelection, 1);

  scr_dpy_data->entry = GTK_ENTRY (GTK_COMBO (combo_dpy)->entry);
  scr_dpy_data->radio_dpy = radio_dpy;
  scr_dpy_data->toplevel = gtk_widget_get_toplevel (widget);
  scr_dpy_data->dialog_window = window;
  scr_dpy_data->valid_display_list = valid_display_list;
  scr_dpy_data->combo = combo_dpy;

  g_signal_connect (cancelb, "clicked", 
		    G_CALLBACK (screen_display_destroy_diag), window);
  g_signal_connect (applyb, "clicked", 
		    G_CALLBACK (screen_display_check), scr_dpy_data);
  gtk_widget_show_all (window);
}

/* Event Watcher
 */
static gboolean event_watcher_enter_id = 0;
static gboolean event_watcher_leave_id = 0;

static gboolean
event_watcher (GSignalInvocationHint *ihint,
	       guint                  n_param_values,
	       const GValue          *param_values,
	       gpointer               data)
{
  g_print ("Watch: \"%s\" emitted for %s\n",
	   g_signal_name (ihint->signal_id),
	   G_OBJECT_TYPE_NAME (g_value_get_object (param_values + 0)));

  return TRUE;
}

static void
event_watcher_down (void)
{
  if (event_watcher_enter_id)
    {
      guint signal_id;

      signal_id = g_signal_lookup ("enter_notify_event", GTK_TYPE_WIDGET);
      g_signal_remove_emission_hook (signal_id, event_watcher_enter_id);
      event_watcher_enter_id = 0;
      signal_id = g_signal_lookup ("leave_notify_event", GTK_TYPE_WIDGET);
      g_signal_remove_emission_hook (signal_id, event_watcher_leave_id);
      event_watcher_leave_id = 0;
    }
}

static void
event_watcher_toggle (void)
{
  if (event_watcher_enter_id)
    event_watcher_down ();
  else
    {
      guint signal_id;

      signal_id = g_signal_lookup ("enter_notify_event", GTK_TYPE_WIDGET);
      event_watcher_enter_id = g_signal_add_emission_hook (signal_id, 0, event_watcher, NULL, NULL);
      signal_id = g_signal_lookup ("leave_notify_event", GTK_TYPE_WIDGET);
      event_watcher_leave_id = g_signal_add_emission_hook (signal_id, 0, event_watcher, NULL, NULL);
    }
}

static void
create_event_watcher (GtkWidget *widget)
{
  GtkWidget *button;

  if (!dialog_window)
    {
      dialog_window = gtk_dialog_new ();
      gtk_window_set_screen (GTK_WINDOW (dialog_window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (dialog_window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&dialog_window);
      g_signal_connect (dialog_window, "destroy",
			G_CALLBACK (event_watcher_down),
			NULL);

      gtk_window_set_title (GTK_WINDOW (dialog_window), "Event Watcher");
      gtk_container_set_border_width (GTK_CONTAINER (dialog_window), 0);
      gtk_widget_set_size_request (dialog_window, 200, 110);

      button = gtk_toggle_button_new_with_label ("Activate Watch");
      g_signal_connect (button, "clicked",
			G_CALLBACK (event_watcher_toggle),
			NULL);
      gtk_container_set_border_width (GTK_CONTAINER (button), 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_window)->vbox), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				dialog_window);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_window)->action_area),
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!gtk_widget_get_visible (dialog_window))
    gtk_widget_show (dialog_window);
  else
    gtk_widget_destroy (dialog_window);
}

/*
 * GtkRange
 */

static gchar*
reformat_value (GtkScale *scale,
                gdouble   value)
{
  return g_strdup_printf ("-->%0.*g<--",
                          gtk_scale_get_digits (scale), value);
}

static void
create_range_controls (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *scrollbar;
  GtkWidget *scale;
  GtkWidget *separator;
  GtkObject *adjustment;
  GtkWidget *hbox;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "range controls");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);


      adjustment = gtk_adjustment_new (0.0, 0.0, 101.0, 0.1, 1.0, 1.0);

      scale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
      gtk_widget_set_size_request (GTK_WIDGET (scale), 150, -1);
      gtk_range_set_update_policy (GTK_RANGE (scale), GTK_UPDATE_DELAYED);
      gtk_scale_set_digits (GTK_SCALE (scale), 1);
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_box_pack_start (GTK_BOX (box2), scale, TRUE, TRUE, 0);
      gtk_widget_show (scale);

      scrollbar = gtk_hscrollbar_new (GTK_ADJUSTMENT (adjustment));
      gtk_range_set_update_policy (GTK_RANGE (scrollbar), 
				   GTK_UPDATE_CONTINUOUS);
      gtk_box_pack_start (GTK_BOX (box2), scrollbar, TRUE, TRUE, 0);
      gtk_widget_show (scrollbar);

      scale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      g_signal_connect (scale,
                        "format_value",
                        G_CALLBACK (reformat_value),
                        NULL);
      gtk_box_pack_start (GTK_BOX (box2), scale, TRUE, TRUE, 0);
      gtk_widget_show (scale);
      
      hbox = gtk_hbox_new (FALSE, 0);

      scale = gtk_vscale_new (GTK_ADJUSTMENT (adjustment));
      gtk_widget_set_size_request (scale, -1, 200);
      gtk_scale_set_digits (GTK_SCALE (scale), 2);
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_box_pack_start (GTK_BOX (hbox), scale, TRUE, TRUE, 0);
      gtk_widget_show (scale);

      scale = gtk_vscale_new (GTK_ADJUSTMENT (adjustment));
      gtk_widget_set_size_request (scale, -1, 200);
      gtk_scale_set_digits (GTK_SCALE (scale), 2);
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_range_set_inverted (GTK_RANGE (scale), TRUE);
      gtk_box_pack_start (GTK_BOX (hbox), scale, TRUE, TRUE, 0);
      gtk_widget_show (scale);

      scale = gtk_vscale_new (GTK_ADJUSTMENT (adjustment));
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      g_signal_connect (scale,
                        "format_value",
                        G_CALLBACK (reformat_value),
                        NULL);
      gtk_box_pack_start (GTK_BOX (hbox), scale, TRUE, TRUE, 0);
      gtk_widget_show (scale);

      
      gtk_box_pack_start (GTK_BOX (box2), hbox, TRUE, TRUE, 0);
      gtk_widget_show (hbox);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkRulers
 */

void
create_rulers (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *table;
  GtkWidget *ruler;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_object_set (window, "allow_shrink", TRUE, "allow_grow", TRUE, NULL);

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "rulers");
      gtk_widget_set_size_request (window, 300, 300);
      gtk_widget_set_events (window, 
			     GDK_POINTER_MOTION_MASK 
			     | GDK_POINTER_MOTION_HINT_MASK);
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      table = gtk_table_new (2, 2, FALSE);
      gtk_container_add (GTK_CONTAINER (window), table);
      gtk_widget_show (table);

      ruler = gtk_hruler_new ();
      gtk_ruler_set_metric (GTK_RULER (ruler), GTK_CENTIMETERS);
      gtk_ruler_set_range (GTK_RULER (ruler), 100, 0, 0, 20);

      g_signal_connect_swapped (window, 
			        "motion_notify_event",
				G_CALLBACK (GTK_WIDGET_GET_CLASS (ruler)->motion_notify_event),
			        ruler);
      
      gtk_table_attach (GTK_TABLE (table), ruler, 1, 2, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (ruler);


      ruler = gtk_vruler_new ();
      gtk_ruler_set_range (GTK_RULER (ruler), 5, 15, 0, 20);

      g_signal_connect_swapped (window, 
			        "motion_notify_event",
			        G_CALLBACK (GTK_WIDGET_GET_CLASS (ruler)->motion_notify_event),
			        ruler);
      
      gtk_table_attach (GTK_TABLE (table), ruler, 0, 1, 1, 2,
			GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (ruler);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

static void
text_toggle_editable (GtkWidget *checkbutton,
		       GtkWidget *text)
{
   gtk_text_set_editable(GTK_TEXT(text),
			  GTK_TOGGLE_BUTTON(checkbutton)->active);
}

static void
text_toggle_word_wrap (GtkWidget *checkbutton,
		       GtkWidget *text)
{
   gtk_text_set_word_wrap(GTK_TEXT(text),
			  GTK_TOGGLE_BUTTON(checkbutton)->active);
}

struct {
  GdkColor color;
  gchar *name;
} text_colors[] = {
 { { 0, 0x0000, 0x0000, 0x0000 }, "black" },
 { { 0, 0xFFFF, 0xFFFF, 0xFFFF }, "white" },
 { { 0, 0xFFFF, 0x0000, 0x0000 }, "red" },
 { { 0, 0x0000, 0xFFFF, 0x0000 }, "green" },
 { { 0, 0x0000, 0x0000, 0xFFFF }, "blue" }, 
 { { 0, 0x0000, 0xFFFF, 0xFFFF }, "cyan" },
 { { 0, 0xFFFF, 0x0000, 0xFFFF }, "magenta" },
 { { 0, 0xFFFF, 0xFFFF, 0x0000 }, "yellow" }
};

int ntext_colors = sizeof(text_colors) / sizeof(text_colors[0]);

/*
 * GtkText
 */
void
text_insert_random (GtkWidget *w, GtkText *text)
{
  int i;
  char c;
   for (i=0; i<10; i++)
    {
      c = 'A' + rand() % ('Z' - 'A');
      gtk_text_set_point (text, rand() % gtk_text_get_length (text));
      gtk_text_insert (text, NULL, NULL, NULL, &c, 1);
    }
}

void
create_text (GtkWidget *widget)
{
  int i, j;

  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *check;
  GtkWidget *separator;
  GtkWidget *scrolled_window;
  GtkWidget *text;

  FILE *infile;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      gtk_widget_set_name (window, "text window");
      g_object_set (window, "allow_shrink", TRUE, "allow_grow", TRUE, NULL);
      gtk_widget_set_size_request (window, 500, 500);

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);


      scrolled_window = gtk_scrolled_window_new (NULL, NULL);
      gtk_box_pack_start (GTK_BOX (box2), scrolled_window, TRUE, TRUE, 0);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				      GTK_POLICY_NEVER,
				      GTK_POLICY_ALWAYS);
      gtk_widget_show (scrolled_window);

      text = gtk_text_new (NULL, NULL);
      gtk_text_set_editable (GTK_TEXT (text), TRUE);
      gtk_container_add (GTK_CONTAINER (scrolled_window), text);
      gtk_widget_grab_focus (text);
      gtk_widget_show (text);


      gtk_text_freeze (GTK_TEXT (text));

      for (i=0; i<ntext_colors; i++)
	{
	  gtk_text_insert (GTK_TEXT (text), NULL, NULL, NULL, 
			   text_colors[i].name, -1);
	  gtk_text_insert (GTK_TEXT (text), NULL, NULL, NULL, "\t", -1);

	  for (j=0; j<ntext_colors; j++)
	    {
	      gtk_text_insert (GTK_TEXT (text), NULL,
			       &text_colors[j].color, &text_colors[i].color,
			       "XYZ", -1);
	    }
	  gtk_text_insert (GTK_TEXT (text), NULL, NULL, NULL, "\n", -1);
	}

      infile = fopen("testgtk.c", "r");
      
      if (infile)
	{
	  char *buffer;
	  int nbytes_read, nbytes_alloc;
	  
	  nbytes_read = 0;
	  nbytes_alloc = 1024;
	  buffer = g_new (char, nbytes_alloc);
	  while (1)
	    {
	      int len;
	      if (nbytes_alloc < nbytes_read + 1024)
		{
		  nbytes_alloc *= 2;
		  buffer = g_realloc (buffer, nbytes_alloc);
		}
	      len = fread (buffer + nbytes_read, 1, 1024, infile);
	      nbytes_read += len;
	      if (len < 1024)
		break;
	    }
	  
	  gtk_text_insert (GTK_TEXT (text), NULL, NULL,
			   NULL, buffer, nbytes_read);
	  g_free(buffer);
	  fclose (infile);
	}
      
      gtk_text_thaw (GTK_TEXT (text));

      hbox = gtk_hbutton_box_new ();
      gtk_box_pack_start (GTK_BOX (box2), hbox, FALSE, FALSE, 0);
      gtk_widget_show (hbox);

      check = gtk_check_button_new_with_label("Editable");
      gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, FALSE, 0);
      g_signal_connect (check, "toggled",
			G_CALLBACK (text_toggle_editable), text);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), TRUE);
      gtk_widget_show (check);

      check = gtk_check_button_new_with_label("Wrap Words");
      gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, TRUE, 0);
      g_signal_connect (check, "toggled",
			G_CALLBACK (text_toggle_word_wrap), text);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), FALSE);
      gtk_widget_show (check);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("insert random");
      g_signal_connect (button, "clicked",
			G_CALLBACK (text_insert_random),
			text);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkNotebook
 */

GdkPixbuf *book_open;
GdkPixbuf *book_closed;
GtkWidget *sample_notebook;

static void
set_page_image (GtkNotebook *notebook, gint page_num, GdkPixbuf *pixbuf)
{
  GtkWidget *page_widget;
  GtkWidget *pixwid;

  page_widget = gtk_notebook_get_nth_page (notebook, page_num);

  pixwid = g_object_get_data (G_OBJECT (page_widget), "tab_pixmap");
  gtk_image_set_from_pixbuf (GTK_IMAGE (pixwid), pixbuf);
  
  pixwid = g_object_get_data (G_OBJECT (page_widget), "menu_pixmap");
  gtk_image_set_from_pixbuf (GTK_IMAGE (pixwid), pixbuf);
}

static void
page_switch (GtkWidget *widget, gpointer *page, gint page_num)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  gint old_page_num = gtk_notebook_get_current_page (notebook);
 
  if (page_num == old_page_num)
    return;

  set_page_image (notebook, page_num, book_open);

  if (old_page_num != -1)
    set_page_image (notebook, old_page_num, book_closed);
}

static void
tab_fill (GtkToggleButton *button, GtkWidget *child)
{
  gboolean expand;
  GtkPackType pack_type;

  gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (sample_notebook), child,
					&expand, NULL, &pack_type);
  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (sample_notebook), child,
				      expand, button->active, pack_type);
}

static void
tab_expand (GtkToggleButton *button, GtkWidget *child)
{
  gboolean fill;
  GtkPackType pack_type;

  gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (sample_notebook), child,
					NULL, &fill, &pack_type);
  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (sample_notebook), child,
				      button->active, fill, pack_type);
}

static void
tab_pack (GtkToggleButton *button, GtkWidget *child)
	  
{ 
  gboolean expand;
  gboolean fill;

  gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (sample_notebook), child,
					&expand, &fill, NULL);
  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (sample_notebook), child,
				      expand, fill, button->active);
}

static void
create_pages (GtkNotebook *notebook, gint start, gint end)
{
  GtkWidget *child = NULL;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *label_box;
  GtkWidget *menu_box;
  GtkWidget *pixwid;
  gint i;
  char buffer[32];
  char accel_buffer[32];

  for (i = start; i <= end; i++)
    {
      sprintf (buffer, "Page %d", i);
      sprintf (accel_buffer, "Page _%d", i);

      child = gtk_frame_new (buffer);
      gtk_container_set_border_width (GTK_CONTAINER (child), 10);

      vbox = gtk_vbox_new (TRUE,0);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
      gtk_container_add (GTK_CONTAINER (child), vbox);

      hbox = gtk_hbox_new (TRUE,0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 5);

      button = gtk_check_button_new_with_label ("Fill Tab");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
      g_signal_connect (button, "toggled",
			G_CALLBACK (tab_fill), child);

      button = gtk_check_button_new_with_label ("Expand Tab");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
      g_signal_connect (button, "toggled",
			G_CALLBACK (tab_expand), child);

      button = gtk_check_button_new_with_label ("Pack end");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
      g_signal_connect (button, "toggled",
			G_CALLBACK (tab_pack), child);

      button = gtk_button_new_with_label ("Hide Page");
      gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 5);
      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_hide),
				child);

      gtk_widget_show_all (child);

      label_box = gtk_hbox_new (FALSE, 0);
      pixwid = gtk_image_new_from_pixbuf (book_closed);
      g_object_set_data (G_OBJECT (child), "tab_pixmap", pixwid);
			   
      gtk_box_pack_start (GTK_BOX (label_box), pixwid, FALSE, TRUE, 0);
      gtk_misc_set_padding (GTK_MISC (pixwid), 3, 1);
      label = gtk_label_new_with_mnemonic (accel_buffer);
      gtk_box_pack_start (GTK_BOX (label_box), label, FALSE, TRUE, 0);
      gtk_widget_show_all (label_box);
      
				       
      menu_box = gtk_hbox_new (FALSE, 0);
      pixwid = gtk_image_new_from_pixbuf (book_closed);
      g_object_set_data (G_OBJECT (child), "menu_pixmap", pixwid);
      
      gtk_box_pack_start (GTK_BOX (menu_box), pixwid, FALSE, TRUE, 0);
      gtk_misc_set_padding (GTK_MISC (pixwid), 3, 1);
      label = gtk_label_new (buffer);
      gtk_box_pack_start (GTK_BOX (menu_box), label, FALSE, TRUE, 0);
      gtk_widget_show_all (menu_box);

      gtk_notebook_append_page_menu (notebook, child, label_box, menu_box);
    }
}

static void
rotate_notebook (GtkButton   *button,
		 GtkNotebook *notebook)
{
  gtk_notebook_set_tab_pos (notebook, (notebook->tab_pos + 1) % 4);
}

static void
show_all_pages (GtkButton   *button,
		GtkNotebook *notebook)
{  
  gtk_container_foreach (GTK_CONTAINER (notebook),
			 (GtkCallback) gtk_widget_show, NULL);
}

static void
notebook_type_changed (GtkWidget *optionmenu,
		       gpointer   data)
{
  GtkNotebook *notebook;
  gint i, c;

  enum {
    STANDARD,
    NOTABS,
    BORDERLESS,
    SCROLLABLE
  };

  notebook = GTK_NOTEBOOK (data);

  c = gtk_option_menu_get_history (GTK_OPTION_MENU (optionmenu));

  switch (c)
    {
    case STANDARD:
      /* standard notebook */
      gtk_notebook_set_show_tabs (notebook, TRUE);
      gtk_notebook_set_show_border (notebook, TRUE);
      gtk_notebook_set_scrollable (notebook, FALSE);
      break;

    case NOTABS:
      /* notabs notebook */
      gtk_notebook_set_show_tabs (notebook, FALSE);
      gtk_notebook_set_show_border (notebook, TRUE);
      break;

    case BORDERLESS:
      /* borderless */
      gtk_notebook_set_show_tabs (notebook, FALSE);
      gtk_notebook_set_show_border (notebook, FALSE);
      break;

    case SCROLLABLE:  
      /* scrollable */
      gtk_notebook_set_show_tabs (notebook, TRUE);
      gtk_notebook_set_show_border (notebook, TRUE);
      gtk_notebook_set_scrollable (notebook, TRUE);
      if (g_list_length (notebook->children) == 5)
	create_pages (notebook, 6, 15);
      
      return;
      break;
    }
  
  if (g_list_length (notebook->children) == 15)
    for (i = 0; i < 10; i++)
      gtk_notebook_remove_page (notebook, 5);
}

static void
notebook_popup (GtkToggleButton *button,
		GtkNotebook     *notebook)
{
  if (button->active)
    gtk_notebook_popup_enable (notebook);
  else
    gtk_notebook_popup_disable (notebook);
}

static void
notebook_homogeneous (GtkToggleButton *button,
		      GtkNotebook     *notebook)
{
  g_object_set (notebook, "homogeneous", button->active, NULL);
}

static void
create_notebook (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *omenu;
  GtkWidget *label;

  static gchar *items[] =
  {
    "Standard",
    "No tabs",
    "Borderless",
    "Scrollable"
  };
  
  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "notebook");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      sample_notebook = gtk_notebook_new ();
      g_signal_connect (sample_notebook, "switch_page",
			G_CALLBACK (page_switch), NULL);
      gtk_notebook_set_tab_pos (GTK_NOTEBOOK (sample_notebook), GTK_POS_TOP);
      gtk_box_pack_start (GTK_BOX (box1), sample_notebook, TRUE, TRUE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (sample_notebook), 10);

      gtk_widget_realize (sample_notebook);

      if (!book_open)
	book_open = gdk_pixbuf_new_from_xpm_data ((const char **)book_open_xpm);
						  
      if (!book_closed)
	book_closed = gdk_pixbuf_new_from_xpm_data ((const char **)book_closed_xpm);

      create_pages (GTK_NOTEBOOK (sample_notebook), 1, 5);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 10);
      
      box2 = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_check_button_new_with_label ("popup menu");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, FALSE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (notebook_popup),
			sample_notebook);

      button = gtk_check_button_new_with_label ("homogeneous tabs");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, FALSE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (notebook_homogeneous),
			sample_notebook);

      box2 = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      label = gtk_label_new ("Notebook Style :");
      gtk_box_pack_start (GTK_BOX (box2), label, FALSE, TRUE, 0);

      omenu = build_option_menu (items, G_N_ELEMENTS (items), 0,
				 notebook_type_changed,
				 sample_notebook);
      gtk_box_pack_start (GTK_BOX (box2), omenu, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("Show all Pages");
      gtk_box_pack_start (GTK_BOX (box2), button, FALSE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (show_all_pages), sample_notebook);

      box2 = gtk_hbox_new (TRUE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("prev");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_notebook_prev_page),
				sample_notebook);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_button_new_with_label ("next");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_notebook_next_page),
				sample_notebook);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_button_new_with_label ("rotate");
      g_signal_connect (button, "clicked",
			G_CALLBACK (rotate_notebook), sample_notebook);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 5);

      button = gtk_button_new_with_label ("close");
      gtk_container_set_border_width (GTK_CONTAINER (button), 5);
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_box_pack_start (GTK_BOX (box1), button, FALSE, FALSE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkPanes
 */

void
toggle_resize (GtkWidget *widget, GtkWidget *child)
{
  GtkContainer *container = GTK_CONTAINER (gtk_widget_get_parent (child));
  GValue value = { 0, };
  g_value_init (&value, G_TYPE_BOOLEAN);
  gtk_container_child_get_property (container, child, "resize", &value);
  g_value_set_boolean (&value, !g_value_get_boolean (&value));
  gtk_container_child_set_property (container, child, "resize", &value);
}

void
toggle_shrink (GtkWidget *widget, GtkWidget *child)
{
  GtkContainer *container = GTK_CONTAINER (gtk_widget_get_parent (child));
  GValue value = { 0, };
  g_value_init (&value, G_TYPE_BOOLEAN);
  gtk_container_child_get_property (container, child, "shrink", &value);
  g_value_set_boolean (&value, !g_value_get_boolean (&value));
  gtk_container_child_set_property (container, child, "shrink", &value);
}

static void
paned_props_clicked (GtkWidget *button,
		     GObject   *paned)
{
  GtkWidget *window = create_prop_editor (paned, GTK_TYPE_PANED);
  
  gtk_window_set_title (GTK_WINDOW (window), "Paned Properties");
}

GtkWidget *
create_pane_options (GtkPaned    *paned,
		     const gchar *frame_label,
		     const gchar *label1,
		     const gchar *label2)
{
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *check_button;
  
  frame = gtk_frame_new (frame_label);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  
  table = gtk_table_new (4, 2, 4);
  gtk_container_add (GTK_CONTAINER (frame), table);
  
  label = gtk_label_new (label1);
  gtk_table_attach_defaults (GTK_TABLE (table), label,
			     0, 1, 0, 1);
  
  check_button = gtk_check_button_new_with_label ("Resize");
  gtk_table_attach_defaults (GTK_TABLE (table), check_button,
			     0, 1, 1, 2);
  g_signal_connect (check_button, "toggled",
		    G_CALLBACK (toggle_resize),
		    paned->child1);
  
  check_button = gtk_check_button_new_with_label ("Shrink");
  gtk_table_attach_defaults (GTK_TABLE (table), check_button,
			     0, 1, 2, 3);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
			       TRUE);
  g_signal_connect (check_button, "toggled",
		    G_CALLBACK (toggle_shrink),
		    paned->child1);
  
  label = gtk_label_new (label2);
  gtk_table_attach_defaults (GTK_TABLE (table), label,
			     1, 2, 0, 1);
  
  check_button = gtk_check_button_new_with_label ("Resize");
  gtk_table_attach_defaults (GTK_TABLE (table), check_button,
			     1, 2, 1, 2);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
			       TRUE);
  g_signal_connect (check_button, "toggled",
		    G_CALLBACK (toggle_resize),
		    paned->child2);
  
  check_button = gtk_check_button_new_with_label ("Shrink");
  gtk_table_attach_defaults (GTK_TABLE (table), check_button,
			     1, 2, 2, 3);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
			       TRUE);
  g_signal_connect (check_button, "toggled",
		    G_CALLBACK (toggle_shrink),
		    paned->child2);

  button = gtk_button_new_with_mnemonic ("_Properties");
  gtk_table_attach_defaults (GTK_TABLE (table), button,
			     0, 2, 3, 4);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (paned_props_clicked),
		    paned);

  return frame;
}

void
create_panes (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *frame;
  GtkWidget *hpaned;
  GtkWidget *vpaned;
  GtkWidget *button;
  GtkWidget *vbox;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));
      
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "Panes");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);
      
      vpaned = gtk_vpaned_new ();
      gtk_box_pack_start (GTK_BOX (vbox), vpaned, TRUE, TRUE, 0);
      gtk_container_set_border_width (GTK_CONTAINER(vpaned), 5);

      hpaned = gtk_hpaned_new ();
      gtk_paned_add1 (GTK_PANED (vpaned), hpaned);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
      gtk_widget_set_size_request (frame, 60, 60);
      gtk_paned_add1 (GTK_PANED (hpaned), frame);
      
      button = gtk_button_new_with_label ("Hi there");
      gtk_container_add (GTK_CONTAINER(frame), button);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
      gtk_widget_set_size_request (frame, 80, 60);
      gtk_paned_add2 (GTK_PANED (hpaned), frame);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
      gtk_widget_set_size_request (frame, 60, 80);
      gtk_paned_add2 (GTK_PANED (vpaned), frame);

      /* Now create toggle buttons to control sizing */

      gtk_box_pack_start (GTK_BOX (vbox),
			  create_pane_options (GTK_PANED (hpaned),
					       "Horizontal",
					       "Left",
					       "Right"),
			  FALSE, FALSE, 0);

      gtk_box_pack_start (GTK_BOX (vbox),
			  create_pane_options (GTK_PANED (vpaned),
					       "Vertical",
					       "Top",
					       "Bottom"),
			  FALSE, FALSE, 0);

      gtk_widget_show_all (vbox);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Paned keyboard navigation
 */

static GtkWidget*
paned_keyboard_window1 (GtkWidget *widget)
{
  GtkWidget *window1;
  GtkWidget *hpaned1;
  GtkWidget *frame1;
  GtkWidget *vbox1;
  GtkWidget *button7;
  GtkWidget *button8;
  GtkWidget *button9;
  GtkWidget *vpaned1;
  GtkWidget *frame2;
  GtkWidget *frame5;
  GtkWidget *hbox1;
  GtkWidget *button5;
  GtkWidget *button6;
  GtkWidget *frame3;
  GtkWidget *frame4;
  GtkWidget *table1;
  GtkWidget *button1;
  GtkWidget *button2;
  GtkWidget *button3;
  GtkWidget *button4;

  window1 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window1), "Basic paned navigation");
  gtk_window_set_screen (GTK_WINDOW (window1), 
			 gtk_widget_get_screen (widget));

  hpaned1 = gtk_hpaned_new ();
  gtk_container_add (GTK_CONTAINER (window1), hpaned1);

  frame1 = gtk_frame_new (NULL);
  gtk_paned_pack1 (GTK_PANED (hpaned1), frame1, FALSE, TRUE);
  gtk_frame_set_shadow_type (GTK_FRAME (frame1), GTK_SHADOW_IN);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame1), vbox1);

  button7 = gtk_button_new_with_label ("button7");
  gtk_box_pack_start (GTK_BOX (vbox1), button7, FALSE, FALSE, 0);

  button8 = gtk_button_new_with_label ("button8");
  gtk_box_pack_start (GTK_BOX (vbox1), button8, FALSE, FALSE, 0);

  button9 = gtk_button_new_with_label ("button9");
  gtk_box_pack_start (GTK_BOX (vbox1), button9, FALSE, FALSE, 0);

  vpaned1 = gtk_vpaned_new ();
  gtk_paned_pack2 (GTK_PANED (hpaned1), vpaned1, TRUE, TRUE);

  frame2 = gtk_frame_new (NULL);
  gtk_paned_pack1 (GTK_PANED (vpaned1), frame2, FALSE, TRUE);
  gtk_frame_set_shadow_type (GTK_FRAME (frame2), GTK_SHADOW_IN);

  frame5 = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (frame2), frame5);

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame5), hbox1);

  button5 = gtk_button_new_with_label ("button5");
  gtk_box_pack_start (GTK_BOX (hbox1), button5, FALSE, FALSE, 0);

  button6 = gtk_button_new_with_label ("button6");
  gtk_box_pack_start (GTK_BOX (hbox1), button6, FALSE, FALSE, 0);

  frame3 = gtk_frame_new (NULL);
  gtk_paned_pack2 (GTK_PANED (vpaned1), frame3, TRUE, TRUE);
  gtk_frame_set_shadow_type (GTK_FRAME (frame3), GTK_SHADOW_IN);

  frame4 = gtk_frame_new ("Buttons");
  gtk_container_add (GTK_CONTAINER (frame3), frame4);
  gtk_container_set_border_width (GTK_CONTAINER (frame4), 15);

  table1 = gtk_table_new (2, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (frame4), table1);
  gtk_container_set_border_width (GTK_CONTAINER (table1), 11);

  button1 = gtk_button_new_with_label ("button1");
  gtk_table_attach (GTK_TABLE (table1), button1, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  button2 = gtk_button_new_with_label ("button2");
  gtk_table_attach (GTK_TABLE (table1), button2, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  button3 = gtk_button_new_with_label ("button3");
  gtk_table_attach (GTK_TABLE (table1), button3, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  button4 = gtk_button_new_with_label ("button4");
  gtk_table_attach (GTK_TABLE (table1), button4, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  return window1;
}

static GtkWidget*
paned_keyboard_window2 (GtkWidget *widget)
{
  GtkWidget *window2;
  GtkWidget *hpaned2;
  GtkWidget *frame6;
  GtkWidget *button13;
  GtkWidget *hbox2;
  GtkWidget *vpaned2;
  GtkWidget *frame7;
  GtkWidget *button12;
  GtkWidget *frame8;
  GtkWidget *button11;
  GtkWidget *button10;

  window2 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window2), "\"button 10\" is not inside the horisontal pane");

  gtk_window_set_screen (GTK_WINDOW (window2), 
			 gtk_widget_get_screen (widget));

  hpaned2 = gtk_hpaned_new ();
  gtk_container_add (GTK_CONTAINER (window2), hpaned2);

  frame6 = gtk_frame_new (NULL);
  gtk_paned_pack1 (GTK_PANED (hpaned2), frame6, FALSE, TRUE);
  gtk_frame_set_shadow_type (GTK_FRAME (frame6), GTK_SHADOW_IN);

  button13 = gtk_button_new_with_label ("button13");
  gtk_container_add (GTK_CONTAINER (frame6), button13);

  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_paned_pack2 (GTK_PANED (hpaned2), hbox2, TRUE, TRUE);

  vpaned2 = gtk_vpaned_new ();
  gtk_box_pack_start (GTK_BOX (hbox2), vpaned2, TRUE, TRUE, 0);

  frame7 = gtk_frame_new (NULL);
  gtk_paned_pack1 (GTK_PANED (vpaned2), frame7, FALSE, TRUE);
  gtk_frame_set_shadow_type (GTK_FRAME (frame7), GTK_SHADOW_IN);

  button12 = gtk_button_new_with_label ("button12");
  gtk_container_add (GTK_CONTAINER (frame7), button12);

  frame8 = gtk_frame_new (NULL);
  gtk_paned_pack2 (GTK_PANED (vpaned2), frame8, TRUE, TRUE);
  gtk_frame_set_shadow_type (GTK_FRAME (frame8), GTK_SHADOW_IN);

  button11 = gtk_button_new_with_label ("button11");
  gtk_container_add (GTK_CONTAINER (frame8), button11);

  button10 = gtk_button_new_with_label ("button10");
  gtk_box_pack_start (GTK_BOX (hbox2), button10, FALSE, FALSE, 0);

  return window2;
}

static GtkWidget*
paned_keyboard_window3 (GtkWidget *widget)
{
  GtkWidget *window3;
  GtkWidget *vbox2;
  GtkWidget *label1;
  GtkWidget *hpaned3;
  GtkWidget *frame9;
  GtkWidget *button14;
  GtkWidget *hpaned4;
  GtkWidget *frame10;
  GtkWidget *button15;
  GtkWidget *hpaned5;
  GtkWidget *frame11;
  GtkWidget *button16;
  GtkWidget *frame12;
  GtkWidget *button17;

  window3 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_set_data (G_OBJECT (window3), "window3", window3);
  gtk_window_set_title (GTK_WINDOW (window3), "Nested panes");

  gtk_window_set_screen (GTK_WINDOW (window3), 
			 gtk_widget_get_screen (widget));
  

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window3), vbox2);

  label1 = gtk_label_new ("Three panes nested inside each other");
  gtk_box_pack_start (GTK_BOX (vbox2), label1, FALSE, FALSE, 0);

  hpaned3 = gtk_hpaned_new ();
  gtk_box_pack_start (GTK_BOX (vbox2), hpaned3, TRUE, TRUE, 0);

  frame9 = gtk_frame_new (NULL);
  gtk_paned_pack1 (GTK_PANED (hpaned3), frame9, FALSE, TRUE);
  gtk_frame_set_shadow_type (GTK_FRAME (frame9), GTK_SHADOW_IN);

  button14 = gtk_button_new_with_label ("button14");
  gtk_container_add (GTK_CONTAINER (frame9), button14);

  hpaned4 = gtk_hpaned_new ();
  gtk_paned_pack2 (GTK_PANED (hpaned3), hpaned4, TRUE, TRUE);

  frame10 = gtk_frame_new (NULL);
  gtk_paned_pack1 (GTK_PANED (hpaned4), frame10, FALSE, TRUE);
  gtk_frame_set_shadow_type (GTK_FRAME (frame10), GTK_SHADOW_IN);

  button15 = gtk_button_new_with_label ("button15");
  gtk_container_add (GTK_CONTAINER (frame10), button15);

  hpaned5 = gtk_hpaned_new ();
  gtk_paned_pack2 (GTK_PANED (hpaned4), hpaned5, TRUE, TRUE);

  frame11 = gtk_frame_new (NULL);
  gtk_paned_pack1 (GTK_PANED (hpaned5), frame11, FALSE, TRUE);
  gtk_frame_set_shadow_type (GTK_FRAME (frame11), GTK_SHADOW_IN);

  button16 = gtk_button_new_with_label ("button16");
  gtk_container_add (GTK_CONTAINER (frame11), button16);

  frame12 = gtk_frame_new (NULL);
  gtk_paned_pack2 (GTK_PANED (hpaned5), frame12, TRUE, TRUE);
  gtk_frame_set_shadow_type (GTK_FRAME (frame12), GTK_SHADOW_IN);

  button17 = gtk_button_new_with_label ("button17");
  gtk_container_add (GTK_CONTAINER (frame12), button17);

  return window3;
}

static GtkWidget*
paned_keyboard_window4 (GtkWidget *widget)
{
  GtkWidget *window4;
  GtkWidget *vbox3;
  GtkWidget *label2;
  GtkWidget *hpaned6;
  GtkWidget *vpaned3;
  GtkWidget *button19;
  GtkWidget *button18;
  GtkWidget *hbox3;
  GtkWidget *vpaned4;
  GtkWidget *button21;
  GtkWidget *button20;
  GtkWidget *vpaned5;
  GtkWidget *button23;
  GtkWidget *button22;
  GtkWidget *vpaned6;
  GtkWidget *button25;
  GtkWidget *button24;

  window4 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_set_data (G_OBJECT (window4), "window4", window4);
  gtk_window_set_title (GTK_WINDOW (window4), "window4");

  gtk_window_set_screen (GTK_WINDOW (window4), 
			 gtk_widget_get_screen (widget));

  vbox3 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window4), vbox3);

  label2 = gtk_label_new ("Widget tree:\n\nhpaned \n - vpaned\n - hbox\n    - vpaned\n    - vpaned\n    - vpaned\n");
  gtk_box_pack_start (GTK_BOX (vbox3), label2, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label2), GTK_JUSTIFY_LEFT);

  hpaned6 = gtk_hpaned_new ();
  gtk_box_pack_start (GTK_BOX (vbox3), hpaned6, TRUE, TRUE, 0);

  vpaned3 = gtk_vpaned_new ();
  gtk_paned_pack1 (GTK_PANED (hpaned6), vpaned3, FALSE, TRUE);

  button19 = gtk_button_new_with_label ("button19");
  gtk_paned_pack1 (GTK_PANED (vpaned3), button19, FALSE, TRUE);

  button18 = gtk_button_new_with_label ("button18");
  gtk_paned_pack2 (GTK_PANED (vpaned3), button18, TRUE, TRUE);

  hbox3 = gtk_hbox_new (FALSE, 0);
  gtk_paned_pack2 (GTK_PANED (hpaned6), hbox3, TRUE, TRUE);

  vpaned4 = gtk_vpaned_new ();
  gtk_box_pack_start (GTK_BOX (hbox3), vpaned4, TRUE, TRUE, 0);

  button21 = gtk_button_new_with_label ("button21");
  gtk_paned_pack1 (GTK_PANED (vpaned4), button21, FALSE, TRUE);

  button20 = gtk_button_new_with_label ("button20");
  gtk_paned_pack2 (GTK_PANED (vpaned4), button20, TRUE, TRUE);

  vpaned5 = gtk_vpaned_new ();
  gtk_box_pack_start (GTK_BOX (hbox3), vpaned5, TRUE, TRUE, 0);

  button23 = gtk_button_new_with_label ("button23");
  gtk_paned_pack1 (GTK_PANED (vpaned5), button23, FALSE, TRUE);

  button22 = gtk_button_new_with_label ("button22");
  gtk_paned_pack2 (GTK_PANED (vpaned5), button22, TRUE, TRUE);

  vpaned6 = gtk_vpaned_new ();
  gtk_box_pack_start (GTK_BOX (hbox3), vpaned6, TRUE, TRUE, 0);

  button25 = gtk_button_new_with_label ("button25");
  gtk_paned_pack1 (GTK_PANED (vpaned6), button25, FALSE, TRUE);

  button24 = gtk_button_new_with_label ("button24");
  gtk_paned_pack2 (GTK_PANED (vpaned6), button24, TRUE, TRUE);

  return window4;
}

static void
create_paned_keyboard_navigation (GtkWidget *widget)
{
  static GtkWidget *window1 = NULL;
  static GtkWidget *window2 = NULL;
  static GtkWidget *window3 = NULL;
  static GtkWidget *window4 = NULL;

  if (window1 && 
     (gtk_widget_get_screen (window1) != gtk_widget_get_screen (widget)))
    {
      gtk_widget_destroy (window1);
      gtk_widget_destroy (window2);
      gtk_widget_destroy (window3);
      gtk_widget_destroy (window4);
    }
  
  if (!window1)
    {
      window1 = paned_keyboard_window1 (widget);
      g_signal_connect (window1, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window1);
    }

  if (!window2)
    {
      window2 = paned_keyboard_window2 (widget);
      g_signal_connect (window2, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window2);
    }

  if (!window3)
    {
      window3 = paned_keyboard_window3 (widget);
      g_signal_connect (window3, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window3);
    }

  if (!window4)
    {
      window4 = paned_keyboard_window4 (widget);
      g_signal_connect (window4, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window4);
    }

  if (gtk_widget_get_visible (window1))
    gtk_widget_destroy (GTK_WIDGET (window1));
  else
    gtk_widget_show_all (GTK_WIDGET (window1));

  if (gtk_widget_get_visible (window2))
    gtk_widget_destroy (GTK_WIDGET (window2));
  else
    gtk_widget_show_all (GTK_WIDGET (window2));

  if (gtk_widget_get_visible (window3))
    gtk_widget_destroy (GTK_WIDGET (window3));
  else
    gtk_widget_show_all (GTK_WIDGET (window3));

  if (gtk_widget_get_visible (window4))
    gtk_widget_destroy (GTK_WIDGET (window4));
  else
    gtk_widget_show_all (GTK_WIDGET (window4));
}


/*
 * Shaped Windows
 */

typedef struct _cursoroffset {gint x,y;} CursorOffset;

static void
shape_pressed (GtkWidget *widget, GdkEventButton *event)
{
  CursorOffset *p;

  /* ignore double and triple click */
  if (event->type != GDK_BUTTON_PRESS)
    return;

  p = g_object_get_data (G_OBJECT (widget), "cursor_offset");
  p->x = (int) event->x;
  p->y = (int) event->y;

  gtk_grab_add (widget);
  gdk_pointer_grab (widget->window, TRUE,
		    GDK_BUTTON_RELEASE_MASK |
		    GDK_BUTTON_MOTION_MASK |
		    GDK_POINTER_MOTION_HINT_MASK,
		    NULL, NULL, 0);
}

static void
shape_released (GtkWidget *widget)
{
  gtk_grab_remove (widget);
  gdk_display_pointer_ungrab (gtk_widget_get_display (widget),
			      GDK_CURRENT_TIME);
}

static void
shape_motion (GtkWidget      *widget, 
	      GdkEventMotion *event)
{
  gint xp, yp;
  CursorOffset * p;
  GdkModifierType mask;

  p = g_object_get_data (G_OBJECT (widget), "cursor_offset");

  /*
   * Can't use event->x / event->y here 
   * because I need absolute coordinates.
   */
  gdk_window_get_pointer (NULL, &xp, &yp, &mask);
  gtk_widget_set_uposition (widget, xp  - p->x, yp  - p->y);
}

GtkWidget *
shape_create_icon (GdkScreen *screen,
		   char      *xpm_file,
		   gint       x,
		   gint       y,
		   gint       px,
		   gint       py,
		   gint       window_type)
{
  GtkWidget *window;
  GtkWidget *pixmap;
  GtkWidget *fixed;
  CursorOffset* icon_pos;
  GdkBitmap *gdk_pixmap_mask;
  GdkPixmap *gdk_pixmap;
  GtkStyle *style;

  style = gtk_widget_get_default_style ();

  /*
   * GDK_WINDOW_TOPLEVEL works also, giving you a title border
   */
  window = gtk_window_new (window_type);
  gtk_window_set_screen (GTK_WINDOW (window), screen);
  
  fixed = gtk_fixed_new ();
  gtk_widget_set_size_request (fixed, 100, 100);
  gtk_container_add (GTK_CONTAINER (window), fixed);
  gtk_widget_show (fixed);
  
  gtk_widget_set_events (window, 
			 gtk_widget_get_events (window) |
			 GDK_BUTTON_MOTION_MASK |
			 GDK_POINTER_MOTION_HINT_MASK |
			 GDK_BUTTON_PRESS_MASK);

  gtk_widget_realize (window);
  gdk_pixmap = gdk_pixmap_create_from_xpm (window->window, &gdk_pixmap_mask, 
					   &style->bg[GTK_STATE_NORMAL],
					   xpm_file);

  pixmap = gtk_image_new_from_pixmap (gdk_pixmap, gdk_pixmap_mask);
  gtk_fixed_put (GTK_FIXED (fixed), pixmap, px,py);
  gtk_widget_show (pixmap);
  
  gtk_widget_shape_combine_mask (window, gdk_pixmap_mask, px, py);
  
  g_object_unref (gdk_pixmap_mask);
  g_object_unref (gdk_pixmap);

  g_signal_connect (window, "button_press_event",
		    G_CALLBACK (shape_pressed), NULL);
  g_signal_connect (window, "button_release_event",
		    G_CALLBACK (shape_released), NULL);
  g_signal_connect (window, "motion_notify_event",
		    G_CALLBACK (shape_motion), NULL);

  icon_pos = g_new (CursorOffset, 1);
  g_object_set_data (G_OBJECT (window), "cursor_offset", icon_pos);

  gtk_widget_set_uposition (window, x, y);
  gtk_widget_show (window);
  
  return window;
}

void 
create_shapes (GtkWidget *widget)
{
  /* Variables used by the Drag/Drop and Shape Window demos */
  static GtkWidget *modeller = NULL;
  static GtkWidget *sheets = NULL;
  static GtkWidget *rings = NULL;
  static GtkWidget *with_region = NULL;
  GdkScreen *screen = gtk_widget_get_screen (widget);
  
  if (!(file_exists ("Modeller.xpm") &&
	file_exists ("FilesQueue.xpm") &&
	file_exists ("3DRings.xpm")))
    return;
  

  if (!modeller)
    {
      modeller = shape_create_icon (screen, "Modeller.xpm",
				    440, 140, 0,0, GTK_WINDOW_POPUP);

      g_signal_connect (modeller, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&modeller);
    }
  else
    gtk_widget_destroy (modeller);

  if (!sheets)
    {
      sheets = shape_create_icon (screen, "FilesQueue.xpm",
				  580, 170, 0,0, GTK_WINDOW_POPUP);

      g_signal_connect (sheets, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&sheets);

    }
  else
    gtk_widget_destroy (sheets);

  if (!rings)
    {
      rings = shape_create_icon (screen, "3DRings.xpm",
				 460, 270, 25,25, GTK_WINDOW_TOPLEVEL);

      g_signal_connect (rings, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&rings);
    }
  else
    gtk_widget_destroy (rings);

  if (!with_region)
    {
      GdkRegion *region;
      gint x, y;
      
      with_region = shape_create_icon (screen, "3DRings.xpm",
                                       460, 270, 25,25, GTK_WINDOW_TOPLEVEL);

      gtk_window_set_decorated (GTK_WINDOW (with_region), FALSE);
      
      g_signal_connect (with_region, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&with_region);

      /* reset shape from mask to a region */
      x = 0;
      y = 0;
      region = gdk_region_new ();

      while (x < 460)
        {
          while (y < 270)
            {
              GdkRectangle rect;
              rect.x = x;
              rect.y = y;
              rect.width = 10;
              rect.height = 10;

              gdk_region_union_with_rect (region, &rect);
              
              y += 20;
            }
          y = 0;
          x += 20;
        }

      gdk_window_shape_combine_region (with_region->window,
                                       region,
                                       0, 0);
    }
  else
    gtk_widget_destroy (with_region);
}

/*
 * WM Hints demo
 */

void
create_wmhints (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *label;
  GtkWidget *separator;
  GtkWidget *button;
  GtkWidget *box1;
  GtkWidget *box2;

  GdkBitmap *circles;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));
      
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "WM Hints");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      gtk_widget_realize (window);
      
      circles = gdk_bitmap_create_from_data (window->window,
					     (gchar *) circles_bits,
					     circles_width,
					     circles_height);
      gdk_window_set_icon (window->window, NULL,
			   circles, circles);
      
      gdk_window_set_icon_name (window->window, "WMHints Test Icon");
  
      gdk_window_set_decorations (window->window, GDK_DECOR_ALL | GDK_DECOR_MENU);
      gdk_window_set_functions (window->window, GDK_FUNC_ALL | GDK_FUNC_RESIZE);
      
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);

      label = gtk_label_new ("Try iconizing me!");
      gtk_widget_set_size_request (label, 150, 50);
      gtk_box_pack_start (GTK_BOX (box1), label, TRUE, TRUE, 0);
      gtk_widget_show (label);


      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("close");

      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_destroy),
				window);

      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}


/*
 * Window state tracking
 */

static gint
window_state_callback (GtkWidget *widget,
                       GdkEventWindowState *event,
                       gpointer data)
{
  GtkWidget *label = data;
  gchar *msg;

  msg = g_strconcat (GTK_WINDOW (widget)->title, ": ",
                     (event->new_window_state & GDK_WINDOW_STATE_WITHDRAWN) ?
                     "withdrawn" : "not withdrawn", ", ",
                     (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) ?
                     "iconified" : "not iconified", ", ",
                     (event->new_window_state & GDK_WINDOW_STATE_STICKY) ?
                     "sticky" : "not sticky", ", ",
                     (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) ?
                     "maximized" : "not maximized", ", ",
                     (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) ?
                     "fullscreen" : "not fullscreen",
                     (event->new_window_state & GDK_WINDOW_STATE_ABOVE) ?
                     "above" : "not above", ", ",
                     (event->new_window_state & GDK_WINDOW_STATE_BELOW) ?
                     "below" : "not below", ", ",
                     NULL);
  
  gtk_label_set_text (GTK_LABEL (label), msg);

  g_free (msg);

  return FALSE;
}

static GtkWidget*
tracking_label (GtkWidget *window)
{
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *button;

  hbox = gtk_hbox_new (FALSE, 5);

  g_signal_connect_object (hbox,
			   "destroy",
			   G_CALLBACK (gtk_widget_destroy),
			   window,
			   G_CONNECT_SWAPPED);
  
  label = gtk_label_new ("<no window state events received>");
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  
  g_signal_connect (window,
		    "window_state_event",
		    G_CALLBACK (window_state_callback),
		    label);

  button = gtk_button_new_with_label ("Deiconify");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_deiconify),
                           window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Iconify");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_iconify),
                           window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Fullscreen");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_fullscreen),
                           window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Unfullscreen");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_unfullscreen),
                           window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  
  button = gtk_button_new_with_label ("Present");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_present),
                           window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Show");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_widget_show),
                           window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  
  gtk_widget_show_all (hbox);
  
  return hbox;
}

void
keep_window_above (GtkToggleButton *togglebutton, gpointer data)
{
  GtkWidget *button = g_object_get_data (G_OBJECT (togglebutton), "radio");

  gtk_window_set_keep_above (GTK_WINDOW (data),
                             gtk_toggle_button_get_active (togglebutton));

  if (gtk_toggle_button_get_active (togglebutton))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
}

void
keep_window_below (GtkToggleButton *togglebutton, gpointer data)
{
  GtkWidget *button = g_object_get_data (G_OBJECT (togglebutton), "radio");

  gtk_window_set_keep_below (GTK_WINDOW (data),
                             gtk_toggle_button_get_active (togglebutton));

  if (gtk_toggle_button_get_active (togglebutton))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
}


static GtkWidget*
get_state_controls (GtkWidget *window)
{
  GtkWidget *vbox;
  GtkWidget *button;
  GtkWidget *button_above;
  GtkWidget *button_below;

  vbox = gtk_vbox_new (FALSE, 0);
  
  button = gtk_button_new_with_label ("Stick");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_stick),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Unstick");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_unstick),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  button = gtk_button_new_with_label ("Maximize");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_maximize),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Unmaximize");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_unmaximize),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Iconify");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_iconify),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Fullscreen");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_fullscreen),
                           window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Unfullscreen");
  g_signal_connect_object (button,
			   "clicked",
                           G_CALLBACK (gtk_window_unfullscreen),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button_above = gtk_toggle_button_new_with_label ("Keep above");
  g_signal_connect (button_above,
		    "toggled",
		    G_CALLBACK (keep_window_above),
		    window);
  gtk_box_pack_start (GTK_BOX (vbox), button_above, FALSE, FALSE, 0);

  button_below = gtk_toggle_button_new_with_label ("Keep below");
  g_signal_connect (button_below,
		    "toggled",
		    G_CALLBACK (keep_window_below),
		    window);
  gtk_box_pack_start (GTK_BOX (vbox), button_below, FALSE, FALSE, 0);

  g_object_set_data (G_OBJECT (button_above), "radio", button_below);
  g_object_set_data (G_OBJECT (button_below), "radio", button_above);

  button = gtk_button_new_with_label ("Hide (withdraw)");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_widget_hide),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  gtk_widget_show_all (vbox);

  return vbox;
}

void
create_window_states (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *label;
  GtkWidget *box1;
  GtkWidget *iconified;
  GtkWidget *normal;
  GtkWidget *controls;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "Window states");
      
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      iconified = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (iconified),
			     gtk_widget_get_screen (widget));
      
      g_signal_connect_object (iconified, "destroy",
			       G_CALLBACK (gtk_widget_destroy),
			       window,
			       G_CONNECT_SWAPPED);
      gtk_window_iconify (GTK_WINDOW (iconified));
      gtk_window_set_title (GTK_WINDOW (iconified), "Iconified initially");
      controls = get_state_controls (iconified);
      gtk_container_add (GTK_CONTAINER (iconified), controls);
      
      normal = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (normal),
			     gtk_widget_get_screen (widget));
      
      g_signal_connect_object (normal, "destroy",
			       G_CALLBACK (gtk_widget_destroy),
			       window,
			       G_CONNECT_SWAPPED);
      
      gtk_window_set_title (GTK_WINDOW (normal), "Deiconified initially");
      controls = get_state_controls (normal);
      gtk_container_add (GTK_CONTAINER (normal), controls);
      
      label = tracking_label (iconified);
      gtk_container_add (GTK_CONTAINER (box1), label);

      label = tracking_label (normal);
      gtk_container_add (GTK_CONTAINER (box1), label);

      gtk_widget_show_all (iconified);
      gtk_widget_show_all (normal);
      gtk_widget_show_all (box1);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Window sizing
 */

static gint
configure_event_callback (GtkWidget *widget,
                          GdkEventConfigure *event,
                          gpointer data)
{
  GtkWidget *label = data;
  gchar *msg;
  gint x, y;
  
  gtk_window_get_position (GTK_WINDOW (widget), &x, &y);
  
  msg = g_strdup_printf ("event: %d,%d  %d x %d\n"
                         "position: %d, %d",
                         event->x, event->y, event->width, event->height,
                         x, y);
  
  gtk_label_set_text (GTK_LABEL (label), msg);

  g_free (msg);

  return FALSE;
}

static void
get_ints (GtkWidget *window,
          gint      *a,
          gint      *b)
{
  GtkWidget *spin1;
  GtkWidget *spin2;

  spin1 = g_object_get_data (G_OBJECT (window), "spin1");
  spin2 = g_object_get_data (G_OBJECT (window), "spin2");

  *a = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin1));
  *b = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin2));
}

static void
set_size_callback (GtkWidget *widget,
                   gpointer   data)
{
  gint w, h;
  
  get_ints (data, &w, &h);

  gtk_window_resize (GTK_WINDOW (g_object_get_data (data, "target")), w, h);
}

static void
unset_default_size_callback (GtkWidget *widget,
                             gpointer   data)
{
  gtk_window_set_default_size (g_object_get_data (data, "target"),
                               -1, -1);
}

static void
set_default_size_callback (GtkWidget *widget,
                           gpointer   data)
{
  gint w, h;
  
  get_ints (data, &w, &h);

  gtk_window_set_default_size (g_object_get_data (data, "target"),
                               w, h);
}

static void
unset_size_request_callback (GtkWidget *widget,
			     gpointer   data)
{
  gtk_widget_set_size_request (g_object_get_data (data, "target"),
                               -1, -1);
}

static void
set_size_request_callback (GtkWidget *widget,
			   gpointer   data)
{
  gint w, h;
  
  get_ints (data, &w, &h);

  gtk_widget_set_size_request (g_object_get_data (data, "target"),
                               w, h);
}

static void
set_location_callback (GtkWidget *widget,
                       gpointer   data)
{
  gint x, y;
  
  get_ints (data, &x, &y);

  gtk_window_move (g_object_get_data (data, "target"), x, y);
}

static void
move_to_position_callback (GtkWidget *widget,
                           gpointer   data)
{
  gint x, y;
  GtkWindow *window;

  window = g_object_get_data (data, "target");
  
  gtk_window_get_position (window, &x, &y);

  gtk_window_move (window, x, y);
}

static void
set_geometry_callback (GtkWidget *entry,
                       gpointer   data)
{
  gchar *text;
  GtkWindow *target;

  target = GTK_WINDOW (g_object_get_data (G_OBJECT (data), "target"));
  
  text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);

  if (!gtk_window_parse_geometry (target, text))
    g_print ("Bad geometry string '%s'\n", text);

  g_free (text);
}

static void
allow_shrink_callback (GtkWidget *widget,
                       gpointer   data)
{
  g_object_set (g_object_get_data (data, "target"),
                "allow_shrink",
                GTK_TOGGLE_BUTTON (widget)->active,
                NULL);
}

static void
allow_grow_callback (GtkWidget *widget,
                     gpointer   data)
{
  g_object_set (g_object_get_data (data, "target"),
                "allow_grow",
                GTK_TOGGLE_BUTTON (widget)->active,
                NULL);
}

static void
gravity_selected (GtkWidget *widget,
                  gpointer   data)
{
  gtk_window_set_gravity (GTK_WINDOW (g_object_get_data (data, "target")),
                          gtk_option_menu_get_history (GTK_OPTION_MENU (widget)) + GDK_GRAVITY_NORTH_WEST);
}

static void
pos_selected (GtkWidget *widget,
              gpointer   data)
{
  gtk_window_set_position (GTK_WINDOW (g_object_get_data (data, "target")),
                           gtk_option_menu_get_history (GTK_OPTION_MENU (widget)) + GTK_WIN_POS_NONE);
}

static void
move_gravity_window_to_current_position (GtkWidget *widget,
                                         gpointer   data)
{
  gint x, y;
  GtkWindow *window;

  window = GTK_WINDOW (data);    
  
  gtk_window_get_position (window, &x, &y);

  gtk_window_move (window, x, y);
}

static void
get_screen_corner (GtkWindow *window,
                   gint      *x,
                   gint      *y)
{
  int w, h;
  GdkScreen * screen = gtk_window_get_screen (window);
  
  gtk_window_get_size (GTK_WINDOW (window), &w, &h);

  switch (gtk_window_get_gravity (window))
    {
    case GDK_GRAVITY_SOUTH_EAST:
      *x = gdk_screen_get_width (screen) - w;
      *y = gdk_screen_get_height (screen) - h;
      break;

    case GDK_GRAVITY_NORTH_EAST:
      *x = gdk_screen_get_width (screen) - w;
      *y = 0;
      break;

    case GDK_GRAVITY_SOUTH_WEST:
      *x = 0;
      *y = gdk_screen_get_height (screen) - h;
      break;

    case GDK_GRAVITY_NORTH_WEST:
      *x = 0;
      *y = 0;
      break;
      
    case GDK_GRAVITY_SOUTH:
      *x = (gdk_screen_get_width (screen) - w) / 2;
      *y = gdk_screen_get_height (screen) - h;
      break;

    case GDK_GRAVITY_NORTH:
      *x = (gdk_screen_get_width (screen) - w) / 2;
      *y = 0;
      break;

    case GDK_GRAVITY_WEST:
      *x = 0;
      *y = (gdk_screen_get_height (screen) - h) / 2;
      break;

    case GDK_GRAVITY_EAST:
      *x = gdk_screen_get_width (screen) - w;
      *y = (gdk_screen_get_height (screen) - h) / 2;
      break;

    case GDK_GRAVITY_CENTER:
      *x = (gdk_screen_get_width (screen) - w) / 2;
      *y = (gdk_screen_get_height (screen) - h) / 2;
      break;

    case GDK_GRAVITY_STATIC:
      /* pick some random numbers */
      *x = 350;
      *y = 350;
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static void
move_gravity_window_to_starting_position (GtkWidget *widget,
                                          gpointer   data)
{
  gint x, y;
  GtkWindow *window;

  window = GTK_WINDOW (data);    
  
  get_screen_corner (window,
                     &x, &y);
  
  gtk_window_move (window, x, y);
}

static GtkWidget*
make_gravity_window (GtkWidget   *destroy_with,
                     GdkGravity   gravity,
                     const gchar *title)
{
  GtkWidget *window;
  GtkWidget *button;
  GtkWidget *vbox;
  int x, y;
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_screen (GTK_WINDOW (window),
			 gtk_widget_get_screen (destroy_with));

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox);
  
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_window_set_title (GTK_WINDOW (window), title);
  gtk_window_set_gravity (GTK_WINDOW (window), gravity);

  g_signal_connect_object (destroy_with,
			   "destroy",
			   G_CALLBACK (gtk_widget_destroy),
			   window,
			   G_CONNECT_SWAPPED);

  
  button = gtk_button_new_with_mnemonic ("_Move to current position");

  g_signal_connect (button, "clicked",
                    G_CALLBACK (move_gravity_window_to_current_position),
                    window);

  gtk_container_add (GTK_CONTAINER (vbox), button);
  gtk_widget_show (button);

  button = gtk_button_new_with_mnemonic ("Move to _starting position");

  g_signal_connect (button, "clicked",
                    G_CALLBACK (move_gravity_window_to_starting_position),
                    window);

  gtk_container_add (GTK_CONTAINER (vbox), button);
  gtk_widget_show (button);
  
  /* Pretend this is the result of --geometry.
   * DO NOT COPY THIS CODE unless you are setting --geometry results,
   * and in that case you probably should just use gtk_window_parse_geometry().
   * AGAIN, DO NOT SET GDK_HINT_USER_POS! It violates the ICCCM unless
   * you are parsing --geometry or equivalent.
   */
  gtk_window_set_geometry_hints (GTK_WINDOW (window),
                                 NULL, NULL,
                                 GDK_HINT_USER_POS);

  gtk_window_set_default_size (GTK_WINDOW (window),
                               200, 200);

  get_screen_corner (GTK_WINDOW (window), &x, &y);
  
  gtk_window_move (GTK_WINDOW (window),
                   x, y);
  
  return window;
}

static void
do_gravity_test (GtkWidget *widget,
                 gpointer   data)
{
  GtkWidget *destroy_with = data;
  GtkWidget *window;
  
  /* We put a window at each gravity point on the screen. */
  window = make_gravity_window (destroy_with, GDK_GRAVITY_NORTH_WEST,
                                "NorthWest");
  gtk_widget_show (window);
  
  window = make_gravity_window (destroy_with, GDK_GRAVITY_SOUTH_EAST,
                                "SouthEast");
  gtk_widget_show (window);

  window = make_gravity_window (destroy_with, GDK_GRAVITY_NORTH_EAST,
                                "NorthEast");
  gtk_widget_show (window);

  window = make_gravity_window (destroy_with, GDK_GRAVITY_SOUTH_WEST,
                                "SouthWest");
  gtk_widget_show (window);

  window = make_gravity_window (destroy_with, GDK_GRAVITY_SOUTH,
                                "South");
  gtk_widget_show (window);

  window = make_gravity_window (destroy_with, GDK_GRAVITY_NORTH,
                                "North");
  gtk_widget_show (window);

  
  window = make_gravity_window (destroy_with, GDK_GRAVITY_WEST,
                                "West");
  gtk_widget_show (window);

    
  window = make_gravity_window (destroy_with, GDK_GRAVITY_EAST,
                                "East");
  gtk_widget_show (window);

  window = make_gravity_window (destroy_with, GDK_GRAVITY_CENTER,
                                "Center");
  gtk_widget_show (window);

  window = make_gravity_window (destroy_with, GDK_GRAVITY_STATIC,
                                "Static");
  gtk_widget_show (window);
}

static GtkWidget*
window_controls (GtkWidget *window)
{
  GtkWidget *control_window;
  GtkWidget *label;
  GtkWidget *vbox;
  GtkWidget *button;
  GtkWidget *spin;
  GtkAdjustment *adj;
  GtkWidget *entry;
  GtkWidget *om;
  gint i;
  
  control_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_screen (GTK_WINDOW (control_window),
			 gtk_widget_get_screen (window));

  gtk_window_set_title (GTK_WINDOW (control_window), "Size controls");
  
  g_object_set_data (G_OBJECT (control_window),
                     "target",
                     window);
  
  g_signal_connect_object (control_window,
			   "destroy",
			   G_CALLBACK (gtk_widget_destroy),
                           window,
			   G_CONNECT_SWAPPED);

  vbox = gtk_vbox_new (FALSE, 5);
  
  gtk_container_add (GTK_CONTAINER (control_window), vbox);
  
  label = gtk_label_new ("<no configure events>");
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  
  g_signal_connect (window,
		    "configure_event",
		    G_CALLBACK (configure_event_callback),
		    label);

  adj = (GtkAdjustment *) gtk_adjustment_new (10.0, -2000.0, 2000.0, 1.0,
                                              5.0, 0.0);
  spin = gtk_spin_button_new (adj, 0, 0);

  gtk_box_pack_start (GTK_BOX (vbox), spin, FALSE, FALSE, 0);

  g_object_set_data (G_OBJECT (control_window), "spin1", spin);

  adj = (GtkAdjustment *) gtk_adjustment_new (10.0, -2000.0, 2000.0, 1.0,
                                              5.0, 0.0);
  spin = gtk_spin_button_new (adj, 0, 0);

  gtk_box_pack_start (GTK_BOX (vbox), spin, FALSE, FALSE, 0);

  g_object_set_data (G_OBJECT (control_window), "spin2", spin);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

  g_signal_connect (entry, "changed",
		    G_CALLBACK (set_geometry_callback),
		    control_window);

  button = gtk_button_new_with_label ("Show gravity test windows");
  g_signal_connect_swapped (button,
			    "clicked",
			    G_CALLBACK (do_gravity_test),
			    control_window);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Reshow with initial size");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_reshow_with_initial_size),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  button = gtk_button_new_with_label ("Queue resize");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_widget_queue_resize),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  button = gtk_button_new_with_label ("Resize");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (set_size_callback),
		    control_window);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Set default size");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (set_default_size_callback),
		    control_window);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Unset default size");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (unset_default_size_callback),
                    control_window);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  button = gtk_button_new_with_label ("Set size request");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (set_size_request_callback),
		    control_window);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Unset size request");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (unset_size_request_callback),
                    control_window);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  button = gtk_button_new_with_label ("Move");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (set_location_callback),
		    control_window);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Move to current position");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (move_to_position_callback),
		    control_window);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  button = gtk_check_button_new_with_label ("Allow shrink");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
  g_signal_connect (button,
		    "toggled",
		    G_CALLBACK (allow_shrink_callback),
		    control_window);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_check_button_new_with_label ("Allow grow");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  g_signal_connect (button,
		    "toggled",
		    G_CALLBACK (allow_grow_callback),
                    control_window);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  button = gtk_button_new_with_mnemonic ("_Show");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_widget_show),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_mnemonic ("_Hide");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_widget_hide),
                           window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  om = gtk_combo_box_text_new ();
  i = 0;
  while (i < 10)
    {
      static gchar *names[] = {
        "GDK_GRAVITY_NORTH_WEST",
        "GDK_GRAVITY_NORTH",
        "GDK_GRAVITY_NORTH_EAST",
        "GDK_GRAVITY_WEST",
        "GDK_GRAVITY_CENTER",
        "GDK_GRAVITY_EAST",
        "GDK_GRAVITY_SOUTH_WEST",
        "GDK_GRAVITY_SOUTH",
        "GDK_GRAVITY_SOUTH_EAST",
        "GDK_GRAVITY_STATIC",
        NULL
      };

      g_assert (names[i]);
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (om), names[i]);

      ++i;
    }
  
  g_signal_connect (om,
		    "changed",
		    G_CALLBACK (gravity_selected),
		    control_window);

  gtk_box_pack_end (GTK_BOX (vbox), om, FALSE, FALSE, 0);


  om = gtk_combo_box_text_new ();
  i = 0;
  while (i < 5)
    {
      static gchar *names[] = {
        "GTK_WIN_POS_NONE",
        "GTK_WIN_POS_CENTER",
        "GTK_WIN_POS_MOUSE",
        "GTK_WIN_POS_CENTER_ALWAYS",
        "GTK_WIN_POS_CENTER_ON_PARENT",
        NULL
      };

      g_assert (names[i]);
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (om), names[i]);

      ++i;
    }
  
  g_signal_connect (om,
		    "changed",
		    G_CALLBACK (pos_selected),
		    control_window);

  gtk_box_pack_end (GTK_BOX (vbox), om, FALSE, FALSE, 0);
  
  gtk_widget_show_all (vbox);
  
  return control_window;
}

void
create_window_sizing (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  static GtkWidget *target_window = NULL;

  if (!target_window)
    {
      GtkWidget *label;
      
      target_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (target_window),
			     gtk_widget_get_screen (widget));
      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label), "<span foreground=\"purple\"><big>Window being resized</big></span>\nBlah blah blah blah\nblah blah blah\nblah blah blah blah blah");
      gtk_container_add (GTK_CONTAINER (target_window), label);
      gtk_widget_show (label);
      
      g_signal_connect (target_window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&target_window);

      window = window_controls (target_window);
      
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);
      
      gtk_window_set_title (GTK_WINDOW (target_window), "Window to size");
    }

  /* don't show target window by default, we want to allow testing
   * of behavior on first show.
   */
  
  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkProgressBar
 */

typedef struct _ProgressData {
  GtkWidget *window;
  GtkWidget *pbar;
  GtkWidget *block_spin;
  GtkWidget *x_align_spin;
  GtkWidget *y_align_spin;
  GtkWidget *step_spin;
  GtkWidget *act_blocks_spin;
  GtkWidget *label;
  GtkWidget *omenu1;
  GtkWidget *elmenu;
  GtkWidget *omenu2;
  GtkWidget *entry;
  int timer;
} ProgressData;

gint
progress_timeout (gpointer data)
{
  gdouble new_val;
  GtkAdjustment *adj;

  adj = GTK_PROGRESS (data)->adjustment;

  new_val = adj->value + 1;
  if (new_val > adj->upper)
    new_val = adj->lower;

  gtk_progress_set_value (GTK_PROGRESS (data), new_val);

  return TRUE;
}

static void
destroy_progress (GtkWidget     *widget,
		  ProgressData **pdata)
{
  gtk_timeout_remove ((*pdata)->timer);
  (*pdata)->timer = 0;
  (*pdata)->window = NULL;
  g_free (*pdata);
  *pdata = NULL;
}

static void
progressbar_toggle_orientation (GtkWidget *widget, gpointer data)
{
  ProgressData *pdata;
  gint i;

  pdata = (ProgressData *) data;

  if (!gtk_widget_get_mapped (widget))
    return;

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));

  gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (pdata->pbar),
				    (GtkProgressBarOrientation) i);
}

static void
toggle_show_text (GtkWidget *widget, ProgressData *pdata)
{
  gtk_progress_set_show_text (GTK_PROGRESS (pdata->pbar),
			      GTK_TOGGLE_BUTTON (widget)->active);
  gtk_widget_set_sensitive (pdata->entry, GTK_TOGGLE_BUTTON (widget)->active);
  gtk_widget_set_sensitive (pdata->x_align_spin,
			    GTK_TOGGLE_BUTTON (widget)->active);
  gtk_widget_set_sensitive (pdata->y_align_spin,
			    GTK_TOGGLE_BUTTON (widget)->active);
}

static void
progressbar_toggle_ellipsize (GtkWidget *widget,
                              gpointer   data)
{
  ProgressData *pdata = data;
  if (gtk_widget_is_drawable (widget))
    {
      gint i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));
      gtk_progress_bar_set_ellipsize (GTK_PROGRESS_BAR (pdata->pbar), i);
    }
}

static void
progressbar_toggle_bar_style (GtkWidget *widget, gpointer data)
{
  ProgressData *pdata;
  gint i;

  pdata = (ProgressData *) data;

  if (!gtk_widget_get_mapped (widget))
    return;

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));

  if (i == 1)
    gtk_widget_set_sensitive (pdata->block_spin, TRUE);
  else
    gtk_widget_set_sensitive (pdata->block_spin, FALSE);
  
  gtk_progress_bar_set_bar_style (GTK_PROGRESS_BAR (pdata->pbar),
				  (GtkProgressBarStyle) i);
}

static void
progress_value_changed (GtkAdjustment *adj, ProgressData *pdata)
{
  char buf[20];

  if (GTK_PROGRESS (pdata->pbar)->activity_mode)
    sprintf (buf, "???");
  else
    sprintf (buf, "%.0f%%", 100 *
	     gtk_progress_get_current_percentage (GTK_PROGRESS (pdata->pbar)));
  gtk_label_set_text (GTK_LABEL (pdata->label), buf);
}

static void
adjust_blocks (GtkAdjustment *adj, ProgressData *pdata)
{
  gtk_progress_set_percentage (GTK_PROGRESS (pdata->pbar), 0);
  gtk_progress_bar_set_discrete_blocks (GTK_PROGRESS_BAR (pdata->pbar),
     gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (pdata->block_spin)));
}

static void
adjust_step (GtkAdjustment *adj, ProgressData *pdata)
{
  gtk_progress_bar_set_activity_step (GTK_PROGRESS_BAR (pdata->pbar),
     gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (pdata->step_spin)));
}

static void
adjust_act_blocks (GtkAdjustment *adj, ProgressData *pdata)
{
  gtk_progress_bar_set_activity_blocks (GTK_PROGRESS_BAR (pdata->pbar),
               gtk_spin_button_get_value_as_int 
		      (GTK_SPIN_BUTTON (pdata->act_blocks_spin)));
}

static void
adjust_align (GtkAdjustment *adj, ProgressData *pdata)
{
  gtk_progress_set_text_alignment (GTK_PROGRESS (pdata->pbar),
	 gtk_spin_button_get_value (GTK_SPIN_BUTTON (pdata->x_align_spin)),
	 gtk_spin_button_get_value (GTK_SPIN_BUTTON (pdata->y_align_spin)));
}

static void
toggle_activity_mode (GtkWidget *widget, ProgressData *pdata)
{
  gtk_progress_set_activity_mode (GTK_PROGRESS (pdata->pbar),
				  GTK_TOGGLE_BUTTON (widget)->active);
  gtk_widget_set_sensitive (pdata->step_spin, 
			    GTK_TOGGLE_BUTTON (widget)->active);
  gtk_widget_set_sensitive (pdata->act_blocks_spin, 
			    GTK_TOGGLE_BUTTON (widget)->active);
}

static void
entry_changed (GtkWidget *widget, ProgressData *pdata)
{
  gtk_progress_set_format_string (GTK_PROGRESS (pdata->pbar),
			  gtk_entry_get_text (GTK_ENTRY (pdata->entry)));
}

void
create_progress_bar (GtkWidget *widget)
{
  GtkWidget *button;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *hbox;
  GtkWidget *check;
  GtkWidget *frame;
  GtkWidget *tab;
  GtkWidget *label;
  GtkWidget *align;
  GtkAdjustment *adj;
  static ProgressData *pdata = NULL;

  static gchar *items1[] =
  {
    "Left-Right",
    "Right-Left",
    "Bottom-Top",
    "Top-Bottom"
  };

  static gchar *items2[] =
  {
    "Continuous",
    "Discrete"
  };

  static char *ellipsize_items[] = {
    "None",     // PANGO_ELLIPSIZE_NONE,
    "Start",    // PANGO_ELLIPSIZE_START,
    "Middle",   // PANGO_ELLIPSIZE_MIDDLE,
    "End",      // PANGO_ELLIPSIZE_END
  };
  
  if (!pdata)
    pdata = g_new0 (ProgressData, 1);

  if (!pdata->window)
    {
      pdata->window = gtk_dialog_new ();

      gtk_window_set_screen (GTK_WINDOW (pdata->window),
			     gtk_widget_get_screen (widget));

      gtk_window_set_resizable (GTK_WINDOW (pdata->window), TRUE);

      g_signal_connect (pdata->window, "destroy",
			G_CALLBACK (destroy_progress),
			&pdata);

      pdata->timer = 0;

      gtk_window_set_title (GTK_WINDOW (pdata->window), "GtkProgressBar");
      gtk_container_set_border_width (GTK_CONTAINER (pdata->window), 0);

      vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (pdata->window)->vbox), 
			  vbox, FALSE, TRUE, 0);

      frame = gtk_frame_new ("Progress");
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);

      vbox2 = gtk_vbox_new (FALSE, 5);
      gtk_container_add (GTK_CONTAINER (frame), vbox2);

      align = gtk_alignment_new (0.5, 0.5, 0, 0);
      gtk_box_pack_start (GTK_BOX (vbox2), align, FALSE, FALSE, 5);

      adj = (GtkAdjustment *) gtk_adjustment_new (0, 1, 300, 0, 0, 0);
      g_signal_connect (adj, "value_changed",
			G_CALLBACK (progress_value_changed), pdata);

      pdata->pbar = g_object_new (GTK_TYPE_PROGRESS_BAR,
				    "adjustment", adj,
				    "ellipsize", PANGO_ELLIPSIZE_MIDDLE,
				    NULL);
      gtk_progress_set_format_string (GTK_PROGRESS (pdata->pbar),
				      "%v from [%l,%u] (=%p%%)");
      gtk_container_add (GTK_CONTAINER (align), pdata->pbar);
      pdata->timer = gtk_timeout_add (100, progress_timeout, pdata->pbar);

      align = gtk_alignment_new (0.5, 0.5, 0, 0);
      gtk_box_pack_start (GTK_BOX (vbox2), align, FALSE, FALSE, 5);

      hbox = gtk_hbox_new (FALSE, 5);
      gtk_container_add (GTK_CONTAINER (align), hbox);
      label = gtk_label_new ("Label updated by user :"); 
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      pdata->label = gtk_label_new ("");
      gtk_box_pack_start (GTK_BOX (hbox), pdata->label, FALSE, TRUE, 0);

      frame = gtk_frame_new ("Options");
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);

      vbox2 = gtk_vbox_new (FALSE, 5);
      gtk_container_add (GTK_CONTAINER (frame), vbox2);

      tab = gtk_table_new (7, 2, FALSE);
      gtk_box_pack_start (GTK_BOX (vbox2), tab, FALSE, TRUE, 0);

      label = gtk_label_new ("Orientation :");
      gtk_table_attach (GTK_TABLE (tab), label, 0, 1, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

      pdata->omenu1 = build_option_menu (items1, 4, 0,
					 progressbar_toggle_orientation,
					 pdata);
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (tab), hbox, 1, 2, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->omenu1, TRUE, TRUE, 0);
      
      check = gtk_check_button_new_with_label ("Show text");
      g_signal_connect (check, "clicked",
			G_CALLBACK (toggle_show_text),
			pdata);
      gtk_table_attach (GTK_TABLE (tab), check, 0, 1, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (tab), hbox, 1, 2, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);

      label = gtk_label_new ("Format : ");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

      pdata->entry = gtk_entry_new ();
      g_signal_connect (pdata->entry, "changed",
			G_CALLBACK (entry_changed),
			pdata);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->entry, TRUE, TRUE, 0);
      gtk_entry_set_text (GTK_ENTRY (pdata->entry), "%v from [%l,%u] (=%p%%)");
      gtk_widget_set_size_request (pdata->entry, 100, -1);
      gtk_widget_set_sensitive (pdata->entry, FALSE);

      label = gtk_label_new ("Text align :");
      gtk_table_attach (GTK_TABLE (tab), label, 0, 1, 2, 3,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (tab), hbox, 1, 2, 2, 3,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);

      label = gtk_label_new ("x :");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 5);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (0.5, 0, 1, 0.1, 0.1, 0);
      pdata->x_align_spin = gtk_spin_button_new (adj, 0, 1);
      g_signal_connect (adj, "value_changed",
			G_CALLBACK (adjust_align), pdata);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->x_align_spin, FALSE, TRUE, 0);
      gtk_widget_set_sensitive (pdata->x_align_spin, FALSE);

      label = gtk_label_new ("y :");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 5);

      adj = (GtkAdjustment *) gtk_adjustment_new (0.5, 0, 1, 0.1, 0.1, 0);
      pdata->y_align_spin = gtk_spin_button_new (adj, 0, 1);
      g_signal_connect (adj, "value_changed",
			G_CALLBACK (adjust_align), pdata);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->y_align_spin, FALSE, TRUE, 0);
      gtk_widget_set_sensitive (pdata->y_align_spin, FALSE);

      label = gtk_label_new ("Ellipsize text :");
      gtk_table_attach (GTK_TABLE (tab), label, 0, 1, 10, 11,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      pdata->elmenu = build_option_menu (ellipsize_items,
                                         sizeof (ellipsize_items) / sizeof (ellipsize_items[0]),
                                         2, // PANGO_ELLIPSIZE_MIDDLE
					 progressbar_toggle_ellipsize,
					 pdata);
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (tab), hbox, 1, 2, 10, 11,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->elmenu, TRUE, TRUE, 0);

      label = gtk_label_new ("Bar Style :");
      gtk_table_attach (GTK_TABLE (tab), label, 0, 1, 13, 14,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

      pdata->omenu2 = build_option_menu	(items2, 2, 0,
					 progressbar_toggle_bar_style,
					 pdata);
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (tab), hbox, 1, 2, 13, 14,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->omenu2, TRUE, TRUE, 0);

      label = gtk_label_new ("Block count :");
      gtk_table_attach (GTK_TABLE (tab), label, 0, 1, 14, 15,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (tab), hbox, 1, 2, 14, 15,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      adj = (GtkAdjustment *) gtk_adjustment_new (10, 2, 20, 1, 5, 0);
      pdata->block_spin = gtk_spin_button_new (adj, 0, 0);
      g_signal_connect (adj, "value_changed",
			G_CALLBACK (adjust_blocks), pdata);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->block_spin, FALSE, TRUE, 0);
      gtk_widget_set_sensitive (pdata->block_spin, FALSE);

      check = gtk_check_button_new_with_label ("Activity mode");
      g_signal_connect (check, "clicked",
			G_CALLBACK (toggle_activity_mode), pdata);
      gtk_table_attach (GTK_TABLE (tab), check, 0, 1, 15, 16,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (tab), hbox, 1, 2, 15, 16,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      label = gtk_label_new ("Step size : ");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      adj = (GtkAdjustment *) gtk_adjustment_new (3, 1, 20, 1, 5, 0);
      pdata->step_spin = gtk_spin_button_new (adj, 0, 0);
      g_signal_connect (adj, "value_changed",
			G_CALLBACK (adjust_step), pdata);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->step_spin, FALSE, TRUE, 0);
      gtk_widget_set_sensitive (pdata->step_spin, FALSE);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (tab), hbox, 1, 2, 16, 17,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      label = gtk_label_new ("Blocks :     ");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      adj = (GtkAdjustment *) gtk_adjustment_new (5, 2, 10, 1, 5, 0);
      pdata->act_blocks_spin = gtk_spin_button_new (adj, 0, 0);
      g_signal_connect (adj, "value_changed",
			G_CALLBACK (adjust_act_blocks), pdata);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->act_blocks_spin, FALSE, TRUE,
			  0);
      gtk_widget_set_sensitive (pdata->act_blocks_spin, FALSE);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_destroy),
				pdata->window);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (pdata->window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
    }

  if (!gtk_widget_get_visible (pdata->window))
    gtk_widget_show_all (pdata->window);
  else
    gtk_widget_destroy (pdata->window);
}

/*
 * Properties
 */

typedef struct {
  int x;
  int y;
  gboolean found;
  gboolean first;
  GtkWidget *res_widget;
} FindWidgetData;

static void
find_widget (GtkWidget *widget, FindWidgetData *data)
{
  GtkAllocation new_allocation;
  gint x_offset = 0;
  gint y_offset = 0;

  new_allocation = widget->allocation;

  if (data->found || !gtk_widget_get_mapped (widget))
    return;

  /* Note that in the following code, we only count the
   * position as being inside a WINDOW widget if it is inside
   * widget->window; points that are outside of widget->window
   * but within the allocation are not counted. This is consistent
   * with the way we highlight drag targets.
   */
  if (gtk_widget_get_has_window (widget))
    {
      new_allocation.x = 0;
      new_allocation.y = 0;
    }
  
  if (widget->parent && !data->first)
    {
      GdkWindow *window = widget->window;
      while (window != widget->parent->window)
	{
	  gint tx, ty, twidth, theight;
	  gdk_drawable_get_size (window, &twidth, &theight);

	  if (new_allocation.x < 0)
	    {
	      new_allocation.width += new_allocation.x;
	      new_allocation.x = 0;
	    }
	  if (new_allocation.y < 0)
	    {
	      new_allocation.height += new_allocation.y;
	      new_allocation.y = 0;
	    }
	  if (new_allocation.x + new_allocation.width > twidth)
	    new_allocation.width = twidth - new_allocation.x;
	  if (new_allocation.y + new_allocation.height > theight)
	    new_allocation.height = theight - new_allocation.y;

	  gdk_window_get_position (window, &tx, &ty);
	  new_allocation.x += tx;
	  x_offset += tx;
	  new_allocation.y += ty;
	  y_offset += ty;
	  
	  window = gdk_window_get_parent (window);
	}
    }

  if ((data->x >= new_allocation.x) && (data->y >= new_allocation.y) &&
      (data->x < new_allocation.x + new_allocation.width) && 
      (data->y < new_allocation.y + new_allocation.height))
    {
      /* First, check if the drag is in a valid drop site in
       * one of our children 
       */
      if (GTK_IS_CONTAINER (widget))
	{
	  FindWidgetData new_data = *data;
	  
	  new_data.x -= x_offset;
	  new_data.y -= y_offset;
	  new_data.found = FALSE;
	  new_data.first = FALSE;
	  
	  gtk_container_forall (GTK_CONTAINER (widget),
				(GtkCallback)find_widget,
				&new_data);
	  
	  data->found = new_data.found;
	  if (data->found)
	    data->res_widget = new_data.res_widget;
	}

      /* If not, and this widget is registered as a drop site, check to
       * emit "drag_motion" to check if we are actually in
       * a drop site.
       */
      if (!data->found)
	{
	  data->found = TRUE;
	  data->res_widget = widget;
	}
    }
}

static GtkWidget *
find_widget_at_pointer (GdkDisplay *display)
{
  GtkWidget *widget = NULL;
  GdkWindow *pointer_window;
  gint x, y;
  FindWidgetData data;
 
 pointer_window = gdk_display_get_window_at_pointer (display, NULL, NULL);
 
 if (pointer_window)
   {
     gpointer widget_ptr;

     gdk_window_get_user_data (pointer_window, &widget_ptr);
     widget = widget_ptr;
   }

 if (widget)
   {
     gdk_window_get_pointer (widget->window,
			     &x, &y, NULL);
     
     data.x = x;
     data.y = y;
     data.found = FALSE;
     data.first = TRUE;

     find_widget (widget, &data);
     if (data.found)
       return data.res_widget;
     return widget;
   }
 return NULL;
}

struct PropertiesData {
  GtkWidget **window;
  GdkCursor *cursor;
  gboolean in_query;
  gint handler;
};

static void
destroy_properties (GtkWidget             *widget,
		    struct PropertiesData *data)
{
  if (data->window)
    {
      *data->window = NULL;
      data->window = NULL;
    }

  if (data->cursor)
    {
      gdk_cursor_unref (data->cursor);
      data->cursor = NULL;
    }

  if (data->handler)
    {
      g_signal_handler_disconnect (widget, data->handler);
      data->handler = 0;
    }

  g_free (data);
}

static gint
property_query_event (GtkWidget	       *widget,
		      GdkEvent	       *event,
		      struct PropertiesData *data)
{
  GtkWidget *res_widget = NULL;

  if (!data->in_query)
    return FALSE;
  
  if (event->type == GDK_BUTTON_RELEASE)
    {
      gtk_grab_remove (widget);
      gdk_display_pointer_ungrab (gtk_widget_get_display (widget),
				  GDK_CURRENT_TIME);
      
      res_widget = find_widget_at_pointer (gtk_widget_get_display (widget));
      if (res_widget)
	{
	  g_object_set_data (G_OBJECT (res_widget), "prop-editor-screen",
			     gtk_widget_get_screen (widget));
	  create_prop_editor (G_OBJECT (res_widget), 0);
	}

      data->in_query = FALSE;
    }
  return FALSE;
}


static void
query_properties (GtkButton *button,
		  struct PropertiesData *data)
{
  gint failure;

  g_signal_connect (button, "event",
		    G_CALLBACK (property_query_event), data);


  if (!data->cursor)
    data->cursor = gdk_cursor_new_for_display (gtk_widget_get_display (GTK_WIDGET (button)),
					       GDK_TARGET);
  
  failure = gdk_pointer_grab (GTK_WIDGET (button)->window,
			      TRUE,
			      GDK_BUTTON_RELEASE_MASK,
			      NULL,
			      data->cursor,
			      GDK_CURRENT_TIME);

  gtk_grab_add (GTK_WIDGET (button));

  data->in_query = TRUE;
}

static void
create_properties (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *button;
  GtkWidget *vbox;
  GtkWidget *label;
  struct PropertiesData *data;

  data = g_new (struct PropertiesData, 1);
  data->window = &window;
  data->in_query = FALSE;
  data->cursor = NULL;
  data->handler = 0;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));      

      data->handler = g_signal_connect (window, "destroy",
					G_CALLBACK (destroy_properties),
					data);

      gtk_window_set_title (GTK_WINDOW (window), "test properties");
      gtk_container_set_border_width (GTK_CONTAINER (window), 10);

      vbox = gtk_vbox_new (FALSE, 1);
      gtk_container_add (GTK_CONTAINER (window), vbox);
            
      label = gtk_label_new ("This is just a dumb test to test properties.\nIf you need a generic module, get GLE.");
      gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
      
      button = gtk_button_new_with_label ("Query properties");
      gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (query_properties),
			data);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
  
}

struct SnapshotData {
  GtkWidget *toplevel_button;
  GtkWidget **window;
  GdkCursor *cursor;
  gboolean in_query;
  gboolean is_toplevel;
  gint handler;
};

static void
destroy_snapshot_data (GtkWidget             *widget,
		       struct SnapshotData *data)
{
  if (*data->window)
    *data->window = NULL;
  
  if (data->cursor)
    {
      gdk_cursor_unref (data->cursor);
      data->cursor = NULL;
    }

  if (data->handler)
    {
      g_signal_handler_disconnect (widget, data->handler);
      data->handler = 0;
    }

  g_free (data);
}

static gint
snapshot_widget_event (GtkWidget	       *widget,
		       GdkEvent	       *event,
		       struct SnapshotData *data)
{
  GtkWidget *res_widget = NULL;

  if (!data->in_query)
    return FALSE;
  
  if (event->type == GDK_BUTTON_RELEASE)
    {
      gtk_grab_remove (widget);
      gdk_display_pointer_ungrab (gtk_widget_get_display (widget),
				  GDK_CURRENT_TIME);
      
      res_widget = find_widget_at_pointer (gtk_widget_get_display (widget));
      if (data->is_toplevel && res_widget)
	res_widget = gtk_widget_get_toplevel (res_widget);
      if (res_widget)
	{
	  GdkPixmap *pixmap;
	  GtkWidget *window, *image;

	  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	  pixmap = gtk_widget_get_snapshot (res_widget, NULL);
          gtk_widget_realize (window);
          if (gdk_drawable_get_depth (window->window) != gdk_drawable_get_depth (pixmap))
            {
              /* this branch is needed to convert ARGB -> RGB */
              int width, height;
              GdkPixbuf *pixbuf;
              gdk_drawable_get_size (pixmap, &width, &height);
              pixbuf = gdk_pixbuf_get_from_drawable (NULL, pixmap,
                                                     gtk_widget_get_colormap (res_widget),
                                                     0, 0,
                                                     0, 0,
                                                     width, height);
              image = gtk_image_new_from_pixbuf (pixbuf);
              g_object_unref (pixbuf);
            }
          else
            image = gtk_image_new_from_pixmap (pixmap, NULL);
	  gtk_container_add (GTK_CONTAINER (window), image);
          g_object_unref (pixmap);
	  gtk_widget_show_all (window);
	}

      data->in_query = FALSE;
    }
  return FALSE;
}


static void
snapshot_widget (GtkButton *button,
		 struct SnapshotData *data)
{
  gint failure;

  g_signal_connect (button, "event",
		    G_CALLBACK (snapshot_widget_event), data);

  data->is_toplevel = GTK_WIDGET (button) == data->toplevel_button;
  
  if (!data->cursor)
    data->cursor = gdk_cursor_new_for_display (gtk_widget_get_display (GTK_WIDGET (button)),
					       GDK_TARGET);
  
  failure = gdk_pointer_grab (GTK_WIDGET (button)->window,
			      TRUE,
			      GDK_BUTTON_RELEASE_MASK,
			      NULL,
			      data->cursor,
			      GDK_CURRENT_TIME);

  gtk_grab_add (GTK_WIDGET (button));

  data->in_query = TRUE;
}

static void
create_snapshot (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *button;
  GtkWidget *vbox;
  struct SnapshotData *data;

  data = g_new (struct SnapshotData, 1);
  data->window = &window;
  data->in_query = FALSE;
  data->cursor = NULL;
  data->handler = 0;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));      

      data->handler = g_signal_connect (window, "destroy",
					G_CALLBACK (destroy_snapshot_data),
					data);

      gtk_window_set_title (GTK_WINDOW (window), "test snapshot");
      gtk_container_set_border_width (GTK_CONTAINER (window), 10);

      vbox = gtk_vbox_new (FALSE, 1);
      gtk_container_add (GTK_CONTAINER (window), vbox);
            
      button = gtk_button_new_with_label ("Snapshot widget");
      gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (snapshot_widget),
			data);
      
      button = gtk_button_new_with_label ("Snapshot toplevel");
      data->toplevel_button = button;
      gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (snapshot_widget),
			data);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
  
}



/*
 * Color Preview
 */

static int color_idle = 0;

gint
color_idle_func (GtkWidget *preview)
{
  static int count = 1;
  guchar buf[768];
  int i, j, k;

  for (i = 0; i < 256; i++)
    {
      for (j = 0, k = 0; j < 256; j++)
	{
	  buf[k+0] = i + count;
	  buf[k+1] = 0;
	  buf[k+2] = j + count;
	  k += 3;
	}

      gtk_preview_draw_row (GTK_PREVIEW (preview), buf, 0, i, 256);
    }

  count += 1;

  gtk_widget_queue_draw (preview);
  gdk_window_process_updates (preview->window, TRUE);

  return TRUE;
}

static void
color_preview_destroy (GtkWidget  *widget,
		       GtkWidget **window)
{
  gtk_idle_remove (color_idle);
  color_idle = 0;

  *window = NULL;
}

void
create_color_preview (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *preview;
  guchar buf[768];
  int i, j, k;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (color_preview_destroy),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 10);

      preview = gtk_preview_new (GTK_PREVIEW_COLOR);
      gtk_preview_size (GTK_PREVIEW (preview), 256, 256);
      gtk_container_add (GTK_CONTAINER (window), preview);

      for (i = 0; i < 256; i++)
	{
	  for (j = 0, k = 0; j < 256; j++)
	    {
	      buf[k+0] = i;
	      buf[k+1] = 0;
	      buf[k+2] = j;
	      k += 3;
	    }

	  gtk_preview_draw_row (GTK_PREVIEW (preview), buf, 0, i, 256);
	}

      color_idle = gtk_idle_add ((GtkFunction) color_idle_func, preview);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Gray Preview
 */

static int gray_idle = 0;

gint
gray_idle_func (GtkWidget *preview)
{
  static int count = 1;
  guchar buf[256];
  int i, j;

  for (i = 0; i < 256; i++)
    {
      for (j = 0; j < 256; j++)
	buf[j] = i + j + count;

      gtk_preview_draw_row (GTK_PREVIEW (preview), buf, 0, i, 256);
    }

  count += 1;

  gtk_widget_draw (preview, NULL);

  return TRUE;
}

static void
gray_preview_destroy (GtkWidget  *widget,
		      GtkWidget **window)
{
  gtk_idle_remove (gray_idle);
  gray_idle = 0;

  *window = NULL;
}

void
create_gray_preview (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *preview;
  guchar buf[256];
  int i, j;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gray_preview_destroy),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 10);

      preview = gtk_preview_new (GTK_PREVIEW_GRAYSCALE);
      gtk_preview_size (GTK_PREVIEW (preview), 256, 256);
      gtk_container_add (GTK_CONTAINER (window), preview);

      for (i = 0; i < 256; i++)
	{
	  for (j = 0; j < 256; j++)
	    buf[j] = i + j;

	  gtk_preview_draw_row (GTK_PREVIEW (preview), buf, 0, i, 256);
	}

      gray_idle = gtk_idle_add ((GtkFunction) gray_idle_func, preview);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}


/*
 * Selection Test
 */

void
selection_test_received (GtkWidget *list, GtkSelectionData *data)
{
  GdkAtom *atoms;
  GtkWidget *list_item;
  GList *item_list;
  int i, l;

  if (data->length < 0)
    {
      g_print ("Selection retrieval failed\n");
      return;
    }
  if (data->type != GDK_SELECTION_TYPE_ATOM)
    {
      g_print ("Selection \"TARGETS\" was not returned as atoms!\n");
      return;
    }

  /* Clear out any current list items */

  gtk_list_clear_items (GTK_LIST(list), 0, -1);

  /* Add new items to list */

  atoms = (GdkAtom *)data->data;

  item_list = NULL;
  l = data->length / sizeof (GdkAtom);
  for (i = 0; i < l; i++)
    {
      char *name;
      name = gdk_atom_name (atoms[i]);
      if (name != NULL)
	{
	  list_item = gtk_list_item_new_with_label (name);
	  g_free (name);
	}
      else
	list_item = gtk_list_item_new_with_label ("(bad atom)");

      gtk_widget_show (list_item);
      item_list = g_list_append (item_list, list_item);
    }

  gtk_list_append_items (GTK_LIST (list), item_list);

  return;
}

void
selection_test_get_targets (GtkWidget *widget, GtkWidget *list)
{
  static GdkAtom targets_atom = GDK_NONE;

  if (targets_atom == GDK_NONE)
    targets_atom = gdk_atom_intern ("TARGETS", FALSE);

  gtk_selection_convert (list, GDK_SELECTION_PRIMARY, targets_atom,
			 GDK_CURRENT_TIME);
}

void
create_selection_test (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *button;
  GtkWidget *vbox;
  GtkWidget *scrolled_win;
  GtkWidget *list;
  GtkWidget *label;

  if (!window)
    {
      window = gtk_dialog_new ();
      
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "Selection Test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      /* Create the list */

      vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), vbox,
			  TRUE, TRUE, 0);

      label = gtk_label_new ("Gets available targets for current selection");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      scrolled_win = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				      GTK_POLICY_AUTOMATIC, 
				      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (vbox), scrolled_win, TRUE, TRUE, 0);
      gtk_widget_set_size_request (scrolled_win, 100, 200);

      list = gtk_list_new ();
      gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_win), list);

      g_signal_connect (list, "selection_received",
			G_CALLBACK (selection_test_received), NULL);

      /* .. And create some buttons */
      button = gtk_button_new_with_label ("Get Targets");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area),
			  button, TRUE, TRUE, 0);

      g_signal_connect (button, "clicked",
			G_CALLBACK (selection_test_get_targets), list);

      button = gtk_button_new_with_label ("Quit");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area),
			  button, TRUE, TRUE, 0);

      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_destroy),
				window);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Gamma Curve
 */

void
create_gamma_curve (GtkWidget *widget)
{
  static GtkWidget *window = NULL, *curve;
  static int count = 0;
  gfloat vec[256];
  gint max;
  gint i;
  
  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      gtk_window_set_title (GTK_WINDOW (window), "test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 10);

      g_signal_connect (window, "destroy",
			G_CALLBACK(gtk_widget_destroyed),
			&window);

      curve = gtk_gamma_curve_new ();
      gtk_container_add (GTK_CONTAINER (window), curve);
      gtk_widget_show (curve);
    }

  max = 127 + (count % 2)*128;
  gtk_curve_set_range (GTK_CURVE (GTK_GAMMA_CURVE (curve)->curve),
		       0, max, 0, max);
  for (i = 0; i < max; ++i)
    vec[i] = (127 / sqrt (max)) * sqrt (i);
  gtk_curve_set_vector (GTK_CURVE (GTK_GAMMA_CURVE (curve)->curve),
			max, vec);

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else if (count % 4 == 3)
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  ++count;
}

/*
 * Test scrolling
 */

static int scroll_test_pos = 0.0;

static gint
scroll_test_expose (GtkWidget *widget, GdkEventExpose *event,
		    GtkAdjustment *adj)
{
  gint i,j;
  gint imin, imax, jmin, jmax;
  cairo_t *cr;
  
  imin = (event->area.x) / 10;
  imax = (event->area.x + event->area.width + 9) / 10;

  jmin = ((int)adj->value + event->area.y) / 10;
  jmax = ((int)adj->value + event->area.y + event->area.height + 9) / 10;

  gdk_window_clear_area (widget->window,
			 event->area.x, event->area.y,
			 event->area.width, event->area.height);

  cr = gdk_cairo_create (widget->window);

  for (i=imin; i<imax; i++)
    for (j=jmin; j<jmax; j++)
      if ((i+j) % 2)
	cairo_rectangle (cr, 10*i, 10*j - (int)adj->value, 1+i%10, 1+j%10);

  cairo_fill (cr);

  cairo_destroy (cr);

  return TRUE;
}

static gint
scroll_test_scroll (GtkWidget *widget, GdkEventScroll *event,
		    GtkAdjustment *adj)
{
  gdouble new_value = adj->value + ((event->direction == GDK_SCROLL_UP) ?
				    -adj->page_increment / 2:
				    adj->page_increment / 2);
  new_value = CLAMP (new_value, adj->lower, adj->upper - adj->page_size);
  gtk_adjustment_set_value (adj, new_value);  
  
  return TRUE;
}

static void
scroll_test_configure (GtkWidget *widget, GdkEventConfigure *event,
		       GtkAdjustment *adj)
{
  adj->page_increment = 0.9 * widget->allocation.height;
  adj->page_size = widget->allocation.height;

  g_signal_emit_by_name (adj, "changed");
}

static void
scroll_test_adjustment_changed (GtkAdjustment *adj, GtkWidget *widget)
{
  /* gint source_min = (int)adj->value - scroll_test_pos; */
  gint dy;

  dy = scroll_test_pos - (int)adj->value;
  scroll_test_pos = adj->value;

  if (!gtk_widget_is_drawable (widget))
    return;
  gdk_window_scroll (widget->window, 0, dy);
  gdk_window_process_updates (widget->window, FALSE);
}


void
create_scroll_test (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *hbox;
  GtkWidget *drawing_area;
  GtkWidget *scrollbar;
  GtkWidget *button;
  GtkAdjustment *adj;
  GdkGeometry geometry;
  GdkWindowHints geometry_mask;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "Scroll Test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), hbox,
			  TRUE, TRUE, 0);
      gtk_widget_show (hbox);

      drawing_area = gtk_drawing_area_new ();
      gtk_widget_set_size_request (drawing_area, 200, 200);
      gtk_box_pack_start (GTK_BOX (hbox), drawing_area, TRUE, TRUE, 0);
      gtk_widget_show (drawing_area);

      gtk_widget_set_events (drawing_area, GDK_EXPOSURE_MASK | GDK_SCROLL_MASK);

      adj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 1000.0, 1.0, 180.0, 200.0));
      scroll_test_pos = 0.0;

      scrollbar = gtk_vscrollbar_new (adj);
      gtk_box_pack_start (GTK_BOX (hbox), scrollbar, FALSE, FALSE, 0);
      gtk_widget_show (scrollbar);

      g_signal_connect (drawing_area, "expose_event",
			G_CALLBACK (scroll_test_expose), adj);
      g_signal_connect (drawing_area, "configure_event",
			G_CALLBACK (scroll_test_configure), adj);
      g_signal_connect (drawing_area, "scroll_event",
			G_CALLBACK (scroll_test_scroll), adj);
      
      g_signal_connect (adj, "value_changed",
			G_CALLBACK (scroll_test_adjustment_changed),
			drawing_area);
      
      /* .. And create some buttons */

      button = gtk_button_new_with_label ("Quit");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area),
			  button, TRUE, TRUE, 0);

      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_widget_show (button);

      /* Set up gridded geometry */

      geometry_mask = GDK_HINT_MIN_SIZE | 
	               GDK_HINT_BASE_SIZE | 
	               GDK_HINT_RESIZE_INC;

      geometry.min_width = 20;
      geometry.min_height = 20;
      geometry.base_width = 0;
      geometry.base_height = 0;
      geometry.width_inc = 10;
      geometry.height_inc = 10;
      
      gtk_window_set_geometry_hints (GTK_WINDOW (window),
			       drawing_area, &geometry, geometry_mask);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Timeout Test
 */

static int timer = 0;

gint
timeout_test (GtkWidget *label)
{
  static int count = 0;
  static char buffer[32];

  sprintf (buffer, "count: %d", ++count);
  gtk_label_set_text (GTK_LABEL (label), buffer);

  return TRUE;
}

void
start_timeout_test (GtkWidget *widget,
		    GtkWidget *label)
{
  if (!timer)
    {
      timer = gtk_timeout_add (100, (GtkFunction) timeout_test, label);
    }
}

void
stop_timeout_test (GtkWidget *widget,
		   gpointer   data)
{
  if (timer)
    {
      gtk_timeout_remove (timer);
      timer = 0;
    }
}

void
destroy_timeout_test (GtkWidget  *widget,
		      GtkWidget **window)
{
  stop_timeout_test (NULL, NULL);

  *window = NULL;
}

void
create_timeout_test (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *button;
  GtkWidget *label;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (destroy_timeout_test),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "Timeout Test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      label = gtk_label_new ("count: 0");
      gtk_misc_set_padding (GTK_MISC (label), 10, 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  label, TRUE, TRUE, 0);
      gtk_widget_show (label);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("start");
      g_signal_connect (button, "clicked",
			G_CALLBACK(start_timeout_test),
			label);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("stop");
      g_signal_connect (button, "clicked",
			G_CALLBACK (stop_timeout_test),
			NULL);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Idle Test
 */

static int idle_id = 0;

static gint
idle_test (GtkWidget *label)
{
  static int count = 0;
  static char buffer[32];

  sprintf (buffer, "count: %d", ++count);
  gtk_label_set_text (GTK_LABEL (label), buffer);

  return TRUE;
}

static void
start_idle_test (GtkWidget *widget,
		 GtkWidget *label)
{
  if (!idle_id)
    {
      idle_id = gtk_idle_add ((GtkFunction) idle_test, label);
    }
}

static void
stop_idle_test (GtkWidget *widget,
		gpointer   data)
{
  if (idle_id)
    {
      gtk_idle_remove (idle_id);
      idle_id = 0;
    }
}

static void
destroy_idle_test (GtkWidget  *widget,
		   GtkWidget **window)
{
  stop_idle_test (NULL, NULL);

  *window = NULL;
}

static void
toggle_idle_container (GObject *button,
		       GtkContainer *container)
{
  gtk_container_set_resize_mode (container, GPOINTER_TO_INT (g_object_get_data (button, "user_data")));
}

static void
create_idle_test (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *container;

  if (!window)
    {
      GtkWidget *button2;
      GtkWidget *frame;
      GtkWidget *box;

      window = gtk_dialog_new ();

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (destroy_idle_test),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "Idle Test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      label = gtk_label_new ("count: 0");
      gtk_misc_set_padding (GTK_MISC (label), 10, 10);
      gtk_widget_show (label);
      
      container =
	g_object_new (GTK_TYPE_HBOX,
			"visible", TRUE,
			/* "GtkContainer::child", g_object_new (GTK_TYPE_HBOX,
			 * "GtkWidget::visible", TRUE,
			 */
			 "child", label,
			/* NULL), */
			NULL);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  container, TRUE, TRUE, 0);

      frame =
	g_object_new (GTK_TYPE_FRAME,
			"border_width", 5,
			"label", "Label Container",
			"visible", TRUE,
			"parent", GTK_DIALOG (window)->vbox,
			NULL);
      box =
	g_object_new (GTK_TYPE_VBOX,
			"visible", TRUE,
			"parent", frame,
			NULL);
      button =
	g_object_connect (g_object_new (GTK_TYPE_RADIO_BUTTON,
					  "label", "Resize-Parent",
					  "user_data", (void*)GTK_RESIZE_PARENT,
					  "visible", TRUE,
					  "parent", box,
					  NULL),
			  "signal::clicked", toggle_idle_container, container,
			  NULL);
      button = g_object_new (GTK_TYPE_RADIO_BUTTON,
			       "label", "Resize-Queue",
			       "user_data", (void*)GTK_RESIZE_QUEUE,
			       "group", button,
			       "visible", TRUE,
			       "parent", box,
			       NULL);
      g_object_connect (button,
			"signal::clicked", toggle_idle_container, container,
			NULL);
      button2 = g_object_new (GTK_TYPE_RADIO_BUTTON,
				"label", "Resize-Immediate",
				"user_data", (void*)GTK_RESIZE_IMMEDIATE,
				NULL);
      g_object_connect (button2,
			"signal::clicked", toggle_idle_container, container,
			NULL);
      g_object_set (button2,
		    "group", button,
		    "visible", TRUE,
		    "parent", box,
		    NULL);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("start");
      g_signal_connect (button, "clicked",
			G_CALLBACK (start_idle_test),
			label);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("stop");
      g_signal_connect (button, "clicked",
			G_CALLBACK (stop_idle_test),
			NULL);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * rc file test
 */

void
reload_all_rc_files (void)
{
  static GdkAtom atom_rcfiles = GDK_NONE;

  GdkEvent *send_event = gdk_event_new (GDK_CLIENT_EVENT);
  int i;
  
  if (!atom_rcfiles)
    atom_rcfiles = gdk_atom_intern("_GTK_READ_RCFILES", FALSE);

  for(i = 0; i < 5; i++)
    send_event->client.data.l[i] = 0;
  send_event->client.data_format = 32;
  send_event->client.message_type = atom_rcfiles;
  gdk_event_send_clientmessage_toall (send_event);

  gdk_event_free (send_event);
}

void
create_rc_file (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *button;
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *label;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      frame = gtk_aspect_frame_new ("Testing RC file prioritization", 0.5, 0.5, 0.0, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), frame, FALSE, FALSE, 0);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (frame), vbox);
      
      label = gtk_label_new ("This label should be red");
      gtk_widget_set_name (label, "testgtk-red-label");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      label = gtk_label_new ("This label should be green");
      gtk_widget_set_name (label, "testgtk-green-label");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      label = gtk_label_new ("This label should be blue");
      gtk_widget_set_name (label, "testgtk-blue-label");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      gtk_window_set_title (GTK_WINDOW (window), "Reload Rc file");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      button = gtk_button_new_with_label ("Reload");
      g_signal_connect (button, "clicked",
			G_CALLBACK (gtk_rc_reparse_all), NULL);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);

      button = gtk_button_new_with_label ("Reload All");
      g_signal_connect (button, "clicked",
			G_CALLBACK (reload_all_rc_files), NULL);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);

      button = gtk_button_new_with_label ("Close");
      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Test of recursive mainloop
 */

void
mainloop_destroyed (GtkWidget *w, GtkWidget **window)
{
  *window = NULL;
  gtk_main_quit ();
}

void
create_mainloop (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *label;
  GtkWidget *button;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      gtk_window_set_title (GTK_WINDOW (window), "Test Main Loop");

      g_signal_connect (window, "destroy",
			G_CALLBACK (mainloop_destroyed),
			&window);

      label = gtk_label_new ("In recursive main loop...");
      gtk_misc_set_padding (GTK_MISC(label), 20, 20);

      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), label,
			  TRUE, TRUE, 0);
      gtk_widget_show (label);

      button = gtk_button_new_with_label ("Leave");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), button, 
			  FALSE, TRUE, 0);

      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_destroy),
				window);

      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);

      gtk_widget_show (button);
    }

  if (!gtk_widget_get_visible (window))
    {
      gtk_widget_show (window);

      g_print ("create_mainloop: start\n");
      gtk_main ();
      g_print ("create_mainloop: done\n");
    }
  else
    gtk_widget_destroy (window);
}

gboolean
layout_expose_handler (GtkWidget *widget, GdkEventExpose *event)
{
  GtkLayout *layout;
  GdkWindow *bin_window;
  cairo_t *cr;

  gint i,j;
  gint imin, imax, jmin, jmax;

  layout = GTK_LAYOUT (widget);
  bin_window = gtk_layout_get_bin_window (layout);

  if (event->window != bin_window)
    return FALSE;
  
  imin = (event->area.x) / 10;
  imax = (event->area.x + event->area.width + 9) / 10;

  jmin = (event->area.y) / 10;
  jmax = (event->area.y + event->area.height + 9) / 10;

  cr = gdk_cairo_create (bin_window);

  for (i=imin; i<imax; i++)
    for (j=jmin; j<jmax; j++)
      if ((i+j) % 2)
	cairo_rectangle (cr,
			 10*i, 10*j, 
			 1+i%10, 1+j%10);
  
  cairo_fill (cr);

  cairo_destroy (cr);

  return FALSE;
}

void create_layout (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *layout;
  GtkWidget *scrolledwindow;
  GtkWidget *button;

  if (!window)
    {
      gchar buf[16];

      gint i, j;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "Layout");
      gtk_widget_set_size_request (window, 200, 200);

      scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow),
					   GTK_SHADOW_IN);
      gtk_scrolled_window_set_placement (GTK_SCROLLED_WINDOW (scrolledwindow),
					 GTK_CORNER_TOP_RIGHT);

      gtk_container_add (GTK_CONTAINER (window), scrolledwindow);
      
      layout = gtk_layout_new (NULL, NULL);
      gtk_container_add (GTK_CONTAINER (scrolledwindow), layout);

      /* We set step sizes here since GtkLayout does not set
       * them itself.
       */
      GTK_LAYOUT (layout)->hadjustment->step_increment = 10.0;
      GTK_LAYOUT (layout)->vadjustment->step_increment = 10.0;
      
      gtk_widget_set_events (layout, GDK_EXPOSURE_MASK);
      g_signal_connect (layout, "expose_event",
			G_CALLBACK (layout_expose_handler), NULL);
      
      gtk_layout_set_size (GTK_LAYOUT (layout), 1600, 128000);
      
      for (i=0 ; i < 16 ; i++)
	for (j=0 ; j < 16 ; j++)
	  {
	    sprintf(buf, "Button %d, %d", i, j);
	    if ((i + j) % 2)
	      button = gtk_button_new_with_label (buf);
	    else
	      button = gtk_label_new (buf);

	    gtk_layout_put (GTK_LAYOUT (layout), button,
			    j*100, i*100);
	  }

      for (i=16; i < 1280; i++)
	{
	  sprintf(buf, "Button %d, %d", i, 0);
	  if (i % 2)
	    button = gtk_button_new_with_label (buf);
	  else
	    button = gtk_label_new (buf);

	  gtk_layout_put (GTK_LAYOUT (layout), button,
			  0, i*100);
	}
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

void
create_styles (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *entry;
  GtkWidget *vbox;
  static GdkColor red =    { 0, 0xffff, 0,      0      };
  static GdkColor green =  { 0, 0,      0xffff, 0      };
  static GdkColor blue =   { 0, 0,      0,      0xffff };
  static GdkColor yellow = { 0, 0xffff, 0xffff, 0      };
  static GdkColor cyan =   { 0, 0     , 0xffff, 0xffff };
  PangoFontDescription *font_desc;

  GtkRcStyle *rc_style;

  if (!window)
    {
      window = gtk_dialog_new ();
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));
     
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      
      button = gtk_button_new_with_label ("Close");
      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), vbox, FALSE, FALSE, 0);
      
      label = gtk_label_new ("Font:");
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      font_desc = pango_font_description_from_string ("Helvetica,Sans Oblique 18");

      button = gtk_button_new_with_label ("Some Text");
      gtk_widget_modify_font (GTK_BIN (button)->child, font_desc);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

      label = gtk_label_new ("Foreground:");
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Some Text");
      gtk_widget_modify_fg (GTK_BIN (button)->child, GTK_STATE_NORMAL, &red);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

      label = gtk_label_new ("Background:");
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Some Text");
      gtk_widget_modify_bg (button, GTK_STATE_NORMAL, &green);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

      label = gtk_label_new ("Text:");
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      entry = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (entry), "Some Text");
      gtk_widget_modify_text (entry, GTK_STATE_NORMAL, &blue);
      gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

      label = gtk_label_new ("Base:");
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      entry = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (entry), "Some Text");
      gtk_widget_modify_base (entry, GTK_STATE_NORMAL, &yellow);
      gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

      label = gtk_label_new ("Cursor:");
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      entry = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (entry), "Some Text");
      gtk_widget_modify_cursor (entry, &red, &red);
      gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

      label = gtk_label_new ("Multiple:");
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Some Text");

      rc_style = gtk_rc_style_new ();

      rc_style->font_desc = pango_font_description_copy (font_desc);
      rc_style->color_flags[GTK_STATE_NORMAL] = GTK_RC_FG | GTK_RC_BG;
      rc_style->color_flags[GTK_STATE_PRELIGHT] = GTK_RC_FG | GTK_RC_BG;
      rc_style->color_flags[GTK_STATE_ACTIVE] = GTK_RC_FG | GTK_RC_BG;
      rc_style->fg[GTK_STATE_NORMAL] = yellow;
      rc_style->bg[GTK_STATE_NORMAL] = blue;
      rc_style->fg[GTK_STATE_PRELIGHT] = blue;
      rc_style->bg[GTK_STATE_PRELIGHT] = yellow;
      rc_style->fg[GTK_STATE_ACTIVE] = red;
      rc_style->bg[GTK_STATE_ACTIVE] = cyan;
      rc_style->xthickness = 5;
      rc_style->ythickness = 5;

      gtk_widget_modify_style (button, rc_style);
      gtk_widget_modify_style (GTK_BIN (button)->child, rc_style);

      g_object_unref (rc_style);
      
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    }
  
  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Main Window and Exit
 */

void
do_exit (GtkWidget *widget, GtkWidget *window)
{
  gtk_widget_destroy (window);
  gtk_main_quit ();
}

struct {
  char *label;
  void (*func) (GtkWidget *widget);
  gboolean do_not_benchmark;
} buttons[] =
{
  { "alpha window", create_alpha_window },
  { "big windows", create_big_windows },
  { "button box", create_button_box },
  { "buttons", create_buttons },
  { "check buttons", create_check_buttons },
  { "clist", create_clist},
  { "color selection", create_color_selection },
  { "composited window", create_composited_window },
  { "ctree", create_ctree },
  { "cursors", create_cursors },
  { "dialog", create_dialog },
  { "display & screen", create_display_screen, TRUE },
  { "entry", create_entry },
  { "event box", create_event_box },
  { "event watcher", create_event_watcher },
  { "expander", create_expander },
  { "file selection", create_file_selection },
  { "flipping", create_flipping },
  { "focus", create_focus },
  { "font selection", create_font_selection },
  { "gamma curve", create_gamma_curve, TRUE },
  { "gridded geometry", create_gridded_geometry },
  { "handle box", create_handle_box },
  { "image", create_image },
  { "item factory", create_item_factory },
  { "key lookup", create_key_lookup },
  { "labels", create_labels },
  { "layout", create_layout },
  { "list", create_list },
  { "menus", create_menus },
  { "message dialog", create_message_dialog },
  { "modal window", create_modal_window, TRUE },
  { "notebook", create_notebook },
  { "panes", create_panes },
  { "paned keyboard", create_paned_keyboard_navigation },
  { "pixmap", create_pixmap },
  { "preview color", create_color_preview, TRUE },
  { "preview gray", create_gray_preview, TRUE },
  { "progress bar", create_progress_bar },
  { "properties", create_properties },
  { "radio buttons", create_radio_buttons },
  { "range controls", create_range_controls },
  { "rc file", create_rc_file },
  { "reparent", create_reparent },
  { "resize grips", create_resize_grips },
  { "rotated label", create_rotated_label },
  { "rotated text", create_rotated_text },
  { "rulers", create_rulers },
  { "saved position", create_saved_position },
  { "scrolled windows", create_scrolled_windows },
  { "shapes", create_shapes },
  { "size groups", create_size_groups },
  { "snapshot", create_snapshot },
  { "spinbutton", create_spins },
  { "statusbar", create_statusbar },
  { "styles", create_styles },
  { "test idle", create_idle_test },
  { "test mainloop", create_mainloop, TRUE },
  { "test scrolling", create_scroll_test },
  { "test selection", create_selection_test },
  { "test timeout", create_timeout_test },
  { "text", create_text },
  { "toggle buttons", create_toggle_buttons },
  { "toolbar", create_toolbar },
  { "tooltips", create_tooltips },
  { "tree", create_tree_mode_window},
  { "WM hints", create_wmhints },
  { "window sizing", create_window_sizing },
  { "window states", create_window_states }
};
int nbuttons = sizeof (buttons) / sizeof (buttons[0]);

void
create_main_window (void)
{
  GtkWidget *window;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *scrolled_window;
  GtkWidget *button;
  GtkWidget *label;
  gchar buffer[64];
  GtkWidget *separator;
  GdkGeometry geometry;
  int i;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (window, "main window");
  gtk_widget_set_uposition (window, 50, 20);
  gtk_window_set_default_size (GTK_WINDOW (window), -1, 400);

  geometry.min_width = -1;
  geometry.min_height = -1;
  geometry.max_width = -1;
  geometry.max_height = G_MAXSHORT;
  gtk_window_set_geometry_hints (GTK_WINDOW (window), NULL,
				 &geometry,
				 GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);

  g_signal_connect (window, "destroy",
		    G_CALLBACK (gtk_main_quit),
		    NULL);
  g_signal_connect (window, "delete-event",
		    G_CALLBACK (gtk_false),
		    NULL);

  box1 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), box1);

  if (gtk_micro_version > 0)
    sprintf (buffer,
	     "Gtk+ v%d.%d.%d",
	     gtk_major_version,
	     gtk_minor_version,
	     gtk_micro_version);
  else
    sprintf (buffer,
	     "Gtk+ v%d.%d",
	     gtk_major_version,
	     gtk_minor_version);

  label = gtk_label_new (buffer);
  gtk_box_pack_start (GTK_BOX (box1), label, FALSE, FALSE, 0);
  gtk_widget_set_name (label, "testgtk-version-label");

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 10);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
     		                  GTK_POLICY_NEVER, 
                                  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (box1), scrolled_window, TRUE, TRUE, 0);

  box2 = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), box2);
  gtk_container_set_focus_vadjustment (GTK_CONTAINER (box2),
				       gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window)));
  gtk_widget_show (box2);

  for (i = 0; i < nbuttons; i++)
    {
      button = gtk_button_new_with_label (buttons[i].label);
      if (buttons[i].func)
        g_signal_connect (button, 
			  "clicked", 
			  G_CALLBACK(buttons[i].func),
			  NULL);
      else
        gtk_widget_set_sensitive (button, FALSE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
    }

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

  box2 = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
  gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

  button = gtk_button_new_with_mnemonic ("_Close");
  g_signal_connect (button, "clicked",
		    G_CALLBACK (do_exit),
		    window);
  gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
  gtk_widget_set_can_default (button, TRUE);
  gtk_widget_grab_default (button);

  gtk_widget_show_all (window);
}

static void
test_init (void)
{
  if (g_file_test ("../gdk-pixbuf/libpixbufloader-pnm.la",
		   G_FILE_TEST_EXISTS))
    {
      g_setenv ("GDK_PIXBUF_MODULE_FILE", "../gdk-pixbuf/gdk-pixbuf.loaders", TRUE);
      g_setenv ("GTK_IM_MODULE_FILE", "../modules/input/immodules.cache", TRUE);
    }
}

static char *
pad (const char *str, int to)
{
  static char buf[256];
  int len = strlen (str);
  int i;

  for (i = 0; i < to; i++)
    buf[i] = ' ';

  buf[to] = '\0';

  memcpy (buf, str, len);

  return buf;
}

static void
bench_iteration (GtkWidget *widget, void (* fn) (GtkWidget *widget))
{
  fn (widget); /* on */
  while (g_main_context_iteration (NULL, FALSE));
  fn (widget); /* off */
  while (g_main_context_iteration (NULL, FALSE));
}

void
do_real_bench (GtkWidget *widget, void (* fn) (GtkWidget *widget), char *name, int num)
{
  GTimeVal tv0, tv1;
  double dt_first;
  double dt;
  int n;
  static gboolean printed_headers = FALSE;

  if (!printed_headers) {
    g_print ("Test                 Iters      First      Other\n");
    g_print ("-------------------- ----- ---------- ----------\n");
    printed_headers = TRUE;
  }

  g_get_current_time (&tv0);
  bench_iteration (widget, fn); 
  g_get_current_time (&tv1);

  dt_first = ((double)tv1.tv_sec - tv0.tv_sec) * 1000.0
	+ (tv1.tv_usec - tv0.tv_usec) / 1000.0;

  g_get_current_time (&tv0);
  for (n = 0; n < num - 1; n++)
    bench_iteration (widget, fn); 
  g_get_current_time (&tv1);
  dt = ((double)tv1.tv_sec - tv0.tv_sec) * 1000.0
	+ (tv1.tv_usec - tv0.tv_usec) / 1000.0;

  g_print ("%s %5d ", pad (name, 20), num);
  if (num > 1)
    g_print ("%10.1f %10.1f\n", dt_first, dt/(num-1));
  else
    g_print ("%10.1f\n", dt_first);
}

void
do_bench (char* what, int num)
{
  int i;
  GtkWidget *widget;
  void (* fn) (GtkWidget *widget);
  fn = NULL;
  widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  if (g_ascii_strcasecmp (what, "ALL") == 0)
    {
      for (i = 0; i < nbuttons; i++)
	{
	  if (!buttons[i].do_not_benchmark)
	    do_real_bench (widget, buttons[i].func, buttons[i].label, num);
	}

      return;
    }
  else
    {
      for (i = 0; i < nbuttons; i++)
	{
	  if (strcmp (buttons[i].label, what) == 0)
	    {
	      fn = buttons[i].func;
	      break;
	    }
	}
      
      if (!fn)
	g_print ("Can't bench: \"%s\" not found.\n", what);
      else
	do_real_bench (widget, fn, buttons[i].label, num);
    }
}

void 
usage (void)
{
  fprintf (stderr, "Usage: testgtk [--bench ALL|<bench>[:<count>]]\n");
  exit (1);
}

int
main (int argc, char *argv[])
{
  GtkBindingSet *binding_set;
  int i;
  gboolean done_benchmarks = FALSE;

  srand (time (NULL));

  test_init ();

  /* Check to see if we are being run from the correct
   * directory.
   */
  if (file_exists ("testgtkrc"))
    gtk_rc_add_default_file ("testgtkrc");
  else if (file_exists ("tests/testgtkrc"))
    gtk_rc_add_default_file ("tests/testgtkrc");
  else
    g_warning ("Couldn't find file \"testgtkrc\".");

  g_set_application_name ("GTK+ Test Program");

  gtk_init (&argc, &argv);

  gtk_accelerator_set_default_mod_mask (GDK_SHIFT_MASK |
					GDK_CONTROL_MASK |
					GDK_MOD1_MASK | 
					GDK_META_MASK |
					GDK_SUPER_MASK |
					GDK_HYPER_MASK |
					GDK_MOD4_MASK);
  /*  benchmarking
   */
  for (i = 1; i < argc; i++)
    {
      if (strncmp (argv[i], "--bench", strlen("--bench")) == 0)
        {
          int num = 1;
	  char *nextarg;
	  char *what;
	  char *count;
	  
	  nextarg = strchr (argv[i], '=');
	  if (nextarg)
	    nextarg++;
	  else
	    {
	      i++;
	      if (i == argc)
		usage ();
	      nextarg = argv[i];
	    }

	  count = strchr (nextarg, ':');
	  if (count)
	    {
	      what = g_strndup (nextarg, count - nextarg);
	      count++;
	      num = atoi (count);
	      if (num <= 0)
		usage ();
	    }
	  else
	    what = g_strdup (nextarg);

          do_bench (what, num ? num : 1);
	  done_benchmarks = TRUE;
        }
      else
	usage ();
    }
  if (done_benchmarks)
    return 0;

  /* bindings test
   */
  binding_set = gtk_binding_set_by_class (g_type_class_ref (GTK_TYPE_WIDGET));
  gtk_binding_entry_add_signal (binding_set,
				'9', GDK_CONTROL_MASK | GDK_RELEASE_MASK,
				"debug_msg",
				1,
				G_TYPE_STRING, "GtkWidgetClass <ctrl><release>9 test");
  
  /* We use gtk_rc_parse_string() here so we can make sure it works across theme
   * changes
   */

  gtk_rc_parse_string ("style \"testgtk-version-label\" { "
		       "   fg[NORMAL] = \"#ff0000\"\n"
		       "   font = \"Sans 18\"\n"
		       "}\n"
		       "widget \"*.testgtk-version-label\" style \"testgtk-version-label\"");
  
  create_main_window ();

  gtk_main ();

  if (1)
    {
      while (g_main_context_pending (NULL))
	g_main_context_iteration (NULL, FALSE);
#if 0
      sleep (1);
      while (g_main_context_pending (NULL))
	g_main_context_iteration (NULL, FALSE);
#endif
    }
  return 0;
}
