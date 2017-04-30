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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
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
#include "gtkcolorchooser.h"
#include "gtkcolorchooserprivate.h"
#include "gtkcolorchooserdialog.h"
#include "gtkcolorswatchprivate.h"
#include "gtkdnd.h"
#include "gtkdragdest.h"
#include "gtkdragsource.h"
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
 * color and allows to open a color selection dialog to change the color.
 * It is suitable widget for selecting a color in a preference dialog.
 *
 * # CSS nodes
 *
 * GtkColorButton has a single CSS node with name button. To differentiate
 * it from a plain #GtkButton, it gets the .color style class.
 */


struct _GtkColorButtonPrivate
{
  GtkWidget *button;

  GtkWidget *swatch;    /* Widget where we draw the color sample */
  GtkWidget *cs_dialog; /* Color selection dialog */

  gchar *title;         /* Title for the color selection window */
  GdkRGBA rgba;

  guint use_alpha : 1;  /* Use alpha or not */
  guint show_editor : 1;
};

/* Properties */
enum
{
  PROP_0,
  PROP_USE_ALPHA,
  PROP_TITLE,
  PROP_RGBA,
  PROP_SHOW_EDITOR
};

/* Signals */
enum
{
  COLOR_SET,
  LAST_SIGNAL
};

/* gobject signals */
static void gtk_color_button_finalize      (GObject          *object);
static void gtk_color_button_set_property  (GObject          *object,
                                            guint             param_id,
                                            const GValue     *value,
                                            GParamSpec       *pspec);
static void gtk_color_button_get_property  (GObject          *object,
                                            guint             param_id,
                                            GValue           *value,
                                            GParamSpec       *pspec);

/* gtkbutton signals */
static void gtk_color_button_clicked       (GtkButton        *button,
                                            gpointer          user_data);

/* source side drag signals */
static void gtk_color_button_drag_begin    (GtkWidget        *widget,
                                            GdkDragContext   *context,
                                            gpointer          data);
static void gtk_color_button_drag_data_get (GtkWidget        *widget,
                                            GdkDragContext   *context,
                                            GtkSelectionData *selection_data,
                                            guint             info,
                                            guint             time,
                                            GtkColorButton   *button);

/* target side drag signals */
static void gtk_color_button_drag_data_received (GtkWidget        *widget,
                                                 GdkDragContext   *context,
                                                 gint              x,
                                                 gint              y,
                                                 GtkSelectionData *selection_data,
                                                 guint             info,
                                                 guint32           time,
                                                 GtkColorButton   *button);


static guint color_button_signals[LAST_SIGNAL] = { 0 };

static const GtkTargetEntry drop_types[] = { { (char *) "application/x-color", 0, 0 } };

static void gtk_color_button_iface_init (GtkColorChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkColorButton, gtk_color_button, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkColorButton)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_COLOR_CHOOSER,
                                                gtk_color_button_iface_init))

static void
gtk_color_button_measure (GtkWidget       *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (widget);
  GtkColorButtonPrivate *priv = gtk_color_button_get_instance_private (button);

  gtk_widget_measure (priv->button, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_color_button_snapshot (GtkWidget   *widget,
                           GtkSnapshot *snapshot)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (widget);
  GtkColorButtonPrivate *priv = gtk_color_button_get_instance_private (button);

  gtk_widget_snapshot_child (widget, priv->button, snapshot);
}

static void
gtk_color_button_size_allocate (GtkWidget     *widget,
                                GtkAllocation *allocation)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (widget);
  GtkColorButtonPrivate *priv = gtk_color_button_get_instance_private (button);

  GTK_WIDGET_CLASS (gtk_color_button_parent_class)->size_allocate (widget, allocation);

  gtk_widget_size_allocate (priv->button, allocation);
}

static void
gtk_color_button_class_init (GtkColorButtonClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->get_property = gtk_color_button_get_property;
  gobject_class->set_property = gtk_color_button_set_property;
  gobject_class->finalize = gtk_color_button_finalize;

  widget_class->snapshot = gtk_color_button_snapshot;
  widget_class->measure = gtk_color_button_measure;
  widget_class->size_allocate = gtk_color_button_size_allocate;
  klass->color_set = NULL;

  /**
   * GtkColorButton:use-alpha:
   *
   * If this property is set to %TRUE, the color swatch on the button is
   * rendered against a checkerboard background to show its opacity and
   * the opacity slider is displayed in the color selection dialog.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_ALPHA,
                                   g_param_spec_boolean ("use-alpha", P_("Use alpha"),
                                                         P_("Whether to give the color an alpha value"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

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
   * When handling this signal, use gtk_color_button_get_rgba() to
   * find out which color was just selected.
   *
   * Note that this signal is only emitted when the user
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
                                                  NULL,
                                                  G_TYPE_NONE, 0);

  /**
   * GtkColorButton:show-editor:
   *
   * Set this property to %TRUE to skip the palette
   * in the dialog and go directly to the color editor.
   *
   * This property should be used in cases where the palette
   * in the editor would be redundant, such as when the color
   * button is already part of a palette.
   *
   * Since: 3.20
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_EDITOR,
                                   g_param_spec_boolean ("show-editor", P_("Show Editor"),
                                                         P_("Whether to show the color editor right away"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
}

static void
gtk_color_button_drag_data_received (GtkWidget        *widget,
                                     GdkDragContext   *context,
                                     gint              x,
                                     gint              y,
                                     GtkSelectionData *selection_data,
                                     guint             info,
                                     guint32           time,
                                     GtkColorButton   *button)
{
  GtkColorButtonPrivate *priv = button->priv;
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
      g_warning ("%s: Received invalid color data", G_STRFUNC);
      return;
    }


  dropped = (guint16 *) gtk_selection_data_get_data (selection_data);

  priv->rgba.red = dropped[0] / 65535.;
  priv->rgba.green = dropped[1] / 65535.;
  priv->rgba.blue = dropped[2] / 65535.;
  priv->rgba.alpha = dropped[3] / 65535.;

  gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (priv->swatch), &priv->rgba);

  g_signal_emit (button, color_button_signals[COLOR_SET], 0);

  g_object_freeze_notify (G_OBJECT (button));
  g_object_notify (G_OBJECT (button), "rgba");
  g_object_thaw_notify (G_OBJECT (button));
}

static void
set_color_icon (GdkDragContext *context,
                const GdkRGBA  *rgba)
{
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 48, 32);
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
  GtkColorButton *button = data;

  set_color_icon (context, &button->priv->rgba);
}

static void
gtk_color_button_drag_data_get (GtkWidget        *widget,
                                GdkDragContext   *context,
                                GtkSelectionData *selection_data,
                                guint             info,
                                guint             time,
                                GtkColorButton   *button)
{
  GtkColorButtonPrivate *priv = button->priv;
  guint16 dropped[4];

  dropped[0] = (guint16) (priv->rgba.red * 65535);
  dropped[1] = (guint16) (priv->rgba.green * 65535);
  dropped[2] = (guint16) (priv->rgba.blue * 65535);
  dropped[3] = (guint16) (priv->rgba.alpha * 65535);

  gtk_selection_data_set (selection_data,
                          gtk_selection_data_get_target (selection_data),
                          16, (guchar *)dropped, 8);
}

static void
gtk_color_button_init (GtkColorButton *button)
{
  GtkColorButtonPrivate *priv;
  PangoLayout *layout;
  PangoRectangle rect;
  GtkStyleContext *context;

  gtk_widget_set_has_window (GTK_WIDGET (button), FALSE);

  /* Create the widgets */
  priv = button->priv = gtk_color_button_get_instance_private (button);

  priv->button = gtk_button_new ();
  g_signal_connect (priv->button, "clicked", G_CALLBACK (gtk_color_button_clicked), button);
  gtk_widget_set_parent (priv->button, GTK_WIDGET (button));

  priv->swatch = gtk_color_swatch_new ();
  layout = gtk_widget_create_pango_layout (GTK_WIDGET (button), "Black");
  pango_layout_get_pixel_extents (layout, NULL, &rect);
  g_object_unref (layout);

  gtk_widget_set_size_request (priv->swatch, rect.width, rect.height);

  gtk_container_add (GTK_CONTAINER (priv->button), priv->swatch);

  button->priv->title = g_strdup (_("Pick a Color")); /* default title */

  /* Start with opaque black, alpha disabled */
  priv->rgba.red = 0;
  priv->rgba.green = 0;
  priv->rgba.blue = 0;
  priv->rgba.alpha = 1;
  priv->use_alpha = FALSE;

  gtk_drag_dest_set (priv->button,
                     GTK_DEST_DEFAULT_MOTION |
                     GTK_DEST_DEFAULT_HIGHLIGHT |
                     GTK_DEST_DEFAULT_DROP,
                     drop_types, 1, GDK_ACTION_COPY);
  gtk_drag_source_set (priv->button,
                       GDK_BUTTON1_MASK|GDK_BUTTON3_MASK,
                       drop_types, 1,
                       GDK_ACTION_COPY);
  g_signal_connect (priv->button, "drag-begin",
                    G_CALLBACK (gtk_color_button_drag_begin), button);
  g_signal_connect (priv->button, "drag-data-received",
                    G_CALLBACK (gtk_color_button_drag_data_received), button);
  g_signal_connect (priv->button, "drag-data-get",
                    G_CALLBACK (gtk_color_button_drag_data_get), button);

  context = gtk_widget_get_style_context (GTK_WIDGET (priv->button));
  gtk_style_context_add_class (context, "color");
}

static void
gtk_color_button_finalize (GObject *object)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (object);
  GtkColorButtonPrivate *priv = button->priv;

  if (priv->cs_dialog != NULL)
    gtk_widget_destroy (priv->cs_dialog);

  g_free (priv->title);
  gtk_widget_unparent (priv->button);

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

static gboolean
dialog_destroy (GtkWidget *widget,
                gpointer   data)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (data);

  button->priv->cs_dialog = NULL;

  return FALSE;
}

static void
dialog_response (GtkDialog *dialog,
                 gint       response,
                 gpointer   data)
{
  if (response == GTK_RESPONSE_CANCEL)
    gtk_widget_hide (GTK_WIDGET (dialog));
  else if (response == GTK_RESPONSE_OK)
    {
      GtkColorButton *button = GTK_COLOR_BUTTON (data);
      GtkColorButtonPrivate *priv = button->priv;

      gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog), &priv->rgba);
      gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (priv->swatch), &priv->rgba);

      gtk_widget_hide (GTK_WIDGET (dialog));

      g_object_ref (button);
      g_signal_emit (button, color_button_signals[COLOR_SET], 0);

      g_object_freeze_notify (G_OBJECT (button));
      g_object_notify (G_OBJECT (button), "rgba");
      g_object_thaw_notify (G_OBJECT (button));
      g_object_unref (button);
    }
}

/* Create the dialog and connects its buttons */
static void
ensure_dialog (GtkColorButton *button)
{
  GtkColorButtonPrivate *priv = button->priv;
  GtkWidget *parent, *dialog;

  if (priv->cs_dialog != NULL)
    return;

  parent = gtk_widget_get_toplevel (GTK_WIDGET (button));

  priv->cs_dialog = dialog = gtk_color_chooser_dialog_new (priv->title, NULL);

  if (gtk_widget_is_toplevel (parent) && GTK_IS_WINDOW (parent))
  {
    if (GTK_WINDOW (parent) != gtk_window_get_transient_for (GTK_WINDOW (dialog)))
      gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));

    gtk_window_set_modal (GTK_WINDOW (dialog),
                          gtk_window_get_modal (GTK_WINDOW (parent)));
  }

  g_signal_connect (dialog, "response",
                    G_CALLBACK (dialog_response), button);
  g_signal_connect (dialog, "destroy",
                    G_CALLBACK (dialog_destroy), button);
}


static void
gtk_color_button_clicked (GtkButton *b,
                          gpointer   user_data)
{
  GtkColorButton *button = user_data;
  GtkColorButtonPrivate *priv = button->priv;

  /* if dialog already exists, make sure it's shown and raised */
  ensure_dialog (button);

  g_object_set (priv->cs_dialog, "show-editor", priv->show_editor, NULL);

  gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (priv->cs_dialog), priv->use_alpha);

  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (priv->cs_dialog), &priv->rgba);

  gtk_window_present (GTK_WINDOW (priv->cs_dialog));
}

static void
gtk_color_button_set_rgba (GtkColorChooser *chooser,
                           const GdkRGBA   *rgba)
{
  GtkColorButtonPrivate *priv;

  g_return_if_fail (GTK_IS_COLOR_BUTTON (chooser));
  g_return_if_fail (rgba != NULL);

  priv = GTK_COLOR_BUTTON (chooser)->priv;

  priv->rgba = *rgba;
  gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (priv->swatch), &priv->rgba);

  g_object_notify (G_OBJECT (chooser), "rgba");
}

static void
gtk_color_button_get_rgba (GtkColorChooser *chooser,
                           GdkRGBA        *rgba)
{
  g_return_if_fail (GTK_IS_COLOR_BUTTON (chooser));
  g_return_if_fail (rgba != NULL);

  *rgba = GTK_COLOR_BUTTON (chooser)->priv->rgba;
}

static void
set_use_alpha (GtkColorButton *button,
               gboolean        use_alpha)
{
  GtkColorButtonPrivate *priv = button->priv;

  use_alpha = (use_alpha != FALSE);

  if (priv->use_alpha != use_alpha)
    {
      priv->use_alpha = use_alpha;

      gtk_color_swatch_set_use_alpha (GTK_COLOR_SWATCH (priv->swatch), use_alpha);

      g_object_notify (G_OBJECT (button), "use-alpha");
    }
}

/**
 * gtk_color_button_set_title:
 * @button: a #GtkColorButton
 * @title: String containing new window title
 *
 * Sets the title for the color selection dialog.
 *
 * Since: 2.4
 */
void
gtk_color_button_set_title (GtkColorButton *button,
                            const gchar    *title)
{
  GtkColorButtonPrivate *priv = button->priv;
  gchar *old_title;

  g_return_if_fail (GTK_IS_COLOR_BUTTON (button));

  old_title = priv->title;
  priv->title = g_strdup (title);
  g_free (old_title);

  if (priv->cs_dialog)
    gtk_window_set_title (GTK_WINDOW (priv->cs_dialog), priv->title);

  g_object_notify (G_OBJECT (button), "title");
}

/**
 * gtk_color_button_get_title:
 * @button: a #GtkColorButton
 *
 * Gets the title of the color selection dialog.
 *
 * Returns: An internal string, do not free the return value
 *
 * Since: 2.4
 */
const gchar *
gtk_color_button_get_title (GtkColorButton *button)
{
  g_return_val_if_fail (GTK_IS_COLOR_BUTTON (button), NULL);

  return button->priv->title;
}

static void
gtk_color_button_set_property (GObject      *object,
                               guint         param_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (object);

  switch (param_id)
    {
    case PROP_USE_ALPHA:
      set_use_alpha (button, g_value_get_boolean (value));
      break;
    case PROP_TITLE:
      gtk_color_button_set_title (button, g_value_get_string (value));
      break;
    case PROP_RGBA:
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (button), g_value_get_boxed (value));
      break;
    case PROP_SHOW_EDITOR:
      {
        gboolean show_editor = g_value_get_boolean (value);
        if (button->priv->show_editor != show_editor)
          {
            button->priv->show_editor = show_editor;
            g_object_notify (object, "show-editor");
          }
      }
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
  GtkColorButton *button = GTK_COLOR_BUTTON (object);

  switch (param_id)
    {
    case PROP_USE_ALPHA:
      g_value_set_boolean (value, button->priv->use_alpha);
      break;
    case PROP_TITLE:
      g_value_set_string (value, gtk_color_button_get_title (button));
      break;
    case PROP_RGBA:
      {
        GdkRGBA rgba;

        gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &rgba);
        g_value_set_boxed (value, &rgba);
      }
      break;
    case PROP_SHOW_EDITOR:
      g_value_set_boolean (value, button->priv->show_editor);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_color_button_add_palette (GtkColorChooser *chooser,
                              GtkOrientation   orientation,
                              gint             colors_per_line,
                              gint             n_colors,
                              GdkRGBA         *colors)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (chooser);

  ensure_dialog (button);

  gtk_color_chooser_add_palette (GTK_COLOR_CHOOSER (button->priv->cs_dialog),
                                 orientation, colors_per_line, n_colors, colors);
}

static void
gtk_color_button_iface_init (GtkColorChooserInterface *iface)
{
  iface->get_rgba = gtk_color_button_get_rgba;
  iface->set_rgba = gtk_color_button_set_rgba;
  iface->add_palette = gtk_color_button_add_palette;
}

