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

#include "gtkalias.h"


enum {
  DESTROY,
  LAST_SIGNAL
};


static void       gtk_object_base_class_init     (GtkObjectClass *class);
static void       gtk_object_base_class_finalize (GtkObjectClass *class);
static void       gtk_object_class_init          (GtkObjectClass *klass);
static void       gtk_object_init                (GtkObject      *object,
						  GtkObjectClass *klass);
static void       gtk_object_dispose            (GObject        *object);
static void       gtk_object_real_destroy        (GtkObject      *object);
static void       gtk_object_finalize            (GObject        *object);

static gpointer    parent_class = NULL;
static guint       object_signals[LAST_SIGNAL] = { 0 };


/****************************************************
 * GtkObject type, class and instance initialization
 *
 ****************************************************/

GType
gtk_object_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
	sizeof (GtkObjectClass),
	(GBaseInitFunc) gtk_object_base_class_init,
	(GBaseFinalizeFunc) gtk_object_base_class_finalize,
	(GClassInitFunc) gtk_object_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkObject),
	16,		/* n_preallocs */
	(GInstanceInitFunc) gtk_object_init,
	NULL,		/* value_table */
      };
      
      object_type = g_type_register_static (G_TYPE_INITIALLY_UNOWNED, I_("GtkObject"), 
					    &object_info, G_TYPE_FLAG_ABSTRACT);
    }

  return object_type;
}

static void
gtk_object_base_class_init (GtkObjectClass *class)
{
}

static void
gtk_object_base_class_finalize (GtkObjectClass *class)
{
}

static void
gtk_object_class_init (GtkObjectClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_ref (G_TYPE_OBJECT);

  gobject_class->dispose = gtk_object_dispose;
  gobject_class->finalize = gtk_object_finalize;

  class->destroy = gtk_object_real_destroy;

  object_signals[DESTROY] =
    g_signal_new (I_("destroy"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		  G_STRUCT_OFFSET (GtkObjectClass, destroy),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
gtk_object_init (GtkObject      *object,
		 GtkObjectClass *klass)
{
}

/********************************************
 * Functions to end a GtkObject's life time
 *
 ********************************************/
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

  G_OBJECT_CLASS (parent_class)->dispose (gobject);
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
  
  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

#define __GTK_OBJECT_C__
#include "gtkaliasdef.c"
