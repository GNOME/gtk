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

#include "config.h"

#include "gtkcolorbutton.h"

#include "gtkbutton.h"
#include "gtkmain.h"
#include "gtkalignment.h"
#include "gtkcolorsel.h"
#include "gtkcolorseldialog.h"
#include "gtkdnd.h"
#include "gtkdrawingarea.h"
#include "gtkframe.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"


/**
 * SECTION:gtkcolorbutton
 * @Short_description: A button to launch a color selection dialog
 * @Title: GtkColorButton
 * @See_also: #GtkColorSelectionDialog, #GtkFontButton
 *
 * The #GtkColorButton is a button which displays the currently selected
 * color an allows to open a color selection dialog to change the color.
 * It is suitable widget for selecting a color in a preference dialog.
 */


/* Size of checks and gray levels for alpha compositing checkerboard */
#define CHECK_SIZE  4
#define CHECK_DARK  (1.0 / 3.0)
#define CHECK_LIGHT (2.0 / 3.0)


struct _GtkColorButtonPrivate
{
  GtkWidget *draw_area; /* Widget where we draw the color sample */
  GtkWidget *cs_dialog; /* Color selection dialog */

  gchar *title;         /* Title for the color selection window */
  GdkRGBA rgba;

  guint use_alpha : 1;  /* Use alpha or not */
};

/* Properties */
enum
{
  PROP_0,
  PROP_USE_ALPHA,
  PROP_TITLE,
  PROP_COLOR,
  PROP_ALPHA,
  PROP_RGBA
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
static void gtk_color_button_state_changed (GtkWidget           *widget,
                                            GtkStateType         previous_state);

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
                                                         P_("Whether to give the color an alpha value"),
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
   * GtkColorButton:rgba:
   *
   * The RGBA color.
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_RGBA,
                                   g_param_spec_boxed ("rgba",
                                                       P_("Current RGBA Color"),
                                                       P_("The selected RGBA color"),
                                                       GDK_TYPE_RGBA,
                                                       GTK_PARAM_READWRITE));


  /**
   * GtkColorButton::color-set:
   * @widget: the object which received the signal.
   *
   * The ::color-set signal is emitted when the user selects a color.
   * When handling this signal, use gtk_color_button_get_color() and
   * gtk_color_button_get_alpha() (or gtk_color_button_get_rgba()) to
   * find out which color was just selected.
   *
   * Note that this signal is only emitted when the <emphasis>user</emphasis>
   * changes the color. If you need to react to programmatic color changes
   * as well, use the notify::color signal.
   *
   * Since: 2.4
   */
  color_button_signals[COLOR_SET] = g_signal_new (I_("color-set"),
                                                  G_TYPE_FROM_CLASS (gobject_class),
                                                  G_SIGNAL_RUN_FIRST,
                                                  G_STRUCT_OFFSET (GtkColorButtonClass, color_set),
                                                  NULL, NULL,
                                                  _gtk_marshal_VOID__VOID,
                                                  G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (GtkColorButtonPrivate));
}

static gboolean
gtk_color_button_has_alpha (GtkColorButton *color_button)
{
  return color_button->priv->use_alpha &&
      color_button->priv->rgba.alpha < 1;
}

static cairo_pattern_t *
gtk_color_button_get_checkered (void)
{
  /* need to respect pixman's stride being a multiple of 4 */
  static unsigned char data[8] = { 0xFF, 0x00, 0x00, 0x00,
                                   0x00, 0xFF, 0x00, 0x00 };
  static cairo_surface_t *checkered = NULL;
  cairo_pattern_t *pattern;

  if (checkered == NULL)
    {
      checkered = cairo_image_surface_create_for_data (data,
                                                       CAIRO_FORMAT_A8,
                                                       2, 2, 4);
    }

  pattern = cairo_pattern_create_for_surface (checkered);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
  cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST);

  return pattern;
}

/* Handle exposure events for the color picker's drawing area */
static gint
gtk_color_button_draw_cb (GtkWidget *widget,
                          cairo_t   *cr,
                          gpointer   data)
{
  GtkColorButton *color_button = GTK_COLOR_BUTTON (data);
  cairo_pattern_t *checkered;

  if (gtk_color_button_has_alpha (color_button))
    {
      cairo_set_source_rgb (cr, CHECK_DARK, CHECK_DARK, CHECK_DARK);
      cairo_paint (cr);

      cairo_set_source_rgb (cr, CHECK_LIGHT, CHECK_LIGHT, CHECK_LIGHT);
      cairo_scale (cr, CHECK_SIZE, CHECK_SIZE);

      checkered = gtk_color_button_get_checkered ();
      cairo_mask (cr, checkered);
      cairo_pattern_destroy (checkered);

      gdk_cairo_set_source_rgba (cr, &color_button->priv->rgba);
    }
  else
    {
      cairo_set_source_rgb (cr,
                            color_button->priv->rgba.red,
                            color_button->priv->rgba.green,
                            color_button->priv->rgba.blue);
    }

  cairo_paint (cr);

  if (!gtk_widget_is_sensitive (GTK_WIDGET (color_button)))
    {
      GtkStyleContext *context;
      GdkRGBA color;

      context = gtk_widget_get_style_context (widget);
      gtk_style_context_get_background_color (context, GTK_STATE_FLAG_INSENSITIVE, &color);

      gdk_cairo_set_source_rgba (cr, &color);
      checkered = gtk_color_button_get_checkered ();
      cairo_mask (cr, checkered);
      cairo_pattern_destroy (checkered);
    }

  return FALSE;
}

static void
gtk_color_button_state_changed (GtkWidget   *widget,
                                GtkStateType previous_state)
{
  gtk_widget_queue_draw (widget);
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
  gint length;
  guint16 *dropped;

  length = gtk_selection_data_get_length (selection_data);

  if (length < 0)
    return;

  /* We accept drops with the wrong format, since the KDE color
   * chooser incorrectly drops application/x-color with format 8.
   */
  if (length != 8)
    {
      g_warning (_("Received invalid color data\n"));
      return;
    }


  dropped = (guint16 *) gtk_selection_data_get_data (selection_data);

  color_button->priv->rgba.red = dropped[0] / 65535.;
  color_button->priv->rgba.green = dropped[1] / 65535.;
  color_button->priv->rgba.blue = dropped[2] / 65535.;
  color_button->priv->rgba.alpha = dropped[3] / 65535.;

  gtk_widget_queue_draw (color_button->priv->draw_area);

  g_signal_emit (color_button, color_button_signals[COLOR_SET], 0);

  g_object_freeze_notify (G_OBJECT (color_button));
  g_object_notify (G_OBJECT (color_button), "color");
  g_object_notify (G_OBJECT (color_button), "alpha");
  g_object_notify (G_OBJECT (color_button), "rgba");
  g_object_thaw_notify (G_OBJECT (color_button));
}

static void
set_color_icon (GdkDragContext *context,
                GdkRGBA        *rgba)
{
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
                                        48, 32);
  cr = cairo_create (surface);

  gdk_cairo_set_source_rgba (cr, rgba);
  cairo_paint (cr);

  gtk_drag_set_icon_surface (context, surface);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);
}

static void
gtk_color_button_drag_begin (GtkWidget      *widget,
                             GdkDragContext *context,
                             gpointer        data)
{
  GtkColorButton *color_button = data;

  set_color_icon (context, &color_button->priv->rgba);
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

  dropped[0] = (guint16) (color_button->priv->rgba.red * 65535);
  dropped[1] = (guint16) (color_button->priv->rgba.green * 65535);
  dropped[2] = (guint16) (color_button->priv->rgba.blue * 65535);
  dropped[3] = (guint16) (color_button->priv->rgba.alpha * 65535);

  gtk_selection_data_set (selection_data,
                          gtk_selection_data_get_target (selection_data),
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
  color_button->priv = G_TYPE_INSTANCE_GET_PRIVATE (color_button,
                                                    GTK_TYPE_COLOR_BUTTON,
                                                    GtkColorButtonPrivate);

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
  g_signal_connect (color_button->priv->draw_area, "draw",
                    G_CALLBACK (gtk_color_button_draw_cb), color_button);
  gtk_container_add (GTK_CONTAINER (frame), color_button->priv->draw_area);
  gtk_widget_show (color_button->priv->draw_area);

  color_button->priv->title = g_strdup (_("Pick a Color")); /* default title */

  /* Start with opaque black, alpha disabled */

  color_button->priv->rgba.red = 0;
  color_button->priv->rgba.green = 0;
  color_button->priv->rgba.blue = 0;
  color_button->priv->rgba.alpha = 1;
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
  g_signal_connect (color_button, "drag-begin",
                    G_CALLBACK (gtk_color_button_drag_begin), color_button);
  g_signal_connect (color_button, "drag-data-received",
                    G_CALLBACK (gtk_color_button_drag_data_received), color_button);
  g_signal_connect (color_button, "drag-data-get",
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

  g_free (color_button->priv->title);
  color_button->priv->title = NULL;

  G_OBJECT_CLASS (gtk_color_button_parent_class)->finalize (object);
}


/**
 * gtk_color_button_new:
 *
 * Creates a new color button.
 *
 * This returns a widget in the form of a small button containing
 * a swatch representing the current selected color. When the button
 * is clicked, a color-selection dialog will open, allowing the user
 * to select a color. The swatch will be updated to reflect the new
 * color when the user finishes.
 *
 * Returns: a new color button
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
 * @color: A #GdkColor to set the current color with
 *
 * Creates a new color button.
 *
 * Returns: a new color button
 *
 * Since: 2.4
 */
GtkWidget *
gtk_color_button_new_with_color (const GdkColor *color)
{
  return g_object_new (GTK_TYPE_COLOR_BUTTON, "color", color, NULL);
}

/**
 * gtk_color_button_new_with_rgba:
 * @rgba: A #GdkRGBA to set the current color with
 *
 * Creates a new color button.
 *
 * Returns: a new color button
 *
 * Since: 3.0
 */
GtkWidget *
gtk_color_button_new_with_rgba (const GdkRGBA *rgba)
{
  return g_object_new (GTK_TYPE_COLOR_BUTTON, "rgba", rgba, NULL);
}

static void
dialog_ok_clicked (GtkWidget *widget,
                   gpointer   data)
{
  GtkColorButton *color_button = GTK_COLOR_BUTTON (data);
  GtkColorSelection *color_selection;
  GtkColorSelectionDialog *selection_dialog;

  selection_dialog = GTK_COLOR_SELECTION_DIALOG (color_button->priv->cs_dialog);
  color_selection = GTK_COLOR_SELECTION (gtk_color_selection_dialog_get_color_selection (selection_dialog));

  gtk_color_selection_get_current_rgba (color_selection, &color_button->priv->rgba);

  gtk_widget_hide (color_button->priv->cs_dialog);

  gtk_widget_queue_draw (color_button->priv->draw_area);

  g_signal_emit (color_button, color_button_signals[COLOR_SET], 0);

  g_object_freeze_notify (G_OBJECT (color_button));
  g_object_notify (G_OBJECT (color_button), "color");
  g_object_notify (G_OBJECT (color_button), "alpha");
  g_object_notify (G_OBJECT (color_button), "rgba");
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
  GtkColorSelection *color_selection;
  GtkColorSelectionDialog *color_dialog;

  /* if dialog already exists, make sure it's shown and raised */
  if (!color_button->priv->cs_dialog)
    {
      /* Create the dialog and connects its buttons */
      GtkWidget *parent;
      GtkWidget *ok_button, *cancel_button;

      parent = gtk_widget_get_toplevel (GTK_WIDGET (color_button));

      color_button->priv->cs_dialog = gtk_color_selection_dialog_new (color_button->priv->title);

      color_dialog = GTK_COLOR_SELECTION_DIALOG (color_button->priv->cs_dialog);

      if (gtk_widget_is_toplevel (parent) && GTK_IS_WINDOW (parent))
        {
          if (GTK_WINDOW (parent) != gtk_window_get_transient_for (GTK_WINDOW (color_dialog)))
            gtk_window_set_transient_for (GTK_WINDOW (color_dialog), GTK_WINDOW (parent));

          gtk_window_set_modal (GTK_WINDOW (color_dialog),
                                gtk_window_get_modal (GTK_WINDOW (parent)));
        }

      g_object_get (color_dialog,
                    "ok-button", &ok_button,
                    "cancel-button", &cancel_button,
                    NULL);

      g_signal_connect (ok_button, "clicked",
                        G_CALLBACK (dialog_ok_clicked), color_button);
      g_signal_connect (cancel_button, "clicked",
                        G_CALLBACK (dialog_cancel_clicked), color_button);
      g_signal_connect (color_dialog, "destroy",
                        G_CALLBACK (dialog_destroy), color_button);
    }

  color_dialog = GTK_COLOR_SELECTION_DIALOG (color_button->priv->cs_dialog);
  color_selection = GTK_COLOR_SELECTION (gtk_color_selection_dialog_get_color_selection (color_dialog));

  gtk_color_selection_set_has_opacity_control (color_selection,
                                               color_button->priv->use_alpha);

  gtk_color_selection_set_previous_rgba (color_selection,
                                         &color_button->priv->rgba);
  gtk_color_selection_set_current_rgba (color_selection,
                                        &color_button->priv->rgba);

  gtk_window_present (GTK_WINDOW (color_button->priv->cs_dialog));
}

/**
 * gtk_color_button_set_color:
 * @color_button: a #GtkColorButton
 * @color: A #GdkColor to set the current color with
 *
 * Sets the current color to be @color.
 *
 * Since: 2.4
 */
void
gtk_color_button_set_color (GtkColorButton *color_button,
                            const GdkColor *color)
{
  g_return_if_fail (GTK_IS_COLOR_BUTTON (color_button));
  g_return_if_fail (color != NULL);

  color_button->priv->rgba.red = color->red / 65535.;
  color_button->priv->rgba.green = color->green / 65535.;
  color_button->priv->rgba.blue = color->blue / 65535.;

  gtk_widget_queue_draw (color_button->priv->draw_area);

  g_object_notify (G_OBJECT (color_button), "color");
  g_object_notify (G_OBJECT (color_button), "rgba");
}


/**
 * gtk_color_button_set_alpha:
 * @color_button: a #GtkColorButton
 * @alpha: an integer between 0 and 65535
 *
 * Sets the current opacity to be @alpha.
 *
 * Since: 2.4
 */
void
gtk_color_button_set_alpha (GtkColorButton *color_button,
                            guint16         alpha)
{
  g_return_if_fail (GTK_IS_COLOR_BUTTON (color_button));

  color_button->priv->rgba.alpha = alpha / 65535.;

  gtk_widget_queue_draw (color_button->priv->draw_area);

  g_object_notify (G_OBJECT (color_button), "alpha");
  g_object_notify (G_OBJECT (color_button), "rgba");
}

/**
 * gtk_color_button_get_color:
 * @color_button: a #GtkColorButton
 * @color: (out): a #GdkColor to fill in with the current color
 *
 * Sets @color to be the current color in the #GtkColorButton widget.
 *
 * Since: 2.4
 */
void
gtk_color_button_get_color (GtkColorButton *color_button,
                            GdkColor       *color)
{
  g_return_if_fail (GTK_IS_COLOR_BUTTON (color_button));

  color->red = (guint16) (color_button->priv->rgba.red * 65535);
  color->green = (guint16) (color_button->priv->rgba.green * 65535);
  color->blue = (guint16) (color_button->priv->rgba.blue * 65535);
}

/**
 * gtk_color_button_get_alpha:
 * @color_button: a #GtkColorButton
 *
 * Returns the current alpha value.
 *
 * Return value: an integer between 0 and 65535
 *
 * Since: 2.4
 */
guint16
gtk_color_button_get_alpha (GtkColorButton *color_button)
{
  g_return_val_if_fail (GTK_IS_COLOR_BUTTON (color_button), 0);

  return (guint16) (color_button->priv->rgba.alpha * 65535);
}

/**
 * gtk_color_button_set_rgba:
 * @color_button: a #GtkColorButton
 * @rgba: a #GdkRGBA to set the current color with
 *
 * Sets the current color to be @rgba.
 *
 * Since: 3.0
 */
void
gtk_color_button_set_rgba (GtkColorButton *color_button,
                           const GdkRGBA  *rgba)
{
  g_return_if_fail (GTK_IS_COLOR_BUTTON (color_button));
  g_return_if_fail (rgba != NULL);

  color_button->priv->rgba = *rgba;

  gtk_widget_queue_draw (color_button->priv->draw_area);

  g_object_notify (G_OBJECT (color_button), "rgba");
}

/**
 * gtk_color_button_get_rgba:
 * @color_button: a #GtkColorButton
 * @rgba: (out): a #GdkRGBA to fill in with the current color
 *
 * Sets @rgba to be the current color in the #GtkColorButton widget.
 *
 * Since: 3.0
 */
void
gtk_color_button_get_rgba (GtkColorButton *color_button,
                           GdkRGBA        *rgba)
{
  g_return_if_fail (GTK_IS_COLOR_BUTTON (color_button));
  g_return_if_fail (rgba != NULL);

  *rgba = color_button->priv->rgba;
}

/**
 * gtk_color_button_set_use_alpha:
 * @color_button: a #GtkColorButton
 * @use_alpha: %TRUE if color button should use alpha channel, %FALSE if not
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

      gtk_widget_queue_draw (color_button->priv->draw_area);

      g_object_notify (G_OBJECT (color_button), "use-alpha");
    }
}

/**
 * gtk_color_button_get_use_alpha:
 * @color_button: a #GtkColorButton
 *
 * Does the color selection dialog use the alpha channel ?
 *
 * Returns: %TRUE if the color sample uses alpha channel, %FALSE if not
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
 * @title: String containing new window title
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
    case PROP_RGBA:
      gtk_color_button_set_rgba (color_button, g_value_get_boxed (value));
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
    case PROP_RGBA:
      {
        GdkRGBA rgba;

        gtk_color_button_get_rgba (color_button, &rgba);
        g_value_set_boxed (value, &rgba);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}
