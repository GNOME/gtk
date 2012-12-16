/*
 * Gtk.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009  Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:_gtk-bin-layout
 * @short_description: A simple layout manager
 *
 * #GtkBinLayout is a layout manager which implements the following
 * policy:
 *
 * <itemizedlist>
 *   <listitem><simpara>the preferred size is the maximum preferred size
 *   between all the children of the container using the
 *   layout;</simpara></listitem>
 *   <listitem><simpara>each child is allocated in "layers", on on top
 *   of the other;</simpara></listitem>
 * </itemizedlist>
 *
 * <figure id="bin-layout">
 *   <title>Bin layout</title>
 *   <para>The image shows a #GtkBinLayout with three layers:
 *   a background #GtkCairoTexture, set to fill on both the X
 *   and Y axis; a #GtkTexture, set to center on both the X and
 *   Y axis; and a #GtkRectangle, set to %GTK_BIN_ALIGNMENT_END
 *   on both the X and Y axis.</para>
 *   <graphic fileref="bin-layout.png" format="PNG"/>
 * </figure>
 *
 * <example id="example-_gtk-bin-layout">
 *  <title>How to pack actors inside a BinLayout</title>
 *  <programlisting>
 * <xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../examples/bin-layout.c">
 *   <xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback>
 * </xi:include>
 *  </programlisting>
 * </example>
 *
 * #GtkBinLayout is available since Gtk 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gtkbinlayoutprivate.h"

struct _GtkBinLayoutPrivate {
  gint dummy;
};

G_DEFINE_TYPE (GtkBinLayout,
               _gtk_bin_layout,
               GTK_TYPE_LAYOUT_MANAGER);

/*
 * GtkBinLayout
 */

static void
gtk_bin_layout_get_preferred_size (GtkLayoutManager *manager,
                                   GtkOrientation    orientation,
                                   gfloat            for_size,
                                   gfloat           *min_size_p,
                                   gfloat           *nat_size_p)
{
  GtkActor *actor = _gtk_layout_manager_get_actor (manager);
  GtkActor *child;
  gfloat min_size, nat_size;

  min_size = nat_size = 0.0;

  for (child = _gtk_actor_get_first_child (actor);
       child;
       child = _gtk_actor_get_next_sibling (child))
    {
      gfloat minimum, natural;

      _gtk_actor_get_preferred_size (child,
                                     orientation,
                                     for_size,
                                     &minimum,
                                     &natural);

      min_size = MAX (min_size, minimum);
      nat_size = MAX (nat_size, natural);
    }

  *min_size_p = min_size;
  *nat_size_p = nat_size;
}

typedef struct _Alignment Alignment;

struct _Alignment {
  gfloat xalign;
  gfloat yalign;
  gfloat xscale;
  gfloat yscale;
};

static const Alignment *
get_alignment (GtkActor *actor)
{
  return g_object_get_data (G_OBJECT (actor), "gtk-bin-layout-alignment");
}

static void
gtk_bin_layout_allocate (GtkLayoutManager     *manager,
                         const cairo_matrix_t *transform,
                         gfloat                width,
                         gfloat                height)
{
  GtkActor *actor, *child;

  actor = _gtk_layout_manager_get_actor (manager);

  for (child = _gtk_actor_get_first_child (actor);
       child;
       child = _gtk_actor_get_next_sibling (child))
    {
      const Alignment *alignment = get_alignment (child);

      if (alignment == NULL)
        {
          _gtk_actor_allocate (child, transform, width, height);
        }
      else
        {
          gfloat min, nat, child_width, child_height;
          cairo_matrix_t child_transform;

          _gtk_actor_get_preferred_size (child, GTK_ORIENTATION_HORIZONTAL, -1, &min, &nat);
          child_width = (width - min) * alignment->xscale + min;
          _gtk_actor_get_preferred_size (child, GTK_ORIENTATION_VERTICAL, child_width, &min, &nat);
          child_height = (height - min) * alignment->yscale + min;

          cairo_matrix_init_translate (&child_transform,
                                       (width - child_width) * alignment->xalign,
                                       (height - child_height) * alignment->yalign);
          cairo_matrix_multiply (&child_transform, transform, &child_transform);
          _gtk_actor_allocate (child, &child_transform, child_width, child_height);
        }
    }
}

static void
_gtk_bin_layout_class_init (GtkBinLayoutClass *klass)
{
  GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GtkBinLayoutPrivate));

  layout_class->get_preferred_size = gtk_bin_layout_get_preferred_size;
  layout_class->allocate = gtk_bin_layout_allocate;
}

static void
_gtk_bin_layout_init (GtkBinLayout *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GTK_TYPE_BIN_LAYOUT,
                                            GtkBinLayoutPrivate);
}

/**
 * _gtk_bin_layout_new:
 *
 * Creates a new #GtkBinLayout layout manager
 *
 * Return value: the newly created layout manager
 *
 * Since: 1.2
 */
GtkLayoutManager *
_gtk_bin_layout_new (void)
{
  return g_object_new (GTK_TYPE_BIN_LAYOUT, NULL);
}

void
_gtk_bin_layout_set_child_alignment (GtkBinLayout *layout,
                                     GtkActor     *child,
                                     gfloat        xalign,
                                     gfloat        yalign,
                                     gfloat        xscale,
                                     gfloat        yscale)
{
  Alignment *alignment;

  g_return_if_fail (GTK_IS_BIN_LAYOUT (layout));
  g_return_if_fail (GTK_IS_ACTOR (child));
  g_return_if_fail (xalign >= 0.0 && xalign <= 1.0);
  g_return_if_fail (yalign >= 0.0 && yalign <= 1.0);
  g_return_if_fail (xscale >= 0.0 && xscale <= 1.0);
  g_return_if_fail (yscale >= 0.0 && yscale <= 1.0);

  alignment = g_new (Alignment, 1);
  alignment->xalign = xalign;
  alignment->yalign = yalign;
  alignment->xscale = xscale;
  alignment->yscale = yscale;

  g_object_set_data_full (G_OBJECT (child),
                          "gtk-bin-layout-alignment",
                          alignment,
                          g_free);

  _gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (layout));
}
