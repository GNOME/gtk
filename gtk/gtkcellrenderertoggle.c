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
						    GParamSpec                 *pspec,
						    const gchar                *trailer);
static void gtk_cell_renderer_toggle_set_property  (GObject                    *object,
						    guint                       param_id,
						    const GValue               *value,
						    GParamSpec                 *pspec,
						    const gchar                *trailer);
static void gtk_cell_renderer_toggle_init       (GtkCellRendererToggle      *celltext);
static void gtk_cell_renderer_toggle_class_init (GtkCellRendererToggleClass *class);
static void gtk_cell_renderer_toggle_get_size   (GtkCellRenderer            *cell,
						 GtkWidget                  *widget,
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
				       GParamSpec  *pspec,
				       const gchar *trailer)
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
				       GParamSpec   *pspec,
				       const gchar  *trailer)
{
  GtkCellRendererToggle *celltoggle = GTK_CELL_RENDERER_TOGGLE (object);
  
  switch (param_id)
    {
    case PROP_ACTIVE:
      gtk_cell_renderer_toggle_set_active (celltoggle, g_value_get_boolean (value));
      break;
    case PROP_RADIO:
      celltoggle->radio = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

GtkCellRenderer *
gtk_cell_renderer_toggle_new (void)
{
  return GTK_CELL_RENDERER (gtk_type_new (gtk_cell_renderer_toggle_get_type ()));
}

static void
gtk_cell_renderer_toggle_get_size (GtkCellRenderer *cell,
				   GtkWidget       *widget,
				   gint            *width,
				   gint            *height)
{
  if (width)
    *width = (gint) cell->xpad * 2 + TOGGLE_WIDTH;

  if (height)
    *height = (gint) cell->ypad * 2 + TOGGLE_WIDTH;
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
  gint real_xoffset, real_yoffset;
  GtkShadowType shadow;
  GtkStateType state;
  
  width = MIN (TOGGLE_WIDTH, cell_area->width - cell->xpad * 2);
  height = MIN (TOGGLE_WIDTH, cell_area->height - cell->ypad * 2);

  if (width <= 0 || height <= 0)
    return;

  real_xoffset = cell->xalign * (cell_area->width - width - (2 * cell->xpad));
  real_xoffset = MAX (real_xoffset, 0) + cell->xpad;
  real_yoffset = cell->yalign * (cell_area->height - height - (2 * cell->ypad));
  real_yoffset = MAX (real_yoffset, 0) + cell->ypad;

  shadow = celltoggle->active ? GTK_SHADOW_IN : GTK_SHADOW_OUT;

  if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED)
    state = GTK_STATE_SELECTED;
  else
    state = GTK_STATE_NORMAL;
  
  if (celltoggle->radio)
    {
      gtk_paint_option (widget->style,
                        window,
                        state, shadow,
                        cell_area, widget, "cellradio",
                        cell_area->x + real_xoffset,
                        cell_area->y + real_yoffset,
                        width, height);
    }
  else
    {
      gtk_paint_check (widget->style,
                       window,
                       state, shadow,
                       cell_area, widget, "cellcheck",
                       cell_area->x + real_xoffset,
                       cell_area->y + real_yoffset,
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

void
gtk_cell_renderer_toggle_set_radio (GtkCellRendererToggle *toggle,
				    gboolean               radio)
{
  g_return_if_fail (toggle != NULL);
  g_return_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (toggle));

  toggle->radio = radio;
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

  setting = !! setting;

  if (toggle->active != setting)
    {
      toggle->active = setting;
      g_object_notify (G_OBJECT (toggle), "active");
    }
}
