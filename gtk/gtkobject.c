/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "gtkobject.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkobject
 * @Short_description: The base class of the GTK+ type hierarchy
 * @Title: GtkObject
 * @See_also:#GObject
 *
 * #GtkObject is the base class for all widgets, and for a few
 * non-widget objects such as #GtkAdjustment. #GtkObject predates
 * #GObject; non-widgets that derive from #GtkObject rather than
 * #GObject do so for backward compatibility reasons.
 *
 * #GtkObject<!-- -->s are created with a "floating" reference count.
 * This means that the initial reference is not owned by anyone. Calling
 * g_object_unref() on a newly-created #GtkObject is incorrect, the floating
 * reference has to be removed first. This can be done by anyone at any time,
 * by calling g_object_ref_sink() to convert the floating reference into a
 * regular reference. g_object_ref_sink() returns a new reference if an object
 * is already sunk (has no floating reference).
 *
 * When you add a widget to its parent container, the parent container
 * will do this:
 * <informalexample><programlisting>
 *   g_object_ref_sink (G_OBJECT (child_widget));
 * </programlisting></informalexample>
 * This means that the container now owns a reference to the child widget
 * and the child widget has no floating reference.
 *
 * The purpose of the floating reference is to keep the child widget alive
 * until you add it to a parent container:
 * <informalexample><programlisting>
 *    button = gtk_button_new (<!-- -->);
 *    /&ast; button has one floating reference to keep it alive &ast;/
 *    gtk_container_add (GTK_CONTAINER (container), button);
 *    /&ast; button has one non-floating reference owned by the container &ast;/
 * </programlisting></informalexample>
 *
 * #GtkWindow is a special case, because GTK+ itself will ref/sink it on creation.
 * That is, after calling gtk_window_new(), the #GtkWindow will have one
 * reference which is owned by GTK+, and no floating references.
 *
 * One more factor comes into play: the #GtkObject::destroy signal, emitted by the
 * gtk_object_destroy() method. The #GtkObject::destroy signal asks all code owning a
 * reference to an object to release said reference. So, for example, if you call
 * gtk_object_destroy() on a #GtkWindow, GTK+ will release the reference count that
 * it owns; if you call gtk_object_destroy() on a #GtkButton, then the button will
 * be removed from its parent container and the parent container will release its
 * reference to the button.  Because these references are released, calling
 * gtk_object_destroy() should result in freeing all memory associated with an
 * object, unless some buggy code fails to release its references in response to
 * the #GtkObject::destroy signal. Freeing memory (referred to as
 * <firstterm>finalization</firstterm>) only happens if the reference count reaches
 * zero.
 *
 * Some simple rules for handling #GtkObject:
 * <itemizedlist>
 * <listitem><para>
 * Never call g_object_unref() unless you have previously called g_object_ref(),
 * even if you created the #GtkObject. (Note: this is <emphasis>not</emphasis>
 * true for #GObject; for #GObject, the creator of the object owns a reference.)
 * </para></listitem>
 * <listitem><para>
 * Call gtk_object_destroy() to get rid of most objects in most cases.
 * In particular, widgets are almost always destroyed in this way.
 * </para></listitem>
 * <listitem><para> Because of the floating reference count, you don't need to
 * worry about reference counting for widgets and toplevel windows, unless you
 * explicitly call g_object_ref() yourself.</para></listitem>
 * </itemizedlist>
 */


enum {
  DESTROY,
  LAST_SIGNAL
};

static void       gtk_object_dispose             (GObject        *object);
static void       gtk_object_real_destroy        (GtkObject      *object);
static void       gtk_object_finalize            (GObject        *object);

static guint       object_signals[LAST_SIGNAL] = { 0 };


/****************************************************
 * GtkObject type, class and instance initialization
 *
 ****************************************************/

G_DEFINE_ABSTRACT_TYPE (GtkObject, gtk_object, G_TYPE_INITIALLY_UNOWNED);

static void
gtk_object_init (GtkObject *object)
{
}

static void
gtk_object_class_init (GtkObjectClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->dispose = gtk_object_dispose;
  gobject_class->finalize = gtk_object_finalize;

  class->destroy = gtk_object_real_destroy;

  /**
   * GtkObject::destroy:
   * @object: the object which received the signal.
   *
   * Signals that all holders of a reference to the #GtkObject should release
   * the reference that they hold. May result in finalization of the object
   * if all references are released.
   */
  object_signals[DESTROY] =
    g_signal_new (I_("destroy"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		  G_STRUCT_OFFSET (GtkObjectClass, destroy),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}


/********************************************
 * Functions to end a GtkObject's life time
 *
 ********************************************/
/**
 * gtk_object_destroy:
 * @object: the object to destroy.
 *
 * Emits the #GtkObject::destroy signal notifying all reference holders that they should
 * release the #GtkObject. See the overview documentation at the top of the
 * page for more details.
 *
 * The memory for the object itself won't be deleted until
 * its reference count actually drops to 0; gtk_object_destroy() merely asks
 * reference holders to release their references, it does not free the object.
 */
void
gtk_object_destroy (GtkObject *object)
{
  g_return_if_fail (GTK_IS_OBJECT (object));
  
  if (!(GTK_OBJECT_FLAGS (object) & GTK_IN_DESTRUCTION))
    g_object_run_dispose (G_OBJECT (object));
}

static void
gtk_object_dispose (GObject *gobject)
{
  GtkObject *object = GTK_OBJECT (gobject);

  /* guard against reinvocations during
   * destruction with the GTK_IN_DESTRUCTION flag.
   */
  if (!(GTK_OBJECT_FLAGS (object) & GTK_IN_DESTRUCTION))
    {
      GTK_OBJECT_SET_FLAGS (object, GTK_IN_DESTRUCTION);
      
      g_signal_emit (object, object_signals[DESTROY], 0);
      
      GTK_OBJECT_UNSET_FLAGS (object, GTK_IN_DESTRUCTION);
    }

  G_OBJECT_CLASS (gtk_object_parent_class)->dispose (gobject);
}

static void
gtk_object_real_destroy (GtkObject *object)
{
  g_signal_handlers_destroy (object);
}

static void
gtk_object_finalize (GObject *gobject)
{
  GtkObject *object = GTK_OBJECT (gobject);

  if (g_object_is_floating (object))
    {
      g_warning ("A floating object was finalized. This means that someone\n"
		 "called g_object_unref() on an object that had only a floating\n"
		 "reference; the initial floating reference is not owned by anyone\n"
		 "and must be removed with g_object_ref_sink().");
    }
  
  G_OBJECT_CLASS (gtk_object_parent_class)->finalize (gobject);
}
