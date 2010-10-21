


#include "gtkcellarea.h"



G_DEFINE_ABSTRACT_TYPE (GtkCellArea, gtk_cell_area, G_TYPE_INITIALLY_UNOWNED);



/* Basic methods */
void
gtk_cell_area_add (GtkCellArea        *area,
		   GtkCellRenderer    *renderer)
{
  GtkCellAreaClass *klass;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->add)
    klass->add (area, renderer);
  else
    g_warning ("GtkCellAreaClass::add not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void
gtk_cell_area_remove (GtkCellArea        *area,
		      GtkCellRenderer    *renderer)
{
  GtkCellAreaClass *klass;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->remove)
    klass->remove (area, renderer);
  else
    g_warning ("GtkCellAreaClass::remove not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void
gtk_cell_area_forall (GtkCellArea        *area,
		      GtkCellCallback     callback,
		      gpointer            callback_data)
{
  GtkCellAreaClass *klass;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (callback != NULL);

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->forall)
    klass->forall (area, callback, callback_data);
  else
    g_warning ("GtkCellAreaClass::forall not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void
gtk_cell_area_apply_attribute (GtkCellArea        *area,
			       gint                attribute,
			       GValue             *value)
{
  GtkCellAreaClass *klass;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (G_IS_VALUE (value));

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->forall)
    klass->apply_attribute (area, attribute, value);
  else
    g_warning ("GtkCellAreaClass::apply_attribute not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));

}

gint
gtk_cell_area_event (GtkCellArea        *area,
		     GtkWidget          *widget,
		     GdkEvent           *event,
		     const GdkRectangle *cell_area)
{
  GtkCellAreaClass *klass;

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  g_return_val_if_fail (event != NULL, 0);
  g_return_val_if_fail (cell_area != NULL, 0);

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->forall)
    klass->apply_attribute (area, attribute, value);
  else
    g_warning ("GtkCellAreaClass::apply_attribute not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void
gtk_cell_area_render (GtkCellArea        *area,
		      cairo_t            *cr,
		      GtkWidget          *widget,
		      const GdkRectangle *cell_area)
{

}


/* Geometry */
GtkSizeRequestMode 
gtk_cell_area_get_request_mode (GtkCellArea        *area)
{

}

void
gtk_cell_area_get_preferred_width (GtkCellArea        *area,
				   GtkWidget          *widget,
				   gint               *minimum_size,
				   gint               *natural_size)
{


}

void
gtk_cell_area_get_preferred_height_for_width (GtkCellArea        *area,
					      GtkWidget          *widget,
					      gint                width,
					      gint               *minimum_height,
					      gint               *natural_height)
{

}

void
gtk_cell_area_get_preferred_height (GtkCellArea        *area,
				    GtkWidget          *widget,
				    gint               *minimum_size,
				    gint               *natural_size)
{


}

void
gtk_cell_area_get_preferred_width_for_height (GtkCellArea        *area,
					      GtkWidget          *widget,
					      gint                height,
					      gint               *minimum_width,
					      gint               *natural_width)
{

}

void
gtk_cell_area_get_preferred_size (GtkCellArea        *area,
				  GtkWidget          *widget,
				  GtkRequisition     *minimum_size,
				  GtkRequisition     *natural_size)
{

}
