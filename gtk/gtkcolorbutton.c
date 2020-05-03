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

#include "gtkbinlayout.h"
#include "gtkbutton.h"
#include "gtkcolorchooser.h"
#include "gtkcolorchooserprivate.h"
#include "gtkcolorchooserdialog.h"
#include "gtkcolorswatchprivate.h"
#include "gtkdragsource.h"
#include "gtkdroptarget.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtksnapshot.h"
#include "gtkwidgetprivate.h"


/**
 * SECTION:gtkcolorbutton
 * @Short_description: A button to launch a color selection dialog
 * @Title: GtkColorButton
 * @See_also: #GtkFontButton
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

typedef struct _GtkColorButtonClass     GtkColorButtonClass;

struct _GtkColorButton {
  GtkWidget parent_instance;

  GtkWidget *button;

  GtkWidget *swatch;    /* Widget where we draw the color sample */
  GtkWidget *cs_dialog; /* Color selection dialog */

  gchar *title;         /* Title for the color selection window */
  GdkRGBA rgba;

  guint use_alpha   : 1;  /* Use alpha or not */
  guint show_editor : 1;
  guint modal       : 1;
};

struct _GtkColorButtonClass {
  GtkWidgetClass parent_class;

  void (* color_set) (GtkColorButton *cp);
};

/* Properties */
enum
{
  PROP_0,
  PROP_USE_ALPHA,
  PROP_TITLE,
  PROP_RGBA,
  PROP_SHOW_EDITOR,
  PROP_MODAL
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


static guint color_button_signals[LAST_SIGNAL] = { 0 };

static void gtk_color_button_iface_init (GtkColorChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkColorButton, gtk_color_button, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_COLOR_CHOOSER,
                                                gtk_color_button_iface_init))

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

  widget_class->grab_focus = gtk_widget_grab_focus_child;
  widget_class->focus = gtk_widget_focus_child;

  klass->color_set = NULL;

  /**
   * GtkColorButton:use-alpha:
   *
   * If this property is set to %TRUE, the color swatch on the button is
   * rendered against a checkerboard background to show its opacity and
   * the opacity slider is displayed in the color selection dialog.
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
   * When handling this signal, use gtk_color_chooser_get_rgba() to
   * find out which color was just selected.
   *
   * Note that this signal is only emitted when the user
   * changes the color. If you need to react to programmatic color changes
   * as well, use the notify::color signal.
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
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_EDITOR,
                                   g_param_spec_boolean ("show-editor", P_("Show Editor"),
                                                         P_("Whether to show the color editor right away"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_MODAL,
                                   g_param_spec_boolean ("modal", P_("Modal"),
                                                         P_("Whether the dialog is modal"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));


  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "colorbutton");
}

static gboolean
gtk_color_button_drop (GtkDropTarget  *dest,
                       const GValue   *value,
                       double          x,
                       double          y,
                       GtkColorButton *button)
{
  GdkRGBA *color = g_value_get_boxed (value);

  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (button), color);
  return TRUE;
}

static GdkContentProvider *
gtk_color_button_drag_prepare (GtkDragSource  *source,
                               double          x,
                               double          y,
                               GtkColorButton *button)
{
  return gdk_content_provider_new_typed (GDK_TYPE_RGBA, &button->rgba);
}

static void
gtk_color_button_init (GtkColorButton *button)
{
  PangoLayout *layout;
  PangoRectangle rect;
  GtkDragSource *source;
  GtkDropTarget *dest;

  button->button = gtk_button_new ();
  g_signal_connect (button->button, "clicked", G_CALLBACK (gtk_color_button_clicked), button);
  gtk_widget_set_parent (button->button, GTK_WIDGET (button));

  button->swatch = gtk_color_swatch_new ();
  gtk_widget_set_can_focus (button->swatch, FALSE);
  g_object_set (button->swatch, "has-menu", FALSE, NULL);
  layout = gtk_widget_create_pango_layout (GTK_WIDGET (button), "Black");
  pango_layout_get_pixel_extents (layout, NULL, &rect);
  g_object_unref (layout);

  gtk_widget_set_size_request (button->swatch, rect.width, rect.height);

  gtk_button_set_child (GTK_BUTTON (button->button), button->swatch);

  button->title = g_strdup (_("Pick a Color")); /* default title */

  /* Start with opaque black, alpha disabled */
  button->rgba.red = 0;
  button->rgba.green = 0;
  button->rgba.blue = 0;
  button->rgba.alpha = 1;
  button->use_alpha = FALSE;
  button->modal = TRUE;

  dest = gtk_drop_target_new (GDK_TYPE_RGBA, GDK_ACTION_COPY);
  g_signal_connect (dest, "drop", G_CALLBACK (gtk_color_button_drop), button);
  gtk_widget_add_controller (GTK_WIDGET (button), GTK_EVENT_CONTROLLER (dest));

  source = gtk_drag_source_new ();
  g_signal_connect (source, "prepare", G_CALLBACK (gtk_color_button_drag_prepare), button);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (source), GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (button->button, GTK_EVENT_CONTROLLER (source));

  gtk_widget_add_css_class (button->button, "color");
}

static void
gtk_color_button_finalize (GObject *object)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (object);

  if (button->cs_dialog != NULL)
    gtk_widget_destroy (button->cs_dialog);

  g_free (button->title);
  gtk_widget_unparent (button->button);

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

  button->cs_dialog = NULL;

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

      gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog), &button->rgba);
      gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (button->swatch), &button->rgba);

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
  GtkWidget *parent, *dialog;

  if (button->cs_dialog != NULL)
    return;

  parent = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (button)));

  button->cs_dialog = dialog = gtk_color_chooser_dialog_new (button->title, NULL);
  gtk_window_set_hide_on_close (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_modal (GTK_WINDOW (dialog), button->modal);

  if (GTK_IS_WINDOW (parent))
  {
    if (GTK_WINDOW (parent) != gtk_window_get_transient_for (GTK_WINDOW (dialog)))
      gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));

    if (gtk_window_get_modal (GTK_WINDOW (parent)))
      gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
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

  /* if dialog already exists, make sure it's shown and raised */
  ensure_dialog (button);

  g_object_set (button->cs_dialog, "show-editor", button->show_editor, NULL);

  gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (button->cs_dialog), button->use_alpha);

  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (button->cs_dialog), &button->rgba);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_window_present (GTK_WINDOW (button->cs_dialog));
  G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
gtk_color_button_set_rgba (GtkColorChooser *chooser,
                           const GdkRGBA   *rgba)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (chooser);

  g_return_if_fail (GTK_IS_COLOR_BUTTON (chooser));
  g_return_if_fail (rgba != NULL);

  button->rgba = *rgba;
  gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (button->swatch), &button->rgba);

  g_object_notify (G_OBJECT (chooser), "rgba");
}

static void
gtk_color_button_get_rgba (GtkColorChooser *chooser,
                           GdkRGBA        *rgba)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (chooser);

  g_return_if_fail (GTK_IS_COLOR_BUTTON (chooser));
  g_return_if_fail (rgba != NULL);

  *rgba = button->rgba;
}

static void
set_use_alpha (GtkColorButton *button,
               gboolean        use_alpha)
{
  use_alpha = (use_alpha != FALSE);

  if (button->use_alpha != use_alpha)
    {
      button->use_alpha = use_alpha;

      gtk_color_swatch_set_use_alpha (GTK_COLOR_SWATCH (button->swatch), use_alpha);

      g_object_notify (G_OBJECT (button), "use-alpha");
    }
}

/**
 * gtk_color_button_set_title:
 * @button: a #GtkColorButton
 * @title: String containing new window title
 *
 * Sets the title for the color selection dialog.
 */
void
gtk_color_button_set_title (GtkColorButton *button,
                            const gchar    *title)
{
  gchar *old_title;

  g_return_if_fail (GTK_IS_COLOR_BUTTON (button));

  old_title = button->title;
  button->title = g_strdup (title);
  g_free (old_title);

  if (button->cs_dialog)
    gtk_window_set_title (GTK_WINDOW (button->cs_dialog), button->title);

  g_object_notify (G_OBJECT (button), "title");
}

/**
 * gtk_color_button_get_title:
 * @button: a #GtkColorButton
 *
 * Gets the title of the color selection dialog.
 *
 * Returns: An internal string, do not free the return value
 */
const gchar *
gtk_color_button_get_title (GtkColorButton *button)
{
  g_return_val_if_fail (GTK_IS_COLOR_BUTTON (button), NULL);

  return button->title;
}

/**
 * gtk_color_button_set_modal:
 * @button: a #GtkColorButton
 * @modal: %TRUE to make the dialog modal
 *
 * Sets whether the dialog should be modal.
 */
void
gtk_color_button_set_modal (GtkColorButton *button,
                            gboolean        modal)
{
  g_return_if_fail (GTK_IS_COLOR_BUTTON (button));

  if (button->modal == modal)
    return;

  button->modal = modal;

  if (button->cs_dialog)
    gtk_window_set_modal (GTK_WINDOW (button->cs_dialog), button->modal);

  g_object_notify (G_OBJECT (button), "modal");
}

/**
 * gtk_color_button_get_modal:
 * @button: a #GtkColorButton
 *
 * Gets whether the dialog is modal.
 *
 * Returns: %TRUE if the dialog is modal
 */
gboolean
gtk_color_button_get_modal (GtkColorButton *button)
{
  g_return_val_if_fail (GTK_IS_COLOR_BUTTON (button), FALSE);

  return button->modal;
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
        if (button->show_editor != show_editor)
          {
            button->show_editor = show_editor;
            g_object_notify (object, "show-editor");
          }
      }
      break;
    case PROP_MODAL:
      gtk_color_button_set_modal (button, g_value_get_boolean (value));
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
      g_value_set_boolean (value, button->use_alpha);
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
      g_value_set_boolean (value, button->show_editor);
      break;
    case PROP_MODAL:
      g_value_set_boolean (value, button->modal);
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

  gtk_color_chooser_add_palette (GTK_COLOR_CHOOSER (button->cs_dialog),
                                 orientation, colors_per_line, n_colors, colors);
}

static void
gtk_color_button_iface_init (GtkColorChooserInterface *iface)
{
  iface->get_rgba = gtk_color_button_get_rgba;
  iface->set_rgba = gtk_color_button_set_rgba;
  iface->add_palette = gtk_color_button_add_palette;
}

