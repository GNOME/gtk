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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */


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

#include "gtk/gtk.h"
#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"

#ifdef G_OS_WIN32
#define sleep(n) _sleep(n)
#endif

#include "prop-editor.h"

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
  gint i;

  omenu = gtk_combo_box_text_new ();
  g_signal_connect (omenu, "changed",
		    G_CALLBACK (func), data);
      
  for (i = 0; i < num_items; i++)
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (omenu), items[i]);

  gtk_combo_box_set_active (GTK_COMBO_BOX (omenu), history);
  
  return omenu;
}

/*
 * Windows with an alpha channel
 */


static gboolean
on_alpha_window_draw (GtkWidget *widget,
                      cairo_t   *cr)
{
  cairo_pattern_t *pattern;
  int radius, width, height;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  radius = MIN (width, height) / 2;
  pattern = cairo_pattern_create_radial (width / 2,
                                         height / 2,
					 0.0,
                                         width / 2,
                                         height / 2,
					 radius * 1.33);

  if (gdk_screen_get_rgba_visual (gtk_widget_get_screen (widget)) &&
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

  return FALSE;
}

static GtkWidget *
build_alpha_widgets (void)
{
  GtkWidget *grid;
  GtkWidget *radio_button;
  GtkWidget *check_button;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *entry;

  grid = gtk_grid_new ();

  radio_button = gtk_radio_button_new_with_label (NULL, "Red");
  gtk_widget_set_hexpand (radio_button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), radio_button, 0, 0, 1, 1);

  radio_button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio_button), "Green");
  gtk_widget_set_hexpand (radio_button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), radio_button, 0, 1, 1, 1);

  radio_button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio_button), "Blue"),
  gtk_widget_set_hexpand (radio_button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), radio_button, 0, 2, 1, 1);

  check_button = gtk_check_button_new_with_label ("Sedentary"),
  gtk_widget_set_hexpand (check_button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), check_button, 1, 0, 1, 1);

  check_button = gtk_check_button_new_with_label ("Nocturnal"),
  gtk_widget_set_hexpand (check_button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), check_button, 1, 1, 1, 1);

  check_button = gtk_check_button_new_with_label ("Compulsive"),
  gtk_widget_set_hexpand (check_button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), check_button, 1, 2, 1, 1);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), "<i>Entry: </i>");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
  gtk_widget_set_hexpand (hbox, TRUE);
  gtk_grid_attach (GTK_GRID (grid), hbox, 0, 3, 2, 1);
  
  return grid;
}

static void
on_alpha_screen_changed (GtkWindow *window,
			 GdkScreen *old_screen,
			 GtkWidget *label)
{
  GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (window));
  GdkVisual *visual = gdk_screen_get_rgba_visual (screen);

  if (!visual)
    {
      visual = gdk_screen_get_system_visual (screen);
      gtk_label_set_markup (GTK_LABEL (label), "<b>Screen doesn't support alpha</b>");
    }
  else
    {
      gtk_label_set_markup (GTK_LABEL (label), "<b>Screen supports alpha</b>");
    }

  gtk_widget_set_visual (GTK_WIDGET (window), visual);
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
      GtkWidget *content_area;
      GtkWidget *vbox;
      GtkWidget *label;
      
      window = gtk_dialog_new_with_buttons ("Alpha Window",
					    GTK_WINDOW (gtk_widget_get_toplevel (widget)), 0,
					    GTK_STOCK_CLOSE, 0,
					    NULL);

      gtk_widget_set_app_paintable (window, TRUE);
      g_signal_connect (window, "draw",
			G_CALLBACK (on_alpha_window_draw), NULL);

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));
      
      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
      gtk_box_pack_start (GTK_BOX (content_area), vbox,
			  TRUE, TRUE, 0);

      label = gtk_label_new (NULL);
      gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
      on_alpha_screen_changed (GTK_WINDOW (window), NULL, label);
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

/* The draw event handler for the event box.
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
transparent_draw (GtkWidget *widget,
                  cairo_t   *cr)
{
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);

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
window_draw (GtkWidget *widget,
             cairo_t   *cr)
{
  GtkAllocation allocation;
  GtkWidget *child;

  /* get our child (in this case, the event box) */ 
  child = gtk_bin_get_child (GTK_BIN (widget));

  gtk_widget_get_allocation (child, &allocation);

  /* the source data is the (composited) event box */
  gdk_cairo_set_source_window (cr, gtk_widget_get_window (child),
                               allocation.x,
                               allocation.y);

  /* composite, with a 50% opacity */
  cairo_paint_with_alpha (cr, 0.5);

  return FALSE;
}

void
create_composited_window (GtkWidget *widget)
{
  static GtkWidget *window;

  if (!window)
    {
      GtkWidget *event, *button;
      GdkRGBA red;

      /* make the widgets */
      button = gtk_button_new_with_label ("A Button");
      event = gtk_event_box_new ();
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &window);

      /* put a red background on the window */
      gdk_rgba_parse (&red, "red");
      gtk_widget_override_background_color (window, 0, &red);

      /* set our event box to have a fully-transparent background
       * drawn on it.  currently there is no way to simply tell gtk
       * that "transparency" is the background colour for a widget.
       */
      gtk_widget_set_app_paintable (GTK_WIDGET (event), TRUE);
      g_signal_connect (event, "draw",
                        G_CALLBACK (transparent_draw), NULL);

      /* put them inside one another */
      gtk_container_set_border_width (GTK_CONTAINER (window), 10);
      gtk_container_add (GTK_CONTAINER (window), event);
      gtk_container_add (GTK_CONTAINER (event), button);

      /* realise and show everything */
      gtk_widget_realize (button);

      /* set the event box GdkWindow to be composited.
       * obviously must be performed after event box is realised.
       */
      gdk_window_set_composited (gtk_widget_get_window (event),
                                 TRUE);

      /* set up the compositing handler.
       * note that we do _after so that the normal (red) background is drawn
       * by gtk before our compositing occurs.
       */
      g_signal_connect_after (window, "draw",
                              G_CALLBACK (window_draw), NULL);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Big windows and guffaw scrolling
 */

static void
pattern_set_bg (GtkWidget   *widget,
		GdkWindow   *child,
		gint         level)
{
  static GdkRGBA colors[] = {
    { 0.27, 0.27, 1.0, 1.0 },
    { 0.53, 0.53, 1.0, 1.0},
    { 0.67, 0.67, 1.0, 1.0 }
  };
    
  gdk_window_set_user_data (child, widget);
  gdk_window_set_background_rgba (child, &colors[level]);
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
	      
	      child = gdk_window_new (parent, &attributes,
				      GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL);

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
pattern_hadj_changed (GtkAdjustment *adjustment,
		      GtkWidget     *darea)
{
  gint *old_value = g_object_get_data (G_OBJECT (adjustment), "old-value");
  gint new_value = gtk_adjustment_get_value (adjustment);

  if (gtk_widget_get_realized (darea))
    {
      gdk_window_scroll (gtk_widget_get_window (darea),
                         *old_value - new_value, 0);
      *old_value = new_value;
    }
}

static void
pattern_vadj_changed (GtkAdjustment *adjustment,
		      GtkWidget *darea)
{
  gint *old_value = g_object_get_data (G_OBJECT (adjustment), "old-value");
  gint new_value = gtk_adjustment_get_value (adjustment);

  if (gtk_widget_get_realized (darea))
    {
      gdk_window_scroll (gtk_widget_get_window (darea),
                         0, *old_value - new_value);
      *old_value = new_value;
    }
}

static void
pattern_realize (GtkWidget *widget,
		 gpointer   data)
{
  GdkWindow *window;

  window = gtk_widget_get_window (widget);
  pattern_set_bg (widget, window, 0);
  create_pattern (widget, window, 1, PATTERN_SIZE, PATTERN_SIZE);
}

static void 
create_big_windows (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *content_area;
  GtkWidget *darea, *grid, *scrollbar;
  GtkWidget *eventbox;
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;
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

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));

      grid = gtk_grid_new ();
      gtk_box_pack_start (GTK_BOX (content_area), grid, TRUE, TRUE, 0);

      darea = gtk_drawing_area_new ();

      hadjustment = gtk_adjustment_new (0, 0, PATTERN_SIZE, 10, 100, 100);
      g_signal_connect (hadjustment, "value_changed",
			G_CALLBACK (pattern_hadj_changed), darea);
      g_object_set_data (G_OBJECT (hadjustment), "old-value", &current_x);

      vadjustment = gtk_adjustment_new (0, 0, PATTERN_SIZE, 10, 100, 100);
      g_signal_connect (vadjustment, "value_changed",
			G_CALLBACK (pattern_vadj_changed), darea);
      g_object_set_data (G_OBJECT (vadjustment), "old-value", &current_y);
      
      g_signal_connect (darea, "realize",
                        G_CALLBACK (pattern_realize),
                        NULL);

      eventbox = gtk_event_box_new ();
      gtk_widget_set_hexpand (eventbox, TRUE);
      gtk_widget_set_vexpand (eventbox, TRUE);
      gtk_grid_attach (GTK_GRID (grid), eventbox, 0, 0, 1, 1);

      gtk_container_add (GTK_CONTAINER (eventbox), darea);

      scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_HORIZONTAL, hadjustment);
      gtk_widget_set_hexpand (scrollbar, TRUE);
      gtk_grid_attach (GTK_GRID (grid), scrollbar, 0, 1, 1, 1);

      scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, vadjustment);
      gtk_widget_set_vexpand (scrollbar, TRUE);
      gtk_grid_attach (GTK_GRID (grid), scrollbar, 1, 0, 1, 1);
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
  GtkWidget *grid;
  GtkWidget *separator;
  GtkWidget *button[10];
  int button_x[9] = { 0, 1, 2, 0, 2, 1, 1, 2, 0 };
  int button_y[9] = { 0, 1, 2, 2, 0, 2, 0, 1, 1 };
  guint i;

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

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      grid = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (grid), 5);
      gtk_grid_set_column_spacing (GTK_GRID (grid), 5);
      gtk_container_set_border_width (GTK_CONTAINER (grid), 10);
      gtk_box_pack_start (GTK_BOX (box1), grid, TRUE, TRUE, 0);

      button[0] = gtk_button_new_with_label ("button1");
      button[1] = gtk_button_new_with_mnemonic ("_button2");
      button[2] = gtk_button_new_with_mnemonic ("_button3");
      button[3] = gtk_button_new_from_stock (GTK_STOCK_OK);
      button[4] = gtk_button_new_with_label ("button5");
      button[5] = gtk_button_new_with_label ("button6");
      button[6] = gtk_button_new_with_label ("button7");
      button[7] = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
      button[8] = gtk_button_new_with_label ("button9");
      
      for (i = 0; i < 9; i++)
        {
          g_signal_connect (button[i], "clicked",
                            G_CALLBACK (button_window),
                            button[(i + 1) % 9]);
          gtk_widget_set_hexpand (button[i], TRUE);
          gtk_widget_set_vexpand (button[i], TRUE);

          gtk_grid_attach (GTK_GRID (grid), button[i],
                           button_x[i], button_y[i] + 1, 1, 1);
        }

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
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

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
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
      
      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
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
  GtkWidget *grid;
  GtkWidget *group_widget = NULL;
  gint i, j;
  
  grid = gtk_grid_new ();
  
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
	    gtk_grid_attach (GTK_GRID (grid), widget, i, j, 1, 1);
	}
    }

  return grid;
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

      box1 = gtk_dialog_get_content_area (GTK_DIALOG (window));
      
      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
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

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
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

      box1 = gtk_dialog_get_content_area (GTK_DIALOG (window));

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
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
      
      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
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

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
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
    bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  else
    bbox = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);

  gtk_container_set_border_width (GTK_CONTAINER (bbox), 5);
  gtk_container_add (GTK_CONTAINER (frame), bbox);

  gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), layout);
  gtk_box_set_spacing (GTK_BOX (bbox), spacing);
  
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

    main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add (GTK_CONTAINER (window), main_vbox);
    
    frame_horz = gtk_frame_new ("Horizontal Button Boxes");
    gtk_box_pack_start (GTK_BOX (main_vbox), frame_horz, TRUE, TRUE, 10);
    
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
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
    
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
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
new_pixbuf (char      *filename,
	    GdkWindow *window)
{
  GtkWidget *widget;
  GdkPixbuf *pixbuf;

  if (strcmp (filename, "test.xpm") == 0)
    pixbuf = NULL;
  else
    pixbuf = gdk_pixbuf_new_from_file (filename, NULL);

  if (pixbuf == NULL)
    pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) openfile);
  
  widget = gtk_image_new_from_pixbuf (pixbuf);

  g_object_unref (pixbuf);

  return widget;
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
  gtk_orientable_set_orientation (GTK_ORIENTABLE (data), GTK_ORIENTATION_HORIZONTAL);
}

static void
set_toolbar_vertical (GtkWidget *widget,
		      gpointer   data)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (data), GTK_ORIENTATION_VERTICAL);
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
  GtkSettings *settings = gtk_widget_get_settings (widget);
  g_object_set (settings, "gtk-enable-tooltips", TRUE, NULL);
}

static void
set_toolbar_disable (GtkWidget *widget,
		     gpointer   data)
{
  GtkSettings *settings = gtk_widget_get_settings (widget);
  g_object_set (settings, "gtk-enable-tooltips", FALSE, NULL);
}

static GtkActionEntry create_toolbar_items[] = {
    { NULL, GTK_STOCK_NEW, NULL, NULL, "Stock icon: New",
      G_CALLBACK (set_toolbar_small_stock) },
    { NULL, GTK_STOCK_OPEN, NULL, NULL, "Stock icon: Open",
      G_CALLBACK (set_toolbar_large_stock) },
    { NULL, NULL, "Horizontal", NULL, "Horizontal toolbar layout",
      G_CALLBACK (set_toolbar_horizontal) },
    { NULL, NULL, "Vertical", NULL, "Vertical toolbar layout",
      G_CALLBACK (set_toolbar_vertical) },
    { NULL },
    { NULL, NULL, "Icons", NULL, "Only show toolbar icons",
      G_CALLBACK (set_toolbar_icons) },
    { NULL, NULL, "Text", NULL, "Only show toolbar text",
      G_CALLBACK (set_toolbar_text) },
    { NULL, NULL, "Both", NULL, "Show toolbar icons and text",
      G_CALLBACK (set_toolbar_both) },
    { NULL, NULL, "Both (horizontal)", NULL, "Show toolbar icons and text in a horizontal fashion",
      G_CALLBACK (set_toolbar_both_horiz) },
    { NULL },
    { "entry", NULL, NULL, "This is an unusable GtkEntry ;)",
      NULL },
    { NULL },
    { NULL },
    { NULL, NULL, "Enable", NULL, "Enable tooltips",
      G_CALLBACK (set_toolbar_enable) },
    { NULL, NULL, "Disable", NULL, "Disable tooltips",
      G_CALLBACK (set_toolbar_disable) },
    { NULL },
    { NULL, NULL, "Frobate", NULL, "Frobate tooltip",
      NULL },
    { NULL, NULL, "Baz", NULL, "Baz tooltip",
      NULL },
    { NULL },
    { NULL, NULL, "Blah", NULL, "Blash tooltip",
      NULL },
    { NULL, NULL, "Bar", NULL, "Bar tooltip",
      NULL },
};

static void
create_toolbar (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *toolbar;

  if (!window)
    {
      guint i;

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
      for (i = 0; i < G_N_ELEMENTS (create_toolbar_items); i++)
        {
          GtkToolItem *toolitem;

          if (create_toolbar_items[i].tooltip == NULL)
            toolitem = gtk_separator_tool_item_new ();
          else if (g_strcmp0 (create_toolbar_items[i].name, "entry") == 0)
            {
              GtkWidget *entry;

              toolitem = gtk_tool_item_new ();
              entry = gtk_entry_new ();
              gtk_container_add (GTK_CONTAINER (toolitem), entry);
            }
          else if (create_toolbar_items[i].stock_id)
            toolitem = gtk_tool_button_new_from_stock (create_toolbar_items[i].stock_id);
          else
            {
              GtkWidget *icon;

              icon = new_pixbuf ("test.xpm", gtk_widget_get_window (window));
              toolitem = gtk_tool_button_new (icon, create_toolbar_items[i].label);
            }
          if (create_toolbar_items[i].callback)
            g_signal_connect (toolitem, "clicked",
                              create_toolbar_items[i].callback, toolbar);
          gtk_tool_item_set_tooltip_text (toolitem, create_toolbar_items[i].tooltip);
          gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
        }

      gtk_container_add (GTK_CONTAINER (window), toolbar);

      gtk_widget_set_size_request (toolbar, 200, -1);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
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
  if (!text)
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

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
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
      
      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
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

/* Alpha demo */

static void
alpha_changed (GtkRange *range, GtkWidget *widget)
{
  gdouble alpha;

  alpha = gtk_range_get_value (range);

  gtk_widget_set_opacity (widget, alpha / 100.0);
}


void
create_alpha_widgets (GtkWidget *widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *vbox2, *vbox, *main_hbox;
      GtkWidget *button, *event_box, *label, *scale;
      GtkWidget *alpha1, *alpha2, *alpha3;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (widget));
      gtk_window_set_default_size (GTK_WINDOW (window),
                                   450, 450);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_window_set_title (GTK_WINDOW (window), "Alpha");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      main_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_add (GTK_CONTAINER (window), main_hbox);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

      gtk_box_pack_start (GTK_BOX (main_hbox), vbox, FALSE, FALSE, 0);

      /* Plain button (no gdkwindows */

      label = gtk_label_new ("non-window widget");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      alpha1 = button = gtk_button_new_with_label ("A Button");
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

      /* windowed container with both windowed and normal button */
      label = gtk_label_new ("\nwindow widget");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      alpha2 = event_box = gtk_event_box_new ();
      gtk_box_pack_start (GTK_BOX (vbox), event_box, FALSE, FALSE, 0);

      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (event_box), vbox2);

      button = gtk_button_new_with_label ("A Button");
      gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);

      event_box = gtk_event_box_new ();
      button = gtk_button_new_with_label ("A Button (in window)");
      gtk_container_add (GTK_CONTAINER (event_box), button);
      gtk_box_pack_start (GTK_BOX (vbox2), event_box, FALSE, FALSE, 0);

      /* non-windowed container with both windowed and normal button */
      label = gtk_label_new ("\nnon-window widget with widget child");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      alpha3 = vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_pack_start (GTK_BOX (vbox), vbox2, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("A Button");
      gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);

      event_box = gtk_event_box_new ();
      button = gtk_button_new_with_label ("A Button (in window)");
      gtk_container_add (GTK_CONTAINER (event_box), button);
      gtk_box_pack_start (GTK_BOX (vbox2), event_box, FALSE, FALSE, 0);

      scale = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL,
                                         0, 100, 1);
      gtk_box_pack_start (GTK_BOX (main_hbox), scale, FALSE, FALSE, 0);
      g_signal_connect (scale, "value_changed", G_CALLBACK (alpha_changed), alpha1);
      gtk_range_set_value (GTK_RANGE (scale), 50);

      scale = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL,
                                         0, 100, 1);
      gtk_box_pack_start (GTK_BOX (main_hbox), scale, FALSE, FALSE, 0);
      g_signal_connect (scale, "value_changed", G_CALLBACK (alpha_changed), alpha2);
      gtk_range_set_value (GTK_RANGE (scale), 50);

      scale = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL,
                                         0, 100, 1);
      gtk_box_pack_start (GTK_BOX (main_hbox), scale, FALSE, FALSE, 0);
      g_signal_connect (scale, "value_changed", G_CALLBACK (alpha_changed), alpha3);
      gtk_range_set_value (GTK_RANGE (scale), 50);

      gtk_widget_show_all (main_hbox);
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
  gtk_widget_set_sensitive (widget,
                            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)));
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
                            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)));
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

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      button = create_sensitivity_control (hbox);

      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

      button = create_selectable_control (hbox);

      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      
      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      
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

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
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

      g_assert (gtk_label_get_mnemonic_keyval (GTK_LABEL (label)) == GDK_KEY_s);
      
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
  GtkWidget *content_area;
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
			G_CALLBACK (gtk_widget_destroy), NULL);
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed), &window);

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_box_pack_start (GTK_BOX (content_area), vbox, TRUE, TRUE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label), "Hello World\n<i>Rotate</i> <span underline='single' foreground='blue'>me</span>");
      gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

      scale_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start (GTK_BOX (vbox), scale_hbox, FALSE, FALSE, 0);
      
      scale_label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (scale_label), "<i>Angle: </i>");
      gtk_box_pack_start (GTK_BOX (scale_hbox), scale_label, FALSE, FALSE, 0);

      hscale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                         0, 360, 5);
      g_signal_connect (hscale, "value-changed",
			G_CALLBACK (on_angle_scale_changed), label);
      
      gtk_range_set_value (GTK_RANGE (hscale), 45);
      gtk_widget_set_size_request (hscale, 200, -1);
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
on_rotated_text_draw (GtkWidget *widget,
                      cairo_t   *cr,
	              GdkPixbuf *tile_pixbuf)
{
  static const gchar *words[] = { "The", "grand", "old", "Duke", "of", "York",
                                  "had", "10,000", "men" };
  int n_words;
  int i;
  int width, height;
  double radius;
  PangoLayout *layout;
  PangoContext *context;
  PangoFontDescription *desc;

  if (tile_pixbuf)
    {
      gdk_cairo_set_source_pixbuf (cr, tile_pixbuf, 0, 0);
      cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);
    }
  else
    cairo_set_source_rgb (cr, 0, 0, 0);

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  radius = MIN (width, height) / 2.;

  cairo_translate (cr,
                   radius + (width - 2 * radius) / 2,
                   radius + (height - 2 * radius) / 2);
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

  return FALSE;
}

static void
create_rotated_text (GtkWidget *widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      const GdkRGBA white = { 1.0, 1.0, 1.0, 1.0 };
      GtkRequisition requisition;
      GtkWidget *content_area;
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
			G_CALLBACK (gtk_widget_destroy), NULL);
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed), &window);

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));

      drawing_area = gtk_drawing_area_new ();
      gtk_box_pack_start (GTK_BOX (content_area), drawing_area, TRUE, TRUE, 0);
      gtk_widget_override_background_color (drawing_area, 0, &white);

      tile_pixbuf = gdk_pixbuf_new_from_file ("marble.xpm", NULL);
      
      g_signal_connect (drawing_area, "draw",
			G_CALLBACK (on_rotated_text_draw), tile_pixbuf);
      g_signal_connect (drawing_area, "unrealize",
			G_CALLBACK (on_rotated_text_unrealize), NULL);

      gtk_widget_show_all (gtk_bin_get_child (GTK_BIN (window)));
      
      gtk_widget_set_size_request (drawing_area, DEFAULT_TEXT_RADIUS * 2, DEFAULT_TEXT_RADIUS * 2);
      gtk_widget_get_preferred_size ( (window),
                                 &requisition, NULL);
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
  GtkWidget *parent;

  parent = gtk_widget_get_parent (child);
  g_message ("set_parent for \"%s\": new parent: \"%s\", old parent: \"%s\", data: %d\n",
             g_type_name (G_OBJECT_TYPE (child)),
             parent ? g_type_name (G_OBJECT_TYPE (parent)) : "NULL",
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

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      label = gtk_label_new ("Hello World");

      frame = gtk_frame_new ("Frame 1");
      gtk_box_pack_start (GTK_BOX (box2), frame, TRUE, TRUE, 0);

      box3 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
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

      box3 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
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

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
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
      if (event->button == GDK_BUTTON_PRIMARY)
	gtk_window_begin_resize_drag (GTK_WINDOW (gtk_widget_get_toplevel (area)), edge,
				      event->button, event->x_root, event->y_root,
				      event->time);
      else if (event->button == GDK_BUTTON_MIDDLE)
	gtk_window_begin_move_drag (GTK_WINDOW (gtk_widget_get_toplevel (area)), 
				    event->button, event->x_root, event->y_root,
				    event->time);
    }
  return TRUE;
}

static gboolean
grippy_draw (GtkWidget *area, cairo_t *cr, GdkWindowEdge edge)
{
  GtkStyleContext *context;
  GtkJunctionSides sides;

  switch (edge)
    {
    case GDK_WINDOW_EDGE_NORTH_WEST:
      sides = GTK_JUNCTION_CORNER_TOPLEFT;
      break;
    case GDK_WINDOW_EDGE_NORTH:
      sides = GTK_JUNCTION_TOP;
      break;
    case GDK_WINDOW_EDGE_NORTH_EAST:
      sides = GTK_JUNCTION_CORNER_TOPRIGHT;
      break;
    case GDK_WINDOW_EDGE_WEST:
      sides = GTK_JUNCTION_LEFT;
      break;
    case GDK_WINDOW_EDGE_EAST:
      sides = GTK_JUNCTION_RIGHT;
      break;
    case GDK_WINDOW_EDGE_SOUTH_WEST:
      sides = GTK_JUNCTION_CORNER_BOTTOMLEFT;
      break;
    case GDK_WINDOW_EDGE_SOUTH:
      sides = GTK_JUNCTION_BOTTOM;
      break;
    case GDK_WINDOW_EDGE_SOUTH_EAST:
      sides = GTK_JUNCTION_CORNER_BOTTOMRIGHT;
      break;
    default:
      g_assert_not_reached();
    }

  context = gtk_widget_get_style_context (area);
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "grip");
  gtk_style_context_set_junction_sides (context, sides);
  gtk_render_handle (context, cr,
                     0, 0,
                     gtk_widget_get_allocated_width (area),
                     gtk_widget_get_allocated_height (area));

  gtk_style_context_restore (context);

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

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);
      
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

      /* North west */
      area = gtk_drawing_area_new ();
      gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK);
      gtk_box_pack_start (GTK_BOX (hbox), area, TRUE, TRUE, 0);
      g_signal_connect (area, "draw", G_CALLBACK (grippy_draw),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_NORTH_WEST));
      g_signal_connect (area, "button_press_event", G_CALLBACK (grippy_button_press),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_NORTH_WEST));
      
      /* North */
      area = gtk_drawing_area_new ();
      gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK);
      gtk_box_pack_start (GTK_BOX (hbox), area, TRUE, TRUE, 0);
      g_signal_connect (area, "draw", G_CALLBACK (grippy_draw),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_NORTH));
      g_signal_connect (area, "button_press_event", G_CALLBACK (grippy_button_press),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_NORTH));

      /* North east */
      area = gtk_drawing_area_new ();
      gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK);
      gtk_box_pack_start (GTK_BOX (hbox), area, TRUE, TRUE, 0);
      g_signal_connect (area, "draw", G_CALLBACK (grippy_draw),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_NORTH_EAST));
      g_signal_connect (area, "button_press_event", G_CALLBACK (grippy_button_press),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_NORTH_EAST));

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

      /* West */
      area = gtk_drawing_area_new ();
      gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK);
      gtk_box_pack_start (GTK_BOX (hbox), area, TRUE, TRUE, 0);
      g_signal_connect (area, "draw", G_CALLBACK (grippy_draw),
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
      g_signal_connect (area, "draw", G_CALLBACK (grippy_draw),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_EAST));
      g_signal_connect (area, "button_press_event", G_CALLBACK (grippy_button_press),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_EAST));


      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

      /* South west */
      area = gtk_drawing_area_new ();
      gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK);
      gtk_box_pack_start (GTK_BOX (hbox), area, TRUE, TRUE, 0);
      g_signal_connect (area, "draw", G_CALLBACK (grippy_draw),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_SOUTH_WEST));
      g_signal_connect (area, "button_press_event", G_CALLBACK (grippy_button_press),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_SOUTH_WEST));
      /* South */
      area = gtk_drawing_area_new ();
      gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK);
      gtk_box_pack_start (GTK_BOX (hbox), area, TRUE, TRUE, 0);
      g_signal_connect (area, "draw", G_CALLBACK (grippy_draw),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_SOUTH));
      g_signal_connect (area, "button_press_event", G_CALLBACK (grippy_button_press),
			GINT_TO_POINTER (GDK_WINDOW_EDGE_SOUTH));
      
      /* South east */
      area = gtk_drawing_area_new ();
      gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK);
      gtk_box_pack_start (GTK_BOX (hbox), area, TRUE, TRUE, 0);
      g_signal_connect (area, "draw", G_CALLBACK (grippy_draw),
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

  gdk_window_get_root_origin (gtk_widget_get_window (window),
                              &upositionx, &upositiony);
  sprintf (buffer, "%d", upositionx);
  gtk_label_set_text (lx, buffer);
  sprintf (buffer, "%d", upositiony);
  gtk_label_set_text (ly, buffer);

  return FALSE;
}

static void
uposition_stop_configure (GtkToggleButton *toggle,
			  GObject         *window)
{
  if (gtk_toggle_button_get_active (toggle))
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

      main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 0);
      gtk_container_add (GTK_CONTAINER (window), main_vbox);

      vbox =
	g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
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

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

      label = gtk_label_new ("X Origin : ");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

      x_label = gtk_label_new ("");
      gtk_box_pack_start (GTK_BOX (hbox), x_label, TRUE, TRUE, 0);
      g_object_set_data (G_OBJECT (window), "x", x_label);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

      label = gtk_label_new ("Y Origin : ");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

      y_label = gtk_label_new ("");
      gtk_box_pack_start (GTK_BOX (hbox), y_label, TRUE, TRUE, 0);
      g_object_set_data (G_OBJECT (window), "y", y_label);

      any =
	g_object_new (gtk_separator_get_type (),
			"GtkWidget::visible", TRUE,
			NULL);
      gtk_box_pack_start (GTK_BOX (main_vbox), any, FALSE, TRUE, 0);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
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
create_pixbuf (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *box3;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *separator;
  GtkWidget *pixbufwid;
  GdkWindow *gdk_window;

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

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      button = gtk_button_new ();
      gtk_box_pack_start (GTK_BOX (box2), button, FALSE, FALSE, 0);

      gdk_window = gtk_widget_get_window (window);

      pixbufwid = new_pixbuf ("test.xpm", gdk_window);

      label = gtk_label_new ("Pixbuf\ntest");
      box3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_set_border_width (GTK_CONTAINER (box3), 2);
      gtk_container_add (GTK_CONTAINER (box3), pixbufwid);
      gtk_container_add (GTK_CONTAINER (box3), label);
      gtk_container_add (GTK_CONTAINER (button), box3);

      button = gtk_button_new ();
      gtk_box_pack_start (GTK_BOX (box2), button, FALSE, FALSE, 0);

      pixbufwid = new_pixbuf ("test.xpm", gdk_window);

      label = gtk_label_new ("Pixbuf\ntest");
      box3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_set_border_width (GTK_CONTAINER (box3), 2);
      gtk_container_add (GTK_CONTAINER (box3), pixbufwid);
      gtk_container_add (GTK_CONTAINER (box3), label);
      gtk_container_add (GTK_CONTAINER (button), box3);

      gtk_widget_set_sensitive (button, FALSE);
      
      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
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
create_tooltips (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *box3;
  GtkWidget *button;
  GtkWidget *toggle;
  GtkWidget *frame;
  GtkWidget *separator;

  if (!window)
    {
      window =
	g_object_new (gtk_window_get_type (),
			"GtkWindow::type", GTK_WINDOW_TOPLEVEL,
			"GtkContainer::border_width", 0,
			"GtkWindow::title", "Tooltips",
			"GtkWindow::resizable", FALSE,
			NULL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      button = gtk_toggle_button_new_with_label ("button1");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      gtk_widget_set_tooltip_text (button, "This is button 1");

      button = gtk_toggle_button_new_with_label ("button2");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      gtk_widget_set_tooltip_text (button,
        "This is button 2. This is also a really long tooltip which probably "
        "won't fit on a single line and will therefore need to be wrapped. "
        "Hopefully the wrapping will work correctly.");

      toggle = gtk_toggle_button_new_with_label ("Override TipsQuery Label");
      gtk_box_pack_start (GTK_BOX (box2), toggle, TRUE, TRUE, 0);

      gtk_widget_set_tooltip_text (toggle, "Toggle TipsQuery view.");

      box3 =
	g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
			"homogeneous", FALSE,
			"spacing", 5,
			"border_width", 5,
			"visible", TRUE,
			NULL);

      button =
	g_object_new (gtk_button_get_type (),
			"label", "[?]",
			"visible", TRUE,
			"parent", box3,
			NULL);
      gtk_box_set_child_packing (GTK_BOX (box3), button, FALSE, FALSE, 0, GTK_PACK_START);
      gtk_widget_set_tooltip_text (button, "Start the Tooltips Inspector");
      
      frame = g_object_new (gtk_frame_get_type (),
			      "label", "ToolTips Inspector",
			      "label_xalign", (double) 0.5,
			      "border_width", 0,
			      "visible", TRUE,
			      "parent", box2,
			      "child", box3,
			      NULL);
      gtk_box_set_child_packing (GTK_BOX (box2), frame, TRUE, TRUE, 10, GTK_PACK_START);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);

      gtk_widget_set_tooltip_text (button, "Push this button to close window");
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
      GdkPixbuf *pixbuf;
        
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      /* this is bogus for testing drawing when allocation < request,
       * don't copy into real code
       */
      gtk_window_set_resizable (GTK_WINDOW (window), TRUE);

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

      gtk_container_add (GTK_CONTAINER (window), vbox);

      pack_image (vbox, "Stock Warning Dialog",
                  gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING,
                                            GTK_ICON_SIZE_DIALOG));

      pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) openfile);
      
      pack_image (vbox, "Pixbuf",
                  gtk_image_new_from_pixbuf (pixbuf));

      g_object_unref (pixbuf);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * ListBox demo
 */

static int
list_sort_cb (GtkListBoxRow *a, GtkListBoxRow *b, gpointer data)
{
  gint aa = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (a), "value"));
  gint bb = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (b), "value"));
  return aa - bb;
}

static gboolean
list_filter_cb (GtkListBoxRow *row, gpointer data)
{
  gint value = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "value"));

  return value % 2 == 0;
}

static void
list_sort_clicked_cb (GtkButton *button,
                      gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_sort_func (list, list_sort_cb, NULL, NULL);
}

static void
list_filter_clicked_cb (GtkButton *button,
                        gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_filter_func (list, list_filter_cb, NULL, NULL);
}


static void
list_unfilter_clicked_cb (GtkButton *button,
                          gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_filter_func (list, NULL, NULL, NULL);
}


static void
create_listbox (GtkWidget *widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *hbox, *vbox, *scrolled, *scrolled_box, *list, *label, *button;
      GdkScreen *screen = gtk_widget_get_screen (widget);
      int i;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window), screen);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &window);
      g_signal_connect (window, "delete-event",
                        G_CALLBACK (gtk_true),
                        NULL);

      gtk_window_set_title (GTK_WINDOW (window), "listbox");

      hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_add (GTK_CONTAINER (window), hbox);

      scrolled = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
      gtk_container_add (GTK_CONTAINER (hbox), scrolled);

      scrolled_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (scrolled), scrolled_box);

      label = gtk_label_new ("This is \na LABEL\nwith rows");
      gtk_container_add (GTK_CONTAINER (scrolled_box), label);

      list = gtk_list_box_new();
      gtk_list_box_set_adjustment (GTK_LIST_BOX (list), gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled)));
      gtk_container_add (GTK_CONTAINER (scrolled_box), list);

      for (i = 0; i < 1000; i++)
        {
          gint value = g_random_int_range (0, 10000);
          label = gtk_label_new (g_strdup_printf ("Value %u", value));
          gtk_widget_show (label);
          gtk_container_add (GTK_CONTAINER (list), label);
          g_object_set_data (G_OBJECT (gtk_widget_get_parent (label)), "value", GINT_TO_POINTER (value));
        }

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (hbox), vbox);

      button = gtk_button_new_with_label ("sort");
      gtk_container_add (GTK_CONTAINER (vbox), button);
      g_signal_connect (button, "clicked", G_CALLBACK (list_sort_clicked_cb), list);

      button = gtk_button_new_with_label ("filter odd");
      gtk_container_add (GTK_CONTAINER (vbox), button);
      g_signal_connect (button, "clicked", G_CALLBACK (list_filter_clicked_cb), list);

      button = gtk_button_new_with_label ("unfilter");
      gtk_container_add (GTK_CONTAINER (vbox), button);
      g_signal_connect (button, "clicked", G_CALLBACK (list_unfilter_clicked_cb), list);
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
create_menu (GdkScreen *screen, gint depth, gint length)
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

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
      gtk_widget_show (menuitem);
      if (i == 3)
	gtk_widget_set_sensitive (menuitem, FALSE);

      if (i == 5)
        gtk_check_menu_item_set_inconsistent (GTK_CHECK_MENU_ITEM (menuitem),
                                              TRUE);

      if (i < 5)
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), 
				   create_menu (screen, depth - 1, 5));
    }

  return menu;
}

static GtkWidget*
create_table_menu (GdkScreen *screen, gint cols, gint rows)
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
      
      
      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);
      
      menubar = gtk_menu_bar_new ();
      gtk_box_pack_start (GTK_BOX (box1), menubar, FALSE, TRUE, 0);
      gtk_widget_show (menubar);
      
      menu = create_menu (screen, 2, 50);
      
      menuitem = gtk_menu_item_new_with_label ("test\nline2");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
      gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
      gtk_widget_show (menuitem);

      menu = create_table_menu (screen, 2, 50);
      
      menuitem = gtk_menu_item_new_with_label ("table");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
      gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
      gtk_widget_show (menuitem);
      
      menuitem = gtk_menu_item_new_with_label ("foo");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_menu (screen, 3, 5));
      gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
      gtk_widget_show (menuitem);

      image = gtk_image_new_from_stock (GTK_STOCK_HELP,
                                        GTK_ICON_SIZE_MENU);
      gtk_widget_show (image);
      menuitem = gtk_image_menu_item_new_with_label ("Help");
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_menu (screen, 4, 5));
      gtk_widget_set_hexpand (menuitem, TRUE);
      gtk_widget_set_halign (menuitem, GTK_ALIGN_END);
      gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
      gtk_widget_show (menuitem);
      
      menubar = gtk_menu_bar_new ();
      gtk_box_pack_start (GTK_BOX (box1), menubar, FALSE, TRUE, 0);
      gtk_widget_show (menubar);
      
      menu = create_menu (screen, 2, 10);
      
      menuitem = gtk_menu_item_new_with_label ("Second menu bar");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
      gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
      gtk_widget_show (menuitem);
      
      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);
      
      menu = create_menu (screen, 1, 5);
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
				  GDK_KEY_F1,
				  0,
				  GTK_ACCEL_VISIBLE);
      menuitem = gtk_check_menu_item_new_with_label ("Accelerator Locked");
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
      gtk_widget_show (menuitem);
      gtk_widget_add_accelerator (menuitem,
				  "activate",
				  accel_group,
				  GDK_KEY_F2,
				  0,
				  GTK_ACCEL_VISIBLE | GTK_ACCEL_LOCKED);
      menuitem = gtk_check_menu_item_new_with_label ("Accelerators Frozen");
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
      gtk_widget_show (menuitem);
      gtk_widget_add_accelerator (menuitem,
				  "activate",
				  accel_group,
				  GDK_KEY_F2,
				  0,
				  GTK_ACCEL_VISIBLE);
      gtk_widget_add_accelerator (menuitem,
				  "activate",
				  accel_group,
				  GDK_KEY_F3,
				  0,
				  GTK_ACCEL_VISIBLE);

      optionmenu = gtk_combo_box_text_new ();
      gtk_combo_box_set_active (GTK_COMBO_BOX (optionmenu), 3);
      gtk_box_pack_start (GTK_BOX (box2), optionmenu, TRUE, TRUE, 0);
      gtk_widget_show (optionmenu);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
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
      GtkWidget *content_area;

      window = gtk_dialog_new_with_buttons ("Key Lookup", NULL, 0,
					    GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					    NULL);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      /* We have to expand it so the accel labels will draw their labels
       */
      gtk_window_set_default_size (GTK_WINDOW (window), 300, -1);
      
      gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));
      
      button = gtk_button_new_with_mnemonic ("Button 1 (_a)");
      gtk_box_pack_start (GTK_BOX (content_area), button, FALSE, FALSE, 0);
      button = gtk_button_new_with_mnemonic ("Button 2 (_A)");
      gtk_box_pack_start (GTK_BOX (content_area), button, FALSE, FALSE, 0);
      button = gtk_button_new_with_mnemonic ("Button 3 (_\321\204)");
      gtk_box_pack_start (GTK_BOX (content_area), button, FALSE, FALSE, 0);
      button = gtk_button_new_with_mnemonic ("Button 4 (_\320\244)");
      gtk_box_pack_start (GTK_BOX (content_area), button, FALSE, FALSE, 0);
      button = gtk_button_new_with_mnemonic ("Button 6 (_b)");
      gtk_box_pack_start (GTK_BOX (content_area), button, FALSE, FALSE, 0);
      button = accel_button_new (accel_group, "Button 7", "<Alt><Shift>b");
      gtk_box_pack_start (GTK_BOX (content_area), button, FALSE, FALSE, 0);
      button = accel_button_new (accel_group, "Button 8", "<Alt>d");
      gtk_box_pack_start (GTK_BOX (content_area), button, FALSE, FALSE, 0);
      button = accel_button_new (accel_group, "Button 9", "<Alt>Cyrillic_ve");
      gtk_box_pack_start (GTK_BOX (content_area), button, FALSE, FALSE, 0);
      button = gtk_button_new_with_mnemonic ("Button 10 (_1)");
      gtk_box_pack_start (GTK_BOX (content_area), button, FALSE, FALSE, 0);
      button = gtk_button_new_with_mnemonic ("Button 11 (_!)");
      gtk_box_pack_start (GTK_BOX (content_area), button, FALSE, FALSE, 0);
      button = accel_button_new (accel_group, "Button 12", "<Super>a");
      gtk_box_pack_start (GTK_BOX (content_area), button, FALSE, FALSE, 0);
      button = accel_button_new (accel_group, "Button 13", "<Hyper>a");
      gtk_box_pack_start (GTK_BOX (content_area), button, FALSE, FALSE, 0);
      button = accel_button_new (accel_group, "Button 14", "<Meta>a");
      gtk_box_pack_start (GTK_BOX (content_area), button, FALSE, FALSE, 0);
      button = accel_button_new (accel_group, "Button 15", "<Shift><Mod4>b");
      gtk_box_pack_start (GTK_BOX (content_area), button, FALSE, FALSE, 0);

      window_ptr = &window;
      g_object_add_weak_pointer (G_OBJECT (window), window_ptr);
      g_signal_connect (window, "response", G_CALLBACK (gtk_widget_destroy), NULL);

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

    csd = gtk_color_chooser_dialog_new ("This is a modal color selection dialog", GTK_WINDOW (parent));

    /* Set as modal */
    gtk_window_set_modal (GTK_WINDOW(csd),TRUE);

    g_signal_connect (csd, "destroy",
		      G_CALLBACK (cmw_destroy_cb), NULL);
    g_signal_connect (csd, "response",
                      G_CALLBACK (gtk_widget_destroy), NULL);
    
    /* wait until destroy calls gtk_main_quit */
    gtk_widget_show (csd);    
    gtk_main ();
}

static void
cmw_file (GtkWidget *widget, GtkWidget *parent)
{
    GtkWidget *fs;

    fs = gtk_file_chooser_dialog_new ("This is a modal file selection dialog",
      GTK_WINDOW (parent), GTK_FILE_CHOOSER_ACTION_OPEN,
      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
      NULL);
    gtk_window_set_screen (GTK_WINDOW (fs), gtk_widget_get_screen (parent));
    gtk_window_set_modal (GTK_WINDOW (fs), TRUE);

    g_signal_connect (fs, "destroy",
                      G_CALLBACK (cmw_destroy_cb), NULL);
    g_signal_connect_swapped (fs, "response",
                      G_CALLBACK (gtk_widget_destroy), fs);

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
  box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  frame1 = gtk_frame_new ("Standard dialogs in modal form");
  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_box_set_homogeneous (GTK_BOX (box2), TRUE);
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
  gtk_box_pack_start (GTK_BOX (box1), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);
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
static gulong sw_destroyed_handler = 0;

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
      sw_parent = gtk_widget_get_parent (scrollwin);
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
  GtkWidget *content_area, *action_area;
  GtkWidget *scrolled_window;
  GtkWidget *grid;
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

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));
      action_area = gtk_dialog_get_action_area (GTK_DIALOG (window));

      gtk_window_set_title (GTK_WINDOW (window), "dialog");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      scrolled_window = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 10);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (content_area), scrolled_window, TRUE, TRUE, 0);
      gtk_widget_show (scrolled_window);

      grid = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
      gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
      gtk_container_add (GTK_CONTAINER (scrolled_window), grid);
      gtk_container_set_focus_hadjustment (GTK_CONTAINER (grid),
					   gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scrolled_window)));
      gtk_container_set_focus_vadjustment (GTK_CONTAINER (grid),
					   gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window)));
      gtk_widget_show (grid);

      for (i = 0; i < 20; i++)
	for (j = 0; j < 20; j++)
	  {
	    sprintf (buffer, "button (%d,%d)\n", i, j);
	    button = gtk_toggle_button_new_with_label (buffer);
	    gtk_grid_attach (GTK_GRID (grid), button, i, j, 1, 1);
	    gtk_widget_show (button);
	  }


      button = gtk_button_new_with_label ("Close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Reparent Out");
      g_signal_connect (button, "clicked",
			G_CALLBACK (scrolled_windows_remove),
			scrolled_window);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);
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
                            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

static void
entry_toggle_sensitive (GtkWidget *checkbutton,
			GtkWidget *entry)
{
   gtk_widget_set_sensitive (entry,
                             gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton)));
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

  return G_SOURCE_CONTINUE;
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
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)))
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
                     GUINT_TO_POINTER ((guint) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton))));
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
  GtkWidget *entry;
  GtkComboBoxText *cb;
  GtkWidget *cb_entry;
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

      gtk_window_set_title (GTK_WINDOW (window), "entry");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);


      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
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
      gtk_box_pack_start (GTK_BOX (box2), GTK_WIDGET (cb), TRUE, TRUE, 0);

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

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
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
      
      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
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
                                    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

static void
event_box_toggle_above_child (GtkWidget *checkbutton,
			      GtkEventBox *event_box)
{
  gtk_event_box_set_above_child (event_box,
                                 gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
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
  GdkRGBA color;

  if (!window)
    {
      color.red = 0;
      color.blue = 1;
      color.green = 0;
      color.alpha = 1;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      gtk_window_set_title (GTK_WINDOW (window), "event box");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_override_background_color (window, 0, &color);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start (GTK_BOX (box1), hbox, TRUE, FALSE, 0);
      
      event_box = gtk_event_box_new ();
      gtk_box_pack_start (GTK_BOX (hbox), event_box, TRUE, FALSE, 0);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
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
      
      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
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
  gtk_widget_set_size_request (gtk_bin_get_child (GTK_BIN (button)),
			       gtk_spin_button_get_value_as_int (spin_button),
			       -1);
}

static void
size_group_vsize_changed (GtkSpinButton *spin_button,
			  GtkWidget     *button)
{
  gtk_widget_set_size_request (gtk_bin_get_child (GTK_BIN (button)),
			       -1,
			       gtk_spin_button_get_value_as_int (spin_button));
}

static GtkWidget *
create_size_group_window (GdkScreen    *screen,
			  GtkSizeGroup *master_size_group)
{
  GtkWidget *content_area;
  GtkWidget *window;
  GtkWidget *grid;
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

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));

  grid = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (content_area), grid, TRUE, TRUE, 0);

  gtk_grid_set_row_spacing (GTK_GRID (grid), 5);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 5);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 5);
  gtk_widget_set_size_request (grid, 250, 250);

  hgroup1 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  hgroup2 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  vgroup1 = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
  vgroup2 = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

  main_button = gtk_button_new_with_label ("X");
  gtk_widget_set_hexpand (main_button, TRUE);
  gtk_widget_set_vexpand (main_button, TRUE);
  gtk_widget_set_halign (main_button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (main_button, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (grid), main_button, 0, 0, 1, 1);
  
  gtk_size_group_add_widget (master_size_group, main_button);
  gtk_size_group_add_widget (hgroup1, main_button);
  gtk_size_group_add_widget (vgroup1, main_button);
  gtk_widget_set_size_request (gtk_bin_get_child (GTK_BIN (main_button)),
			       SIZE_GROUP_INITIAL_SIZE,
			       SIZE_GROUP_INITIAL_SIZE);

  button = gtk_button_new ();
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_vexpand (button, TRUE);
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (grid), button, 1, 0, 1, 1);

  gtk_size_group_add_widget (vgroup1, button);
  gtk_size_group_add_widget (vgroup2, button);

  button = gtk_button_new ();
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_vexpand (button, TRUE);
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (grid), button, 0, 1, 1, 1);

  gtk_size_group_add_widget (hgroup1, button);
  gtk_size_group_add_widget (hgroup2, button);

  button = gtk_button_new ();
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_vexpand (button, TRUE);
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (grid), button, 1, 1, 1, 1);

  gtk_size_group_add_widget (hgroup2, button);
  gtk_size_group_add_widget (vgroup2, button);

  g_object_unref (hgroup1);
  g_object_unref (hgroup2);
  g_object_unref (vgroup1);
  g_object_unref (vgroup2);
  
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start (GTK_BOX (content_area), hbox, FALSE, FALSE, 0);
  
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
  gtk_spin_button_set_snap_to_ticks (spin,
                                     gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static void
toggle_numeric (GtkWidget *widget, GtkSpinButton *spin)
{
  gtk_spin_button_set_numeric (spin,
                               gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
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
    sprintf (buf, "%0.*f",
             gtk_spin_button_get_digits (spin),
             gtk_spin_button_get_value (spin));

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

  buffer = g_strdup_printf ("%0.*f",
                            gtk_spin_button_get_digits (spin),
			    gtk_spin_button_get_value (spin));
  gtk_label_set_text (label, buffer);

  g_free (buffer);
}

static gint
spin_button_time_output_func (GtkSpinButton *spin_button)
{
  GtkAdjustment *adjustment;
  static gchar buf[6];
  gdouble hours;
  gdouble minutes;

  adjustment = gtk_spin_button_get_adjustment (spin_button);
  hours = gtk_adjustment_get_value (adjustment) / 60.0;
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
  GtkAdjustment *adjustment;
  gdouble value;
  gint i;
  static gchar *month[12] = { "January", "February", "March", "April",
			      "May", "June", "July", "August", "September",
			      "October", "November", "December" };

  adjustment = gtk_spin_button_get_adjustment (spin_button);
  value = gtk_adjustment_get_value (adjustment);
  for (i = 1; i <= 12; i++)
    if (fabs (value - (double)i) < 1e-5)
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
  GtkAdjustment *adjustment;
  static gchar buf[7];
  gint val;

  adjustment = gtk_spin_button_get_adjustment (spin_button);
  val = (gint) gtk_adjustment_get_value (adjustment);
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
  GtkAdjustment *adjustment;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));
      
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);
      
      gtk_window_set_title (GTK_WINDOW (window), "GtkSpinButton");
      
      main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 10);
      gtk_container_add (GTK_CONTAINER (window), main_vbox);
      
      frame = gtk_frame_new ("Not accelerated");
      gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, TRUE, 0);
      
      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
      gtk_container_add (GTK_CONTAINER (frame), vbox);
      
      /* Time, month, hex spinners */
      
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 5);
      
      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);
      
      label = gtk_label_new ("Time :");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);

      adjustment = gtk_adjustment_new (0, 0, 1410, 30, 60, 0);
      spinner = gtk_spin_button_new (adjustment, 0, 0);
      gtk_editable_set_editable (GTK_EDITABLE (spinner), FALSE);
      g_signal_connect (spinner,
			"output",
			G_CALLBACK (spin_button_time_output_func),
			NULL);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner), TRUE);
      gtk_entry_set_width_chars (GTK_ENTRY (spinner), 5);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner, FALSE, TRUE, 0);

      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);
      
      label = gtk_label_new ("Month :");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);

      adjustment = gtk_adjustment_new (1.0, 1.0, 12.0, 1.0,
						  5.0, 0.0);
      spinner = gtk_spin_button_new (adjustment, 0, 0);
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
      
      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);

      label = gtk_label_new ("Hex :");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);

      adjustment = gtk_adjustment_new (0, 0, 255, 1, 16, 0);
      spinner = gtk_spin_button_new (adjustment, 0, 0);
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
  
      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
      gtk_container_add (GTK_CONTAINER (frame), vbox);
      
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 5);
      
      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 5);
      
      label = gtk_label_new ("Value :");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);

      adjustment = gtk_adjustment_new (0.0, -10000.0, 10000.0,
						  0.5, 100.0, 0.0);
      spinner1 = gtk_spin_button_new (adjustment, 1.0, 2);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner1), TRUE);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner1, FALSE, TRUE, 0);

      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 5);

      label = gtk_label_new ("Digits :");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);

      adjustment = gtk_adjustment_new (2, 1, 15, 1, 1, 0);
      spinner2 = gtk_spin_button_new (adjustment, 0.0, 0);
      g_signal_connect (adjustment, "value_changed",
			G_CALLBACK (change_digits),
			spinner2);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner2, FALSE, TRUE, 0);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
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

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
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
  
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_container_add (GTK_CONTAINER (frame), hbox);
      
      val_label = gtk_label_new ("0.0");

      spinner = gtk_spin_button_new_with_range (0.0, 10.0, 0.009);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (spinner), 0.0);
      g_signal_connect (spinner, "value_changed",
			G_CALLBACK (get_spin_value), val_label);
      gtk_box_pack_start (GTK_BOX (hbox), spinner, TRUE, TRUE, 5);
      gtk_box_pack_start (GTK_BOX (hbox), val_label, TRUE, TRUE, 5);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
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
cursor_draw (GtkWidget *widget,
	     cairo_t   *cr,
	     gpointer   user_data)
{
  int width, height;
  GtkStyleContext *context;
  GdkRGBA bg;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_rectangle (cr, 0, 0, width, height / 2);
  cairo_fill (cr);

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_rectangle (cr, 0, height / 2, width, height / 2);
  cairo_fill (cr);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, &bg);
  gdk_cairo_set_source_rgba (cr, &bg);
  cairo_rectangle (cr, width / 3, height / 3, width / 3, height / 3);
  cairo_fill (cr);

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
  gdk_window_set_cursor (gtk_widget_get_window (widget),
                         cursor);
  g_object_unref (cursor);
}

static gint
cursor_event (GtkWidget          *widget,
	      GdkEvent           *event,
	      GtkSpinButton	 *spinner)
{
  if ((event->type == GDK_BUTTON_PRESS) &&
      ((event->button.button == GDK_BUTTON_PRIMARY) ||
       (event->button.button == GDK_BUTTON_SECONDARY)))
    {
      gtk_spin_button_spin (spinner, event->button.button == GDK_BUTTON_PRIMARY ?
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
  GtkAdjustment *adjustment;
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
      
      main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 0);
      gtk_container_add (GTK_CONTAINER (window), main_vbox);

      vbox =
	g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
			"GtkBox::homogeneous", FALSE,
			"GtkBox::spacing", 5,
			"GtkContainer::border_width", 10,
			"GtkWidget::parent", main_vbox,
			"GtkWidget::visible", TRUE,
			NULL);

#ifdef GDK_WINDOWING_X11
      if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (vbox)))
        {
          hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
          gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
          gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

          label = gtk_label_new ("Cursor Theme : ");
          gtk_widget_set_halign (label, GTK_ALIGN_START);
          gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
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
        }
#endif

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

      label = gtk_label_new ("Cursor Value : ");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

      adjustment = gtk_adjustment_new (0,
                                0, 152,
                                2,
                                10, 0);
      spinner = gtk_spin_button_new (adjustment, 0, 0);
      gtk_box_pack_start (GTK_BOX (hbox), spinner, TRUE, TRUE, 0);

      frame =
	g_object_new (gtk_frame_get_type (),
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
			"draw",
			G_CALLBACK (cursor_draw),
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

      any = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (main_vbox), any, FALSE, TRUE, 0);
  
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
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
 * GtkColorSelection
 */

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

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
      gtk_container_add (GTK_CONTAINER (window), hbox);
      
      label = gtk_label_new ("Pick a color");
      gtk_container_add (GTK_CONTAINER (hbox), label);

      picker = gtk_color_button_new ();
      gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (picker), TRUE);
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
  GtkWidget *content_area;
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (widget);
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (toplevel));
  orientable_toggle_orientation (GTK_ORIENTABLE (content_area));
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
  GtkWidget *bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
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
  GtkWidget *action_area, *content_area;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));
      action_area = gtk_dialog_get_action_area (GTK_DIALOG (window));

      gtk_window_set_title (GTK_WINDOW (window), "Bidirectional Flipping");

      check_button = gtk_check_button_new_with_label ("Right-to-left global direction");
      gtk_container_set_border_width (GTK_CONTAINER (check_button), 10);
      gtk_box_pack_start (GTK_BOX (content_area), check_button, TRUE, TRUE, 0);

      if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button), TRUE);

      g_signal_connect (check_button, "toggled",
			G_CALLBACK (flipping_toggled_cb), NULL);

      check_button = gtk_check_button_new_with_label ("Toggle orientation of all boxes");
      gtk_container_set_border_width (GTK_CONTAINER (check_button), 10);
      gtk_box_pack_start (GTK_BOX (content_area), check_button, TRUE, TRUE, 0);

      g_signal_connect (check_button, "toggled",
			G_CALLBACK (flipping_orientation_toggled_cb), NULL);

      gtk_box_pack_start (GTK_BOX (content_area),
			  create_forward_back ("Default", GTK_TEXT_DIR_NONE),
			  TRUE, TRUE, 0);

      gtk_box_pack_start (GTK_BOX (content_area),
			  create_forward_back ("Left-to-Right", GTK_TEXT_DIR_LTR),
			  TRUE, TRUE, 0);

      gtk_box_pack_start (GTK_BOX (content_area),
			  create_forward_back ("Right-to-Left", GTK_TEXT_DIR_RTL),
			  TRUE, TRUE, 0);

      button = gtk_button_new_with_label ("Close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy), window);
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);
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
  GtkWidget *grid;
  gint i, j;
  
  grid = gtk_grid_new ();

  gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);

  for (i = 0; i < 5; i++)
    {
      for (j = 0; j < 5; j++)
        {
          GtkWidget *widget;
          
          if ((i + j) % 2)
            widget = gtk_entry_new ();
          else
            widget = gtk_button_new_with_label ("Foo");

          *list = g_list_prepend (*list, widget);
          
          gtk_widget_set_hexpand (widget, TRUE);
          gtk_widget_set_vexpand (widget, TRUE);

          gtk_grid_attach (GTK_GRID (grid), widget, i, j, 1, 1);
        }
    }

  *list = g_list_reverse (*list);
  
  return grid;
}

static void
create_focus (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  
  if (!window)
    {
      GtkWidget *content_area;
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

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));
      
      gtk_window_set_title (GTK_WINDOW (window), "Keyboard Focus Navigation");

      frame = gtk_frame_new ("Weird tab focus chain");

      gtk_box_pack_start (GTK_BOX (content_area), frame, TRUE, TRUE, 0);
      
      table = make_focus_table (&list);

      gtk_container_add (GTK_CONTAINER (frame), table);

      gtk_container_set_focus_chain (GTK_CONTAINER (table),
                                     list);

      g_list_free (list);
      
      frame = gtk_frame_new ("Default tab focus chain");

      gtk_box_pack_start (GTK_BOX (content_area), frame, TRUE, TRUE, 0);

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

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
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
      g_object_set (*label, "margin", 10, NULL);
      gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog_window))),
			  *label, TRUE, TRUE, 0);
      gtk_widget_show (*label);
    }
  else
    gtk_widget_destroy (*label);
}

static void
create_dialog (GtkWidget *widget)
{
  static GtkWidget *label;
  GtkWidget *action_area;
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

      g_signal_connect (dialog_window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&dialog_window);

      action_area = gtk_dialog_get_action_area (GTK_DIALOG (dialog_window));

      gtk_window_set_title (GTK_WINDOW (dialog_window), "GtkDialog");
      gtk_container_set_border_width (GTK_CONTAINER (dialog_window), 0);

      button = gtk_button_new_with_label ("OK");
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Toggle");
      g_signal_connect (button, "clicked",
			G_CALLBACK (label_toggle),
			&label);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      label = NULL;
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
  GtkWidget *combo;
  GtkWidget *entry;
  GtkWidget *toplevel;
  GtkWidget *dialog_window;
} ScreenDisplaySelection;

static void
screen_display_check (GtkWidget *widget, ScreenDisplaySelection *data)
{
  const gchar *display_name;
  GdkDisplay *display = gtk_widget_get_display (widget);
  GtkWidget *dialog;
  GdkScreen *new_screen = NULL;
  GdkScreen *current_screen = gtk_widget_get_screen (widget);
  
  display_name = gtk_entry_get_text (GTK_ENTRY (data->entry));
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
  GtkWidget *grid, *frame, *window, *combo_dpy, *vbox;
  GtkWidget *label_dpy, *applyb, *cancelb;
  GtkWidget *bbox;
  ScreenDisplaySelection *scr_dpy_data;
  GdkScreen *screen = gtk_widget_get_screen (widget);

  window = g_object_new (gtk_window_get_type (),
			 "screen", screen,
			 "type", GTK_WINDOW_TOPLEVEL,
			 "title", "Screen or Display selection",
			 "border_width",
                         10, NULL);
  g_signal_connect (window, "destroy", 
		    G_CALLBACK (gtk_widget_destroy), NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  
  frame = gtk_frame_new ("Select display");
  gtk_container_add (GTK_CONTAINER (vbox), frame);
  
  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 3);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 3);

  gtk_container_add (GTK_CONTAINER (frame), grid);

  label_dpy = gtk_label_new ("move to another X display");
  combo_dpy = gtk_combo_box_text_new_with_entry ();
  gtk_widget_set_hexpand (combo_dpy, TRUE);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_dpy), "diabolo:0.0");
  gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (combo_dpy))),
                      "<hostname>:<X Server Num>.<Screen Num>");

  gtk_grid_attach (GTK_GRID (grid), label_dpy, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), combo_dpy, 0, 1, 1, 1);

  bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  applyb = gtk_button_new_from_stock (GTK_STOCK_APPLY);
  cancelb = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  
  gtk_container_add (GTK_CONTAINER (vbox), bbox);

  gtk_container_add (GTK_CONTAINER (bbox), applyb);
  gtk_container_add (GTK_CONTAINER (bbox), cancelb);

  scr_dpy_data = g_new0 (ScreenDisplaySelection, 1);

  scr_dpy_data->entry = gtk_bin_get_child (GTK_BIN (combo_dpy));
  scr_dpy_data->toplevel = gtk_widget_get_toplevel (widget);
  scr_dpy_data->dialog_window = window;

  g_signal_connect (cancelb, "clicked", 
		    G_CALLBACK (screen_display_destroy_diag), window);
  g_signal_connect (applyb, "clicked", 
		    G_CALLBACK (screen_display_check), scr_dpy_data);
  gtk_widget_show_all (window);
}

/* Event Watcher
 */
static gulong event_watcher_enter_id = 0;
static gulong event_watcher_leave_id = 0;

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
  GtkWidget *action_area, *content_area;
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

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog_window));
      action_area = gtk_dialog_get_action_area (GTK_DIALOG (dialog_window));

      gtk_window_set_title (GTK_WINDOW (dialog_window), "Event Watcher");
      gtk_container_set_border_width (GTK_CONTAINER (dialog_window), 0);
      gtk_widget_set_size_request (dialog_window, 200, 110);

      button = gtk_toggle_button_new_with_label ("Activate Watch");
      g_signal_connect (button, "clicked",
			G_CALLBACK (event_watcher_toggle),
			NULL);
      gtk_container_set_border_width (GTK_CONTAINER (button), 10);
      gtk_box_pack_start (GTK_BOX (content_area), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_widget_destroy),
				dialog_window);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);
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
  GtkAdjustment *adjustment;
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


      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);


      adjustment = gtk_adjustment_new (0.0, 0.0, 101.0, 0.1, 1.0, 1.0);

      scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, GTK_ADJUSTMENT (adjustment));
      gtk_widget_set_size_request (GTK_WIDGET (scale), 150, -1);
      gtk_scale_set_digits (GTK_SCALE (scale), 1);
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_box_pack_start (GTK_BOX (box2), scale, TRUE, TRUE, 0);
      gtk_widget_show (scale);

      scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_ADJUSTMENT (adjustment));
      gtk_box_pack_start (GTK_BOX (box2), scrollbar, TRUE, TRUE, 0);
      gtk_widget_show (scrollbar);

      scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, GTK_ADJUSTMENT (adjustment));
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      g_signal_connect (scale,
                        "format_value",
                        G_CALLBACK (reformat_value),
                        NULL);
      gtk_box_pack_start (GTK_BOX (box2), scale, TRUE, TRUE, 0);
      gtk_widget_show (scale);
      
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

      scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, GTK_ADJUSTMENT (adjustment));
      gtk_widget_set_size_request (scale, -1, 200);
      gtk_scale_set_digits (GTK_SCALE (scale), 2);
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_box_pack_start (GTK_BOX (hbox), scale, TRUE, TRUE, 0);
      gtk_widget_show (scale);

      scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, GTK_ADJUSTMENT (adjustment));
      gtk_widget_set_size_request (scale, -1, 200);
      gtk_scale_set_digits (GTK_SCALE (scale), 2);
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_range_set_inverted (GTK_RANGE (scale), TRUE);
      gtk_box_pack_start (GTK_BOX (hbox), scale, TRUE, TRUE, 0);
      gtk_widget_show (scale);

      scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, GTK_ADJUSTMENT (adjustment));
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      g_signal_connect (scale,
                        "format_value",
                        G_CALLBACK (reformat_value),
                        NULL);
      gtk_box_pack_start (GTK_BOX (hbox), scale, TRUE, TRUE, 0);
      gtk_widget_show (scale);

      
      gtk_box_pack_start (GTK_BOX (box2), hbox, TRUE, TRUE, 0);
      gtk_widget_show (hbox);
      
      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
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
 * GtkNotebook
 */

static const char * book_open_xpm[] = {
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

static const char * book_closed_xpm[] = {
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
  gtk_container_child_set (GTK_CONTAINER (sample_notebook), child,
                           "tab-fill", gtk_toggle_button_get_active (button),
                           NULL);
}

static void
tab_expand (GtkToggleButton *button, GtkWidget *child)
{
  gtk_container_child_set (GTK_CONTAINER (sample_notebook), child,
                           "tab-expand", gtk_toggle_button_get_active (button),
                           NULL);
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

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_set_homogeneous (GTK_BOX (vbox), TRUE);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
      gtk_container_add (GTK_CONTAINER (child), vbox);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);
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

      button = gtk_button_new_with_label ("Hide Page");
      gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 5);
      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_hide),
				child);

      gtk_widget_show_all (child);

      label_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      pixwid = gtk_image_new_from_pixbuf (book_closed);
      g_object_set_data (G_OBJECT (child), "tab_pixmap", pixwid);
			   
      gtk_box_pack_start (GTK_BOX (label_box), pixwid, FALSE, TRUE, 0);
      gtk_widget_set_margin_left (pixwid, 3);
      gtk_widget_set_margin_right (pixwid, 3);
      gtk_widget_set_margin_bottom (pixwid, 1);
      gtk_widget_set_margin_top (pixwid, 1);
      label = gtk_label_new_with_mnemonic (accel_buffer);
      gtk_box_pack_start (GTK_BOX (label_box), label, FALSE, TRUE, 0);
      gtk_widget_show_all (label_box);
      
				       
      menu_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      pixwid = gtk_image_new_from_pixbuf (book_closed);
      g_object_set_data (G_OBJECT (child), "menu_pixmap", pixwid);
      
      gtk_box_pack_start (GTK_BOX (menu_box), pixwid, FALSE, TRUE, 0);
      gtk_widget_set_margin_left (pixwid, 3);
      gtk_widget_set_margin_right (pixwid, 3);
      gtk_widget_set_margin_bottom (pixwid, 1);
      gtk_widget_set_margin_top (pixwid, 1);
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
  gtk_notebook_set_tab_pos (notebook, (gtk_notebook_get_tab_pos (notebook) + 1) % 4);
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

  c = gtk_combo_box_get_active (GTK_COMBO_BOX (optionmenu));

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
      if (gtk_notebook_get_n_pages (notebook) == 5)
	create_pages (notebook, 6, 15);

      return;
      break;
    }

  if (gtk_notebook_get_n_pages (notebook) == 15)
    for (i = 0; i < 10; i++)
      gtk_notebook_remove_page (notebook, 5);
}

static void
notebook_popup (GtkToggleButton *button,
		GtkNotebook     *notebook)
{
  if (gtk_toggle_button_get_active (button))
    gtk_notebook_popup_enable (notebook);
  else
    gtk_notebook_popup_disable (notebook);
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

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      sample_notebook = gtk_notebook_new ();
      g_signal_connect (sample_notebook, "switch_page",
			G_CALLBACK (page_switch), NULL);
      gtk_notebook_set_tab_pos (GTK_NOTEBOOK (sample_notebook), GTK_POS_TOP);
      gtk_box_pack_start (GTK_BOX (box1), sample_notebook, TRUE, TRUE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (sample_notebook), 10);

      gtk_widget_realize (sample_notebook);

      if (!book_open)
	book_open = gdk_pixbuf_new_from_xpm_data (book_open_xpm);
						  
      if (!book_closed)
	book_closed = gdk_pixbuf_new_from_xpm_data (book_closed_xpm);

      create_pages (GTK_NOTEBOOK (sample_notebook), 1, 5);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 10);
      
      box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_check_button_new_with_label ("popup menu");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, FALSE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (notebook_popup),
			sample_notebook);

      box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
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

      box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_box_set_homogeneous (GTK_BOX (box2), TRUE);
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

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
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

void
create_settings (GtkWidget *widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      window = create_prop_editor (G_OBJECT (gtk_settings_get_default ()), GTK_TYPE_SETTINGS);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (widget));

      gtk_widget_hide (window);
      gtk_window_set_title (GTK_WINDOW (window), "GTK+ Settings");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &window);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
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
  GValue value = G_VALUE_INIT;
  g_value_init (&value, G_TYPE_BOOLEAN);
  gtk_container_child_get_property (container, child, "resize", &value);
  g_value_set_boolean (&value, !g_value_get_boolean (&value));
  gtk_container_child_set_property (container, child, "resize", &value);
}

void
toggle_shrink (GtkWidget *widget, GtkWidget *child)
{
  GtkContainer *container = GTK_CONTAINER (gtk_widget_get_parent (child));
  GValue value = G_VALUE_INIT;
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
  GtkWidget *child1, *child2;
  GtkWidget *frame;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *check_button;

  child1 = gtk_paned_get_child1 (paned);
  child2 = gtk_paned_get_child2 (paned);

  frame = gtk_frame_new (frame_label);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  
  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (frame), grid);
  
  label = gtk_label_new (label1);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  
  check_button = gtk_check_button_new_with_label ("Resize");
  gtk_grid_attach (GTK_GRID (grid), check_button, 0, 1, 1, 1);
  g_signal_connect (check_button, "toggled",
		    G_CALLBACK (toggle_resize),
                    child1);

  check_button = gtk_check_button_new_with_label ("Shrink");
  gtk_grid_attach (GTK_GRID (grid), check_button, 0, 2, 1, 1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
			       TRUE);
  g_signal_connect (check_button, "toggled",
		    G_CALLBACK (toggle_shrink),
                    child1);

  label = gtk_label_new (label2);
  gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);
  
  check_button = gtk_check_button_new_with_label ("Resize");
  gtk_grid_attach (GTK_GRID (grid), check_button, 1, 1, 1, 1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
			       TRUE);
  g_signal_connect (check_button, "toggled",
		    G_CALLBACK (toggle_resize),
                    child2);

  check_button = gtk_check_button_new_with_label ("Shrink");
  gtk_grid_attach (GTK_GRID (grid), check_button, 1, 2, 1, 1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
			       TRUE);
  g_signal_connect (check_button, "toggled",
		    G_CALLBACK (toggle_shrink),
                    child2);

  button = gtk_button_new_with_mnemonic ("_Properties");
  gtk_grid_attach (GTK_GRID (grid), button, 0, 3, 2, 1);
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

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);
      
      vpaned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
      gtk_box_pack_start (GTK_BOX (vbox), vpaned, TRUE, TRUE, 0);
      gtk_container_set_border_width (GTK_CONTAINER(vpaned), 5);

      hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
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
  GtkWidget *grid1;
  GtkWidget *button1;
  GtkWidget *button2;
  GtkWidget *button3;
  GtkWidget *button4;

  window1 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window1), "Basic paned navigation");
  gtk_window_set_screen (GTK_WINDOW (window1), 
			 gtk_widget_get_screen (widget));

  hpaned1 = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_container_add (GTK_CONTAINER (window1), hpaned1);

  frame1 = gtk_frame_new (NULL);
  gtk_paned_pack1 (GTK_PANED (hpaned1), frame1, FALSE, TRUE);
  gtk_frame_set_shadow_type (GTK_FRAME (frame1), GTK_SHADOW_IN);

  vbox1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (frame1), vbox1);

  button7 = gtk_button_new_with_label ("button7");
  gtk_box_pack_start (GTK_BOX (vbox1), button7, FALSE, FALSE, 0);

  button8 = gtk_button_new_with_label ("button8");
  gtk_box_pack_start (GTK_BOX (vbox1), button8, FALSE, FALSE, 0);

  button9 = gtk_button_new_with_label ("button9");
  gtk_box_pack_start (GTK_BOX (vbox1), button9, FALSE, FALSE, 0);

  vpaned1 = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_paned_pack2 (GTK_PANED (hpaned1), vpaned1, TRUE, TRUE);

  frame2 = gtk_frame_new (NULL);
  gtk_paned_pack1 (GTK_PANED (vpaned1), frame2, FALSE, TRUE);
  gtk_frame_set_shadow_type (GTK_FRAME (frame2), GTK_SHADOW_IN);

  frame5 = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (frame2), frame5);

  hbox1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
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

  grid1 = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (frame4), grid1);
  gtk_container_set_border_width (GTK_CONTAINER (grid1), 11);

  button1 = gtk_button_new_with_label ("button1");
  gtk_grid_attach (GTK_GRID (grid1), button1, 0, 0, 1, 1);

  button2 = gtk_button_new_with_label ("button2");
  gtk_grid_attach (GTK_GRID (grid1), button2, 1, 0, 1, 1);

  button3 = gtk_button_new_with_label ("button3");
  gtk_grid_attach (GTK_GRID (grid1), button3, 0, 1, 1, 1);

  button4 = gtk_button_new_with_label ("button4");
  gtk_grid_attach (GTK_GRID (grid1), button4, 1, 1, 1, 1);

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

  hpaned2 = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_container_add (GTK_CONTAINER (window2), hpaned2);

  frame6 = gtk_frame_new (NULL);
  gtk_paned_pack1 (GTK_PANED (hpaned2), frame6, FALSE, TRUE);
  gtk_frame_set_shadow_type (GTK_FRAME (frame6), GTK_SHADOW_IN);

  button13 = gtk_button_new_with_label ("button13");
  gtk_container_add (GTK_CONTAINER (frame6), button13);

  hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_paned_pack2 (GTK_PANED (hpaned2), hbox2, TRUE, TRUE);

  vpaned2 = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
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
  

  vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window3), vbox2);

  label1 = gtk_label_new ("Three panes nested inside each other");
  gtk_box_pack_start (GTK_BOX (vbox2), label1, FALSE, FALSE, 0);

  hpaned3 = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start (GTK_BOX (vbox2), hpaned3, TRUE, TRUE, 0);

  frame9 = gtk_frame_new (NULL);
  gtk_paned_pack1 (GTK_PANED (hpaned3), frame9, FALSE, TRUE);
  gtk_frame_set_shadow_type (GTK_FRAME (frame9), GTK_SHADOW_IN);

  button14 = gtk_button_new_with_label ("button14");
  gtk_container_add (GTK_CONTAINER (frame9), button14);

  hpaned4 = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_pack2 (GTK_PANED (hpaned3), hpaned4, TRUE, TRUE);

  frame10 = gtk_frame_new (NULL);
  gtk_paned_pack1 (GTK_PANED (hpaned4), frame10, FALSE, TRUE);
  gtk_frame_set_shadow_type (GTK_FRAME (frame10), GTK_SHADOW_IN);

  button15 = gtk_button_new_with_label ("button15");
  gtk_container_add (GTK_CONTAINER (frame10), button15);

  hpaned5 = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
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

  vbox3 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window4), vbox3);

  label2 = gtk_label_new ("Widget tree:\n\nhpaned \n - vpaned\n - hbox\n    - vpaned\n    - vpaned\n    - vpaned\n");
  gtk_box_pack_start (GTK_BOX (vbox3), label2, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label2), GTK_JUSTIFY_LEFT);

  hpaned6 = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start (GTK_BOX (vbox3), hpaned6, TRUE, TRUE, 0);

  vpaned3 = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_paned_pack1 (GTK_PANED (hpaned6), vpaned3, FALSE, TRUE);

  button19 = gtk_button_new_with_label ("button19");
  gtk_paned_pack1 (GTK_PANED (vpaned3), button19, FALSE, TRUE);

  button18 = gtk_button_new_with_label ("button18");
  gtk_paned_pack2 (GTK_PANED (vpaned3), button18, TRUE, TRUE);

  hbox3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_paned_pack2 (GTK_PANED (hpaned6), hbox3, TRUE, TRUE);

  vpaned4 = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_pack_start (GTK_BOX (hbox3), vpaned4, TRUE, TRUE, 0);

  button21 = gtk_button_new_with_label ("button21");
  gtk_paned_pack1 (GTK_PANED (vpaned4), button21, FALSE, TRUE);

  button20 = gtk_button_new_with_label ("button20");
  gtk_paned_pack2 (GTK_PANED (vpaned4), button20, TRUE, TRUE);

  vpaned5 = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_pack_start (GTK_BOX (hbox3), vpaned5, TRUE, TRUE, 0);

  button23 = gtk_button_new_with_label ("button23");
  gtk_paned_pack1 (GTK_PANED (vpaned5), button23, FALSE, TRUE);

  button22 = gtk_button_new_with_label ("button22");
  gtk_paned_pack2 (GTK_PANED (vpaned5), button22, TRUE, TRUE);

  vpaned6 = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
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
  gdk_device_grab (gdk_event_get_device ((GdkEvent*)event),
                   gtk_widget_get_window (widget),
                   GDK_OWNERSHIP_NONE,
                   TRUE,
                   GDK_BUTTON_RELEASE_MASK |
                   GDK_BUTTON_MOTION_MASK |
                   GDK_POINTER_MOTION_HINT_MASK,
                   NULL,
                   event->time);
}

static void
shape_released (GtkWidget      *widget,
                GdkEventButton *event)
{
  gtk_grab_remove (widget);
  gdk_device_ungrab (gdk_event_get_device ((GdkEvent*)event), event->time);
}

static void
shape_motion (GtkWidget      *widget,
	      GdkEventMotion *event)
{
  gint xp, yp;
  CursorOffset * p;

  p = g_object_get_data (G_OBJECT (widget), "cursor_offset");

  /*
   * Can't use event->x / event->y here 
   * because I need absolute coordinates.
   */
  gdk_window_get_device_position (gdk_screen_get_root_window (gtk_widget_get_screen (widget)),
                                  gdk_event_get_device ((GdkEvent *) event),
                                  &xp, &yp, NULL);
  gtk_window_move (GTK_WINDOW (widget), xp  - p->x, yp  - p->y);
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
  GtkWidget *image;
  GtkWidget *fixed;
  CursorOffset* icon_pos;
  cairo_surface_t *mask;
  cairo_region_t *mask_region;
  GdkPixbuf *pixbuf;
  cairo_t *cr;

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

  pixbuf = gdk_pixbuf_new_from_file (xpm_file, NULL);
  g_assert (pixbuf); /* FIXME: error handling */

  mask = cairo_image_surface_create (CAIRO_FORMAT_A1,
                                     gdk_pixbuf_get_width (pixbuf),
                                     gdk_pixbuf_get_height (pixbuf));
  cr = cairo_create (mask);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  mask_region = gdk_cairo_region_create_from_surface (mask);
                                                  
  cairo_region_translate (mask_region, px, py);

  image = gtk_image_new_from_pixbuf (pixbuf);
  gtk_fixed_put (GTK_FIXED (fixed), image, px,py);
  gtk_widget_show (image);
  
  gtk_widget_shape_combine_region (window, mask_region);
  
  cairo_region_destroy (mask_region);
  cairo_surface_destroy (mask);
  g_object_unref (pixbuf);

  g_signal_connect (window, "button_press_event",
		    G_CALLBACK (shape_pressed), NULL);
  g_signal_connect (window, "button_release_event",
		    G_CALLBACK (shape_released), NULL);
  g_signal_connect (window, "motion_notify_event",
		    G_CALLBACK (shape_motion), NULL);

  icon_pos = g_new (CursorOffset, 1);
  g_object_set_data (G_OBJECT (window), "cursor_offset", icon_pos);

  gtk_window_move (GTK_WINDOW (window), x, y);
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
      cairo_region_t *region;
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
      region = cairo_region_create ();

      while (x < 460)
        {
          while (y < 270)
            {
              GdkRectangle rect;
              rect.x = x;
              rect.y = y;
              rect.width = 10;
              rect.height = 10;

              cairo_region_union_rectangle (region, &rect);
              
              y += 20;
            }
          y = 0;
          x += 20;
        }

      gdk_window_shape_combine_region (gtk_widget_get_window (with_region),
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
  GdkWindow *gdk_window;
  GdkPixbuf *pixbuf;
  GList *list;

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

      gdk_window = gtk_widget_get_window (window);

      pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) openfile);
      list = g_list_prepend (NULL, pixbuf);

      gdk_window_set_icon_list (gdk_window, list);
      
      g_list_free (list);
      g_object_unref (pixbuf);

      gdk_window_set_icon_name (gdk_window, "WMHints Test Icon");
  
      gdk_window_set_decorations (gdk_window, GDK_DECOR_ALL | GDK_DECOR_MENU);
      gdk_window_set_functions (gdk_window, GDK_FUNC_ALL | GDK_FUNC_RESIZE);
      
      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);

      label = gtk_label_new ("Try iconizing me!");
      gtk_widget_set_size_request (label, 150, 50);
      gtk_box_pack_start (GTK_BOX (box1), label, TRUE, TRUE, 0);
      gtk_widget_show (label);


      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
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

  msg = g_strconcat (gtk_window_get_title (GTK_WINDOW (widget)), ": ",
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

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);

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

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  
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
      
      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
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
resizable_callback (GtkWidget *widget,
                     gpointer   data)
{
  g_object_set (g_object_get_data (data, "target"),
                "resizable", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)),
                NULL);
}

static void
gravity_selected (GtkWidget *widget,
                  gpointer   data)
{
  gtk_window_set_gravity (GTK_WINDOW (g_object_get_data (data, "target")),
                          gtk_combo_box_get_active (GTK_COMBO_BOX (widget)) + GDK_GRAVITY_NORTH_WEST);
}

static void
pos_selected (GtkWidget *widget,
              gpointer   data)
{
  gtk_window_set_position (GTK_WINDOW (g_object_get_data (data, "target")),
                           gtk_combo_box_get_active (GTK_COMBO_BOX (widget)) + GTK_WIN_POS_NONE);
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

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
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
  GtkAdjustment *adjustment;
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

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  
  gtk_container_add (GTK_CONTAINER (control_window), vbox);
  
  label = gtk_label_new ("<no configure events>");
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  
  g_signal_connect (window,
		    "configure_event",
		    G_CALLBACK (configure_event_callback),
		    label);

  adjustment = gtk_adjustment_new (10.0, -2000.0, 2000.0, 1.0, 5.0, 0.0);
  spin = gtk_spin_button_new (adjustment, 0, 0);

  gtk_box_pack_start (GTK_BOX (vbox), spin, FALSE, FALSE, 0);

  g_object_set_data (G_OBJECT (control_window), "spin1", spin);

  adjustment = gtk_adjustment_new (10.0, -2000.0, 2000.0, 1.0, 5.0, 0.0);
  spin = gtk_spin_button_new (adjustment, 0, 0);

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

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  button = gtk_button_new_with_label ("Reshow with initial size");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_reshow_with_initial_size),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  G_GNUC_END_IGNORE_DEPRECATIONS;
  
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

  button = gtk_check_button_new_with_label ("Allow resize");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  g_signal_connect (button,
		    "toggled",
		    G_CALLBACK (resizable_callback),
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
  gboolean activity;
} ProgressData;

gboolean
progress_timeout (gpointer data)
{
  ProgressData *pdata = data;
  gdouble new_val;
  gchar *text;

  if (pdata->activity)
    {
      gtk_progress_bar_pulse (GTK_PROGRESS_BAR (pdata->pbar));

      text = g_strdup_printf ("%s", "???");
    }
  else
    {
      new_val = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (pdata->pbar)) + 0.01;
      if (new_val > 1.00)
        new_val = 0.00;
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (pdata->pbar), new_val);

      text = g_strdup_printf ("%.0f%%", 100 * new_val);
    }

  gtk_label_set_text (GTK_LABEL (pdata->label), text);
  g_free (text);

  return TRUE;
}

static void
destroy_progress (GtkWidget     *widget,
		  ProgressData **pdata)
{
  if ((*pdata)->timer)
    {
      g_source_remove ((*pdata)->timer);
      (*pdata)->timer = 0;
    }
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

  i = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

  if (i == 0 || i == 1)
    gtk_orientable_set_orientation (GTK_ORIENTABLE (pdata->pbar), GTK_ORIENTATION_HORIZONTAL);
  else
    gtk_orientable_set_orientation (GTK_ORIENTABLE (pdata->pbar), GTK_ORIENTATION_VERTICAL);
 
  if (i == 1 || i == 2)
    gtk_progress_bar_set_inverted (GTK_PROGRESS_BAR (pdata->pbar), TRUE);
  else
    gtk_progress_bar_set_inverted (GTK_PROGRESS_BAR (pdata->pbar), FALSE);
}

static void
toggle_show_text (GtkWidget *widget, ProgressData *pdata)
{
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (pdata->pbar), active);
}

static void
progressbar_toggle_ellipsize (GtkWidget *widget,
                              gpointer   data)
{
  ProgressData *pdata = data;
  if (gtk_widget_is_drawable (widget))
    {
      gint i = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
      gtk_progress_bar_set_ellipsize (GTK_PROGRESS_BAR (pdata->pbar), i);
    }
}

static void
toggle_activity_mode (GtkWidget *widget, ProgressData *pdata)
{
  pdata->activity = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void
toggle_running (GtkWidget *widget, ProgressData *pdata)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    {
      if (pdata->timer == 0)
        pdata->timer = g_timeout_add (100, (GSourceFunc)progress_timeout, pdata);
    }
  else
    {
      if (pdata->timer != 0)
        {
          g_source_remove (pdata->timer);
          pdata->timer = 0;
        }
    }
}

static void
entry_changed (GtkWidget *widget, ProgressData *pdata)
{
  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (pdata->pbar),
			  gtk_entry_get_text (GTK_ENTRY (pdata->entry)));
}

void
create_progress_bar (GtkWidget *widget)
{
  GtkWidget *action_area, *content_area;
  GtkWidget *button;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *hbox;
  GtkWidget *check;
  GtkWidget *frame;
  GtkWidget *grid;
  GtkWidget *label;
  static ProgressData *pdata = NULL;

  static gchar *items1[] =
  {
    "Left-Right",
    "Right-Left",
    "Bottom-Top",
    "Top-Bottom"
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

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (pdata->window));
      action_area = gtk_dialog_get_action_area (GTK_DIALOG (pdata->window));

      gtk_window_set_title (GTK_WINDOW (pdata->window), "GtkProgressBar");
      gtk_container_set_border_width (GTK_CONTAINER (pdata->window), 0);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
      gtk_box_pack_start (GTK_BOX (content_area), vbox, FALSE, TRUE, 0);

      frame = gtk_frame_new ("Progress");
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);

      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_container_add (GTK_CONTAINER (frame), vbox2);

      pdata->pbar = gtk_progress_bar_new ();
      gtk_progress_bar_set_ellipsize (GTK_PROGRESS_BAR (pdata->pbar),
                                      PANGO_ELLIPSIZE_MIDDLE);
      gtk_widget_set_halign (pdata->pbar, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (pdata->pbar, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox2), pdata->pbar, FALSE, FALSE, 5);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
      gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (hbox, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 5);
      label = gtk_label_new ("Label updated by user :"); 
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      pdata->label = gtk_label_new ("");
      gtk_box_pack_start (GTK_BOX (hbox), pdata->label, FALSE, TRUE, 0);

      frame = gtk_frame_new ("Options");
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);

      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_container_add (GTK_CONTAINER (frame), vbox2);

      grid = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
      gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
      gtk_box_pack_start (GTK_BOX (vbox2), grid, FALSE, TRUE, 0);

      label = gtk_label_new ("Orientation :");
      gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

      pdata->omenu1 = build_option_menu (items1, 4, 0,
					 progressbar_toggle_orientation,
					 pdata);
      gtk_grid_attach (GTK_GRID (grid), pdata->omenu1, 1, 0, 1, 1);
      
      check = gtk_check_button_new_with_label ("Running");
      g_signal_connect (check, "toggled",
			G_CALLBACK (toggle_running),
			pdata);
      gtk_grid_attach (GTK_GRID (grid), check, 0, 1, 2, 1);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);

      check = gtk_check_button_new_with_label ("Show text");
      g_signal_connect (check, "clicked",
			G_CALLBACK (toggle_show_text),
			pdata);
      gtk_grid_attach (GTK_GRID (grid), check, 0, 2, 1, 1);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_grid_attach (GTK_GRID (grid), hbox, 1, 2, 1, 1);

      label = gtk_label_new ("Text: ");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

      pdata->entry = gtk_entry_new ();
      gtk_widget_set_hexpand (pdata->entry, TRUE);
      g_signal_connect (pdata->entry, "changed",
			G_CALLBACK (entry_changed),
			pdata);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->entry, TRUE, TRUE, 0);
      gtk_widget_set_size_request (pdata->entry, 100, -1);

      label = gtk_label_new ("Ellipsize text :");
      gtk_grid_attach (GTK_GRID (grid), label, 0, 10, 1, 1);

      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      pdata->elmenu = build_option_menu (ellipsize_items,
                                         sizeof (ellipsize_items) / sizeof (ellipsize_items[0]),
                                         2, // PANGO_ELLIPSIZE_MIDDLE
					 progressbar_toggle_ellipsize,
					 pdata);
      gtk_grid_attach (GTK_GRID (grid), pdata->elmenu, 1, 10, 1, 1);

      check = gtk_check_button_new_with_label ("Activity mode");
      g_signal_connect (check, "clicked",
			G_CALLBACK (toggle_activity_mode), pdata);
      gtk_grid_attach (GTK_GRID (grid), check, 0, 15, 1, 1);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_destroy),
				pdata->window);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);
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

  gtk_widget_get_allocation (widget, &new_allocation);

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

  if (gtk_widget_get_parent (widget) && !data->first)
    {
      GdkWindow *window = gtk_widget_get_window (widget);
      while (window != gtk_widget_get_window (gtk_widget_get_parent (widget)))
	{
	  gint tx, ty, twidth, theight;
	  
          twidth = gdk_window_get_width (window);
          theight = gdk_window_get_height (window);

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
find_widget_at_pointer (GdkDevice *device)
{
  GtkWidget *widget = NULL;
  GdkWindow *pointer_window;
  gint x, y;
  FindWidgetData data;
 
 pointer_window = gdk_device_get_window_at_position (device, NULL, NULL);
 
 if (pointer_window)
   {
     gpointer widget_ptr;

     gdk_window_get_user_data (pointer_window, &widget_ptr);
     widget = widget_ptr;
   }

 if (widget)
   {
     gdk_window_get_device_position (gtk_widget_get_window (widget),
                                     device,
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
  gulong handler;
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
      g_object_unref (data->cursor);
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
property_query_event (GtkWidget             *widget,
                      GdkEvent              *event,
                      struct PropertiesData *data)
{
  GtkWidget *res_widget = NULL;

  if (!data->in_query)
    return FALSE;

  if (event->type == GDK_BUTTON_RELEASE)
    {
      gtk_grab_remove (widget);
      gdk_device_ungrab (gdk_event_get_device (event), GDK_CURRENT_TIME);

      res_widget = find_widget_at_pointer (gdk_event_get_device (event));
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
  GtkWidget *widget = GTK_WIDGET (button);
  GdkDisplay *display;
  GdkDeviceManager *device_manager;
  GdkDevice *device;

  g_signal_connect (button, "event",
		    G_CALLBACK (property_query_event), data);

  display = gtk_widget_get_display (widget);

  if (!data->cursor)
    data->cursor = gdk_cursor_new_for_display (display, GDK_TARGET);

  device_manager = gdk_display_get_device_manager (display);
  device = gdk_device_manager_get_client_pointer (device_manager);
  gdk_device_grab (device,
                   gtk_widget_get_window (widget),
                   GDK_OWNERSHIP_NONE,
                   TRUE,
                   GDK_BUTTON_RELEASE_MASK,
                   data->cursor,
                   GDK_CURRENT_TIME);
  gtk_grab_add (widget);

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

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
      gtk_container_add (GTK_CONTAINER (window), vbox);
            
      label = gtk_label_new ("This is just a dumb test to test properties.\nIf you need a generic module, get gtkparasite.");
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
      g_object_unref (data->cursor);
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
      gdk_device_ungrab (gdk_event_get_device (event),
			 GDK_CURRENT_TIME);
      
      res_widget = find_widget_at_pointer (gdk_event_get_device (event));
      if (data->is_toplevel && res_widget)
	res_widget = gtk_widget_get_toplevel (res_widget);
      if (res_widget)
	{
	  cairo_surface_t *surface;
	  GtkWidget *window, *image;
          GdkPixbuf *pixbuf;
          int width, height;
          cairo_t *cr;

          width = gtk_widget_get_allocated_width (res_widget);
          height = gtk_widget_get_allocated_height (res_widget);

          surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

          cr = cairo_create (surface);
          gtk_widget_draw (res_widget, cr);
          cairo_destroy (cr);

          pixbuf = gdk_pixbuf_get_from_surface (surface,
                                                0, 0,
                                                width, height);
          cairo_surface_destroy (surface);

	  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
          image = gtk_image_new_from_pixbuf (pixbuf);
          g_object_unref (pixbuf);

	  gtk_container_add (GTK_CONTAINER (window), image);
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
  GtkWidget *widget = GTK_WIDGET (button);
  GdkDevice *device;

  device = gtk_get_current_event_device ();
  if (device == NULL)
    return;

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    device = gdk_device_get_associated_device (device);

  data->is_toplevel = widget == data->toplevel_button;

  if (!data->cursor)
    data->cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget),
					       GDK_TARGET);

  gdk_device_grab (device,
                   gtk_widget_get_window (widget),
                   GDK_OWNERSHIP_APPLICATION,
		   TRUE,
		   GDK_BUTTON_RELEASE_MASK,
		   data->cursor,
		   GDK_CURRENT_TIME);

  g_signal_connect (button, "event",
		    G_CALLBACK (snapshot_widget_event), data);

  gtk_grab_add (widget);

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

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
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
 * Selection Test
 */

void
selection_test_received (GtkWidget        *tree_view,
                         GtkSelectionData *selection_data)
{
  GtkTreeModel *model;
  GtkListStore *store;
  GdkAtom *atoms;
  int i, l;

  if (gtk_selection_data_get_length (selection_data) < 0)
    {
      g_print ("Selection retrieval failed\n");
      return;
    }
  if (gtk_selection_data_get_data_type (selection_data) != GDK_SELECTION_TYPE_ATOM)
    {
      g_print ("Selection \"TARGETS\" was not returned as atoms!\n");
      return;
    }

  /* Clear out any current list items */

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
  store = GTK_LIST_STORE (model);
  gtk_list_store_clear (store);

  /* Add new items to list */

  gtk_selection_data_get_targets (selection_data,
                                  &atoms, &l);

  for (i = 0; i < l; i++)
    {
      char *name;
      GtkTreeIter iter;

      name = gdk_atom_name (atoms[i]);
      if (name != NULL)
        {
          gtk_list_store_insert_with_values (store, &iter, i, 0, name, -1);
          g_free (name);
        }
      else
       gtk_list_store_insert_with_values (store, &iter, i, 0,  "(bad atom)", -1);
    }

  return;
}

void
selection_test_get_targets (GtkWidget *widget, GtkWidget *tree_view)
{
  static GdkAtom targets_atom = GDK_NONE;

  if (targets_atom == GDK_NONE)
    targets_atom = gdk_atom_intern ("TARGETS", FALSE);

  gtk_selection_convert (tree_view, GDK_SELECTION_PRIMARY, targets_atom,
			 GDK_CURRENT_TIME);
}

void
create_selection_test (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *action_area, *content_area;
  GtkWidget *button;
  GtkWidget *vbox;
  GtkWidget *scrolled_win;
  GtkListStore* store;
  GtkWidget *tree_view;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkWidget *label;

  if (!window)
    {
      window = gtk_dialog_new ();
      
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&window);

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));
      action_area = gtk_dialog_get_action_area (GTK_DIALOG (window));

      gtk_window_set_title (GTK_WINDOW (window), "Selection Test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      /* Create the list */

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
      gtk_box_pack_start (GTK_BOX (content_area), vbox, TRUE, TRUE, 0);

      label = gtk_label_new ("Gets available targets for current selection");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      scrolled_win = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				      GTK_POLICY_AUTOMATIC, 
				      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (vbox), scrolled_win, TRUE, TRUE, 0);
      gtk_widget_set_size_request (scrolled_win, 100, 200);

      store = gtk_list_store_new (1, G_TYPE_STRING);
      tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
      gtk_container_add (GTK_CONTAINER (scrolled_win), tree_view);

      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("Target", renderer,
                                                         "text", 0, NULL);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

      g_signal_connect (tree_view, "selection_received",
			G_CALLBACK (selection_test_received), NULL);

      /* .. And create some buttons */
      button = gtk_button_new_with_label ("Get Targets");
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);

      g_signal_connect (button, "clicked",
			G_CALLBACK (selection_test_get_targets), tree_view);

      button = gtk_button_new_with_label ("Quit");
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);

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
 * Test scrolling
 */

static int scroll_test_pos = 0.0;

static gint
scroll_test_draw (GtkWidget     *widget,
                  cairo_t       *cr,
                  GtkAdjustment *adjustment)
{
  gint i,j;
  gint imin, imax, jmin, jmax;
  GdkRectangle clip;
  
  gdk_cairo_get_clip_rectangle (cr, &clip);

  imin = (clip.x) / 10;
  imax = (clip.x + clip.width + 9) / 10;

  jmin = ((int)gtk_adjustment_get_value (adjustment) + clip.y) / 10;
  jmax = ((int)gtk_adjustment_get_value (adjustment) + clip.y + clip.height + 9) / 10;

  for (i=imin; i<imax; i++)
    for (j=jmin; j<jmax; j++)
      if ((i+j) % 2)
	cairo_rectangle (cr, 10*i, 10*j - (int)gtk_adjustment_get_value (adjustment), 1+i%10, 1+j%10);

  cairo_fill (cr);

  return TRUE;
}

static gint
scroll_test_scroll (GtkWidget *widget, GdkEventScroll *event,
		    GtkAdjustment *adjustment)
{
  gdouble new_value = gtk_adjustment_get_value (adjustment) + ((event->direction == GDK_SCROLL_UP) ?
				    -gtk_adjustment_get_page_increment (adjustment) / 2:
				    gtk_adjustment_get_page_increment (adjustment) / 2);
  new_value = CLAMP (new_value, gtk_adjustment_get_lower (adjustment), gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_page_size (adjustment));
  gtk_adjustment_set_value (adjustment, new_value);  
  
  return TRUE;
}

static void
scroll_test_configure (GtkWidget *widget, GdkEventConfigure *event,
		       GtkAdjustment *adjustment)
{
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);
  gtk_adjustment_configure (adjustment,
                            gtk_adjustment_get_value (adjustment),
                            gtk_adjustment_get_lower (adjustment),
                            gtk_adjustment_get_upper (adjustment),
                            0.1 * allocation.height,
                            0.9 * allocation.height,
                            allocation.height);
}

static void
scroll_test_adjustment_changed (GtkAdjustment *adjustment, GtkWidget *widget)
{
  GdkWindow *window;
  gint dy;

  dy = scroll_test_pos - (int)gtk_adjustment_get_value (adjustment);
  scroll_test_pos = gtk_adjustment_get_value (adjustment);

  if (!gtk_widget_is_drawable (widget))
    return;

  window = gtk_widget_get_window (widget);
  gdk_window_scroll (window, 0, dy);
  gdk_window_process_updates (window, FALSE);
}


void
create_scroll_test (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *action_area, *content_area;
  GtkWidget *hbox;
  GtkWidget *drawing_area;
  GtkWidget *scrollbar;
  GtkWidget *button;
  GtkAdjustment *adjustment;
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

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));
      action_area = gtk_dialog_get_action_area (GTK_DIALOG (window));

      gtk_window_set_title (GTK_WINDOW (window), "Scroll Test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start (GTK_BOX (content_area), hbox, TRUE, TRUE, 0);
      gtk_widget_show (hbox);

      drawing_area = gtk_drawing_area_new ();
      gtk_widget_set_size_request (drawing_area, 200, 200);
      gtk_box_pack_start (GTK_BOX (hbox), drawing_area, TRUE, TRUE, 0);
      gtk_widget_show (drawing_area);

      gtk_widget_set_events (drawing_area, GDK_EXPOSURE_MASK | GDK_SCROLL_MASK);

      adjustment = gtk_adjustment_new (0.0, 0.0, 1000.0, 1.0, 180.0, 200.0);
      scroll_test_pos = 0.0;

      scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, adjustment);
      gtk_box_pack_start (GTK_BOX (hbox), scrollbar, FALSE, FALSE, 0);
      gtk_widget_show (scrollbar);

      g_signal_connect (drawing_area, "draw",
			G_CALLBACK (scroll_test_draw), adjustment);
      g_signal_connect (drawing_area, "configure_event",
			G_CALLBACK (scroll_test_configure), adjustment);
      g_signal_connect (drawing_area, "scroll_event",
			G_CALLBACK (scroll_test_scroll), adjustment);
      
      g_signal_connect (adjustment, "value_changed",
			G_CALLBACK (scroll_test_adjustment_changed),
			drawing_area);
      
      /* .. And create some buttons */

      button = gtk_button_new_with_label ("Quit");
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);

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
      timer = g_timeout_add (100, (GSourceFunc)timeout_test, label);
    }
}

void
stop_timeout_test (GtkWidget *widget,
		   gpointer   data)
{
  if (timer)
    {
      g_source_remove (timer);
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
  GtkWidget *action_area, *content_area;
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

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));
      action_area = gtk_dialog_get_content_area (GTK_DIALOG (window));

      gtk_window_set_title (GTK_WINDOW (window), "Timeout Test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      label = gtk_label_new ("count: 0");
      g_object_set (label, "margin", 10, NULL);
      gtk_box_pack_start (GTK_BOX (content_area), label, TRUE, TRUE, 0);
      gtk_widget_show (label);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("start");
      g_signal_connect (button, "clicked",
			G_CALLBACK(start_timeout_test),
			label);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("stop");
      g_signal_connect (button, "clicked",
			G_CALLBACK (stop_timeout_test),
			NULL);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);
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

  return G_SOURCE_CONTINUE;
}

static void
start_idle_test (GtkWidget *widget,
		 GtkWidget *label)
{
  if (!idle_id)
    {
      idle_id = g_idle_add ((GSourceFunc) idle_test, label);
    }
}

static void
stop_idle_test (GtkWidget *widget,
		gpointer   data)
{
  if (idle_id)
    {
      g_source_remove (idle_id);
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
      GtkWidget *action_area, *content_area;
      GtkWidget *button2;
      GtkWidget *frame;
      GtkWidget *box;

      window = gtk_dialog_new ();

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (destroy_idle_test),
			&window);

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));
      action_area = gtk_dialog_get_action_area (GTK_DIALOG (window));

      gtk_window_set_title (GTK_WINDOW (window), "Idle Test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      label = gtk_label_new ("count: 0");
      g_object_set (label, "margin", 10, NULL);
      gtk_widget_show (label);
      
      container =
	g_object_new (GTK_TYPE_BOX,
			"visible", TRUE,
			/* "GtkContainer::child", g_object_new (GTK_TYPE_HBOX,
			 * "GtkWidget::visible", TRUE,
			 */
			 "child", label,
			/* NULL), */
			NULL);
      gtk_box_pack_start (GTK_BOX (content_area), container, TRUE, TRUE, 0);

      frame =
	g_object_new (GTK_TYPE_FRAME,
			"border_width", 5,
			"label", "Label Container",
			"visible", TRUE,
			"parent", content_area,
			NULL);
      box =
	g_object_new (GTK_TYPE_BOX,
			"visible", TRUE,
			"parent", frame,
                        "orientation", GTK_ORIENTATION_VERTICAL,
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
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("start");
      g_signal_connect (button, "clicked",
			G_CALLBACK (start_idle_test),
			label);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("stop");
      g_signal_connect (button, "clicked",
			G_CALLBACK (stop_idle_test),
			NULL);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);
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
create_rc_file (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *action_area, *content_area;
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

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));
      action_area = gtk_dialog_get_action_area (GTK_DIALOG (window));

      frame = gtk_aspect_frame_new ("Testing RC file prioritization", 0.5, 0.5, 0.0, TRUE);
      gtk_box_pack_start (GTK_BOX (content_area), frame, FALSE, FALSE, 0);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
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
      g_signal_connect_swapped (button, "clicked",
                                G_CALLBACK (gtk_style_context_reset_widgets),
                                gtk_widget_get_screen (button));
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);

      button = gtk_button_new_with_label ("Close");
      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);
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
  GtkWidget *action_area, *content_area;
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

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));
      action_area = gtk_dialog_get_action_area (GTK_DIALOG (window));

      label = gtk_label_new ("In recursive main loop...");
      g_object_set (label, "margin", 20, NULL);

      gtk_box_pack_start (GTK_BOX (content_area), label, TRUE, TRUE, 0);
      gtk_widget_show (label);

      button = gtk_button_new_with_label ("Leave");
      gtk_box_pack_start (GTK_BOX (action_area), button, FALSE, TRUE, 0);

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

static gboolean
layout_draw_handler (GtkWidget *widget, cairo_t *cr)
{
  GtkLayout *layout;
  GdkWindow *bin_window;
  GdkRectangle clip;
  gint i,j,x,y;
  gint imin, imax, jmin, jmax;

  layout = GTK_LAYOUT (widget);
  bin_window = gtk_layout_get_bin_window (layout);

  if (!gtk_cairo_should_draw_window (cr, bin_window))
    return FALSE;
  
  gdk_window_get_position (bin_window, &x, &y);
  cairo_translate (cr, x, y);

  gdk_cairo_get_clip_rectangle (cr, &clip);

  imin = (clip.x) / 10;
  imax = (clip.x + clip.width + 9) / 10;

  jmin = (clip.y) / 10;
  jmax = (clip.y + clip.height + 9) / 10;

  for (i=imin; i<imax; i++)
    for (j=jmin; j<jmax; j++)
      if ((i+j) % 2)
	cairo_rectangle (cr,
			 10*i, 10*j, 
			 1+i%10, 1+j%10);
  
  cairo_fill (cr);

  return FALSE;
}

void create_layout (GtkWidget *widget)
{
  GtkAdjustment *hadjustment, *vadjustment;
  GtkLayout *layout;
  static GtkWidget *window = NULL;
  GtkWidget *layout_widget;
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

      layout_widget = gtk_layout_new (NULL, NULL);
      layout = GTK_LAYOUT (layout_widget);
      gtk_container_add (GTK_CONTAINER (scrolledwindow), layout_widget);

      /* We set step sizes here since GtkLayout does not set
       * them itself.
       */
      hadjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (layout));
      vadjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (layout));
      gtk_adjustment_set_step_increment (hadjustment, 10.0);
      gtk_adjustment_set_step_increment (vadjustment, 10.0);
      gtk_scrollable_set_hadjustment (GTK_SCROLLABLE (layout), hadjustment);
      gtk_scrollable_set_vadjustment (GTK_SCROLLABLE (layout), vadjustment);

      gtk_widget_set_events (layout_widget, GDK_EXPOSURE_MASK);
      g_signal_connect (layout, "draw",
			G_CALLBACK (layout_draw_handler), NULL);

      gtk_layout_set_size (layout, 1600, 128000);

      for (i=0 ; i < 16 ; i++)
	for (j=0 ; j < 16 ; j++)
	  {
	    sprintf(buf, "Button %d, %d", i, j);
	    if ((i + j) % 2)
	      button = gtk_button_new_with_label (buf);
	    else
	      button = gtk_label_new (buf);

	    gtk_layout_put (layout, button, j*100, i*100);
	  }

      for (i=16; i < 1280; i++)
	{
	  sprintf(buf, "Button %d, %d", i, 0);
	  if (i % 2)
	    button = gtk_button_new_with_label (buf);
	  else
	    button = gtk_label_new (buf);

	  gtk_layout_put (layout, button, 0, i*100);
	}
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

#if 0
/* FIXME: need to completely redo this for GtkStyleContext */
void
create_styles (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *content_area, *action_area;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *entry;
  GtkWidget *vbox;
  static GdkRGBA red =    { 1,0,0,1 };
  static GdkRGBA green =  { 0,1,0,1 };
  static GdkRGBA blue =   { 0,0,1,1 };
  static GdkRGBA yellow = { 1,1,0,1 };
  static GdkRGBA cyan =   { 0,1,1,1 };
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

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));
      action_area = gtk_dialog_get_action_area (GTK_DIALOG (window));

      button = gtk_button_new_with_label ("Close");
      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_destroy),
				window);
      gtk_widget_set_can_default (button, TRUE);
      gtk_box_pack_start (GTK_BOX (action_area), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
      gtk_box_pack_start (GTK_BOX (content_area), vbox, FALSE, FALSE, 0);
      
      label = gtk_label_new ("Font:");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      font_desc = pango_font_description_from_string ("Helvetica,Sans Oblique 18");

      button = gtk_button_new_with_label ("Some Text");
      gtk_widget_override_font (gtk_bin_get_child (GTK_BIN (button)), font_desc);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

      label = gtk_label_new ("Foreground:");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Some Text");
      gtk_widget_override_color (gtk_bin_get_child (GTK_BIN (button)), 0, &red);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

      label = gtk_label_new ("Background:");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Some Text");
      gtk_widget_override_background_color (button, 0, &green);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

      label = gtk_label_new ("Text:");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      entry = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (entry), "Some Text");
      gtk_widget_override_color (entry, 0, &blue);
      gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

      label = gtk_label_new ("Base:");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      entry = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (entry), "Some Text");
      gtk_widget_override_background_color (entry, 0, &yellow);
      gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

      label = gtk_label_new ("Cursor:");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      entry = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (entry), "Some Text");
      gtk_widget_modify_cursor (entry, &red, &red);
      gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

      label = gtk_label_new ("Multiple:");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
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
      gtk_widget_modify_style (gtk_bin_get_child (GTK_BIN (button)), rc_style);

      g_object_unref (rc_style);
      
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    }
  
  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}
#endif

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
  { "alpha widget", create_alpha_widgets },
  { "big windows", create_big_windows },
  { "button box", create_button_box },
  { "buttons", create_buttons },
  { "check buttons", create_check_buttons },
  { "color selection", create_color_selection },
  { "composited window", create_composited_window },
  { "cursors", create_cursors },
  { "dialog", create_dialog },
  { "display", create_display_screen, TRUE },
  { "entry", create_entry },
  { "event box", create_event_box },
  { "event watcher", create_event_watcher },
  { "expander", create_expander },
  { "flipping", create_flipping },
  { "focus", create_focus },
  { "font selection", create_font_selection },
  { "image", create_image },
  { "key lookup", create_key_lookup },
  { "labels", create_labels },
  { "layout", create_layout },
  { "listbox", create_listbox },
  { "menus", create_menus },
  { "message dialog", create_message_dialog },
  { "modal window", create_modal_window, TRUE },
  { "notebook", create_notebook },
  { "panes", create_panes },
  { "paned keyboard", create_paned_keyboard_navigation },
  { "pixbuf", create_pixbuf },
  { "progress bar", create_progress_bar },
  { "properties", create_properties },
  { "radio buttons", create_radio_buttons },
  { "range controls", create_range_controls },
  { "rc file", create_rc_file },
  { "reparent", create_reparent },
  { "resize grips", create_resize_grips },
  { "rotated label", create_rotated_label },
  { "rotated text", create_rotated_text },
  { "saved position", create_saved_position },
  { "scrolled windows", create_scrolled_windows },
  { "settings", create_settings },
  { "shapes", create_shapes },
  { "size groups", create_size_groups },
  { "snapshot", create_snapshot },
  { "spinbutton", create_spins },
  { "statusbar", create_statusbar },
#if 0
  { "styles", create_styles },
#endif
  { "test idle", create_idle_test },
  { "test mainloop", create_mainloop, TRUE },
  { "test scrolling", create_scroll_test },
  { "test selection", create_selection_test },
  { "test timeout", create_timeout_test },
  { "toggle buttons", create_toggle_buttons },
  { "toolbar", create_toolbar },
  { "tooltips", create_tooltips },
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
  gtk_widget_set_name (window, "main_window");
  gtk_window_move (GTK_WINDOW (window), 50, 20);
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

  box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), box1);

  if (gtk_micro_version > 0)
    sprintf (buffer,
	     "Gtk+ v%d.%d.%d",
	     gtk_get_major_version (),
	     gtk_get_minor_version (),
	     gtk_get_micro_version ());
  else
    sprintf (buffer,
	     "Gtk+ v%d.%d",
	     gtk_get_major_version (),
	     gtk_get_minor_version ());

  label = gtk_label_new (buffer);
  gtk_box_pack_start (GTK_BOX (box1), label, FALSE, FALSE, 0);
  gtk_widget_set_name (label, "testgtk-version-label");

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 10);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
     		                  GTK_POLICY_NEVER, 
                                  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (box1), scrolled_window, TRUE, TRUE, 0);

  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
  gtk_container_add (GTK_CONTAINER (scrolled_window), box2);
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

  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
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
  if (g_file_test ("../modules/input/immodules.cache", G_FILE_TEST_EXISTS))
    g_setenv ("GTK_IM_MODULE_FILE", "../modules/input/immodules.cache", TRUE);
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
  GtkCssProvider *provider, *memory_provider;
  GdkDisplay *display;
  GdkScreen *screen;
  GtkBindingSet *binding_set;
  int i;
  gboolean done_benchmarks = FALSE;

  srand (time (NULL));

  test_init ();

  g_set_application_name ("GTK+ Test Program");

  gtk_init (&argc, &argv);

  provider = gtk_css_provider_new ();

  /* Check to see if we are being run from the correct
   * directory.
   */
  if (file_exists ("testgtk.css"))
    gtk_css_provider_load_from_path (provider, "testgtk.css", NULL);
  else if (file_exists ("tests/testgtk.css"))
    gtk_css_provider_load_from_path (provider, "tests/testgtk.css", NULL);
  else
    g_warning ("Couldn't find file \"testgtk.css\".");

  display = gdk_display_get_default ();
  screen = gdk_display_get_default_screen (display);

  gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

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

  memory_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (memory_provider,
                                   "#testgtk-version-label {\n"
                                   "  color: #f00;\n"
                                   "  font: Sans 18;\n"
                                   "}",
                                   -1, NULL);
  gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (memory_provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);

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
