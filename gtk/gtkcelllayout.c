/* gtkcelllayout.c
 * Copyright (C) 2003  Kristian Rietveld  <kris@gtk.org>
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

/**
 * SECTION:gtkcelllayout
 * @Short_Description: An interface for packing cells
 * @Title: GtkCellLayout
 *
 * #GtkCellLayout is an interface to be implemented by all objects which
 * want to provide a #GtkTreeViewColumn-like API for packing cells, setting
 * attributes and data funcs.
 *
 * One of the notable features provided by implementations of GtkCellLayout
 * are <emphasis>attributes</emphasis>. Attributes let you set the properties
 * in flexible ways. They can just be set to constant values like regular
 * properties. But they can also be mapped to a column of the underlying
 * tree model with gtk_cell_layout_set_attributes(), which means that the value
 * of the attribute can change from cell to cell as they are rendered by the
 * cell renderer. Finally, it is possible to specify a function with
 * gtk_cell_layout_set_cell_data_func() that is called to determine the value
 * of the attribute for each cell that is rendered.
 *
 * <refsect2 id="GtkCellLayout-BUILDER-UI">
 * <title>GtkCellLayouts as GtkBuildable</title>
 * <para>
 * Implementations of GtkCellLayout which also implement the GtkBuildable
 * interface (#GtkCellView, #GtkIconView, #GtkComboBox, #GtkComboBoxEntry,
 * #GtkEntryCompletion, #GtkTreeViewColumn) accept GtkCellRenderer objects
 * as &lt;child&gt; elements in UI definitions. They support a custom
 * &lt;attributes&gt; element for their children, which can contain
 * multiple &lt;attribute&gt; elements. Each &lt;attribute&gt; element has
 * a name attribute which specifies a property of the cell renderer; the
 * content of the element is the attribute value.
 *
 * <example>
 * <title>A UI definition fragment specifying attributes</title>
 * <programlisting><![CDATA[
 * <object class="GtkCellView">
 *   <child>
 *     <object class="GtkCellRendererText"/>
 *     <attributes>
 *       <attribute name="text">0</attribute>
 *     </attributes>
 *   </child>"
 * </object>
 * ]]></programlisting>
 * </example>
 *
 * Furthermore for implementations of GtkCellLayout that use a #GtkCellArea
 * to lay out cells (all GtkCellLayouts in GTK+ use a GtkCellArea)
 * <link linkend="cell-properties">cell properties</link> can also be defined
 * in the format by specifying the custom &lt;cell-packing&gt; attribute which
 * can contain multiple &lt;property&gt; elements defined in the normal way.
 * <example>
 * <title>A UI definition fragment specifying cell properties</title>
 * <programlisting><![CDATA[
 * <object class="GtkTreeViewColumn">
 *   <child>
 *     <object class="GtkCellRendererText"/>
 *     <cell-packing>
 *       <property name="align">True</property>
 *       <property name="expand">False</property>
 *     </cell-packing>
 *   </child>"
 * </object>
 * ]]></programlisting>
 * </example>
 * </para>
 * </refsect2>
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "gtkcelllayout.h"
#include "gtkbuilderprivate.h"
#include "gtkintl.h"


typedef GtkCellLayoutIface GtkCellLayoutInterface;
G_DEFINE_INTERFACE (GtkCellLayout, gtk_cell_layout, G_TYPE_OBJECT);


static void
gtk_cell_layout_default_init (GtkCellLayoutInterface *iface)
{
}

/**
 * gtk_cell_layout_pack_start:
 * @cell_layout: a #GtkCellLayout
 * @cell: a #GtkCellRenderer
 * @expand: %TRUE if @cell is to be given extra space allocated to @cell_layout
 *
 * Packs the @cell into the beginning of @cell_layout. If @expand is %FALSE,
 * then the @cell is allocated no more space than it needs. Any unused space
 * is divided evenly between cells for which @expand is %TRUE.
 *
 * Note that reusing the same cell renderer is not supported.
 *
 * Since: 2.4
 */
void
gtk_cell_layout_pack_start (GtkCellLayout   *cell_layout,
                            GtkCellRenderer *cell,
                            gboolean         expand)
{
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  if (iface->pack_start)
    iface->pack_start (cell_layout, cell, expand);
  else
    {
      area = iface->get_area (cell_layout);

      if (area)
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (area), cell, expand);
    }
}

/**
 * gtk_cell_layout_pack_end:
 * @cell_layout: a #GtkCellLayout
 * @cell: a #GtkCellRenderer
 * @expand: %TRUE if @cell is to be given extra space allocated to @cell_layout
 *
 * Adds the @cell to the end of @cell_layout. If @expand is %FALSE, then the
 * @cell is allocated no more space than it needs. Any unused space is
 * divided evenly between cells for which @expand is %TRUE.
 *
 * Note that reusing the same cell renderer is not supported.
 *
 * Since: 2.4
 */
void
gtk_cell_layout_pack_end (GtkCellLayout   *cell_layout,
                          GtkCellRenderer *cell,
                          gboolean         expand)
{
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  if (iface->pack_end)
    iface->pack_end (cell_layout, cell, expand);
  else
    {
      area = iface->get_area (cell_layout);

      if (area)
	gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (area), cell, expand);
    }
}

/**
 * gtk_cell_layout_clear:
 * @cell_layout: a #GtkCellLayout
 *
 * Unsets all the mappings on all renderers on @cell_layout and
 * removes all renderers from @cell_layout.
 *
 * Since: 2.4
 */
void
gtk_cell_layout_clear (GtkCellLayout *cell_layout)
{
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  if (iface->clear)
    iface->clear (cell_layout);
  else
    {
      area = iface->get_area (cell_layout);

      if (area)
	gtk_cell_layout_clear (GTK_CELL_LAYOUT (area));
    }
}

static void
gtk_cell_layout_set_attributesv (GtkCellLayout   *cell_layout,
                                 GtkCellRenderer *cell,
                                 va_list          args)
{
  gchar *attribute;
  gint column;
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  attribute = va_arg (args, gchar *);

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  if (iface->get_area)
    area = iface->get_area (cell_layout);

  if (iface->clear_attributes)
    iface->clear_attributes (cell_layout, cell);
  else if (area)
    gtk_cell_layout_clear_attributes (GTK_CELL_LAYOUT (area), cell);

  while (attribute != NULL)
    {
      column = va_arg (args, gint);
      if (iface->add_attribute)
	iface->add_attribute (cell_layout, cell, attribute, column);
      else if (area)
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (area), cell, attribute, column);

      attribute = va_arg (args, gchar *);
    }
}

/**
 * gtk_cell_layout_set_attributes:
 * @cell_layout: a #GtkCellLayout
 * @cell: a #GtkCellRenderer
 * @Varargs: a %NULL-terminated list of attributes
 *
 * Sets the attributes in list as the attributes of @cell_layout.
 *
 * The attributes should be in attribute/column order, as in
 * gtk_cell_layout_add_attribute(). All existing attributes are
 * removed, and replaced with the new attributes.
 *
 * Since: 2.4
 */
void
gtk_cell_layout_set_attributes (GtkCellLayout   *cell_layout,
                                GtkCellRenderer *cell,
                                ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  va_start (args, cell);
  gtk_cell_layout_set_attributesv (cell_layout, cell, args);
  va_end (args);
}

/**
 * gtk_cell_layout_add_attribute:
 * @cell_layout: a #GtkCellLayout
 * @cell: a #GtkCellRenderer
 * @attribute: an attribute on the renderer
 * @column: the column position on the model to get the attribute from
 *
 * Adds an attribute mapping to the list in @cell_layout.
 *
 * The @column is the column of the model to get a value from, and the
 * @attribute is the parameter on @cell to be set from the value. So for
 * example if column 2 of the model contains strings, you could have the
 * "text" attribute of a #GtkCellRendererText get its values from column 2.
 *
 * Since: 2.4
 */
void
gtk_cell_layout_add_attribute (GtkCellLayout   *cell_layout,
                               GtkCellRenderer *cell,
                               const gchar     *attribute,
                               gint             column)
{
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (attribute != NULL);
  g_return_if_fail (column >= 0);

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  if (iface->add_attribute)
    iface->add_attribute (cell_layout,
			  cell,
			  attribute,
			  column);
  else
    {
      area = iface->get_area (cell_layout);

      if (area)
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (area), cell, attribute, column);
    }
}

/**
 * gtk_cell_layout_set_cell_data_func:
 * @cell_layout: a #GtkCellLayout
 * @cell: a #GtkCellRenderer
 * @func (allow-none: the #GtkCellLayoutDataFunc to use, or %NULL
 * @func_data: user data for @func
 * @destroy: destroy notify for @func_data
 *
 * Sets the #GtkCellLayoutDataFunc to use for @cell_layout.
 *
 * This function is used instead of the standard attributes mapping
 * for setting the column value, and should set the value of @cell_layout's
 * cell renderer(s) as appropriate.
 *
 * @func may be %NULL to remove a previously set function.
 *
 * Since: 2.4
 */
void
gtk_cell_layout_set_cell_data_func (GtkCellLayout         *cell_layout,
                                    GtkCellRenderer       *cell,
                                    GtkCellLayoutDataFunc  func,
                                    gpointer               func_data,
                                    GDestroyNotify         destroy)
{
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  if (iface->set_cell_data_func)
    iface->set_cell_data_func (cell_layout,
			       cell,
			       func,
			       func_data,
			       destroy);
  else
    {
      area = iface->get_area (cell_layout);

      if (area)
	{
	  /* Ensure that the correct proxy object is sent to 'func' */
	  if (GTK_CELL_LAYOUT (area) != cell_layout)
	    _gtk_cell_area_set_cell_data_func_with_proxy (area, cell, 
							  (GFunc)func, func_data, destroy, 
							  cell_layout);
	  else
	    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (area), cell, 
						func, func_data, destroy);
	}
    }
}

/**
 * gtk_cell_layout_clear_attributes:
 * @cell_layout: a #GtkCellLayout
 * @cell: a #GtkCellRenderer to clear the attribute mapping on
 *
 * Clears all existing attributes previously set with
 * gtk_cell_layout_set_attributes().
 *
 * Since: 2.4
 */
void
gtk_cell_layout_clear_attributes (GtkCellLayout   *cell_layout,
                                  GtkCellRenderer *cell)
{
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  if (iface->clear_attributes)
    iface->clear_attributes (cell_layout, cell);
  else
    {
      area = iface->get_area (cell_layout);
      
      if (area)
	gtk_cell_layout_clear_attributes (GTK_CELL_LAYOUT (area), cell);
    }
}

/**
 * gtk_cell_layout_reorder:
 * @cell_layout: a #GtkCellLayout
 * @cell: a #GtkCellRenderer to reorder
 * @position: new position to insert @cell at
 *
 * Re-inserts @cell at @position.
 *
 * Note that @cell has already to be packed into @cell_layout
 * for this to function properly.
 *
 * Since: 2.4
 */
void
gtk_cell_layout_reorder (GtkCellLayout   *cell_layout,
                         GtkCellRenderer *cell,
                         gint             position)
{
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  if (iface->reorder)
    iface->reorder (cell_layout, cell, position);
  else
    {
      area = iface->get_area (cell_layout);
      
      if (area)
	gtk_cell_layout_reorder (GTK_CELL_LAYOUT (area), cell, position);
    }
}

/**
 * gtk_cell_layout_get_cells:
 * @cell_layout: a #GtkCellLayout
 *
 * Returns the cell renderers which have been added to @cell_layout.
 *
 * Return value: (element-type GtkCellRenderer) (transfer container):
 *     a list of cell renderers. The list, but not the renderers has
 *     been newly allocated and should be freed with g_list_free()
 *     when no longer needed.
 *
 * Since: 2.12
 */
GList *
gtk_cell_layout_get_cells (GtkCellLayout *cell_layout)
{
  GtkCellLayoutIface *iface;
  GtkCellArea        *area;

  g_return_val_if_fail (GTK_IS_CELL_LAYOUT (cell_layout), NULL);

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);  
  if (iface->get_cells)
    return iface->get_cells (cell_layout);
  else
    {
      area = iface->get_area (cell_layout);
      
      if (area)
	return gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (area));
    }

  return NULL;
}

/**
 * gtk_cell_layout_get_area:
 * @cell_layout: a #GtkCellLayout
 *
 * Returns the underlying #GtkCellArea which might be @cell_layout
 * if called on a #GtkCellArea or might be %NULL if no #GtkCellArea
 * is used by @cell_layout.
 *
 * Return value: (transfer none): a list of cell renderers. The list,
 *     but not the renderers has been newly allocated and should be
 *     freed with g_list_free() when no longer needed.
 *
 * Since: 3.0
 */
GtkCellArea *
gtk_cell_layout_get_area (GtkCellLayout *cell_layout)
{
  GtkCellLayoutIface *iface;

  g_return_val_if_fail (GTK_IS_CELL_LAYOUT (cell_layout), NULL);

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);  
  if (iface->get_area)
    return iface->get_area (cell_layout);

  return NULL;
}

/* Attribute parsing */
typedef struct {
  GtkCellLayout   *cell_layout;
  GtkCellRenderer *renderer;
  gchar           *attr_name;
} AttributesSubParserData;

static void
attributes_start_element (GMarkupParseContext *context,
			  const gchar         *element_name,
			  const gchar        **names,
			  const gchar        **values,
			  gpointer             user_data,
			  GError             **error)
{
  AttributesSubParserData *parser_data = (AttributesSubParserData*)user_data;
  guint i;

  if (strcmp (element_name, "attribute") == 0)
    {
      for (i = 0; names[i]; i++)
	if (strcmp (names[i], "name") == 0)
	  parser_data->attr_name = g_strdup (values[i]);
    }
  else if (strcmp (element_name, "attributes") == 0)
    return;
  else
    g_warning ("Unsupported tag for GtkCellLayout: %s\n", element_name);
}

static void
attributes_text_element (GMarkupParseContext *context,
			 const gchar         *text,
			 gsize                text_len,
			 gpointer             user_data,
			 GError             **error)
{
  AttributesSubParserData *parser_data = (AttributesSubParserData*)user_data;
  glong l;
  gchar *endptr;
  gchar *string;
  
  if (!parser_data->attr_name)
    return;

  string = g_strndup (text, text_len);
  errno = 0;
  l = g_ascii_strtoll (string, &endptr, 0);
  if (errno || endptr == string)
    {
      g_set_error (error, 
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_INVALID_VALUE,
                   "Could not parse integer `%s'",
                   string);
      g_free (string);
      return;
    }
  g_free (string);

  gtk_cell_layout_add_attribute (parser_data->cell_layout,
				 parser_data->renderer,
				 parser_data->attr_name, l);
  g_free (parser_data->attr_name);
  parser_data->attr_name = NULL;
}

static const GMarkupParser attributes_parser =
  {
    attributes_start_element,
    NULL,
    attributes_text_element,
  };


/* Cell packing parsing */
static void
gtk_cell_layout_buildable_set_cell_property (GtkCellArea     *area,
					     GtkBuilder      *builder,
					     GtkCellRenderer *cell,
					     gchar           *name,
					     const gchar     *value)
{
  GParamSpec *pspec;
  GValue gvalue = { 0, };
  GError *error = NULL;

  pspec = gtk_cell_area_class_find_cell_property (GTK_CELL_AREA_GET_CLASS (area), name);
  if (!pspec)
    {
      g_warning ("%s does not have a property called %s",
		 g_type_name (G_OBJECT_TYPE (area)), name);
      return;
    }

  if (!gtk_builder_value_from_string (builder, pspec, value, &gvalue, &error))
    {
      g_warning ("Could not read property %s:%s with value %s of type %s: %s",
		 g_type_name (G_OBJECT_TYPE (area)),
		 name,
		 value,
		 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
		 error->message);
      g_error_free (error);
      return;
    }

  gtk_cell_area_cell_set_property (area, cell, name, &gvalue);
  g_value_unset (&gvalue);
}

typedef struct {
  GtkBuilder      *builder;
  GtkCellLayout   *cell_layout;
  GtkCellRenderer *renderer;
  gchar           *cell_prop_name;
  gchar           *context;
  gboolean         translatable;
} CellPackingSubParserData;

static void
cell_packing_start_element (GMarkupParseContext *context,
			    const gchar         *element_name,
			    const gchar        **names,
			    const gchar        **values,
			    gpointer             user_data,
			    GError             **error)
{
  CellPackingSubParserData *parser_data = (CellPackingSubParserData*)user_data;
  guint i;

  if (strcmp (element_name, "property") == 0)
    {
      for (i = 0; names[i]; i++)
	if (strcmp (names[i], "name") == 0)
	  parser_data->cell_prop_name = g_strdup (values[i]);
	else if (strcmp (names[i], "translatable") == 0)
	  {
	    if (!_gtk_builder_boolean_from_string (values[i],
						   &parser_data->translatable,
						   error))
	      return;
	  }
	else if (strcmp (names[i], "comments") == 0)
	  ; /* for translators */
	else if (strcmp (names[i], "context") == 0)
	  parser_data->context = g_strdup (values[i]);
	else
	  g_warning ("Unsupported attribute for GtkCellLayout Cell "
		     "property: %s\n", names[i]);
    }
  else if (strcmp (element_name, "cell-packing") == 0)
    return;
  else
    g_warning ("Unsupported tag for GtkCellLayout: %s\n", element_name);
}

static void
cell_packing_text_element (GMarkupParseContext *context,
			   const gchar         *text,
			   gsize                text_len,
			   gpointer             user_data,
			   GError             **error)
{
  CellPackingSubParserData *parser_data = (CellPackingSubParserData*)user_data;
  GtkCellArea *area;
  gchar* value;

  if (!parser_data->cell_prop_name)
    return;

  if (parser_data->translatable && text_len)
    {
      const gchar* domain;
      domain = gtk_builder_get_translation_domain (parser_data->builder);

      value = _gtk_builder_parser_translate (domain,
					     parser_data->context,
					     text);
    }
  else
    {
      value = g_strdup (text);
    }

  area = gtk_cell_layout_get_area (parser_data->cell_layout);

  if (!area)
    {
      g_warning ("%s does not have an internal GtkCellArea class and cannot apply child cell properties",
		 g_type_name (G_OBJECT_TYPE (parser_data->cell_layout)));
      return;
    }

  gtk_cell_layout_buildable_set_cell_property (area, 
					       parser_data->builder,
					       parser_data->renderer,
					       parser_data->cell_prop_name,
					       value);

  g_free (parser_data->cell_prop_name);
  g_free (parser_data->context);
  g_free (value);
  parser_data->cell_prop_name = NULL;
  parser_data->context = NULL;
  parser_data->translatable = FALSE;
}

static const GMarkupParser cell_packing_parser =
  {
    cell_packing_start_element,
    NULL,
    cell_packing_text_element,
  };

gboolean
_gtk_cell_layout_buildable_custom_tag_start (GtkBuildable  *buildable,
					     GtkBuilder    *builder,
					     GObject       *child,
					     const gchar   *tagname,
					     GMarkupParser *parser,
					     gpointer      *data)
{
  AttributesSubParserData  *attr_data;
  CellPackingSubParserData *packing_data;

  if (!child)
    return FALSE;

  if (strcmp (tagname, "attributes") == 0)
    {
      attr_data = g_slice_new0 (AttributesSubParserData);
      attr_data->cell_layout = GTK_CELL_LAYOUT (buildable);
      attr_data->renderer = GTK_CELL_RENDERER (child);
      attr_data->attr_name = NULL;

      *parser = attributes_parser;
      *data = attr_data;
      return TRUE;
    }
  else if (strcmp (tagname, "cell-packing") == 0)
    {
      packing_data = g_slice_new0 (CellPackingSubParserData);
      packing_data->builder = builder;
      packing_data->cell_layout = GTK_CELL_LAYOUT (buildable);
      packing_data->renderer = GTK_CELL_RENDERER (child);

      *parser = cell_packing_parser;
      *data = packing_data;
      return TRUE;
    }

  return FALSE;
}

gboolean
_gtk_cell_layout_buildable_custom_tag_end (GtkBuildable *buildable,
					   GtkBuilder   *builder,
					   GObject      *child,
					   const gchar  *tagname,
					   gpointer     *data)
{
  AttributesSubParserData *attr_data;

  if (strcmp (tagname, "attributes") == 0)
    {
      attr_data = (AttributesSubParserData*)data;
      g_assert (!attr_data->attr_name);
      g_slice_free (AttributesSubParserData, attr_data);
      return TRUE;
    }
  else if (strcmp (tagname, "cell-packing") == 0)
    {
      g_slice_free (CellPackingSubParserData, (gpointer)data);
      return TRUE;
    }
  return FALSE;
}

void
_gtk_cell_layout_buildable_add_child (GtkBuildable      *buildable,
				      GtkBuilder        *builder,
				      GObject           *child,
				      const gchar       *type)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (buildable));
  g_return_if_fail (GTK_IS_CELL_RENDERER (child));

  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (buildable), GTK_CELL_RENDERER (child), FALSE);
}
