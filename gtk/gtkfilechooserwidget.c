/* GTK - The GIMP Toolkit
 * gtkfilechooserwidget.c: Embeddable file selector widget
 * Copyright (C) 2003, Red Hat, Inc.
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
 */

#include "config.h"
#include "gtkfilechooserprivate.h"

#include "gtkfilechooserwidget.h"
#include "gtkfilechooserdefault.h"
#include "gtkfilechooserutils.h"
#include "gtktypebuiltins.h"
#include "gtkfilechooserembed.h"
#include "gtkorientable.h"
#include "gtkintl.h"


/**
 * SECTION:gtkfilechooserwidget
 * @Short_description: File chooser widget that can be embedded in other widgets
 * @Title: GtkFileChooserWidget
 * @See_also: #GtkFileChooser, #GtkFileChooserDialog
 *
 * #GtkFileChooserWidget is a widget suitable for selecting files.
 * It is the main building block of a #GtkFileChooserDialog.  Most
 * applications will only need to use the latter; you can use
 * #GtkFileChooserWidget as part of a larger window if you have
 * special needs.
 *
 * Note that #GtkFileChooserWidget does not have any methods of its
 * own.  Instead, you should use the functions that work on a
 * #GtkFileChooser.
 */


#define GTK_FILE_CHOOSER_WIDGET_GET_PRIVATE(o)  (GTK_FILE_CHOOSER_WIDGET (o)->priv)

static void gtk_file_chooser_widget_finalize     (GObject                   *object);

static GObject* gtk_file_chooser_widget_constructor  (GType                  type,
						      guint                  n_construct_properties,
						      GObjectConstructParam *construct_params);
static void     gtk_file_chooser_widget_set_property (GObject               *object,
						      guint                  prop_id,
						      const GValue          *value,
						      GParamSpec            *pspec);
static void     gtk_file_chooser_widget_get_property (GObject               *object,
						      guint                  prop_id,
						      GValue                *value,
						      GParamSpec            *pspec);

G_DEFINE_TYPE_WITH_CODE (GtkFileChooserWidget, gtk_file_chooser_widget, GTK_TYPE_BOX,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_CHOOSER,
						_gtk_file_chooser_delegate_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_CHOOSER_EMBED,
						_gtk_file_chooser_embed_delegate_iface_init))

static void
gtk_file_chooser_widget_class_init (GtkFileChooserWidgetClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->constructor = gtk_file_chooser_widget_constructor;
  gobject_class->set_property = gtk_file_chooser_widget_set_property;
  gobject_class->get_property = gtk_file_chooser_widget_get_property;
  gobject_class->finalize = gtk_file_chooser_widget_finalize;

  _gtk_file_chooser_install_properties (gobject_class);

  g_type_class_add_private (class, sizeof (GtkFileChooserWidgetPrivate));
}

static void
gtk_file_chooser_widget_init (GtkFileChooserWidget *chooser_widget)
{
  GtkFileChooserWidgetPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (chooser_widget,
								   GTK_TYPE_FILE_CHOOSER_WIDGET,
								   GtkFileChooserWidgetPrivate);
  chooser_widget->priv = priv;
  gtk_orientable_set_orientation (GTK_ORIENTABLE (chooser_widget),
                                  GTK_ORIENTATION_VERTICAL);
}

static void
gtk_file_chooser_widget_finalize (GObject *object)
{
  GtkFileChooserWidget *chooser = GTK_FILE_CHOOSER_WIDGET (object);

  g_free (chooser->priv->file_system);

  G_OBJECT_CLASS (gtk_file_chooser_widget_parent_class)->finalize (object);
}

static GObject*
gtk_file_chooser_widget_constructor (GType                  type,
				     guint                  n_construct_properties,
				     GObjectConstructParam *construct_params)
{
  GtkFileChooserWidgetPrivate *priv;
  GObject *object;
  
  object = G_OBJECT_CLASS (gtk_file_chooser_widget_parent_class)->constructor (type,
									       n_construct_properties,
									       construct_params);
  priv = GTK_FILE_CHOOSER_WIDGET_GET_PRIVATE (object);

  gtk_widget_push_composite_child ();

  priv->impl = _gtk_file_chooser_default_new ();
  
  gtk_box_pack_start (GTK_BOX (object), priv->impl, TRUE, TRUE, 0);
  gtk_widget_show (priv->impl);

  _gtk_file_chooser_set_delegate (GTK_FILE_CHOOSER (object),
				  GTK_FILE_CHOOSER (priv->impl));

  _gtk_file_chooser_embed_set_delegate (GTK_FILE_CHOOSER_EMBED (object),
					GTK_FILE_CHOOSER_EMBED (priv->impl));
  
  gtk_widget_pop_composite_child ();

  return object;
}

static void
gtk_file_chooser_widget_set_property (GObject         *object,
				      guint            prop_id,
				      const GValue    *value,
				      GParamSpec      *pspec)
{
  GtkFileChooserWidgetPrivate *priv = GTK_FILE_CHOOSER_WIDGET_GET_PRIVATE (object);

  switch (prop_id)
    {
    default:
      g_object_set_property (G_OBJECT (priv->impl), pspec->name, value);
      break;
    }
}

static void
gtk_file_chooser_widget_get_property (GObject         *object,
				      guint            prop_id,
				      GValue          *value,
				      GParamSpec      *pspec)
{
  GtkFileChooserWidgetPrivate *priv = GTK_FILE_CHOOSER_WIDGET_GET_PRIVATE (object);
  
  g_object_get_property (G_OBJECT (priv->impl), pspec->name, value);
}

/**
 * gtk_file_chooser_widget_new:
 * @action: Open or save mode for the widget
 * 
 * Creates a new #GtkFileChooserWidget.  This is a file chooser widget that can
 * be embedded in custom windows, and it is the same widget that is used by
 * #GtkFileChooserDialog.
 * 
 * Return value: a new #GtkFileChooserWidget
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_file_chooser_widget_new (GtkFileChooserAction action)
{
  return g_object_new (GTK_TYPE_FILE_CHOOSER_WIDGET,
		       "action", action,
		       NULL);
}
