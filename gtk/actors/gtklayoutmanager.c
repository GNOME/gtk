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
 * SECTION:_gtk-layout-manager
 * @short_description: Layout managers base class
 *
 * #GtkLayoutManager is a base abstract class for layout managers. A
 * layout manager implements the layouting policy for a composite or a
 * container actor: it controls the preferred size of the actor to which
 * it has been paired, and it controls the allocation of its children.
 *
 * Any composite or container #GtkActor subclass can delegate the
 * layouting of its children to a #GtkLayoutManager.
 *
 * Gtk provides some simple #GtkLayoutManager sub-classes, like
 * #GtkBoxLayout and #GtkBinLayout.
 *
 * <refsect2 id="GtkLayoutManager-use-in-Actor">
 *   <title>Using a Layout Manager inside an Actor</title>
 *   <para>In order to use a #GtkLayoutManager inside a #GtkActor
 *   sub-class you should invoke _gtk_layout_manager_get_preferred_size()
 *   inside the #GtkActorClass.get_preferred_size() virtual function and
 *   _gtk_layout_manager_get_preferred_height() inside the
 *   #GtkActorClass.get_preferred_height() virtual functions implementation.
 *   You should also call _gtk_layout_manager_allocate() inside the
 *   implementation of the #GtkActorClass.allocate() virtual function.</para>
 *   <para>In order to receive notifications for changes in the layout
 *   manager policies you should also connect to the
 *   #GtkLayoutManager::layout-changed signal and queue a relayout
 *   on your actor. The following code should be enough if the actor
 *   does not need to perform specific operations whenever a layout
 *   manager changes:</para>
 *   <informalexample><programlisting>
 *  g_signal_connect_swapped (layout_manager,
 *                            "layout-changed",
 *                            G_CALLBACK (_gtk_actor_queue_relayout),
 *                            actor);
 *   </programlisting></informalexample>
 * </refsect2>
 *
 * <refsect2 id="GtkLayoutManager-implementation">
 *   <title>Implementing a GtkLayoutManager</title>
 *   <para>The implementation of a layout manager does not differ from
 *   the implementation of the size requisition and allocation bits of
 *   #GtkActor, so you should read the relative documentation
 *   <link linkend="_gtk-subclassing-GtkActor">for subclassing
 *   GtkActor</link>.</para>
 *   <para>The layout manager implementation can hold a back pointer
 *   to the #GtkContainer by implementing the
 *   <function>set_container()</function> virtual function. The layout manager
 *   should not hold a real reference (i.e. call g_object_ref()) on the
 *   container actor, to avoid reference cycles.</para>
 *   <para>If a layout manager has properties affecting the layout
 *   policies then it should emit the #GtkLayoutManager::layout-changed
 *   signal on itself by using the _gtk_layout_manager_layout_changed()
 *   function whenever one of these properties changes.</para>
 * </refsect2>
 *
 * <refsect2 id="GtkLayoutManager-animation">
 *   <title>Animating a GtkLayoutManager</title>
 *   <para>A layout manager is used to let a #GtkContainer take complete
 *   ownership over the layout (that is: the position and sizing) of its
 *   children; this means that using the Gtk animation API, like
 *   _gtk_actor_animate(), to animate the position and sizing of a child of
 *   a layout manager it is not going to work properly, as the animation will
 *   automatically override any setting done by the layout manager
 *   itself.</para>
 *   <para>It is possible for a #GtkLayoutManager sub-class to animate its
 *   children layout by using the base class animation support. The
 *   #GtkLayoutManager animation support consists of three virtual
 *   functions: #GtkLayoutManagerClass.begin_animation(),
 *   #GtkLayoutManagerClass.get_animation_progress(), and
 *   #GtkLayoutManagerClass.end_animation().</para>
 *   <variablelist>
 *     <varlistentry>
 *       <term><function>begin_animation (duration, easing)</function></term>
 *       <listitem><para>This virtual function is invoked when the layout
 *       manager should begin an animation. The implementation should set up
 *       the state for the animation and create the ancillary objects for
 *       animating the layout. The default implementation creates a
 *       #GtkTimeline for the given duration and a #GtkAlpha binding
 *       the timeline to the given easing mode. This function returns a
 *       #GtkAlpha which should be used to control the animation from
 *       the caller perspective.</para></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term><function>get_animation_progress()</function></term>
 *       <listitem><para>This virtual function should be invoked when animating
 *       a layout manager. It returns the progress of the animation, using the
 *       same semantics as the #GtkAlpha:alpha value.</para></listitem>
 *     </varlistentry>
 *     <varlistentry>
 *       <term><function>end_animation()</function></term>
 *       <listitem><para>This virtual function is invoked when the animation of
 *       a layout manager ends, and it is meant to be used for bookkeeping the
 *       objects created in the <function>begin_animation()</function>
 *       function. The default implementation will call it implicitly when the
 *       timeline is complete.</para></listitem>
 *     </varlistentry>
 *   </variablelist>
 *   <para>The simplest way to animate a layout is to create a #GtkTimeline
 *   inside the <function>begin_animation()</function> virtual function, along
 *   with a #GtkAlpha, and for each #GtkTimeline::new-frame signal
 *   emission call _gtk_layout_manager_layout_changed(), which will cause a
 *   relayout. The #GtkTimeline::completed signal emission should cause
 *   _gtk_layout_manager_end_animation() to be called. The default
 *   implementation provided internally by #GtkLayoutManager does exactly
 *   this, so most sub-classes should either not override any animation-related
 *   virtual function or simply override #GtkLayoutManagerClass.begin_animation()
 *   and #GtkLayoutManagerClass.end_animation() to set up ad hoc state, and then
 *   chain up to the parent's implementation.</para>
 *   <example id="example-GtkLayoutManager-animation">
 *     <title>Animation of a Layout Manager</title>
 *     <para>The code below shows how a #GtkLayoutManager sub-class should
 *     provide animating the allocation of its children from within the
 *     #GtkLayoutManagerClass.allocate() virtual function implementation. The
 *     animation is computed between the last stable allocation performed
 *     before the animation started and the desired final allocation.</para>
 *     <para>The <varname>is_animating</varname> variable is stored inside the
 *     #GtkLayoutManager sub-class and it is updated by overriding the
 *     #GtkLayoutManagerClass.begin_animation() and the
 *     #GtkLayoutManagerClass.end_animation() virtual functions and chaining up
 *     to the base class implementation.</para>
 *     <para>The last stable allocation is stored within a #GtkLayoutMeta
 *     sub-class used by the implementation.</para>
 *     <programlisting>
 * static void
 * my_layout_manager_allocate (GtkLayoutManager   *manager,
 *                             GtkContainer       *container,
 *                             const GtkActorBox  *allocation,
 *                             GtkAllocationFlags  flags)
 * {
 *   MyLayoutManager *self = MY_LAYOUT_MANAGER (manager);
 *   GtkActor *child;
 *
 *   for (child = _gtk_actor_get_first_child (GTK_ACTOR (container));
 *        child != NULL;
 *        child = _gtk_actor_get_next_sibling (child))
 *     {
 *       GtkLayoutMeta *meta;
 *       MyLayoutMeta *my_meta;
 *
 *       /&ast; retrieve the layout meta-object &ast;/
 *       meta = _gtk_layout_manager_get_child_meta (manager,
 *                                                     container,
 *                                                     child);
 *       my_meta = MY_LAYOUT_META (meta);
 *
 *       /&ast; compute the desired allocation for the child &ast;/
 *       compute_allocation (self, my_meta, child,
 *                           allocation, flags,
 *                           &amp;child_box);
 *
 *       /&ast; this is the additional code that deals with the animation
 *        &ast; of the layout manager
 *        &ast;/
 *       if (!self-&gt;is_animating)
 *         {
 *           /&ast; store the last stable allocation for later use &ast;/
 *           my_meta-&gt;last_alloc = _gtk_actor_box_copy (&amp;child_box);
 *         }
 *       else
 *         {
 *           GtkActorBox end = { 0, };
 *           gdouble p;
 *
 *           /&ast; get the progress of the animation &ast;/
 *           p = _gtk_layout_manager_get_animation_progress (manager);
 *
 *           if (my_meta-&gt;last_alloc != NULL)
 *             {
 *               /&ast; copy the desired allocation as the final state &ast;/
 *               end = child_box;
 *
 *               /&ast; then interpolate the initial and final state
 *                &ast; depending on the progress of the animation,
 *                &ast; and put the result inside the box we will use
 *                &ast; to allocate the child
 *                &ast;/
 *               _gtk_actor_box_interpolate (my_meta-&gt;last_alloc,
 *                                              &amp;end,
 *                                              p,
 *                                              &amp;child_box);
 *             }
 *           else
 *             {
 *               /&ast; if there is no stable allocation then the child was
 *                &ast; added while animating; one possible course of action
 *                &ast; is to just bail out and fall through to the allocation
 *                &ast; to position the child directly at its final state
 *                &ast;/
 *               my_meta-&gt;last_alloc =
 *                 _gtk_actor_box_copy (&amp;child_box);
 *             }
 *         }
 *
 *       /&ast; allocate the child &ast;/
 *       _gtk_actor_allocate (child, &child_box, flags);
 *     }
 * }
 *     </programlisting>
 *   </example>
 *   <para>Sub-classes of #GtkLayoutManager that support animations of the
 *   layout changes should call _gtk_layout_manager_begin_animation()
 *   whenever a layout property changes value, e.g.:</para>
 *   <informalexample>
 *     <programlisting>
 * if (self->orientation != new_orientation)
 *   {
 *     GtkLayoutManager *manager;
 *
 *     self->orientation = new_orientation;
 *
 *     manager = GTK_LAYOUT_MANAGER (self);
 *     _gtk_layout_manager_layout_changed (manager);
 *     _gtk_layout_manager_begin_animation (manager, 500, GTK_LINEAR);
 *
 *     g_object_notify (G_OBJECT (self), "orientation");
 *   }
 *     </programlisting>
 *   </informalexample>
 *   <para>The code above will animate a change in the
 *   <varname>orientation</varname> layout property of a layout manager.</para>
 * </refsect2>
 *
 * <refsect2 id="_gtk-layout-properties">
 *   <title>Layout Properties</title>
 *   <para>If a layout manager has layout properties, that is properties that
 *   should exist only as the result of the presence of a specific (layout
 *   manager, container actor, child actor) combination, and it wishes to store
 *   those properties inside a #GtkLayoutMeta, then it should override the
 *   #GtkLayoutManagerClass.get_child_meta_type() virtual function to return
 *   the #GType of the #GtkLayoutMeta sub-class used to store the layout
 *   properties; optionally, the #GtkLayoutManager sub-class might also
 *   override the #GtkLayoutManagerClass.create_child_meta() virtual function
 *   to control how the #GtkLayoutMeta instance is created, otherwise the
 *   default implementation will be equivalent to:</para>
 *   <informalexample><programlisting>
 *  GtkLayoutManagerClass *klass;
 *  GType meta_type;
 *
 *  klass = GTK_LAYOUT_MANAGER_GET_CLASS (manager);
 *  meta_type = klass->get_child_meta_type (manager);
 *
 *  return g_object_new (meta_type,
 *                       "manager", manager,
 *                       "container", container,
 *                       "actor", actor,
 *                       NULL);
 *   </programlisting></informalexample>
 *   <para>Where <varname>manager</varname> is the  #GtkLayoutManager,
 *   <varname>container</varname> is the #GtkContainer using the
 *   #GtkLayoutManager and <varname>actor</varname> is the #GtkActor
 *   child of the #GtkContainer.</para>
 * </refsect2>
 *
 * <refsect2 id="_gtk-layout-script">
 *   <title>Using GtkLayoutManager with GtkScript</title>
 *   <para>#GtkLayoutManager instance can be created in the same way
 *   as other objects in #GtkScript; properties can be set using the
 *   common syntax.</para>
 *   <para>Layout properties can be set on children of a container with
 *   a #GtkLayoutManager using the <emphasis>layout::</emphasis>
 *   modifier on the property name, for instance:</para>
 *   <informalexample><programlisting>
 * {
 *   "type" : "GtkBox",
 *   "layout-manager" : { "type" : "GtkTableLayout" },
 *   "children" : [
 *     {
 *       "type" : "GtkTexture",
 *       "filename" : "image-00.png",
 *
 *       "layout::row" : 0,
 *       "layout::column" : 0,
 *       "layout::x-align" : "left",
 *       "layout::y-align" : "center",
 *       "layout::x-expand" : true,
 *       "layout::y-expand" : true
 *     },
 *     {
 *       "type" : "GtkTexture",
 *       "filename" : "image-01.png",
 *
 *       "layout::row" : 0,
 *       "layout::column" : 1,
 *       "layout::x-align" : "right",
 *       "layout::y-align" : "center",
 *       "layout::x-expand" : true,
 *       "layout::y-expand" : true
 *     }
 *   ]
 * }
 *   </programlisting></informalexample>
 * </refsect2>
 *
 * #GtkLayoutManager is available since Gtk 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gtklayoutmanagerprivate.h"

#define LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED(m,method)   G_STMT_START {  \
        GObject *_obj = G_OBJECT (m);                                   \
        g_warning ("Layout managers of type %s do not implement "       \
                   "the GtkLayoutManager::%s method",               \
                   G_OBJECT_TYPE_NAME (_obj),                           \
                   (method));                           } G_STMT_END

struct _GtkLayoutManagerPrivate
{
  GtkActor *actor;
};

G_DEFINE_ABSTRACT_TYPE (GtkLayoutManager,
                        _gtk_layout_manager,
                        G_TYPE_INITIALLY_UNOWNED);

static void
layout_manager_real_get_preferred_size (GtkLayoutManager *manager,
                                        GtkOrientation    orientation,
                                        gfloat            for_size,
                                        gfloat           *min_size_p,
                                        gfloat           *nat_size_p)
{
  LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED (manager, "get_preferred_size");

  *min_size_p = 0.0;
  *nat_size_p = 0.0;
}

static void
layout_manager_real_allocate (GtkLayoutManager   *manager,
                              const cairo_matrix_t *transform,
                              gfloat                width,
                              gfloat                height)
{
  LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED (manager, "allocate");
}

static void
_gtk_layout_manager_class_init (GtkLayoutManagerClass *klass)
{
  g_type_class_add_private (klass, sizeof (GtkLayoutManagerPrivate));

  klass->get_preferred_size = layout_manager_real_get_preferred_size;
  klass->allocate = layout_manager_real_allocate;
}

static void
_gtk_layout_manager_init (GtkLayoutManager *manager)
{
  manager->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (manager, GTK_TYPE_LAYOUT_MANAGER,
                                 GtkLayoutManagerPrivate);
}

/**
 * _gtk_layout_manager_get_preferred_size:
 * @manager: a #GtkLayoutManager
 * @orientation: orientation to query size for
 * @for_size: the size in the opposite direction, or -1
 * @min_width_p: (out) (allow-none): return location for the minimum width
 *   of the layout, or %NULL
 * @nat_width_p: (out) (allow-none): return location for the natural width
 *   of the layout, or %NULL
 *
 * Computes the minimum and natural widths of the @container according
 * to @manager.
 *
 * See also _gtk_actor_get_preferred_size()
 *
 * Since: 1.2
 */
void
_gtk_layout_manager_get_preferred_size (GtkLayoutManager *manager,
                                        GtkOrientation    orientation,
                                        gfloat            for_size,
                                        gfloat           *min_size_p,
                                        gfloat           *nat_size_p)
{
  GtkLayoutManagerClass *klass;
  gfloat ignore;

  g_return_if_fail (GTK_IS_LAYOUT_MANAGER (manager));

  if (min_size_p == NULL)
    min_size_p = &ignore;
  if (nat_size_p == NULL)
    nat_size_p = &ignore;

  klass = GTK_LAYOUT_MANAGER_GET_CLASS (manager);
  klass->get_preferred_size (manager,
                             orientation,
                             for_size,
                             min_size_p,
                             nat_size_p);
}

/**
 * _gtk_layout_manager_allocate:
 * @manager: a #GtkLayoutManager
 * @transform: transformation to apply to all children
 * @width: width of allocation area
 * @height: height of allocation area
 *
 * Allocates the children to the area given with @width and @height.
 *
 * See also _gtk_actor_allocate()
 *
 * Since: 1.2
 */
void
_gtk_layout_manager_allocate (GtkLayoutManager     *manager,
                              const cairo_matrix_t *transform,
                              gfloat                width,
                              gfloat                height)
{
  GtkLayoutManagerClass *klass;

  g_return_if_fail (GTK_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (transform != NULL);
  g_return_if_fail (width >= 0.0);
  g_return_if_fail (height >= 0.0);

  klass = GTK_LAYOUT_MANAGER_GET_CLASS (manager);
  klass->allocate (manager, transform, width, height);
}

/**
 * _gtk_layout_manager_layout_changed:
 * @manager: a #GtkLayoutManager
 *
 * Signals Emits the #GtkLayoutManager::layout-changed signal on @manager
 *
 * This function should only be called by implementations of the
 * #GtkLayoutManager class
 *
 * Since: 1.2
 */
void
_gtk_layout_manager_layout_changed (GtkLayoutManager *manager)
{
  GtkLayoutManagerPrivate *priv;

  g_return_if_fail (GTK_IS_LAYOUT_MANAGER (manager));

  priv = manager->priv;

  if (priv->actor)
    _gtk_actor_layout_manager_changed (priv->actor);
}

/*<private>
 * Called from GtkActor when the layout manager is set.
 */
void
_gtk_layout_manager_set_actor (GtkLayoutManager *manager,
                               GtkActor         *actor)
{
  g_return_if_fail (GTK_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (actor == NULL || GTK_IS_ACTOR (actor));

  manager->priv->actor = actor;
}

GtkActor *
_gtk_layout_manager_get_actor (GtkLayoutManager *manager)
{
  g_return_val_if_fail (GTK_IS_LAYOUT_MANAGER (manager), NULL);

  return manager->priv->actor;
}
