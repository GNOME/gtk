/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkAccelLabel: GtkLabel with accelerator monitoring facilities.
 * Copyright (C) 1998 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */
#include "gtkaccellabel.h"
#include "gtkaccelmap.h"
#include "gtkmain.h"
#include "gtksignal.h"
#include "gtkintl.h"

#include <string.h>
#include <ctype.h>

enum {
  PROP_0,
  PROP_ACCEL_CLOSURE,
  PROP_ACCEL_WIDGET
};

static void     gtk_accel_label_class_init   (GtkAccelLabelClass *klass);
static void     gtk_accel_label_init         (GtkAccelLabel      *accel_label);
static void     gtk_accel_label_set_property (GObject            *object,
					      guint               prop_id,
					      const GValue       *value,
					      GParamSpec         *pspec);
static void     gtk_accel_label_get_property (GObject            *object,
					      guint               prop_id,
					      GValue             *value,
					      GParamSpec         *pspec);
static void     gtk_accel_label_destroy      (GtkObject          *object);
static void     gtk_accel_label_finalize     (GObject            *object);
static void     gtk_accel_label_size_request (GtkWidget          *widget,
					      GtkRequisition     *requisition);
static gboolean gtk_accel_label_expose_event (GtkWidget          *widget,
					      GdkEventExpose     *event);
static gboolean gtk_accel_label_refetch_idle (GtkAccelLabel      *accel_label);

static GtkAccelLabelClass *accel_label_class = NULL;
static GtkLabelClass *parent_class = NULL;


GtkType
gtk_accel_label_get_type (void)
{
  static GtkType accel_label_type = 0;
  
  if (!accel_label_type)
    {
      static const GtkTypeInfo accel_label_info =
      {
	"GtkAccelLabel",
	sizeof (GtkAccelLabel),
	sizeof (GtkAccelLabelClass),
	(GtkClassInitFunc) gtk_accel_label_class_init,
	(GtkObjectInitFunc) gtk_accel_label_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };
      
      accel_label_type = gtk_type_unique (GTK_TYPE_LABEL, &accel_label_info);
    }
  
  return accel_label_type;
}

static void
gtk_accel_label_class_init (GtkAccelLabelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  
  accel_label_class = class;
  parent_class = g_type_class_peek_parent (class);
  
  gobject_class->finalize = gtk_accel_label_finalize;
  gobject_class->set_property = gtk_accel_label_set_property;
  gobject_class->get_property = gtk_accel_label_get_property;
  
  object_class->destroy = gtk_accel_label_destroy;
   
  widget_class->size_request = gtk_accel_label_size_request;
  widget_class->expose_event = gtk_accel_label_expose_event;

  class->signal_quote1 = g_strdup ("<:");
  class->signal_quote2 = g_strdup (":>");
  class->mod_name_shift = g_strdup ("Shift");
  class->mod_name_control = g_strdup ("Ctrl");
  class->mod_name_alt = g_strdup ("Alt");
  class->mod_separator = g_strdup ("+");
  class->accel_seperator = g_strdup (" / ");
  class->latin1_to_char = TRUE;
  
  g_object_class_install_property (G_OBJECT_CLASS (object_class),
                                   PROP_ACCEL_CLOSURE,
                                   g_param_spec_boxed ("accel_closure",
						       _("Accelerator Closure"),
						       _("The closure to be monitored for accelerator changes"),
						       G_TYPE_CLOSURE,
						       G_PARAM_READABLE | G_PARAM_WRITABLE));
  g_object_class_install_property (G_OBJECT_CLASS (object_class),
                                   PROP_ACCEL_WIDGET,
                                   g_param_spec_object ("accel_widget",
                                                        _("Accelerator Widget"),
                                                        _("The widget to be monitored for accelerator changes"),
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gtk_accel_label_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  GtkAccelLabel  *accel_label;

  accel_label = GTK_ACCEL_LABEL (object);

  switch (prop_id)
    {
    case PROP_ACCEL_CLOSURE:
      gtk_accel_label_set_accel_closure (accel_label, g_value_get_boxed (value));
      break;
    case PROP_ACCEL_WIDGET:
      gtk_accel_label_set_accel_widget (accel_label, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_accel_label_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  GtkAccelLabel  *accel_label;

  accel_label = GTK_ACCEL_LABEL (object);

  switch (prop_id)
    {
    case PROP_ACCEL_CLOSURE:
      g_value_set_boxed (value, accel_label->accel_closure);
      break;
    case PROP_ACCEL_WIDGET:
      g_value_set_object (value, accel_label->accel_widget);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_accel_label_init (GtkAccelLabel *accel_label)
{
  accel_label->queue_id = 0;
  accel_label->accel_padding = 3;
  accel_label->accel_widget = NULL;
  accel_label->accel_closure = NULL;
  accel_label->accel_group = NULL;
  accel_label->accel_string = NULL;
  
  gtk_accel_label_refetch (accel_label);
}

GtkWidget*
gtk_accel_label_new (const gchar *string)
{
  GtkAccelLabel *accel_label;
  
  g_return_val_if_fail (string != NULL, NULL);
  
  accel_label = gtk_type_new (GTK_TYPE_ACCEL_LABEL);
  
  gtk_label_set_text (GTK_LABEL (accel_label), string);
  
  return GTK_WIDGET (accel_label);
}

static void
gtk_accel_label_destroy (GtkObject *object)
{
  GtkAccelLabel *accel_label;
  
  g_return_if_fail (GTK_IS_ACCEL_LABEL (object));
  
  accel_label = GTK_ACCEL_LABEL (object);

  gtk_accel_label_set_accel_widget (accel_label, NULL);
  gtk_accel_label_set_accel_closure (accel_label, NULL);
  
  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtk_accel_label_finalize (GObject *object)
{
  GtkAccelLabel *accel_label = GTK_ACCEL_LABEL (object);

  if (accel_label->queue_id)
    {
      gtk_idle_remove (accel_label->queue_id);
      accel_label->queue_id = 0;
    }
  g_free (accel_label->accel_string);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gtk_accel_label_get_accel_widget:
 * @accel_label: a #GtkAccelLabel
 *
 * Fetches the widget monitored by this accelerator label. See
 * gtk_accel_label_set_accel_widget().
 *
 * Return value: the object monitored by the accelerator label,
 *               or %NULL.
 **/
GtkWidget*
gtk_accel_label_get_accel_widget (GtkAccelLabel *accel_label)
{
  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (accel_label), NULL);

  return accel_label->accel_widget;
}

guint
gtk_accel_label_get_accel_width (GtkAccelLabel *accel_label)
{
  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (accel_label), 0);
  
  return (accel_label->accel_string_width +
	  (accel_label->accel_string_width ? accel_label->accel_padding : 0));
}

static void
gtk_accel_label_size_request (GtkWidget	     *widget,
			      GtkRequisition *requisition)
{
  GtkAccelLabel *accel_label;
  PangoLayout *layout;
  gint width;
  
  g_return_if_fail (GTK_IS_ACCEL_LABEL (widget));
  g_return_if_fail (requisition != NULL);
  
  accel_label = GTK_ACCEL_LABEL (widget);
  
  if (GTK_WIDGET_CLASS (parent_class)->size_request)
    GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);

  layout = gtk_widget_create_pango_layout (widget, accel_label->accel_string);
  pango_layout_get_pixel_size (layout, &width, NULL);
  accel_label->accel_string_width = width;
  
  g_object_unref (G_OBJECT (layout));
}

static gboolean 
gtk_accel_label_expose_event (GtkWidget      *widget,
			      GdkEventExpose *event)
{
  GtkMisc *misc;
  GtkAccelLabel *accel_label;
  PangoLayout *layout;
  
  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  accel_label = GTK_ACCEL_LABEL (widget);
  misc = GTK_MISC (accel_label);
	  
  if (GTK_WIDGET_DRAWABLE (accel_label))
    {
      guint ac_width;
      
      ac_width = gtk_accel_label_get_accel_width (accel_label);
      
      if (widget->allocation.width >= widget->requisition.width + ac_width)
	{
	  guint x;
	  guint y;
	  
	  widget->allocation.width -= ac_width;
	  if (GTK_WIDGET_CLASS (parent_class)->expose_event)
	    GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
	  widget->allocation.width += ac_width;
	  
	  x = widget->allocation.x + widget->allocation.width - misc->xpad - ac_width;
	  
	  y = (widget->allocation.y * (1.0 - misc->yalign) +
	       (widget->allocation.y + widget->allocation.height -
		(widget->requisition.height - misc->ypad * 2)) *
	       misc->yalign) + 1.5;
	  
	  layout = gtk_widget_create_pango_layout (widget, accel_label->accel_string);

          gtk_paint_layout (widget->style,
                            widget->window,
                            GTK_WIDGET_STATE (widget),
			    FALSE,
                            &event->area,
                            widget,
                            "accellabel",
                            x, y,
                            layout);                            

          g_object_unref (G_OBJECT (layout));
	}
      else
	{
	  if (GTK_WIDGET_CLASS (parent_class)->expose_event)
	    GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
	}
    }
  
  return TRUE;
}

static void
refetch_widget_accel_closure (GtkAccelLabel *accel_label)
{
  GSList *slist;

  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));
  g_return_if_fail (GTK_IS_WIDGET (accel_label->accel_widget));
  
  for (slist = _gtk_widget_get_accel_closures (accel_label->accel_widget); slist; slist = slist->next)
    if (gtk_accel_group_from_accel_closure (slist->data))
      {
	/* we just take the first correctly used closure */
	gtk_accel_label_set_accel_closure (accel_label, slist->data);
	return;
      }
  gtk_accel_label_set_accel_closure (accel_label, NULL);
}

void
gtk_accel_label_set_accel_widget (GtkAccelLabel *accel_label,
				  GtkWidget     *accel_widget)
{
  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));
  if (accel_widget)
    g_return_if_fail (GTK_IS_WIDGET (accel_widget));
    
  if (accel_widget != accel_label->accel_widget)
    {
      if (accel_label->accel_widget)
	{
	  gtk_accel_label_set_accel_closure (accel_label, NULL);
	  g_signal_handlers_disconnect_by_func (accel_label->accel_widget,
						G_CALLBACK (refetch_widget_accel_closure),
						accel_label);
	  g_object_unref (accel_label->accel_widget);
	}
      accel_label->accel_widget = accel_widget;
      if (accel_label->accel_widget)
	{
	  g_object_ref (accel_label->accel_widget);
	  g_signal_connect_object (accel_label->accel_widget, "accel_closures_changed",
				   G_CALLBACK (refetch_widget_accel_closure),
				   accel_label, G_CONNECT_SWAPPED);
	  refetch_widget_accel_closure (accel_label);
	}
      g_object_notify (G_OBJECT (accel_label), "accel_widget");
    }
}

static void
gtk_accel_label_queue_refetch (GtkAccelLabel *accel_label)
{
  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));

  if (accel_label->queue_id == 0)
    accel_label->queue_id = gtk_idle_add_priority (G_PRIORITY_HIGH_IDLE,
						   (GtkFunction) gtk_accel_label_refetch_idle,
						   accel_label);
}

static void
check_accel_changed (GtkAccelGroup  *accel_group,
		     guint           keyval,
		     GdkModifierType modifier,
		     GClosure       *accel_closure,
		     GtkAccelLabel  *accel_label)
{
  if (accel_closure == accel_label->accel_closure)
    gtk_accel_label_queue_refetch (accel_label);
}

void
gtk_accel_label_set_accel_closure (GtkAccelLabel *accel_label,
				   GClosure      *accel_closure)
{
  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));
  if (accel_closure)
    g_return_if_fail (gtk_accel_group_from_accel_closure (accel_closure) != NULL);

  if (accel_closure != accel_label->accel_closure)
    {
      if (accel_label->accel_closure)
	{
	  g_signal_handlers_disconnect_by_func (accel_label->accel_group,
						G_CALLBACK (check_accel_changed),
						accel_label);
	  accel_label->accel_group = NULL;
	  g_closure_unref (accel_label->accel_closure);
	}
      accel_label->accel_closure = accel_closure;
      if (accel_label->accel_closure)
	{
	  g_closure_ref (accel_label->accel_closure);
	  accel_label->accel_group = gtk_accel_group_from_accel_closure (accel_closure);
	  g_signal_connect_object (accel_label->accel_group, "accel_changed",
				   G_CALLBACK (check_accel_changed),
				   accel_label, 0);
	}
      gtk_accel_label_queue_refetch (accel_label);
      g_object_notify (G_OBJECT (accel_label), "accel_closure");
    }
}

static gboolean
gtk_accel_label_refetch_idle (GtkAccelLabel *accel_label)
{
  gboolean retval;

  GDK_THREADS_ENTER ();
  retval = gtk_accel_label_refetch (accel_label);
  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean
find_accel (GtkAccelKey *key,
	    GClosure    *closure,
	    gpointer     data)
{
  return data == (gpointer) closure;
}

gboolean
gtk_accel_label_refetch (GtkAccelLabel *accel_label)
{
  GtkAccelLabelClass *class;
  gchar *utf8;

  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (accel_label), FALSE);

  class = GTK_ACCEL_LABEL_GET_CLASS (accel_label);

  g_free (accel_label->accel_string);
  accel_label->accel_string = NULL;

  if (accel_label->accel_closure)
    {
      GtkAccelKey *key = gtk_accel_group_find (accel_label->accel_group, find_accel, accel_label->accel_closure);

      if (key && key->accel_flags & GTK_ACCEL_VISIBLE)
	{
	  GString *gstring;
	  gboolean seen_mod = FALSE;
	  
	  gstring = g_string_new (accel_label->accel_string);
	  g_string_append (gstring, gstring->len ? class->accel_seperator : "   ");
	  
	  if (key->accel_mods & GDK_SHIFT_MASK)
	    {
	      g_string_append (gstring, class->mod_name_shift);
	      seen_mod = TRUE;
	    }
	  if (key->accel_mods & GDK_CONTROL_MASK)
	    {
	      if (seen_mod)
		g_string_append (gstring, class->mod_separator);
	      g_string_append (gstring, class->mod_name_control);
	      seen_mod = TRUE;
	    }
	  if (key->accel_mods & GDK_MOD1_MASK)
	    {
	      if (seen_mod)
		g_string_append (gstring, class->mod_separator);
	      g_string_append (gstring, class->mod_name_alt);
	      seen_mod = TRUE;
	    }
	  if (seen_mod)
	    g_string_append (gstring, class->mod_separator);
	  if (key->accel_key < 0x80 ||
	      (key->accel_key > 0x80 &&
	       key->accel_key <= 0xff &&
	       class->latin1_to_char))
	    {
	      switch (key->accel_key)
		{
		case ' ':
		  g_string_append (gstring, "Space");
		  break;
		case '\\':
		  g_string_append (gstring, "Backslash");
		  break;
		default:
		  g_string_append_c (gstring, toupper (key->accel_key));
		  break;
		}
	    }
	  else
	    {
	      gchar *tmp;
	      
	      tmp = gtk_accelerator_name (key->accel_key, 0);
	      if (tmp[0] != 0 && tmp[1] == 0)
		tmp[0] = toupper (tmp[0]);
	      g_string_append (gstring, tmp);
	      g_free (tmp);
	    }
	  g_free (accel_label->accel_string);
	  accel_label->accel_string = gstring->str;
	  g_string_free (gstring, FALSE);
	}
      if (!accel_label->accel_string)
	accel_label->accel_string = g_strdup ("-/-");
    }
  
  if (!accel_label->accel_string)
    accel_label->accel_string = g_strdup ("");

  utf8 = g_locale_to_utf8 (accel_label->accel_string, -1, NULL, NULL, NULL);
  if (utf8)
    {
      g_free (accel_label->accel_string);
      accel_label->accel_string = utf8;
    }

  if (accel_label->queue_id)
    {
      gtk_idle_remove (accel_label->queue_id);
      accel_label->queue_id = 0;
    }

  gtk_widget_queue_resize (GTK_WIDGET (accel_label));

  return FALSE;
}
