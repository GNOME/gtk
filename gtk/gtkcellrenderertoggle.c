/* gtkcellrenderertoggle.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcellrenderertoggle.h"

#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkrendericonprivate.h"
#include "gtksnapshot.h"
#include "gtkstylecontextprivate.h"
#include "gtktreeprivate.h"

#include "a11y/gtkbooleancellaccessible.h"

#include <stdlib.h>

/**
 * SECTION:gtkcellrenderertoggle
 * @Short_description: Renders a toggle button in a cell
 * @Title: GtkCellRendererToggle
 *
 * #GtkCellRendererToggle renders a toggle button in a cell. The
 * button is drawn as a radio or a checkbutton, depending on the
 * #GtkCellRendererToggle:radio property.
 * When activated, it emits the #GtkCellRendererToggle::toggled signal.
 */


static void gtk_cell_renderer_toggle_get_property  (GObject                    *object,
						    guint                       param_id,
						    GValue                     *value,
						    GParamSpec                 *pspec);
static void gtk_cell_renderer_toggle_set_property  (GObject                    *object,
						    guint                       param_id,
						    const GValue               *value,
						    GParamSpec                 *pspec);
static void gtk_cell_renderer_toggle_get_size   (GtkCellRenderer            *cell,
						 GtkWidget                  *widget,
						 const GdkRectangle         *cell_area,
						 gint                       *x_offset,
						 gint                       *y_offset,
						 gint                       *width,
						 gint                       *height);
static void gtk_cell_renderer_toggle_snapshot   (GtkCellRenderer            *cell,
						 GtkSnapshot                *snapshot,
						 GtkWidget                  *widget,
						 const GdkRectangle         *background_area,
						 const GdkRectangle         *cell_area,
						 GtkCellRendererState        flags);
static gboolean gtk_cell_renderer_toggle_activate  (GtkCellRenderer            *cell,
						    GdkEvent                   *event,
						    GtkWidget                  *widget,
						    const gchar                *path,
						    const GdkRectangle         *background_area,
						    const GdkRectangle         *cell_area,
						    GtkCellRendererState        flags);


enum {
  TOGGLED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ACTIVATABLE,
  PROP_ACTIVE,
  PROP_RADIO,
  PROP_INCONSISTENT
};

#define TOGGLE_WIDTH 16

static guint toggle_cell_signals[LAST_SIGNAL] = { 0 };

typedef struct _GtkCellRendererTogglePrivate       GtkCellRendererTogglePrivate;
typedef struct _GtkCellRendererToggleClass         GtkCellRendererToggleClass;

struct _GtkCellRendererToggle
{
  GtkCellRenderer parent;
};

struct _GtkCellRendererToggleClass
{
  GtkCellRendererClass parent_class;

  void (* toggled) (GtkCellRendererToggle *cell,
                    const char            *path);
};

struct _GtkCellRendererTogglePrivate
{
  guint active       : 1;
  guint activatable  : 1;
  guint inconsistent : 1;
  guint radio        : 1;
};


G_DEFINE_TYPE_WITH_PRIVATE (GtkCellRendererToggle, gtk_cell_renderer_toggle, GTK_TYPE_CELL_RENDERER)


static void
gtk_cell_renderer_toggle_init (GtkCellRendererToggle *celltoggle)
{
  GtkCellRendererTogglePrivate *priv = gtk_cell_renderer_toggle_get_instance_private (celltoggle);

  priv->activatable = TRUE;
  priv->active = FALSE;
  priv->radio = FALSE;

  g_object_set (celltoggle, "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);
  gtk_cell_renderer_set_padding (GTK_CELL_RENDERER (celltoggle), 2, 2);

  priv->inconsistent = FALSE;
}

static void
gtk_cell_renderer_toggle_class_init (GtkCellRendererToggleClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

  object_class->get_property = gtk_cell_renderer_toggle_get_property;
  object_class->set_property = gtk_cell_renderer_toggle_set_property;

  cell_class->get_size = gtk_cell_renderer_toggle_get_size;
  cell_class->snapshot = gtk_cell_renderer_toggle_snapshot;
  cell_class->activate = gtk_cell_renderer_toggle_activate;
  
  g_object_class_install_property (object_class,
				   PROP_ACTIVE,
				   g_param_spec_boolean ("active",
							 P_("Toggle state"),
							 P_("The toggle state of the button"),
							 FALSE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
		                   PROP_INCONSISTENT,
				   g_param_spec_boolean ("inconsistent",
					                 P_("Inconsistent state"),
							 P_("The inconsistent state of the button"),
							 FALSE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  
  g_object_class_install_property (object_class,
				   PROP_ACTIVATABLE,
				   g_param_spec_boolean ("activatable",
							 P_("Activatable"),
							 P_("The toggle button can be activated"),
							 TRUE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
				   PROP_RADIO,
				   g_param_spec_boolean ("radio",
							 P_("Radio state"),
							 P_("Draw the toggle button as a radio button"),
							 FALSE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  
  /**
   * GtkCellRendererToggle::toggled:
   * @cell_renderer: the object which received the signal
   * @path: string representation of #GtkTreePath describing the 
   *        event location
   *
   * The ::toggled signal is emitted when the cell is toggled. 
   *
   * It is the responsibility of the application to update the model
   * with the correct value to store at @path.  Often this is simply the
   * opposite of the value currently stored at @path.
   **/
  toggle_cell_signals[TOGGLED] =
    g_signal_new (I_("toggled"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkCellRendererToggleClass, toggled),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1,
		  G_TYPE_STRING);

  gtk_cell_renderer_class_set_accessible_type (cell_class, GTK_TYPE_BOOLEAN_CELL_ACCESSIBLE);
}

static void
gtk_cell_renderer_toggle_get_property (GObject     *object,
				       guint        param_id,
				       GValue      *value,
				       GParamSpec  *pspec)
{
  GtkCellRendererToggle *celltoggle = GTK_CELL_RENDERER_TOGGLE (object);
  GtkCellRendererTogglePrivate *priv = gtk_cell_renderer_toggle_get_instance_private (celltoggle);

  switch (param_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, priv->active);
      break;
    case PROP_INCONSISTENT:
      g_value_set_boolean (value, priv->inconsistent);
      break;
    case PROP_ACTIVATABLE:
      g_value_set_boolean (value, priv->activatable);
      break;
    case PROP_RADIO:
      g_value_set_boolean (value, priv->radio);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}


static void
gtk_cell_renderer_toggle_set_property (GObject      *object,
				       guint         param_id,
				       const GValue *value,
				       GParamSpec   *pspec)
{
  GtkCellRendererToggle *celltoggle = GTK_CELL_RENDERER_TOGGLE (object);
  GtkCellRendererTogglePrivate *priv = gtk_cell_renderer_toggle_get_instance_private (celltoggle);

  switch (param_id)
    {
    case PROP_ACTIVE:
      if (priv->active != g_value_get_boolean (value))
        {
          priv->active = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_INCONSISTENT:
      if (priv->inconsistent != g_value_get_boolean (value))
        {
          priv->inconsistent = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_ACTIVATABLE:
      if (priv->activatable != g_value_get_boolean (value))
        {
          priv->activatable = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_RADIO:
      if (priv->radio != g_value_get_boolean (value))
        {
          priv->radio = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

/**
 * gtk_cell_renderer_toggle_new:
 *
 * Creates a new #GtkCellRendererToggle. Adjust rendering
 * parameters using object properties. Object properties can be set
 * globally (with g_object_set()). Also, with #GtkTreeViewColumn, you
 * can bind a property to a value in a #GtkTreeModel. For example, you
 * can bind the “active” property on the cell renderer to a boolean value
 * in the model, thus causing the check button to reflect the state of
 * the model.
 *
 * Returns: the new cell renderer
 **/
GtkCellRenderer *
gtk_cell_renderer_toggle_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_TOGGLE, NULL);
}

static GtkStyleContext *
gtk_cell_renderer_toggle_save_context (GtkCellRenderer *cell,
				       GtkWidget       *widget)
{
  GtkCellRendererTogglePrivate *priv = gtk_cell_renderer_toggle_get_instance_private (GTK_CELL_RENDERER_TOGGLE (cell));

  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  if (priv->radio)
    gtk_style_context_save_named (context, "radio");
  else
    gtk_style_context_save_named (context, "check");

  return context;
}
 
static void
calc_indicator_size (GtkStyleContext *context,
                     gint            *width,
                     gint            *height)
{
  gtk_style_context_get (context, 
                         "min-width", width,
                         "min-height", height,
                         NULL);

  if (*width == 0)
    *width = TOGGLE_WIDTH;
  if (*height == 0)
    *height = TOGGLE_WIDTH;
}

static void
gtk_cell_renderer_toggle_get_size (GtkCellRenderer    *cell,
				   GtkWidget          *widget,
				   const GdkRectangle *cell_area,
				   gint               *x_offset,
				   gint               *y_offset,
				   gint               *width,
				   gint               *height)
{
  gint calc_width;
  gint calc_height;
  gint xpad, ypad;
  GtkStyleContext *context;
  GtkBorder border, padding;

  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);

  context = gtk_cell_renderer_toggle_save_context (cell, widget);
  gtk_style_context_get_padding (context, &padding);
  gtk_style_context_get_border (context, &border);

  calc_indicator_size (context, &calc_width, &calc_height);
  calc_width += xpad * 2 + padding.left + padding.right + border.left + border.right;
  calc_height += ypad * 2 + padding.top + padding.bottom + border.top + border.bottom;

  gtk_style_context_restore (context);

  if (width)
    *width = calc_width;

  if (height)
    *height = calc_height;

  if (cell_area)
    {
      gfloat xalign, yalign;

      gtk_cell_renderer_get_alignment (cell, &xalign, &yalign);

      if (x_offset)
	{
	  *x_offset = ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ?
		       (1.0 - xalign) : xalign) * (cell_area->width - calc_width);
	  *x_offset = MAX (*x_offset, 0);
	}
      if (y_offset)
	{
	  *y_offset = yalign * (cell_area->height - calc_height);
	  *y_offset = MAX (*y_offset, 0);
	}
    }
  else
    {
      if (x_offset) *x_offset = 0;
      if (y_offset) *y_offset = 0;
    }
}

static void
gtk_cell_renderer_toggle_snapshot (GtkCellRenderer      *cell,
				   GtkSnapshot          *snapshot,
                                   GtkWidget            *widget,
                                   const GdkRectangle   *background_area,
                                   const GdkRectangle   *cell_area,
                                   GtkCellRendererState  flags)
{
  GtkCellRendererToggle *celltoggle = GTK_CELL_RENDERER_TOGGLE (cell);
  GtkCellRendererTogglePrivate *priv = gtk_cell_renderer_toggle_get_instance_private (celltoggle);
  GtkStyleContext *context;
  gint width, height;
  gint x_offset, y_offset;
  gint xpad, ypad;
  GtkStateFlags state;
  GtkBorder padding, border;
  GtkCssImageBuiltinType image_type;

  gtk_cell_renderer_toggle_get_size (cell, widget, cell_area,
				     &x_offset, &y_offset,
				     &width, &height);
  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);
  width -= xpad * 2;
  height -= ypad * 2;

  if (width <= 0 || height <= 0)
    return;

  state = gtk_cell_renderer_get_state (cell, widget, flags);

  if (!priv->activatable)
    state |= GTK_STATE_FLAG_INSENSITIVE;

  state &= ~(GTK_STATE_FLAG_INCONSISTENT | GTK_STATE_FLAG_CHECKED);

  if (priv->inconsistent)
    state |= GTK_STATE_FLAG_INCONSISTENT;
  
  if (priv->active)
    state |= GTK_STATE_FLAG_CHECKED;

  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT (
                             cell_area->x, cell_area->y,
                             cell_area->width, cell_area->height
                          ));

  context = gtk_cell_renderer_toggle_save_context (cell, widget);
  gtk_style_context_set_state (context, state);

  gtk_snapshot_render_background (snapshot, context,
                                  cell_area->x + x_offset + xpad,
                                  cell_area->y + y_offset + ypad,
                                  width, height);
  gtk_snapshot_render_frame (snapshot, context,
                             cell_area->x + x_offset + xpad,
                             cell_area->y + y_offset + ypad,
                             width, height);

  gtk_style_context_get_padding (context, &padding);
  gtk_style_context_get_border (context, &border);

  if (priv->radio)
    {
      if (state & GTK_STATE_FLAG_INCONSISTENT)
        image_type = GTK_CSS_IMAGE_BUILTIN_OPTION_INCONSISTENT;
      else if (state & GTK_STATE_FLAG_CHECKED)
        image_type = GTK_CSS_IMAGE_BUILTIN_OPTION;
      else
        image_type = GTK_CSS_IMAGE_BUILTIN_NONE;
    }
  else
    {
      if (state & GTK_STATE_FLAG_INCONSISTENT)
        image_type = GTK_CSS_IMAGE_BUILTIN_CHECK_INCONSISTENT;
      else if (state & GTK_STATE_FLAG_CHECKED)
        image_type = GTK_CSS_IMAGE_BUILTIN_CHECK;
      else
        image_type = GTK_CSS_IMAGE_BUILTIN_NONE;
    }

  gtk_snapshot_translate (snapshot,
                          &GRAPHENE_POINT_INIT (cell_area->x + x_offset + xpad + padding.left + border.left,
                                                cell_area->y + y_offset + ypad + padding.top + border.top));
  gtk_css_style_snapshot_icon (gtk_style_context_lookup_style (context), snapshot,
                               width - padding.left - padding.right - border.left - border.right,
                               height - padding.top - padding.bottom - border.top - border.bottom,
                               image_type);

  gtk_style_context_restore (context);
  gtk_snapshot_pop (snapshot);
}

static gint
gtk_cell_renderer_toggle_activate (GtkCellRenderer      *cell,
				   GdkEvent             *event,
				   GtkWidget            *widget,
				   const gchar          *path,
				   const GdkRectangle   *background_area,
				   const GdkRectangle   *cell_area,
				   GtkCellRendererState  flags)
{
  GtkCellRendererToggle *celltoggle = GTK_CELL_RENDERER_TOGGLE (cell);
  GtkCellRendererTogglePrivate *priv = gtk_cell_renderer_toggle_get_instance_private (celltoggle);

  if (priv->activatable)
    {
      g_signal_emit (cell, toggle_cell_signals[TOGGLED], 0, path);
      return TRUE;
    }

  return FALSE;
}

/**
 * gtk_cell_renderer_toggle_set_radio:
 * @toggle: a #GtkCellRendererToggle
 * @radio: %TRUE to make the toggle look like a radio button
 * 
 * If @radio is %TRUE, the cell renderer renders a radio toggle
 * (i.e. a toggle in a group of mutually-exclusive toggles).
 * If %FALSE, it renders a check toggle (a standalone boolean option).
 * This can be set globally for the cell renderer, or changed just
 * before rendering each cell in the model (for #GtkTreeView, you set
 * up a per-row setting using #GtkTreeViewColumn to associate model
 * columns with cell renderer properties).
 **/
void
gtk_cell_renderer_toggle_set_radio (GtkCellRendererToggle *toggle,
				    gboolean               radio)
{
  GtkCellRendererTogglePrivate *priv = gtk_cell_renderer_toggle_get_instance_private (toggle);

  g_return_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (toggle));

  priv->radio = radio;
}

/**
 * gtk_cell_renderer_toggle_get_radio:
 * @toggle: a #GtkCellRendererToggle
 *
 * Returns whether we’re rendering radio toggles rather than checkboxes. 
 * 
 * Returns: %TRUE if we’re rendering radio toggles rather than checkboxes
 **/
gboolean
gtk_cell_renderer_toggle_get_radio (GtkCellRendererToggle *toggle)
{
  GtkCellRendererTogglePrivate *priv = gtk_cell_renderer_toggle_get_instance_private (toggle);

  g_return_val_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (toggle), FALSE);

  return priv->radio;
}

/**
 * gtk_cell_renderer_toggle_get_active:
 * @toggle: a #GtkCellRendererToggle
 *
 * Returns whether the cell renderer is active. See
 * gtk_cell_renderer_toggle_set_active().
 *
 * Returns: %TRUE if the cell renderer is active.
 **/
gboolean
gtk_cell_renderer_toggle_get_active (GtkCellRendererToggle *toggle)
{
  GtkCellRendererTogglePrivate *priv = gtk_cell_renderer_toggle_get_instance_private (toggle);

  g_return_val_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (toggle), FALSE);

  return priv->active;
}

/**
 * gtk_cell_renderer_toggle_set_active:
 * @toggle: a #GtkCellRendererToggle.
 * @setting: the value to set.
 *
 * Activates or deactivates a cell renderer.
 **/
void
gtk_cell_renderer_toggle_set_active (GtkCellRendererToggle *toggle,
				     gboolean               setting)
{
  g_return_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (toggle));

  g_object_set (toggle, "active", setting ? TRUE : FALSE, NULL);
}

/**
 * gtk_cell_renderer_toggle_get_activatable:
 * @toggle: a #GtkCellRendererToggle
 *
 * Returns whether the cell renderer is activatable. See
 * gtk_cell_renderer_toggle_set_activatable().
 *
 * Returns: %TRUE if the cell renderer is activatable.
 **/
gboolean
gtk_cell_renderer_toggle_get_activatable (GtkCellRendererToggle *toggle)
{
  GtkCellRendererTogglePrivate *priv = gtk_cell_renderer_toggle_get_instance_private (toggle);

  g_return_val_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (toggle), FALSE);

  return priv->activatable;
}

/**
 * gtk_cell_renderer_toggle_set_activatable:
 * @toggle: a #GtkCellRendererToggle.
 * @setting: the value to set.
 *
 * Makes the cell renderer activatable.
 **/
void
gtk_cell_renderer_toggle_set_activatable (GtkCellRendererToggle *toggle,
                                          gboolean               setting)
{
  GtkCellRendererTogglePrivate *priv = gtk_cell_renderer_toggle_get_instance_private (toggle);

  g_return_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (toggle));

  if (priv->activatable != setting)
    {
      priv->activatable = setting ? TRUE : FALSE;
      g_object_notify (G_OBJECT (toggle), "activatable");
    }
}
