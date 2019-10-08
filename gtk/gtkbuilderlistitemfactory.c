/*
 * Copyright © 2019 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkbuilderlistitemfactory.h"

#include "gtkbuilder.h"
#include "gtklistitemfactoryprivate.h"
#include "gtklistitemprivate.h"

/**
 * SECTION:gtkbuilderlistitemfactory
 * @Tiitle: GtkBuilderListItemFactory
 * @Short_description: A listitem factory using ui files
 *
 * #GtkBuilderListItemFactory is a #GtkListItemFactory that creates
 * widgets by instantiating #GtkBuilder UI templates. The templates
 * must be extending #GtkListItem, and typically use #GtkExpressions
 * to obtain data from the items in the model.
 *
 * Example:
 * |[
 *   <interface>
 *     <template class="GtkListItem">
 *       <property name="child">
 *         <object class="GtkLabel">
 *           <property name="xalign">0</property>
 *           <binding name="label">
 *             <lookup name="name" type="SettingsKey">
 *               <lookup name="item">GtkListItem</lookup>
 *             </lookup>
 *           </binding>
 *         </object>
 *       </property>
 *     </template>
 *   </interface>
 * ]|
 */

struct _GtkBuilderListItemFactory
{
  GtkListItemFactory parent_instance;

  GBytes *bytes;
};

struct _GtkBuilderListItemFactoryClass
{
  GtkListItemFactoryClass parent_class;
};

G_DEFINE_TYPE (GtkBuilderListItemFactory, gtk_builder_list_item_factory, GTK_TYPE_LIST_ITEM_FACTORY)

static void
gtk_builder_list_item_factory_setup (GtkListItemFactory *factory,
                                     GtkListItem        *list_item)
{
  GtkBuilderListItemFactory *self = GTK_BUILDER_LIST_ITEM_FACTORY (factory);
  GtkBuilder *builder;
  GError *error = NULL;

  GTK_LIST_ITEM_FACTORY_CLASS (gtk_builder_list_item_factory_parent_class)->setup (factory, list_item);

  builder = gtk_builder_new ();

  gtk_builder_set_current_object (builder, G_OBJECT (list_item));

  if (!gtk_builder_extend_with_template  (builder, GTK_WIDGET (list_item), G_OBJECT_TYPE (list_item),
					  (const gchar *)g_bytes_get_data (self->bytes, NULL),
					  g_bytes_get_size (self->bytes),
					  &error))
    {
      g_critical ("Error building template for list item: %s", error->message);
      g_error_free (error);

      /* This should never happen, if the template XML cannot be built
       * then it is a critical programming error.
       */
      g_object_unref (builder);
      return;
    }

  g_object_unref (builder);
}

static void
gtk_builder_list_item_factory_finalize (GObject *object)
{
  GtkBuilderListItemFactory *self = GTK_BUILDER_LIST_ITEM_FACTORY (object);

  g_bytes_unref (self->bytes);

  G_OBJECT_CLASS (gtk_builder_list_item_factory_parent_class)->finalize (object);
}

static void
gtk_builder_list_item_factory_class_init (GtkBuilderListItemFactoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkListItemFactoryClass *factory_class = GTK_LIST_ITEM_FACTORY_CLASS (klass);

  object_class->finalize = gtk_builder_list_item_factory_finalize;

  factory_class->setup = gtk_builder_list_item_factory_setup;
}

static void
gtk_builder_list_item_factory_init (GtkBuilderListItemFactory *self)
{
}

/**
 * gtk_builder_list_item_factory_new_from_bytes:
 * @bytes: the bytes containing the ui file to instantiate
 *
 * Creates s new #GtkBuilderListItemFactory that instantiates widgets
 * using @bytes as the data to pass to #GtkBuilder.
 *
 * Returns: a new #GtkBuilderListItemFactory
 **/
GtkListItemFactory *
gtk_builder_list_item_factory_new_from_bytes (GBytes *bytes)
{
  GtkBuilderListItemFactory *self;

  g_return_val_if_fail (bytes != NULL, NULL);

  self = g_object_new (GTK_TYPE_BUILDER_LIST_ITEM_FACTORY, NULL);

  self->bytes = g_bytes_ref (bytes);

  return GTK_LIST_ITEM_FACTORY (self);
}

/**
 * gtk_builder_list_item_factory_new_from_resource:
 * @resource_path: valid path to a resource that contains the data
 *
 * Creates s new #GtkBuilderListItemFactory that instantiates widgets
 * using data read from the given @resource_path to pass to #GtkBuilder.
 *
 * Returns: a new #GtkBuilderListItemFactory
 **/
GtkListItemFactory *
gtk_builder_list_item_factory_new_from_resource (const char *resource_path)
{
  GtkListItemFactory *result;
  GError *error = NULL;
  GBytes *bytes;

  g_return_val_if_fail (resource_path != NULL, NULL);

  bytes = g_resources_lookup_data (resource_path, 0, &error);
  if (!bytes)
    {
      g_critical ("Unable to load resource for list item template: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  result = gtk_builder_list_item_factory_new_from_bytes (bytes);

  g_bytes_unref (bytes);

  return result;
}

