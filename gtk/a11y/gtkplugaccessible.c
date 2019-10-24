/* GTK+ - accessibility implementations
 * Copyright 2019 Samuel Thibault <sthibault@hypra.fr>
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

#include <gtk/gtk.h>
#include "gtkplugaccessible.h"

#ifdef GTK_HAVE_ATK_PLUG_SET_CHILD

/* We can not make GtkPlugAccessible inherit both from GtkContainerAccessible
 * and GtkPlug, so we make it the atk child of an AtkPlug */

struct _GtkPlugAccessiblePrivate
{
  AtkObject *accessible_plug;
};

G_DEFINE_TYPE_WITH_CODE (GtkPlugAccessible, gtk_plug_accessible, GTK_TYPE_WINDOW_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkPlugAccessible))


static void
gtk_plug_accessible_finalize (GObject *object)
{
  GtkPlugAccessible *plug = GTK_PLUG_ACCESSIBLE (object);
  GtkPlugAccessiblePrivate *priv = plug->priv;

  g_clear_object (&priv->accessible_plug);

  G_OBJECT_CLASS (gtk_plug_accessible_parent_class)->finalize (object);
}

static void
gtk_plug_accessible_initialize (AtkObject *plug, gpointer data)
{
  AtkObject *atk_plug;

  ATK_OBJECT_CLASS (gtk_plug_accessible_parent_class)->initialize (plug, data);

  atk_plug = atk_plug_new ();
  atk_plug_set_child (ATK_PLUG (atk_plug), plug);
  GTK_PLUG_ACCESSIBLE (plug)->priv->accessible_plug = atk_plug;
}

static void
gtk_plug_accessible_class_init (GtkPlugAccessibleClass *klass) {
  AtkObjectClass *atk_class     = ATK_OBJECT_CLASS (klass);
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);

  atk_class->initialize   = gtk_plug_accessible_initialize;
  gobject_class->finalize = gtk_plug_accessible_finalize;
}

static void
gtk_plug_accessible_init (GtkPlugAccessible *plug)
{
  plug->priv = gtk_plug_accessible_get_instance_private (plug);
}

gchar *
gtk_plug_accessible_get_id (GtkPlugAccessible *plug)
{
  return atk_plug_get_id (ATK_PLUG (plug->priv->accessible_plug));
}

#endif /* GTK_HAVE_ATK_PLUG_SET_CHILD */
