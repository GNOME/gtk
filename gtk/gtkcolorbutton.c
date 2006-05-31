/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 1998, 1999 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/* Color picker button for GNOME
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 *
 * Modified by the GTK+ Team and others 2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>

#include "gtkcolorbutton.h"
#include "gdk/gdkkeysyms.h"
#include "gdk-pixbuf/gdk-pixbuf.h"
#include "gtkbutton.h"
#include "gtkmain.h"
#include "gtkalignment.h"
#include "gtkcolorsel.h"
#include "gtkcolorseldialog.h"
#include "gtkdnd.h"
#include "gtkdrawingarea.h"
#include "gtkframe.h"
#include "gtksignal.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

/* Size of checks and gray levels for alpha compositing checkerboard */
#define CHECK_SIZE  4
#define CHECK_DARK  21845  /* 65535 / 3     */
#define CHECK_LIGHT 43690 

#define GTK_COLOR_BUTTON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_COLOR_BUTTON, GtkColorButtonPrivate))

struct _GtkColorButtonPrivate 
{
  GdkPixbuf *pixbuf;    /* Pixbuf for rendering sample */
  GdkGC *gc;            /* GC for drawing */
  
  GtkWidget *draw_area; /* Widget where we draw the color sample */
  GtkWidget *cs_dialog; /* Color selection dialog */
  
  gchar *title;         /* Title for the color selection window */
  
  GdkColor color;
  guint16 alpha;
  
  guint use_alpha : 1;  /* Use alpha or not */
};

/* Properties */
enum 
{
  PROP_0,
  PROP_USE_ALPHA,
  PROP_TITLE,
  PROP_COLOR,
  PROP_ALPHA
};

/* Signals */
enum 
{
  COLOR_SET,
  LAST_SIGNAL
};

/* gobject signals */
static void gtk_color_button_finalize      (GObject             *object);
static void gtk_color_button_set_property  (GObject        *object,
					    guint           param_id,
					    const GValue   *value,
					    GParamSpec     *pspec);
static void gtk_color_button_get_property  (GObject        *object,
					    guint           param_id,
					    GValue         *value,
					    GParamSpec     *pspec);

/* gtkwidget signals */
static void gtk_color_button_realize       (GtkWidget *widget);
static void gtk_color_button_unrealize     (GtkWidget *widget);
static void gtk_color_button_state_changed (GtkWidget           *widget, 
					    GtkStateType         previous_state);
static void gtk_color_button_style_set     (GtkWidget *widget, 
					    GtkStyle  *previous_style);

/* gtkbutton signals */
static void gtk_color_button_clicked       (GtkButton           *button);

/* source side drag signals */
static void gtk_color_button_drag_begin (GtkWidget        *widget,
					 GdkDragContext   *context,
					 gpointer          data);
static void gtk_color_button_drag_data_get (GtkWidget        *widget,
                                            GdkDragContext   *context,
                                            GtkSelectionData *selection_data,
                                            guint             info,
                                            guint             time,
                                            GtkColorButton   *color_button);

/* target side drag signals */
static void gtk_color_button_drag_data_received (GtkWidget        *widget,
						 GdkDragContext   *context,
						 gint              x,
						 gint              y,
						 GtkSelectionData *selection_data,
						 guint             info,
						 guint32           time,
						 GtkColorButton   *color_button);


static guint color_button_signals[LAST_SIGNAL] = { 0 };

static const GtkTargetEntry drop_types[] = { { "application/x-color", 0, 0 } };

G_DEFINE_TYPE (GtkColorButton, gtk_color_button, GTK_TYPE_BUTTON)

static void
gtk_color_button_class_init (GtkColorButtonClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkButtonClass *button_class;

  gobject_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);
  button_class = GTK_BUTTON_CLASS (klass);

  gobject_class->get_property = gtk_color_button_get_property;
  gobject_class->set_property = gtk_color_button_set_property;
  gobject_class->finalize = gtk_color_button_finalize;
  widget_class->state_changed = gtk_color_button_state_changed;
  widget_class->realize = gtk_color_button_realize;
  widget_class->unrealize = gtk_color_button_unrealize;
  widget_class->style_set = gtk_color_button_style_set;
  button_class->clicked = gtk_color_button_clicked;
  klass->color_set = NULL;

  /**
   * GtkColorButton:use-alpha:
   *
   * If this property is set to %TRUE, the color swatch on the button is rendered against a 
   * checkerboard background to show its opacity and the opacity slider is displayed in the 
   * color selection dialog. 
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_ALPHA,
                                   g_param_spec_boolean ("use-alpha", P_("Use alpha"), 
                                                         P_("Whether or not to give the color an alpha value"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkColorButton:title:
   *
   * The title of the color selection dialog
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title", 
							P_("Title"), 
                                                        P_("The title of the color selection dialog"),
                                                        _("Pick a Color"),
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkColorButton:color:
   *
   * The selected color.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_COLOR,
                                   g_param_spec_boxed ("color",
                                                       P_("Current Color"),
                                                       P_("The selected color"),
                                                       GDK_TYPE_COLOR,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkColorButton:alpha:
   *
   * The selected opacity value (0 fully transparent, 65535 fully opaque). 
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ALPHA,
                                   g_param_spec_uint ("alpha",
                                                      P_("Current Alpha"),
                                                      P_("The selected opacity value (0 fully transparent, 65535 fully opaque)"),
                                                      0, 65535, 65535,
                                                      GTK_PARAM_READWRITE));
        
  /**
   * GtkColorButton::color-set:
   * @widget: the object which received the signal.
   * 
   * The ::color-set signal is emitted when the user selects a color. 
   * When handling this signal, use gtk_color_button_get_color() and 
   * gtk_color_button_get_alpha() to find out which color was just selected.
   *
   * Note that this signal is only emitted when the <emphasis>user</emphasis>
   * changes the color. If you need to react to programmatic color changes
   * as well, use the notify::color signal.
   *
   * Since: 2.4
   */
  color_button_signals[COLOR_SET] = g_signal_new (I_("color_set"),
						  G_TYPE_FROM_CLASS (gobject_class),
						  G_SIGNAL_RUN_FIRST,
						  G_STRUCT_OFFSET (GtkColorButtonClass, color_set),
						  NULL, NULL,
						  _gtk_marshal_VOID__VOID,
						  G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (GtkColorButtonPrivate));
}

static void
render (GtkColorButton *color_button)
{
  gint dark_r, dark_g, dark_b;
  gint light_r, light_g, light_b;
  gint i, j, rowstride;
  gint width, height;
  gint c1[3], c2[3];
  guchar *pixels;
  guint8 insensitive_r = 0;
  guint8 insensitive_g = 0;
  guint8 insensitive_b = 0;

  width = color_button->priv->draw_area->allocation.width;
  height = color_button->priv->draw_area->allocation.height;
  if (color_button->priv->pixbuf == NULL || 
      gdk_pixbuf_get_width (color_button->priv->pixbuf) != width ||
      gdk_pixbuf_get_height (color_button->priv->pixbuf) != height) 
    {
      if (color_button->priv->pixbuf != NULL)
	g_object_unref (color_button->priv->pixbuf);
      color_button->priv->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    }
  

  /* Compute dark and light check colors */

  insensitive_r = GTK_WIDGET(color_button)->style->bg[GTK_STATE_INSENSITIVE].red >> 8;
  insensitive_g = GTK_WIDGET(color_button)->style->bg[GTK_STATE_INSENSITIVE].green >> 8;
  insensitive_b = GTK_WIDGET(color_button)->style->bg[GTK_STATE_INSENSITIVE].blue >> 8;

  if (color_button->priv->use_alpha) 
    {
      dark_r = ((CHECK_DARK << 16) + (color_button->priv->color.red - CHECK_DARK) * color_button->priv->alpha) >> 24;
      dark_g = ((CHECK_DARK << 16) + (color_button->priv->color.green - CHECK_DARK) * color_button->priv->alpha) >> 24;
      dark_b = ((CHECK_DARK << 16) + (color_button->priv->color.blue - CHECK_DARK) * color_button->priv->alpha) >> 24;
      
      light_r = ((CHECK_LIGHT << 16) + (color_button->priv->color.red - CHECK_LIGHT) * color_button->priv->alpha) >> 24;
      light_g = ((CHECK_LIGHT << 16) + (color_button->priv->color.green - CHECK_LIGHT) * color_button->priv->alpha) >> 24;
      light_b = ((CHECK_LIGHT << 16) + (color_button->priv->color.blue - CHECK_LIGHT) * color_button->priv->alpha) >> 24;
    } 
  else 
    {
      dark_r = light_r = color_button->priv->color.red >> 8;
      dark_g = light_g = color_button->priv->color.green >> 8;
      dark_b = light_b = color_button->priv->color.blue >> 8;
    }

  /* Fill image buffer */
  pixels = gdk_pixbuf_get_pixels (color_button->priv->pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (color_button->priv->pixbuf);
  for (j = 0; j < height; j++) 
    {
      if ((j / CHECK_SIZE) & 1) 
        {
          c1[0] = dark_r;
          c1[1] = dark_g;
          c1[2] = dark_b;
          
          c2[0] = light_r;
          c2[1] = light_g;
          c2[2] = light_b;
        } 
      else 
        {
          c1[0] = light_r;
          c1[1] = light_g;
          c1[2] = light_b;
          
          c2[0] = dark_r;
          c2[1] = dark_g;
          c2[2] = dark_b;
        }
      
    for (i = 0; i < width; i++) 
      {
        if (!GTK_WIDGET_IS_SENSITIVE (GTK_WIDGET (color_button)) && (i+j)%2) 
          {
            *(pixels + j * rowstride + i * 3) = insensitive_r;
            *(pixels + j * rowstride + i * 3 + 1) = insensitive_g;
            *(pixels + j * rowstride + i * 3 + 2) = insensitive_b;
          } 
        else if ((i / CHECK_SIZE) & 1) 
          {
            *(pixels + j * rowstride + i * 3)     = c1[0];
            *(pixels + j * rowstride + i * 3 + 1) = c1[1];
            *(pixels + j * rowstride + i * 3 + 2) = c1[2];
          }
        else
          {
            *(pixels + j * rowstride + i * 3)     = c2[0];
            *(pixels + j * rowstride + i * 3 + 1) = c2[1];
            *(pixels + j * rowstride + i * 3 + 2) = c2[2];
          }
      }
    }
}

/* Handle exposure events for the color picker's drawing area */
static gint
expose_event (GtkWidget      *widget, 
              GdkEventExpose *event, 
              gpointer        data)
{
  GtkColorButton *color_button = GTK_COLOR_BUTTON (data);

  gint width = color_button->priv->draw_area->allocation.width;
  gint height = color_button->priv->draw_area->allocation.height;

  if (color_button->priv->pixbuf == NULL ||
      width != gdk_pixbuf_get_width (color_button->priv->pixbuf) ||
      height != gdk_pixbuf_get_height (color_button->priv->pixbuf)) 
    render (color_button);

  gdk_draw_pixbuf (widget->window,
                   color_button->priv->gc,
                   color_button->priv->pixbuf,
                   event->area.x - widget->allocation.x,
                   event->area.y - widget->allocation.y,
                   event->area.x,
                   event->area.y,
                   event->area.width,
                   event->area.height,
                   GDK_RGB_DITHER_MAX,
                   event->area.x - widget->allocation.x,
                   event->area.y - widget->allocation.y);
  return FALSE;
}

static void
gtk_color_button_realize (GtkWidget *widget)
{
  GtkColorButton *color_button = GTK_COLOR_BUTTON (widget);

  GTK_WIDGET_CLASS (gtk_color_button_parent_class)->realize (widget);

  color_button->priv->gc = gdk_gc_new (widget->window);

  render (color_button);
}

static void
gtk_color_button_unrealize (GtkWidget *widget)
{
  GtkColorButton *color_button = GTK_COLOR_BUTTON (widget);

  g_object_unref (color_button->priv->gc);
  color_button->priv->gc = NULL;

  GTK_WIDGET_CLASS (gtk_color_button_parent_class)->unrealize (widget);
}

static void
gtk_color_button_style_set (GtkWidget *widget, 
			    GtkStyle  *previous_style)
{
  GtkColorButton *color_button = GTK_COLOR_BUTTON (widget);

  GTK_WIDGET_CLASS (gtk_color_button_parent_class)->style_set (widget, previous_style);

  if (GTK_WIDGET_REALIZED (widget)) 
    {
      if (color_button->priv->pixbuf != NULL)
	g_object_unref (color_button->priv->pixbuf);
      color_button->priv->pixbuf = NULL;
    }
}

static void
gtk_color_button_state_changed (GtkWidget   *widget, 
                                          GtkStateType previous_state)
{
  GtkColorButton *color_button = GTK_COLOR_BUTTON (widget);

  if (widget->state == GTK_STATE_INSENSITIVE || previous_state == GTK_STATE_INSENSITIVE)
    {
      if (color_button->priv->pixbuf != NULL)
	g_object_unref (color_button->priv->pixbuf);
      color_button->priv->pixbuf = NULL;
    }
}

static void
gtk_color_button_drag_data_received (GtkWidget        *widget,
				     GdkDragContext   *context,
				     gint              x,
				     gint              y,
				     GtkSelectionData *selection_data,
				     guint             info,
				     guint32           time,
				     GtkColorButton   *color_button)
{
  guint16 *dropped;

  if (selection_data->length < 0)
    return;

  /* We accept drops with the wrong format, since the KDE color
   * chooser incorrectly drops application/x-color with format 8.
   */
  if (selection_data->length != 8) 
    {
      g_warning (_("Received invalid color data\n"));
      return;
    }


  dropped = (guint16 *)selection_data->data;

  color_button->priv->color.red = dropped[0];
  color_button->priv->color.green = dropped[1];
  color_button->priv->color.blue = dropped[2];
  color_button->priv->alpha = dropped[3];

  if (color_button->priv->pixbuf != NULL)
    g_object_unref (color_button->priv->pixbuf);
  color_button->priv->pixbuf = NULL;

  gtk_widget_queue_draw (color_button->priv->draw_area);

  g_signal_emit (color_button, color_button_signals[COLOR_SET], 0);

  g_object_freeze_notify (G_OBJECT (color_button));
  g_object_notify (G_OBJECT (color_button), "color");
  g_object_notify (G_OBJECT (color_button), "alpha");
  g_object_thaw_notify (G_OBJECT (color_button));
}

static void
set_color_icon (GdkDragContext *context,
		GdkColor       *color)
{
  GdkPixbuf *pixbuf;
  guint32 pixel;

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE,
			   8, 48, 32);

  pixel = ((color->red & 0xff00) << 16) | 
          ((color->green & 0xff00) << 8) | 
           (color->blue & 0xff00);

  gdk_pixbuf_fill (pixbuf, pixel);
  
  gtk_drag_set_icon_pixbuf (context, pixbuf, -2, -2);
  g_object_unref (pixbuf);
}

static void
gtk_color_button_drag_begin (GtkWidget      *widget,
			     GdkDragContext *context,
			     gpointer        data)
{
  GtkColorButton *color_button = data;
  
  set_color_icon (context, &color_button->priv->color);
}

static void
gtk_color_button_drag_data_get (GtkWidget        *widget,
				GdkDragContext   *context,
				GtkSelectionData *selection_data,
				guint             info,
				guint             time,
				GtkColorButton   *color_button)
{
  guint16 dropped[4];

  dropped[0] = color_button->priv->color.red;
  dropped[1] = color_button->priv->color.green;
  dropped[2] = color_button->priv->color.blue;
  dropped[3] = color_button->priv->alpha;

  gtk_selection_data_set (selection_data, selection_data->target,
			  16, (guchar *)dropped, 8);
}

static void
gtk_color_button_init (GtkColorButton *color_button)
{
  GtkWidget *alignment;
  GtkWidget *frame;
  PangoLayout *layout;
  PangoRectangle rect;

  /* Create the widgets */
  color_button->priv = GTK_COLOR_BUTTON_GET_PRIVATE (color_button);

  gtk_widget_push_composite_child ();

  alignment = gtk_alignment_new (0.5, 0.5, 0.5, 1.0);
  gtk_container_set_border_width (GTK_CONTAINER (alignment), 1);
  gtk_container_add (GTK_CONTAINER (color_button), alignment);
  gtk_widget_show (alignment);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_OUT);
  gtk_container_add (GTK_CONTAINER (alignment), frame);
  gtk_widget_show (frame);

  /* Just some widget we can hook to expose-event on */
  color_button->priv->draw_area = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (color_button), "Black");
  pango_layout_get_pixel_extents (layout, NULL, &rect);
  g_object_unref (layout);

  gtk_widget_set_size_request (color_button->priv->draw_area, rect.width - 2, rect.height - 2);
  g_signal_connect (color_button->priv->draw_area, "expose-event",
                    G_CALLBACK (expose_event), color_button);
  gtk_container_add (GTK_CONTAINER (frame), color_button->priv->draw_area);
  gtk_widget_show (color_button->priv->draw_area);

  color_button->priv->title = g_strdup (_("Pick a Color")); /* default title */

  /* Create the buffer for the image so that we can create an image.  
   * Also create the picker's pixmap.
   */
  color_button->priv->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, rect.width, rect.height);

  color_button->priv->gc = NULL;

  /* Start with opaque black, alpha disabled */

  color_button->priv->color.red = 0;
  color_button->priv->color.green = 0;
  color_button->priv->color.blue = 0;
  color_button->priv->alpha = 65535;
  color_button->priv->use_alpha = FALSE;

  gtk_drag_dest_set (GTK_WIDGET (color_button),
                     GTK_DEST_DEFAULT_MOTION |
                     GTK_DEST_DEFAULT_HIGHLIGHT |
                     GTK_DEST_DEFAULT_DROP,
                     drop_types, 1, GDK_ACTION_COPY);
  gtk_drag_source_set (GTK_WIDGET(color_button),
                       GDK_BUTTON1_MASK|GDK_BUTTON3_MASK,
                       drop_types, 1,
                       GDK_ACTION_COPY);
  g_signal_connect (color_button, "drag_begin",
		    G_CALLBACK (gtk_color_button_drag_begin), color_button);
  g_signal_connect (color_button, "drag_data_received",
                    G_CALLBACK (gtk_color_button_drag_data_received), color_button);
  g_signal_connect (color_button, "drag_data_get",
                    G_CALLBACK (gtk_color_button_drag_data_get), color_button);

  gtk_widget_pop_composite_child ();
}

static void
gtk_color_button_finalize (GObject *object)
{
  GtkColorButton *color_button = GTK_COLOR_BUTTON (object);

  if (color_button->priv->cs_dialog != NULL)
    gtk_widget_destroy (color_button->priv->cs_dialog);
  color_button->priv->cs_dialog = NULL;

  if (color_button->priv->pixbuf != NULL)
    g_object_unref (color_button->priv->pixbuf);
  color_button->priv->pixbuf = NULL;

  g_free (color_button->priv->title);
  color_button->priv->title = NULL;

  G_OBJECT_CLASS (gtk_color_button_parent_class)->finalize (object);
}


/**
 * gtk_color_button_new:
 *
 * Creates a new color button. This returns a widget in the form of
 * a small button containing a swatch representing the current selected 
 * color. When the button is clicked, a color-selection dialog will open, 
 * allowing the user to select a color. The swatch will be updated to reflect 
 * the new color when the user finishes.
 *
 * Returns: a new color button.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_color_button_new (void)
{
  return g_object_new (GTK_TYPE_COLOR_BUTTON, NULL);
}

/**
 * gtk_color_button_new_with_color:
 * @color: A #GdkColor to set the current color with.
 *
 * Creates a new color button. 
 *
 * Returns: a new color button.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_color_button_new_with_color (const GdkColor *color)
{
  return g_object_new (GTK_TYPE_COLOR_BUTTON, "color", color, NULL);
}

static void
dialog_ok_clicked (GtkWidget *widget, 
		   gpointer   data)
{
  GtkColorButton *color_button = GTK_COLOR_BUTTON (data);
  GtkColorSelection *color_selection;

  color_selection = GTK_COLOR_SELECTION (GTK_COLOR_SELECTION_DIALOG (color_button->priv->cs_dialog)->colorsel);

  gtk_color_selection_get_current_color (color_selection, &color_button->priv->color);
  color_button->priv->alpha = gtk_color_selection_get_current_alpha (color_selection);

  if (color_button->priv->pixbuf != NULL)
    g_object_unref (color_button->priv->pixbuf);
  color_button->priv->pixbuf = NULL;

  gtk_widget_hide (color_button->priv->cs_dialog);

  gtk_widget_queue_draw (color_button->priv->draw_area);

  g_signal_emit (color_button, color_button_signals[COLOR_SET], 0);

  g_object_freeze_notify (G_OBJECT (color_button));
  g_object_notify (G_OBJECT (color_button), "color");
  g_object_notify (G_OBJECT (color_button), "alpha");
  g_object_thaw_notify (G_OBJECT (color_button));
}

static gboolean
dialog_destroy (GtkWidget *widget, 
		gpointer   data)
{
  GtkColorButton *color_button = GTK_COLOR_BUTTON (data);
  
  color_button->priv->cs_dialog = NULL;

  return FALSE;
}

static void
dialog_cancel_clicked (GtkWidget *widget,
		       gpointer   data)
{
  GtkColorButton *color_button = GTK_COLOR_BUTTON (data);
  
  gtk_widget_hide (color_button->priv->cs_dialog);  
}

static void
gtk_color_button_clicked (GtkButton *button)
{
  GtkColorButton *color_button = GTK_COLOR_BUTTON (button);
  GtkColorSelectionDialog *color_dialog;

  /* if dialog already exists, make sure it's shown and raised */
  if (!color_button->priv->cs_dialog) 
    {
      /* Create the dialog and connects its buttons */
      GtkWidget *parent;
      
      parent = gtk_widget_get_toplevel (GTK_WIDGET (color_button));
      
      color_button->priv->cs_dialog = gtk_color_selection_dialog_new (color_button->priv->title);
      
      color_dialog = GTK_COLOR_SELECTION_DIALOG (color_button->priv->cs_dialog);

      if (GTK_WIDGET_TOPLEVEL (parent) && GTK_IS_WINDOW (parent))
        {
          if (GTK_WINDOW (parent) != gtk_window_get_transient_for (GTK_WINDOW (color_dialog)))
 	    gtk_window_set_transient_for (GTK_WINDOW (color_dialog), GTK_WINDOW (parent));
	       
	  gtk_window_set_modal (GTK_WINDOW (color_dialog),
				gtk_window_get_modal (GTK_WINDOW (parent)));
	}
      
      g_signal_connect (color_dialog->ok_button, "clicked",
                        G_CALLBACK (dialog_ok_clicked), color_button);
      g_signal_connect (color_dialog->cancel_button, "clicked",
			G_CALLBACK (dialog_cancel_clicked), color_button);
      g_signal_connect (color_dialog, "destroy",
                        G_CALLBACK (dialog_destroy), color_button);
    }

  color_dialog = GTK_COLOR_SELECTION_DIALOG (color_button->priv->cs_dialog);

  gtk_color_selection_set_has_opacity_control (GTK_COLOR_SELECTION (color_dialog->colorsel),
                                               color_button->priv->use_alpha);
  
  gtk_color_selection_set_previous_color (GTK_COLOR_SELECTION (color_dialog->colorsel), 
					  &color_button->priv->color);
  gtk_color_selection_set_previous_alpha (GTK_COLOR_SELECTION (color_dialog->colorsel), 
					  color_button->priv->alpha);

  gtk_color_selection_set_current_color (GTK_COLOR_SELECTION (color_dialog->colorsel), 
					 &color_button->priv->color);
  gtk_color_selection_set_current_alpha (GTK_COLOR_SELECTION (color_dialog->colorsel), 
					 color_button->priv->alpha);

  gtk_window_present (GTK_WINDOW (color_button->priv->cs_dialog));
}

/**
 * gtk_color_button_set_color:
 * @color_button: a #GtkColorButton.
 * @color: A #GdkColor to set the current color with.
 *
 * Sets the current color to be @color.
 *
 * Since: 2.4
 **/
void
gtk_color_button_set_color (GtkColorButton *color_button,
			    const GdkColor *color)
{
  g_return_if_fail (GTK_IS_COLOR_BUTTON (color_button));
  g_return_if_fail (color != NULL);

  color_button->priv->color.red = color->red;
  color_button->priv->color.green = color->green;
  color_button->priv->color.blue = color->blue;

  if (color_button->priv->pixbuf != NULL)
    g_object_unref (color_button->priv->pixbuf);
  color_button->priv->pixbuf = NULL;

  gtk_widget_queue_draw (color_button->priv->draw_area);
  
  g_object_notify (G_OBJECT (color_button), "color");
}


/**
 * gtk_color_button_set_alpha:
 * @color_button: a #GtkColorButton.
 * @alpha: an integer between 0 and 65535.
 *
 * Sets the current opacity to be @alpha. 
 *
 * Since: 2.4
 **/
void
gtk_color_button_set_alpha (GtkColorButton *color_button,
			    guint16         alpha)
{
  g_return_if_fail (GTK_IS_COLOR_BUTTON (color_button));

  color_button->priv->alpha = alpha;

  if (color_button->priv->pixbuf != NULL)
    g_object_unref (color_button->priv->pixbuf);
  color_button->priv->pixbuf = NULL;

  gtk_widget_queue_draw (color_button->priv->draw_area);

  g_object_notify (G_OBJECT (color_button), "alpha");
}

/**
 * gtk_color_button_get_color:
 * @color_button: a #GtkColorButton.
 * @color: a #GdkColor to fill in with the current color.
 *
 * Sets @color to be the current color in the #GtkColorButton widget.
 *
 * Since: 2.4
 **/
void
gtk_color_button_get_color (GtkColorButton *color_button,
			    GdkColor       *color)
{
  g_return_if_fail (GTK_IS_COLOR_BUTTON (color_button));
  
  color->red = color_button->priv->color.red;
  color->green = color_button->priv->color.green;
  color->blue = color_button->priv->color.blue;
}

/**
 * gtk_color_button_get_alpha:
 * @color_button: a #GtkColorButton.
 *
 * Returns the current alpha value. 
 *
 * Return value: an integer between 0 and 65535.
 *
 * Since: 2.4
 **/
guint16
gtk_color_button_get_alpha (GtkColorButton *color_button)
{
  g_return_val_if_fail (GTK_IS_COLOR_BUTTON (color_button), 0);
  
  return color_button->priv->alpha;
}

/**
 * gtk_color_button_set_use_alpha:
 * @color_button: a #GtkColorButton.
 * @use_alpha: %TRUE if color button should use alpha channel, %FALSE if not.
 *
 * Sets whether or not the color button should use the alpha channel.
 *
 * Since: 2.4
 */
void
gtk_color_button_set_use_alpha (GtkColorButton *color_button, 
				gboolean        use_alpha)
{
  g_return_if_fail (GTK_IS_COLOR_BUTTON (color_button));

  use_alpha = (use_alpha != FALSE);

  if (color_button->priv->use_alpha != use_alpha) 
    {
      color_button->priv->use_alpha = use_alpha;

      render (color_button);
      gtk_widget_queue_draw (color_button->priv->draw_area);

      g_object_notify (G_OBJECT (color_button), "use-alpha");
    }
}

/**
 * gtk_color_button_get_use_alpha:
 * @color_button: a #GtkColorButton.
 *
 * Does the color selection dialog use the alpha channel?
 *
 * Returns: %TRUE if the color sample uses alpha channel, %FALSE if not.
 *
 * Since: 2.4
 */
gboolean
gtk_color_button_get_use_alpha (GtkColorButton *color_button)
{
  g_return_val_if_fail (GTK_IS_COLOR_BUTTON (color_button), FALSE);

  return color_button->priv->use_alpha;
}


/**
 * gtk_color_button_set_title:
 * @color_button: a #GtkColorButton
 * @title: String containing new window title.
 *
 * Sets the title for the color selection dialog.
 *
 * Since: 2.4
 */
void
gtk_color_button_set_title (GtkColorButton *color_button, 
			    const gchar    *title)
{
  gchar *old_title;

  g_return_if_fail (GTK_IS_COLOR_BUTTON (color_button));

  old_title = color_button->priv->title;
  color_button->priv->title = g_strdup (title);
  g_free (old_title);

  if (color_button->priv->cs_dialog)
    gtk_window_set_title (GTK_WINDOW (color_button->priv->cs_dialog), 
			  color_button->priv->title);
  
  g_object_notify (G_OBJECT (color_button), "title");
}

/**
 * gtk_color_button_get_title:
 * @color_button: a #GtkColorButton
 *
 * Gets the title of the color selection dialog.
 *
 * Returns: An internal string, do not free the return value
 *
 * Since: 2.4
 */
G_CONST_RETURN gchar *
gtk_color_button_get_title (GtkColorButton *color_button)
{
  g_return_val_if_fail (GTK_IS_COLOR_BUTTON (color_button), NULL);

  return color_button->priv->title;
}

static void
gtk_color_button_set_property (GObject      *object,
			       guint         param_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
  GtkColorButton *color_button = GTK_COLOR_BUTTON (object);

  switch (param_id) 
    {
    case PROP_USE_ALPHA:
      gtk_color_button_set_use_alpha (color_button, g_value_get_boolean (value));
      break;
    case PROP_TITLE:
      gtk_color_button_set_title (color_button, g_value_get_string (value));
      break;
    case PROP_COLOR:
      gtk_color_button_set_color (color_button, g_value_get_boxed (value));
      break;
    case PROP_ALPHA:
      gtk_color_button_set_alpha (color_button, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_color_button_get_property (GObject    *object,
			       guint       param_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
  GtkColorButton *color_button = GTK_COLOR_BUTTON (object);
  GdkColor color;

  switch (param_id) 
    {
    case PROP_USE_ALPHA:
      g_value_set_boolean (value, gtk_color_button_get_use_alpha (color_button));
      break;
    case PROP_TITLE:
      g_value_set_string (value, gtk_color_button_get_title (color_button));
      break;
    case PROP_COLOR:
      gtk_color_button_get_color (color_button, &color);
      g_value_set_boxed (value, &color);
      break;
    case PROP_ALPHA:
      g_value_set_uint (value, gtk_color_button_get_alpha (color_button));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

#define __GTK_COLOR_BUTTON_C__
#include "gtkaliasdef.c"
