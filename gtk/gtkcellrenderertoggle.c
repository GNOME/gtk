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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <gtk/gtkcellrenderertoggle.h>
#include <gtk/gtksignal.h>
#include "gtkintl.h"

static void gtk_cell_renderer_toggle_get_property  (GObject                    *object,
						    guint                       param_id,
						    GValue                     *value,
						    GParamSpec                 *pspec);
static void gtk_cell_renderer_toggle_set_property  (GObject                    *object,
						    guint                       param_id,
						    const GValue               *value,
						    GParamSpec                 *pspec);
static void gtk_cell_renderer_toggle_init       (GtkCellRendererToggle      *celltext);
static void gtk_cell_renderer_toggle_class_init (GtkCellRendererToggleClass *class);
static void gtk_cell_renderer_toggle_get_size   (GtkCellRenderer            *cell,
						 GtkWidget                  *widget,
 						 GdkRectangle               *cell_area,
						 gint                       *x_offset,
						 gint                       *y_offset,
						 gint                       *width,
						 gint                       *height);
static void gtk_cell_renderer_toggle_render     (GtkCellRenderer            *cell,
						 GdkWindow                  *window,
						 GtkWidget                  *widget,
						 GdkRectangle               *background_area,
						 GdkRectangle               *cell_area,
						 GdkRectangle               *expose_area,
						 guint                       flags);
static gint gtk_cell_renderer_toggle_event      (GtkCellRenderer            *cell,
						 GdkEvent                   *event,
						 GtkWidget                  *widget,
						 gchar                      *path,
						 GdkRectangle               *background_area,
						 GdkRectangle               *cell_area,
						 guint                       flags);


enum {
  TOGGLED,
  LAST_SIGNAL
};

enum {
  PROP_ZERO,
  PROP_ACTIVE,
  PROP_RADIO
};


#define TOGGLE_WIDTH 12

static guint toggle_cell_signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_cell_renderer_toggle_get_type (void)
{
  static GtkType cell_toggle_type = 0;

  if (!cell_toggle_type)
    {
      static const GTypeInfo cell_toggle_info =
      {
	sizeof (GtkCellRendererToggleClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_cell_renderer_toggle_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkCellRendererToggle),
	0,              /* n_preallocs */
	(GInstanceInitFunc) gtk_cell_renderer_toggle_init,
      };

      cell_toggle_type = g_type_register_static (GTK_TYPE_CELL_RENDERER, "GtkCellRendererToggle", &cell_toggle_info, 0);
    }

  return cell_toggle_type;
}

static void
gtk_cell_renderer_toggle_init (GtkCellRendererToggle *celltoggle)
{
  celltoggle->active = FALSE;
  celltoggle->radio = FALSE;
  GTK_CELL_RENDERER (celltoggle)->can_activate = TRUE;
  GTK_CELL_RENDERER (celltoggle)->xpad = 2;
  GTK_CELL_RENDERER (celltoggle)->ypad = 2;
}

static void
gtk_cell_renderer_toggle_class_init (GtkCellRendererToggleClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

  object_class->get_property = gtk_cell_renderer_toggle_get_property;
  object_class->set_property = gtk_cell_renderer_toggle_set_property;

  cell_class->get_size = gtk_cell_renderer_toggle_get_size;
  cell_class->render = gtk_cell_renderer_toggle_render;
  cell_class->event = gtk_cell_renderer_toggle_event;
  
  g_object_class_install_property (object_class,
				   PROP_ACTIVE,
				   g_param_spec_boolean ("active",
							 _("Toggle state"),
							 _("The toggle state of the button"),
							 FALSE,
							 G_PARAM_READABLE |
							 G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_RADIO,
				   g_param_spec_boolean ("radio",
							 _("Radio state"),
							 _("Draw the toggle button as a radio button"),
							 FALSE,
							 G_PARAM_READABLE |
							 G_PARAM_WRITABLE));


  toggle_cell_signals[TOGGLED] =
    gtk_signal_new ("toggled",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkCellRendererToggleClass, toggled),
		    gtk_marshal_VOID__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);
}

static void
gtk_cell_renderer_toggle_get_property (GObject     *object,
				       guint        param_id,
				       GValue      *value,
				       GParamSpec  *pspec)
{
  GtkCellRendererToggle *celltoggle = GTK_CELL_RENDERER_TOGGLE (object);
  
  switch (param_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, celltoggle->active);
      break;
    case PROP_RADIO:
      g_value_set_boolean (value, celltoggle->radio);
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
  
  switch (param_id)
    {
    case PROP_ACTIVE:
      celltoggle->active = g_value_get_boolean (value);
      g_object_notify (G_OBJECT(object), "active");
      break;
    case PROP_RADIO:
      celltoggle->radio = g_value_get_boolean (value);
      g_object_notify (G_OBJECT(object), "radio");
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
 * can bind the "active" property on the cell renderer to a boolean value
 * in the model, thus causing the check button to reflect the state of
 * the model.
 * 
 * Return value: the new cell renderer
 **/
GtkCellRenderer *
gtk_cell_renderer_toggle_new (void)
{
  return GTK_CELL_RENDERER (gtk_type_new (gtk_cell_renderer_toggle_get_type ()));
}

static void
gtk_cell_renderer_toggle_get_size (GtkCellRenderer *cell,
				   GtkWidget       *widget,
				   GdkRectangle    *cell_area,
				   gint            *x_offset,
				   gint            *y_offset,
				   gint            *width,
				   gint            *height)
{
  gint calc_width;
  gint calc_height;

  calc_width = (gint) cell->xpad * 2 + TOGGLE_WIDTH;
  calc_height = (gint) cell->ypad * 2 + TOGGLE_WIDTH;

  if (width)
    *width = calc_width;

  if (height)
    *height = calc_height;

  if (cell_area)
    {
      if (x_offset)
	{
	  *x_offset = cell->xalign * (cell_area->width - calc_width - (2 * cell->xpad));
	  *x_offset = MAX (*x_offset, 0) + cell->xpad;
	}
      if (y_offset)
	{
	  *y_offset = cell->yalign * (cell_area->height - calc_height - (2 * cell->ypad));
	  *y_offset = MAX (*y_offset, 0) + cell->ypad;
	}
    }
}

static void
gtk_cell_renderer_toggle_render (GtkCellRenderer *cell,
				 GdkWindow       *window,
				 GtkWidget       *widget,
				 GdkRectangle    *background_area,
				 GdkRectangle    *cell_area,
				 GdkRectangle    *expose_area,
				 guint            flags)
{
  GtkCellRendererToggle *celltoggle = (GtkCellRendererToggle *) cell;
  gint width, height;
  gint x_offset, y_offset;
  GtkShadowType shadow;
  GtkStateType state;
  
  gtk_cell_renderer_toggle_get_size (cell, widget, cell_area,
				     &x_offset, &y_offset,
				     &width, &height);
  width -= cell->xpad*2;
  height -= cell->ypad*2;

  if (width <= 0 || height <= 0)
    return;

  shadow = celltoggle->active ? GTK_SHADOW_IN : GTK_SHADOW_OUT;

  if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED)
    state = GTK_STATE_SELECTED;
  else if (! cell->can_activate)
    state = GTK_STATE_INSENSITIVE;
  else
    state = GTK_STATE_NORMAL;
  
  if (celltoggle->radio)
    {
      gtk_paint_option (widget->style,
                        window,
                        state, shadow,
                        cell_area, widget, "cellradio",
                        cell_area->x + x_offset,
                        cell_area->y + y_offset,
                        width, height);
    }
  else
    {
      gtk_paint_check (widget->style,
                       window,
                       state, shadow,
                       cell_area, widget, "cellcheck",
                       cell_area->x + x_offset,
                       cell_area->y + y_offset,
                       width, height);
    }
}

static gint
gtk_cell_renderer_toggle_event (GtkCellRenderer *cell,
				GdkEvent        *event,
				GtkWidget       *widget,
				gchar           *path,
				GdkRectangle    *background_area,
				GdkRectangle    *cell_area,
				guint            flags)
{
  GtkCellRendererToggle *celltoggle;
  gint retval = FALSE;
  
  celltoggle = GTK_CELL_RENDERER_TOGGLE (cell);
  
  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      {
        gtk_signal_emit (GTK_OBJECT (cell), toggle_cell_signals[TOGGLED], path);
        retval = TRUE;
      }
      break;

    default:
      break;
    }
      
  return retval;
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
  g_return_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (toggle));

  toggle->radio = radio;
}

/**
 * gtk_cell_renderer_toggle_get_radio:
 * @toggle: a #GtkCellRendererToggle
 * 
 * Return value: %TRUE if we're rendering radio toggles rather than checkboxes
 **/
gboolean
gtk_cell_renderer_toggle_get_radio (GtkCellRendererToggle *toggle)
{
  g_return_val_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (toggle), FALSE);

  return toggle->radio;
}

gboolean
gtk_cell_renderer_toggle_get_active (GtkCellRendererToggle *toggle)
{
  g_return_val_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (toggle), FALSE);

  return toggle->active;
}

void
gtk_cell_renderer_toggle_set_active (GtkCellRendererToggle *toggle,
				     gboolean               setting)
{
  g_return_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (toggle));

  g_object_set (G_OBJECT (toggle), "active", !! setting, NULL);
}
