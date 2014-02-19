/* GTK - The GIMP Toolkit
 * gtkrecentchooserwidget.c: embeddable recently used resources chooser widget
 * Copyright (C) 2006 Emmanuele Bassi
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

#include "gtkrecentchooserwidget.h"
#include "gtkrecentchooserdefault.h"
#include "gtkrecentchooserutils.h"
#include "gtkorientable.h"
#include "gtktypebuiltins.h"

/**
 * SECTION:gtkrecentchooserwidget
 * @Short_description: Displays recently used files
 * @Title: GtkRecentChooserWidget
 * @See_also:#GtkRecentChooser, #GtkRecentChooserDialog
 *
 * #GtkRecentChooserWidget is a widget suitable for selecting recently used
 * files.  It is the main building block of a #GtkRecentChooserDialog.  Most
 * applications will only need to use the latter; you can use
 * #GtkRecentChooserWidget as part of a larger window if you have special needs.
 *
 * Note that #GtkRecentChooserWidget does not have any methods of its own.
 * Instead, you should use the functions that work on a #GtkRecentChooser.
 *
 * Recently used files are supported since GTK+ 2.10.
 */


struct _GtkRecentChooserWidgetPrivate
{
  GtkRecentManager *manager;
  
  GtkWidget *chooser;
};

static void     gtk_recent_chooser_widget_set_property (GObject               *object,
						        guint                  prop_id,
						        const GValue          *value,
						        GParamSpec            *pspec);
static void     gtk_recent_chooser_widget_get_property (GObject               *object,
						        guint                  prop_id,
						        GValue                *value,
						        GParamSpec            *pspec);
static void     gtk_recent_chooser_widget_finalize     (GObject               *object);


G_DEFINE_TYPE_WITH_CODE (GtkRecentChooserWidget,
		         gtk_recent_chooser_widget,
			 GTK_TYPE_BOX,
                         G_ADD_PRIVATE (GtkRecentChooserWidget)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_RECENT_CHOOSER,
						_gtk_recent_chooser_delegate_iface_init))

static void
gtk_recent_chooser_widget_constructed (GObject *gobject)
{
  GtkRecentChooserWidget *self = GTK_RECENT_CHOOSER_WIDGET (gobject);

  self->priv->chooser = _gtk_recent_chooser_default_new (self->priv->manager);

  gtk_container_add (GTK_CONTAINER (self), self->priv->chooser);
  gtk_widget_show (self->priv->chooser);
  _gtk_recent_chooser_set_delegate (GTK_RECENT_CHOOSER (self),
				    GTK_RECENT_CHOOSER (self->priv->chooser));
}

static void
gtk_recent_chooser_widget_set_property (GObject      *object,
				        guint         prop_id,
				        const GValue *value,
				        GParamSpec   *pspec)
{
  GtkRecentChooserWidgetPrivate *priv;

  priv = gtk_recent_chooser_widget_get_instance_private (GTK_RECENT_CHOOSER_WIDGET (object));
  
  switch (prop_id)
    {
    case GTK_RECENT_CHOOSER_PROP_RECENT_MANAGER:
      priv->manager = g_value_get_object (value);
      break;
    default:
      g_object_set_property (G_OBJECT (priv->chooser), pspec->name, value);
      break;
    }
}

static void
gtk_recent_chooser_widget_get_property (GObject    *object,
				        guint       prop_id,
				        GValue     *value,
				        GParamSpec *pspec)
{
  GtkRecentChooserWidgetPrivate *priv;

  priv = gtk_recent_chooser_widget_get_instance_private (GTK_RECENT_CHOOSER_WIDGET (object));

  g_object_get_property (G_OBJECT (priv->chooser), pspec->name, value);
}

static void
gtk_recent_chooser_widget_finalize (GObject *object)
{
  GtkRecentChooserWidget *self = GTK_RECENT_CHOOSER_WIDGET (object);

  self->priv->manager = NULL;
  
  G_OBJECT_CLASS (gtk_recent_chooser_widget_parent_class)->finalize (object);
}

static void
gtk_recent_chooser_widget_class_init (GtkRecentChooserWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = gtk_recent_chooser_widget_constructed;
  gobject_class->set_property = gtk_recent_chooser_widget_set_property;
  gobject_class->get_property = gtk_recent_chooser_widget_get_property;
  gobject_class->finalize = gtk_recent_chooser_widget_finalize;

  _gtk_recent_chooser_install_properties (gobject_class);
}

static void
gtk_recent_chooser_widget_init (GtkRecentChooserWidget *widget)
{
  widget->priv = gtk_recent_chooser_widget_get_instance_private (widget);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (widget),
                                  GTK_ORIENTATION_VERTICAL);
}

/*
 * Public API
 */

/**
 * gtk_recent_chooser_widget_new:
 * 
 * Creates a new #GtkRecentChooserWidget object.  This is an embeddable widget
 * used to access the recently used resources list.
 *
 * Returns: a new #GtkRecentChooserWidget
 *
 * Since: 2.10
 */
GtkWidget *
gtk_recent_chooser_widget_new (void)
{
  return g_object_new (GTK_TYPE_RECENT_CHOOSER_WIDGET, NULL);
}

/**
 * gtk_recent_chooser_widget_new_for_manager:
 * @manager: a #GtkRecentManager
 *
 * Creates a new #GtkRecentChooserWidget with a specified recent manager.
 *
 * This is useful if you have implemented your own recent manager, or if you
 * have a customized instance of a #GtkRecentManager object.
 *
 * Returns: a new #GtkRecentChooserWidget
 *
 * Since: 2.10
 */
GtkWidget *
gtk_recent_chooser_widget_new_for_manager (GtkRecentManager *manager)
{
  g_return_val_if_fail (manager == NULL || GTK_IS_RECENT_MANAGER (manager), NULL);
  
  return g_object_new (GTK_TYPE_RECENT_CHOOSER_WIDGET,
  		       "recent-manager", manager,
  		       NULL);
}
