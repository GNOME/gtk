/* ATK -  Accessibility Toolkit
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "atkcomponent.h"

/**
 * SECTION:atkcomponent
 * @Short_description: The ATK interface provided by UI components
 * which occupy a physical area on the screen.
 * which the user can activate/interact with.
 * @Title:AtkComponent
 *
 * #AtkComponent should be implemented by most if not all UI elements
 * with an actual on-screen presence, i.e. components which can be
 * said to have a screen-coordinate bounding box.  Virtually all
 * widgets will need to have #AtkComponent implementations provided
 * for their corresponding #AtkObject class.  In short, only UI
 * elements which are *not* GUI elements will omit this ATK interface.
 *
 * A possible exception might be textual information with a
 * transparent background, in which case text glyph bounding box
 * information is provided by #AtkText.
 */

enum {
  BOUNDS_CHANGED,
  LAST_SIGNAL
};

static void       atk_component_base_init (AtkComponentIface *class);

static gboolean   atk_component_real_contains                (AtkComponent *component,
                                                              gint         x,
                                                              gint         y,
                                                              AtkCoordType coord_type);

static AtkObject* atk_component_real_ref_accessible_at_point (AtkComponent *component,
                                                              gint         x,
                                                              gint         y,
                                                              AtkCoordType coord_type);

static void      atk_component_real_get_position             (AtkComponent *component,
                                                              gint         *x,
                                                              gint         *y,
                                                              AtkCoordType coord_type);

static void      atk_component_real_get_size                 (AtkComponent *component,
                                                              gint         *width,
                                                              gint         *height);

static guint atk_component_signals[LAST_SIGNAL] = { 0 };

GType
atk_component_get_type (void)
{
  static GType type = 0;

  if (!type) {
    static const GTypeInfo tinfo =
    {
      sizeof (AtkComponentIface),
      (GBaseInitFunc) atk_component_base_init,
      (GBaseFinalizeFunc) NULL,

    };

    type = g_type_register_static (G_TYPE_INTERFACE, "AtkComponent", &tinfo, 0);
  }

  return type;
}

static void
atk_component_base_init (AtkComponentIface *class)
{
  static gboolean initialized = FALSE;

  if (! initialized)
    {
      class->ref_accessible_at_point = atk_component_real_ref_accessible_at_point;
      class->contains = atk_component_real_contains;
      class->get_position = atk_component_real_get_position;
      class->get_size = atk_component_real_get_size;


      /**
       * AtkComponent::bounds-changed:
       * @atkcomponent: the object which received the signal.
       * @arg1: The AtkRectangle giving the new position and size.
       *
       * The 'bounds-changed" signal is emitted when the bposition or
       * size of the component changes.
       */
      atk_component_signals[BOUNDS_CHANGED] =
        g_signal_new ("bounds_changed",
                      ATK_TYPE_COMPONENT,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (AtkComponentIface, bounds_changed),
                      (GSignalAccumulator) NULL, NULL,
                      g_cclosure_marshal_VOID__BOXED,
                      G_TYPE_NONE, 1,
                      ATK_TYPE_RECTANGLE | G_SIGNAL_TYPE_STATIC_SCOPE);

      initialized = TRUE;
    }
}


/**
 * atk_component_contains:
 * @component: the #AtkComponent
 * @x: x coordinate
 * @y: y coordinate
 * @coord_type: specifies whether the coordinates are relative to the screen
 * or to the components top level window
 *
 * Checks whether the specified point is within the extent of the @component.
 *
 * Toolkit implementor note: ATK provides a default implementation for
 * this virtual method. In general there are little reason to
 * re-implement it.
 *
 * Returns: %TRUE or %FALSE indicating whether the specified point is within
 * the extent of the @component or not
 **/
gboolean
atk_component_contains (AtkComponent    *component,
                        gint            x,
                        gint            y,
                        AtkCoordType    coord_type)
{
  AtkComponentIface *iface = NULL;
  g_return_val_if_fail (ATK_IS_COMPONENT (component), FALSE);

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->contains)
    return (iface->contains) (component, x, y, coord_type);
  else
    return FALSE;
}

/**
 * atk_component_ref_accessible_at_point:
 * @component: the #AtkComponent
 * @x: x coordinate
 * @y: y coordinate
 * @coord_type: specifies whether the coordinates are relative to the screen
 * or to the components top level window
 *
 * Gets a reference to the accessible child, if one exists, at the
 * coordinate point specified by @x and @y.
 *
 * Returns: (nullable) (transfer full): a reference to the accessible
 * child, if one exists
 **/
AtkObject*
atk_component_ref_accessible_at_point (AtkComponent    *component,
                                       gint            x,
                                       gint            y,
                                       AtkCoordType    coord_type)
{
  AtkComponentIface *iface = NULL;
  g_return_val_if_fail (ATK_IS_COMPONENT (component), NULL);

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->ref_accessible_at_point)
    return (iface->ref_accessible_at_point) (component, x, y, coord_type);
  else
    return NULL;
}

/**
 * atk_component_get_extents:
 * @component: an #AtkComponent
 * @x: (out) (optional): address of #gint to put x coordinate
 * @y: (out) (optional): address of #gint to put y coordinate
 * @width: (out) (optional): address of #gint to put width
 * @height: (out) (optional): address of #gint to put height
 * @coord_type: specifies whether the coordinates are relative to the screen
 * or to the components top level window
 *
 * Gets the rectangle which gives the extent of the @component.
 *
 * If the extent can not be obtained (e.g. a non-embedded plug or missing
 * support), all of x, y, width, height are set to -1.
 *
 **/
void
atk_component_get_extents    (AtkComponent    *component,
                              gint            *x,
                              gint            *y,
                              gint            *width,
                              gint            *height,
                              AtkCoordType    coord_type)
{
  AtkComponentIface *iface = NULL;
  gint local_x, local_y, local_width, local_height;
  gint *real_x, *real_y, *real_width, *real_height;

  g_return_if_fail (ATK_IS_COMPONENT (component));

  if (x)
    real_x = x;
  else
    real_x = &local_x;
  if (y)
    real_y = y;
  else
    real_y = &local_y;
  if (width)
    real_width = width;
  else
    real_width = &local_width;
  if (height)
    real_height = height;
  else
    real_height = &local_height;

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->get_extents)
    (iface->get_extents) (component, real_x, real_y, real_width, real_height, coord_type);
  else
    {
      *real_x = -1;
      *real_y = -1;
      *real_width = -1;
      *real_height = -1;
    }
}

/**
 * atk_component_get_layer:
 * @component: an #AtkComponent
 *
 * Gets the layer of the component.
 *
 * Returns: an #AtkLayer which is the layer of the component
 **/
AtkLayer
atk_component_get_layer (AtkComponent *component) 
{
  AtkComponentIface *iface;

  g_return_val_if_fail (ATK_IS_COMPONENT (component), ATK_LAYER_INVALID);

  iface = ATK_COMPONENT_GET_IFACE (component);
  if (iface->get_layer)
    return (iface->get_layer) (component);
  else
    return ATK_LAYER_WIDGET;
}

/**
 * atk_component_get_mdi_zorder:
 * @component: an #AtkComponent
 *
 * Gets the zorder of the component. The value G_MININT will be returned 
 * if the layer of the component is not ATK_LAYER_MDI or ATK_LAYER_WINDOW.
 *
 * Returns: a gint which is the zorder of the component, i.e. the depth at 
 * which the component is shown in relation to other components in the same 
 * container.
 **/
gint
atk_component_get_mdi_zorder (AtkComponent *component) 
{
  AtkComponentIface *iface;

  g_return_val_if_fail (ATK_IS_COMPONENT (component), G_MININT);

  iface = ATK_COMPONENT_GET_IFACE (component);
  if (iface->get_mdi_zorder)
    return (iface->get_mdi_zorder) (component);
  else
    return G_MININT;
}

/**
 * atk_component_get_alpha:
 * @component: an #AtkComponent
 *
 * Returns the alpha value (i.e. the opacity) for this
 * @component, on a scale from 0 (fully transparent) to 1.0
 * (fully opaque).
 *
 * Returns: An alpha value from 0 to 1.0, inclusive.
 * Since: 1.12
 **/
gdouble
atk_component_get_alpha (AtkComponent    *component)
{
  AtkComponentIface *iface;

  g_return_val_if_fail (ATK_IS_COMPONENT (component), G_MININT);

  iface = ATK_COMPONENT_GET_IFACE (component);
  if (iface->get_alpha)
    return (iface->get_alpha) (component);
  else
    return (gdouble) 1.0;
}

/**
 * atk_component_grab_focus:
 * @component: an #AtkComponent
 *
 * Grabs focus for this @component.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 **/
gboolean
atk_component_grab_focus (AtkComponent    *component)
{
  AtkComponentIface *iface = NULL;
  g_return_val_if_fail (ATK_IS_COMPONENT (component), FALSE);

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->grab_focus)
    return (iface->grab_focus) (component);
  else
    return FALSE;
}

/**
 * atk_component_set_extents:
 * @component: an #AtkComponent
 * @x: x coordinate
 * @y: y coordinate
 * @width: width to set for @component
 * @height: height to set for @component
 * @coord_type: specifies whether the coordinates are relative to the screen
 * or to the components top level window
 *
 * Sets the extents of @component.
 *
 * Returns: %TRUE or %FALSE whether the extents were set or not
 **/
gboolean
atk_component_set_extents   (AtkComponent    *component,
                             gint            x,
                             gint            y,
                             gint            width,
                             gint            height,
                             AtkCoordType    coord_type)
{
  AtkComponentIface *iface = NULL;
  g_return_val_if_fail (ATK_IS_COMPONENT (component), FALSE);

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->set_extents)
    return (iface->set_extents) (component, x, y, width, height, coord_type);
  else
    return FALSE;
}

/**
 * atk_component_set_position:
 * @component: an #AtkComponent
 * @x: x coordinate
 * @y: y coordinate
 * @coord_type: specifies whether the coordinates are relative to the screen
 * or to the component's top level window
 *
 * Sets the position of @component.
 *
 * Contrary to atk_component_scroll_to, this does not trigger any scrolling,
 * this just moves @component in its parent.
 *
 * Returns: %TRUE or %FALSE whether or not the position was set or not
 **/
gboolean
atk_component_set_position   (AtkComponent    *component,
                              gint            x,
                              gint            y,
                              AtkCoordType    coord_type)
{
  AtkComponentIface *iface = NULL;
  g_return_val_if_fail (ATK_IS_COMPONENT (component), FALSE);

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->set_position)
    return (iface->set_position) (component, x, y, coord_type);
  else
    return FALSE;
}

/**
 * atk_component_set_size:
 * @component: an #AtkComponent
 * @width: width to set for @component
 * @height: height to set for @component
 *
 * Set the size of the @component in terms of width and height.
 *
 * Returns: %TRUE or %FALSE whether the size was set or not
 **/
gboolean
atk_component_set_size       (AtkComponent    *component,
                              gint            x,
                              gint            y)
{
  AtkComponentIface *iface = NULL;
  g_return_val_if_fail (ATK_IS_COMPONENT (component), FALSE);

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->set_size)
    return (iface->set_size) (component, x, y);
  else
    return FALSE;
}

/**
 * atk_component_scroll_to:
 * @component: an #AtkComponent
 * @type: specify where the object should be made visible.
 *
 * Makes @component visible on the screen by scrolling all necessary parents.
 *
 * Contrary to atk_component_set_position, this does not actually move
 * @component in its parent, this only makes the parents scroll so that the
 * object shows up on the screen, given its current position within the parents.
 *
 * Returns: whether scrolling was successful.
 *
 * Since: 2.30
 */
gboolean
atk_component_scroll_to (AtkComponent  *component,
                         AtkScrollType  type)
{
  AtkComponentIface *iface = NULL;
  g_return_val_if_fail (ATK_IS_COMPONENT (component), FALSE);

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->scroll_to)
    return (iface->scroll_to) (component, type);

  return FALSE;
}

/**
 * atk_component_scroll_to_point:
 * @component: an #AtkComponent
 * @coords: specify whether coordinates are relative to the screen or to the
 * parent object.
 * @x: x-position where to scroll to
 * @y: y-position where to scroll to
 *
 * Move the top-left of @component to a given position of the screen by
 * scrolling all necessary parents.
 *
 * Returns: whether scrolling was successful.
 *
 * Since: 2.30
 */
gboolean
atk_component_scroll_to_point (AtkComponent *component,
                               AtkCoordType  coords,
                               gint          x,
                               gint          y)
{
  AtkComponentIface *iface = NULL;
  g_return_val_if_fail (ATK_IS_COMPONENT (component), FALSE);

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->scroll_to_point)
    return (iface->scroll_to_point) (component, coords, x, y);

  return FALSE;
}

static gboolean
atk_component_real_contains (AtkComponent *component,
                             gint         x,
                             gint         y,
                             AtkCoordType coord_type)
{
  gint real_x, real_y, width, height;

  real_x = real_y = width = height = 0;

  atk_component_get_extents (component, &real_x, &real_y, &width, &height, coord_type);

  if ((x >= real_x) &&
      (x < real_x + width) &&
      (y >= real_y) &&
      (y < real_y + height))
    return TRUE;
  else
    return FALSE;
}

static AtkObject* 
atk_component_real_ref_accessible_at_point (AtkComponent *component,
                                            gint         x,
                                            gint         y,
                                            AtkCoordType coord_type)
{
  gint count, i;

  count = atk_object_get_n_accessible_children (ATK_OBJECT (component));

  for (i = 0; i < count; i++)
  {
    AtkObject *obj;

    obj = atk_object_ref_accessible_child (ATK_OBJECT (component), i);

    if (obj != NULL)
    {
      if (atk_component_contains (ATK_COMPONENT (obj), x, y, coord_type))
      {
        return obj;
      }
      else
      {
        g_object_unref (obj);
      }
    }
  }
  return NULL;
}

static void
atk_component_real_get_position (AtkComponent *component,
                                 gint         *x,
                                 gint         *y,
                                 AtkCoordType coord_type)
{
  gint width, height;

  atk_component_get_extents (component, x, y, &width, &height, coord_type);
}

static void
atk_component_real_get_size (AtkComponent *component,
                             gint         *width,
                             gint         *height)
{
  gint x, y;
  AtkCoordType coord_type;

  /*
   * Pick one coordinate type; it does not matter for size
   */
  coord_type = ATK_XY_WINDOW;

  atk_component_get_extents (component, &x, &y, width, height, coord_type);
}

static AtkRectangle *
atk_rectangle_copy (const AtkRectangle *rectangle)
{
  AtkRectangle *result = g_new (AtkRectangle, 1);
  *result = *rectangle;

  return result;
}

GType
atk_rectangle_get_type (void)
{
  static GType our_type = 0;

  if (our_type == 0)
    our_type = g_boxed_type_register_static ("AtkRectangle",
                                             (GBoxedCopyFunc)atk_rectangle_copy,
                                             (GBoxedFreeFunc)g_free);
  return our_type;
}

