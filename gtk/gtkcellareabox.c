/* gtkcellarea.c
 *
 * Copyright (C) 2010 Openismus GmbH
 *
 * Authors:
 *      Tristan Van Berkom <tristanvb@openismus.com>
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

#include "gtkorientable.h"
#include "gtkcellareabox.h"

/* GObjectClass */
static void      gtk_cell_area_box_finalize                       (GObject            *object);
static void      gtk_cell_area_box_dispose                        (GObject            *object);
static void      gtk_cell_area_box_set_property                   (GObject            *object,
								   guint               prop_id,
								   const GValue       *value,
								   GParamSpec         *pspec);
static void      gtk_cell_area_box_get_property                   (GObject            *object,
								   guint               prop_id,
								   GValue             *value,
								   GParamSpec         *pspec);

/* GtkCellAreaClass */
static void      gtk_cell_area_box_add                            (GtkCellArea        *area,
								   GtkCellRenderer    *renderer);
static void      gtk_cell_area_box_remove                         (GtkCellArea        *area,
								   GtkCellRenderer    *renderer);
static void      gtk_cell_area_box_forall                         (GtkCellArea        *area,
								   GtkCellCallback     callback,
								   gpointer            callback_data);
static gint      gtk_cell_area_box_event                          (GtkCellArea        *area,
								   GtkWidget          *widget,
								   GdkEvent           *event,
								   const GdkRectangle *cell_area);
static void      gtk_cell_area_box_render                         (GtkCellArea        *area,
								   cairo_t            *cr,
								   GtkWidget          *widget,
								   const GdkRectangle *cell_area);

static void      gtk_cell_area_box_attribute_connect              (GtkCellArea             *area,
								   GtkCellRenderer         *renderer,
								   const gchar             *attribute,
								   gint                     id); 
static void      gtk_cell_area_box_attribute_disconnect           (GtkCellArea             *area,
								   GtkCellRenderer         *renderer,
								   const gchar             *attribute,
								   gint                     id);
static void      gtk_cell_area_box_attribute_forall               (GtkCellArea             *area,
								   GtkCellRenderer         *renderer,
								   GtkCellAttributeCallback callback,
								   gpointer                 user_data);

static GtkSizeRequestMode gtk_cell_area_box_get_request_mode      (GtkCellArea        *area);
static void      gtk_cell_area_box_get_preferred_width            (GtkCellArea        *area,
								   GtkWidget          *widget,
								   gint               *minimum_width,
								   gint               *natural_width);
static void      gtk_cell_area_box_get_preferred_height           (GtkCellArea        *area,
								   GtkWidget          *widget,
								   gint               *minimum_height,
								   gint               *natural_height);
static void      gtk_cell_area_box_get_preferred_height_for_width (GtkCellArea        *area,
								   GtkWidget          *widget,
								   gint                width,
								   gint               *minimum_height,
								   gint               *natural_height);
static void      gtk_cell_area_box_get_preferred_width_for_height (GtkCellArea        *area,
								   GtkWidget          *widget,
								   gint                height,
								   gint               *minimum_width,
								   gint               *natural_width);


struct _GtkCellAreaBoxPrivate
{
  GtkOrientation orientation;


};

enum {
  PROP_0,
  PROP_ORIENTATION
};


G_DEFINE_TYPE_WITH_CODE (GtkCellAreaBox, gtk_cell_area_box, GTK_TYPE_CELL_AREA,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL));


static void
gtk_cell_area_box_init (GtkCellAreaBox *box)
{
  GtkCellAreaBoxPrivate *priv;

  box->priv = G_TYPE_INSTANCE_GET_PRIVATE (box,
                                           GTK_TYPE_CELL_AREA_BOX,
                                           GtkCellAreaBoxPrivate);
  priv = box->priv;

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
}

static void 
gtk_cell_area_box_class_init (GtkCellAreaBoxClass *class)
{
  GObjectClass     *object_class = G_OBJECT_CLASS (class);
  GtkCellAreaClass *area_class   = GTK_CELL_AREA_CLASS (class);

  /* GObjectClass */
  object_class->finalize     = gtk_cell_area_box_finalize;
  object_class->dispose      = gtk_cell_area_box_dispose;
  object_class->set_property = gtk_cell_area_box_set_property;
  object_class->get_property = gtk_cell_area_box_get_property;

  /* GtkCellAreaClass */
  area_class->add                            = gtk_cell_area_box_add;
  area_class->remove                         = gtk_cell_area_box_remove;
  area_class->forall                         = gtk_cell_area_box_forall;
  area_class->event                          = gtk_cell_area_box_event;
  area_class->render                         = gtk_cell_area_box_render;
  
  area_class->attribute_connect              = gtk_cell_area_box_attribute_connect;
  area_class->attribute_disconnect           = gtk_cell_area_box_attribute_disconnect;
  area_class->attribute_forall               = gtk_cell_area_box_attribute_forall;
  
  area_class->get_request_mode               = gtk_cell_area_box_get_request_mode;
  area_class->get_preferred_width            = gtk_cell_area_box_get_preferred_width;
  area_class->get_preferred_height           = gtk_cell_area_box_get_preferred_height;
  area_class->get_preferred_height_for_width = gtk_cell_area_box_get_preferred_height_for_width;
  area_class->get_preferred_width_for_height = gtk_cell_area_box_get_preferred_width_for_height;

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");


  g_type_class_add_private (object_class, sizeof (GtkCellAreaBoxPrivate));
}


/*************************************************************
 *                      GObjectClass                         *
 *************************************************************/
static void
gtk_cell_area_box_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_cell_area_box_parent_class)->finalize (object);
}

static void
gtk_cell_area_box_dispose (GObject *object)
{
  G_OBJECT_CLASS (gtk_cell_area_box_parent_class)->dispose (object);
}

static void
gtk_cell_area_box_set_property (GObject       *object,
				guint          prop_id,
				const GValue  *value,
				GParamSpec    *pspec)
{

}

static void
gtk_cell_area_box_get_property (GObject     *object,
				guint        prop_id,
				GValue      *value,
				GParamSpec  *pspec)
{

}


/*************************************************************
 *                    GtkCellAreaClass                       *
 *************************************************************/
static void      
gtk_cell_area_box_add (GtkCellArea        *area,
		       GtkCellRenderer    *renderer)
{

}

static void
gtk_cell_area_box_remove (GtkCellArea        *area,
			  GtkCellRenderer    *renderer)
{

}

static void
gtk_cell_area_box_forall (GtkCellArea        *area,
			  GtkCellCallback     callback,
			  gpointer            callback_data)
{

}

static gint
gtk_cell_area_box_event (GtkCellArea        *area,
			 GtkWidget          *widget,
			 GdkEvent           *event,
			 const GdkRectangle *cell_area)
{


  return 0;
}

static void
gtk_cell_area_box_render (GtkCellArea        *area,
			  cairo_t            *cr,
			  GtkWidget          *widget,
			  const GdkRectangle *cell_area)
{

}

static void
gtk_cell_area_box_attribute_connect (GtkCellArea             *area,
				     GtkCellRenderer         *renderer,
				     const gchar             *attribute,
				     gint                     id)
{

}

static void
gtk_cell_area_box_attribute_disconnect (GtkCellArea             *area,
					GtkCellRenderer         *renderer,
					const gchar             *attribute,
					gint                     id)
{

}

static void
gtk_cell_area_box_attribute_forall (GtkCellArea             *area,
				    GtkCellRenderer         *renderer,
				    GtkCellAttributeCallback callback,
				    gpointer                 user_data)
{

}


static GtkSizeRequestMode 
gtk_cell_area_box_get_request_mode (GtkCellArea *area)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = box->priv;

  return (priv->orientation) == GTK_ORIENTATION_HORIZONTAL ?
    GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH :
    GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void
gtk_cell_area_box_get_preferred_width (GtkCellArea        *area,
				       GtkWidget          *widget,
				       gint               *minimum_width,
				       gint               *natural_width)
{

}

static void
gtk_cell_area_box_get_preferred_height (GtkCellArea        *area,
					GtkWidget          *widget,
					gint               *minimum_height,
					gint               *natural_height)
{


}

static void
gtk_cell_area_box_get_preferred_height_for_width (GtkCellArea        *area,
						  GtkWidget          *widget,
						  gint                width,
						  gint               *minimum_height,
						  gint               *natural_height)
{

}

static void
gtk_cell_area_box_get_preferred_width_for_height (GtkCellArea        *area,
						  GtkWidget          *widget,
						  gint                height,
						  gint               *minimum_width,
						  gint               *natural_width)
{

}

/*************************************************************
 *                            API                            *
 *************************************************************/
