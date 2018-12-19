/* gtklayoutmanager.c: Layout manager base class
 * Copyright 2018  The GNOME Foundation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Emmanuele Bassi
 */

/**
 * SECTION:gtklayoutmanager
 * @Title: GtkLayoutManager
 * @Short_description: Base class for layout manager
 *
 * ...
 */

#include "config.h"

#include "gtklayoutmanagerprivate.h"
#include "gtklayoutchild.h"
#include "gtkwidget.h"

#ifdef G_ENABLE_DEBUG
#define LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED(m,method)   G_STMT_START {  \
        GObject *_obj = G_OBJECT (m);                                   \
        g_warning ("Layout managers of type %s do not implement "       \
                   "the GtkLayoutManager::%s method",                   \
                   G_OBJECT_TYPE_NAME (_obj),                           \
                   #method);                           } G_STMT_END
#else
#define LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED(m,method)
#endif

typedef struct {
  GtkWidget *widget;
} GtkLayoutManagerPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkLayoutManager, gtk_layout_manager, G_TYPE_OBJECT)

static GQuark quark_layout_child;

static GtkSizeRequestMode
gtk_layout_manager_real_get_request_mode (GtkLayoutManager *manager,
                                          GtkWidget        *widget)
{
  LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED (manager, get_request_mode);

  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_layout_manager_real_measure (GtkLayoutManager *manager,
                                 GtkWidget        *widget,
                                 GtkOrientation    orientation,
                                 int               for_size,
                                 int              *minimum,
                                 int              *natural,
                                 int              *baseline_minimum,
                                 int              *baseline_natural)
{
  LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED (manager, measure);

  if (minimum != NULL)
    *minimum = 0;

  if (natural != NULL)
    *natural = 0;

  if (baseline_minimum != NULL)
    *baseline_minimum = 0;

  if (baseline_natural != NULL)
    *baseline_natural = 0;
}

static void
gtk_layout_manager_real_allocate (GtkLayoutManager *manager,
                                  GtkWidget        *widget,
                                  int               width,
                                  int               height,
                                  int               baseline)
{
  LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED (manager, allocate);
}

static void
gtk_layout_manager_class_init (GtkLayoutManagerClass *klass)
{
  klass->get_request_mode = gtk_layout_manager_real_get_request_mode;
  klass->measure = gtk_layout_manager_real_measure;
  klass->allocate = gtk_layout_manager_real_allocate;

  quark_layout_child = g_quark_from_static_string ("-GtkLayoutManager-layout-child");
}

static void
gtk_layout_manager_init (GtkLayoutManager *self)
{
}

/*< private >
 * gtk_layout_manager_set_widget:
 * @layout_manager: a #GtkLayoutManager
 * @widget: (nullable): a #GtkWidget
 *
 * ...
 */
void
gtk_layout_manager_set_widget (GtkLayoutManager *layout_manager,
                               GtkWidget        *widget)
{
  GtkLayoutManagerPrivate *priv = gtk_layout_manager_get_instance_private (layout_manager);

  priv->widget = widget;
}

/**
 * gtk_layout_manager_measure:
 * @manager:
 * @widget:
 * @orientation:
 * @for_size:
 * @minimum: (out):
 * @natural: (out):
 * @minimum_baseline: (out):
 * @natural_baseline: (out):
 *
 * ...
 *
 */
void
gtk_layout_manager_measure (GtkLayoutManager *manager,
                            GtkWidget        *widget,
                            GtkOrientation    orientation,
                            int               for_size,
                            int              *minimum,
                            int              *natural,
                            int              *minimum_baseline,
                            int              *natural_baseline)
{
  GtkLayoutManagerClass *klass;

  g_return_if_fail (GTK_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  klass = GTK_LAYOUT_MANAGER_GET_CLASS (manager);

  klass->measure (manager, widget, orientation,
                  for_size,
                  minimum, natural,
                  minimum_baseline, natural_baseline);
}

/**
 * gtk_layout_manager_allocate:
 * @manager:
 * @widget:
 * @width:
 * @height:
 * @baseline:
 *
 * ...
 */
void
gtk_layout_manager_allocate (GtkLayoutManager *manager,
                             GtkWidget        *widget,
                             int               width,
                             int               height,
                             int               baseline)
{
  GtkLayoutManagerClass *klass;

  g_return_if_fail (GTK_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  klass = GTK_LAYOUT_MANAGER_GET_CLASS (manager);

  klass->allocate (manager, widget, width, height, baseline);
}

/**
 * gtk_layout_manager_get_request_mode:
 * @manager:
 * @widget:
 *
 * ...
 *
 * Returns: ...
 */
GtkSizeRequestMode
gtk_layout_manager_get_request_mode (GtkLayoutManager *manager,
                                     GtkWidget        *widget)
{
  GtkLayoutManagerClass *klass;

  g_return_val_if_fail (GTK_IS_LAYOUT_MANAGER (manager), GTK_SIZE_REQUEST_CONSTANT_SIZE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_SIZE_REQUEST_CONSTANT_SIZE);

  klass = GTK_LAYOUT_MANAGER_GET_CLASS (manager);

  return klass->get_request_mode (manager, widget);
}

/**
 * gtk_layout_manager_layout_changed:
 * @manager: a #GtkLayoutManager
 *
 * ...
 */
void
gtk_layout_manager_layout_changed (GtkLayoutManager *manager)
{
  GtkLayoutManagerPrivate *priv = gtk_layout_manager_get_instance_private (manager);

  if (priv->widget != NULL)
    gtk_widget_queue_resize (priv->widget);
}

/**
 * gtk_layout_manager_get_layout_child:
 * @manager: ...
 * @widget: ...
 *
 * ...
 *
 * Returns: (transfer none): ...
 */
GtkLayoutChild *
gtk_layout_manager_get_layout_child (GtkLayoutManager *manager,
                                     GtkWidget        *widget)
{
  GtkLayoutManagerPrivate *priv = gtk_layout_manager_get_instance_private (manager);
  GtkLayoutChild *res;
  GtkWidget *parent;

  g_return_val_if_fail (GTK_IS_LAYOUT_MANAGER (manager), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  parent = gtk_widget_get_parent (widget);
  g_return_val_if_fail (parent != NULL, NULL);

  if (priv->widget != parent)
    {
      g_critical ("The parent %s %p of the widget %s %p does not "
                  "use the given layout manager of type %s %p",
                  gtk_widget_get_name (parent), parent,
                  gtk_widget_get_name (widget), widget,
                  G_OBJECT_TYPE_NAME (manager), manager);
      return NULL;
    }

  if (GTK_LAYOUT_MANAGER_GET_CLASS (manager)->create_layout_child == NULL)
    {
      g_critical ("The layout manager of type %s %p does not create "
                  "GtkLayoutChild instances",
                  G_OBJECT_TYPE_NAME (manager), manager);
      return NULL;
    }

  /* We store the LayoutChild into the Widget, so that the LayoutChild
   * instance goes away once the Widget goes away
   */
  res = g_object_get_qdata (G_OBJECT (widget), quark_layout_child);
  if (res != NULL)
    {
      /* If the LayoutChild instance is stale, and refers to another
       * layout manager, then we simply ask the LayoutManager to
       * replace it, as it means the layout manager for the parent
       * widget was replaced
       */
      if (gtk_layout_child_get_layout_manager (res) == manager)
        return res;
    }

  res = GTK_LAYOUT_MANAGER_GET_CLASS (manager)->create_layout_child (manager, widget);
  g_assert (res != NULL);
  g_assert (g_type_is_a (G_OBJECT_TYPE (res), GTK_TYPE_LAYOUT_CHILD));

  g_object_set_qdata_full (G_OBJECT (widget), quark_layout_child,
                           res,
                           g_object_unref);

  return res;
}
