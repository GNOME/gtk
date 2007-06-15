/* GTK - The GIMP Toolkit
 * Copyright (C) 1998-2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2006-2007 Async Open Source,
 *                         Johan Dahlin <jdahlin@async.com.br>,
 *                         Henrique Romano <henrique@async.com.br>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <ctype.h> /* tolower, toupper */
#include <errno.h> /* errno */
#include <stdlib.h> /* strtol, strtoul */
#include <string.h> /* strlen */

#include "gtkbuilder.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkmain.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkalias.h"

static void gtk_builder_class_init     (GtkBuilderClass *klass);
static void gtk_builder_init           (GtkBuilder      *builder);
static void gtk_builder_finalize       (GObject         *object);
static void gtk_builder_set_property   (GObject         *object,
                                        guint            prop_id,
                                        const GValue    *value,
                                        GParamSpec      *pspec);
static void gtk_builder_get_property   (GObject         *object,
                                        guint            prop_id,
                                        GValue          *value,
                                        GParamSpec      *pspec);
static GType gtk_builder_real_get_type_from_name   (GtkBuilder      *builder,
                                                    const char      *typename);
static gint _gtk_builder_enum_from_string (GType type, const char *string);


enum {
  PROP_0,
  PROP_TRANSLATION_DOMAIN,
};

struct _GtkBuilderPrivate
{
  gchar *domain;
  GHashTable *objects;
  GHashTable *delayed_properties;
  GSList *signals;
  gchar *current_toplevel;
};

G_DEFINE_TYPE (GtkBuilder, gtk_builder, G_TYPE_OBJECT)

static void
gtk_builder_class_init (GtkBuilderClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_builder_finalize;
  gobject_class->set_property = gtk_builder_set_property;
  gobject_class->get_property = gtk_builder_get_property;

  klass->get_type_from_name = gtk_builder_real_get_type_from_name;

  g_object_class_install_property (gobject_class,
                                   PROP_TRANSLATION_DOMAIN,
                                   g_param_spec_string ("translation-domain",
                                                        P_("Translation Domain"),
                                                        P_("The translation domain used by gettext"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  g_type_class_add_private (gobject_class, sizeof (GtkBuilderPrivate));
}

static void
gtk_builder_init (GtkBuilder *builder)
{
  builder->priv = G_TYPE_INSTANCE_GET_PRIVATE (builder, GTK_TYPE_BUILDER,
                                               GtkBuilderPrivate);
  builder->priv->domain = NULL;
  builder->priv->objects = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free, NULL);
  builder->priv->delayed_properties = g_hash_table_new_full (g_str_hash,
                                                             g_str_equal,
                                                             g_free, NULL);
}


/*
 * GObject virtual methods
 */

static void
gtk_builder_finalize (GObject *object)
{
  GtkBuilder *builder = GTK_BUILDER (object);
  
  g_free (builder->priv->domain);

  g_free (builder->priv->current_toplevel);
  g_hash_table_destroy (builder->priv->delayed_properties);
  builder->priv->delayed_properties = NULL;
  g_slist_foreach (builder->priv->signals, (GFunc)_free_signal_info, NULL);
  g_slist_free (builder->priv->signals);
  g_hash_table_destroy (builder->priv->objects);
}

static void
gtk_builder_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtkBuilder *builder = GTK_BUILDER (object);

  switch (prop_id)
    {
    case PROP_TRANSLATION_DOMAIN:
      gtk_builder_set_translation_domain (builder, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_builder_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtkBuilder *builder = GTK_BUILDER (object);

  switch (prop_id)
    {
    case PROP_TRANSLATION_DOMAIN:
      g_value_set_string (value, builder->priv->domain);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


/*
 * Try to map a type name to a _get_type function
 * and call it, eg:
 *
 * GtkWindow -> gtk_window_get_type
 * GtkHBox -> gtk_hbox_get_type
 * GtkUIManager -> gtk_ui_manager_get_type
 *
 */
static GType
_gtk_builder_resolve_type_lazily (const gchar *name)
{
  static GModule *module = NULL;
  GTypeGetFunc func;
  GString *symbol_name = g_string_new ("");
  char c, *symbol;
  int i;
  GType gtype = G_TYPE_INVALID;

  if (!module)
    module = g_module_open (NULL, 0);
  
  for (i = 0; name[i] != '\0'; i++)
    {
      c = name[i];
      /* skip if uppercase, first or previous is uppercase */
      if ((c == toupper (c) &&
           i > 0 && name[i-1] != toupper (name[i-1])) ||
          (i > 2 && name[i]   == toupper (name[i]) &&
           name[i-1] == toupper (name[i-1]) &&
           name[i-2] == toupper (name[i-2])))
        g_string_append_c (symbol_name, '_');
      g_string_append_c (symbol_name, tolower (c));
    }
  g_string_append (symbol_name, "_get_type");
  
  symbol = g_string_free (symbol_name, FALSE);

  if (g_module_symbol (module, symbol, (gpointer)&func))
    gtype = func ();
  
  g_free (symbol);

  return gtype;
}

/*
 * GtkBuilder virtual methods
 */

static GType
gtk_builder_real_get_type_from_name (GtkBuilder *builder, const char *typename)
{
  GType gtype;

  gtype = g_type_from_name (typename);
  if (gtype != G_TYPE_INVALID)
    return gtype;

  return _gtk_builder_resolve_type_lazily (typename);
}

typedef struct
{
  gchar *object;
  gchar *name;
  gchar *value;
} DelayedProperty;

static void
gtk_builder_get_parameters (GtkBuilder  *builder,
                            GType        object_type,
                            const gchar *object_name,
                            GSList      *properties,
                            GArray      **parameters,
                            GArray      **construct_parameters)
{
  GSList *l;
  GParamSpec *pspec;
  GObjectClass *oclass;
  DelayedProperty *property;

  oclass = g_type_class_ref (object_type);
  g_assert (oclass != NULL);

  *parameters = g_array_new (FALSE, FALSE, sizeof (GParameter));
  *construct_parameters = g_array_new (FALSE, FALSE, sizeof (GParameter));

  for (l = properties; l; l = l->next)
    {
      PropertyInfo *prop = (PropertyInfo*)l->data;
      GParameter parameter = { NULL };

      pspec = g_object_class_find_property (G_OBJECT_CLASS (oclass),
                                            prop->name);
      if (!pspec)
        {
          g_warning ("Unknown property: %s.%s\n",
                     g_type_name (object_type), prop->name);
          continue;
        }

      parameter.name = prop->name;

      if (G_IS_PARAM_SPEC_OBJECT (pspec))
        {
          if (pspec->flags & G_PARAM_CONSTRUCT_ONLY)
            {
              GObject *object;
              object = gtk_builder_get_object (builder, prop->data);
              if (!object)
                {
                  g_warning ("failed to get constuct only property %s of %s "
                             "with value `%s'",
                             prop->name, object_name, prop->data);
                  continue;
                }
              g_value_init (&parameter.value, G_OBJECT_TYPE (object));
              g_value_set_object (&parameter.value, g_object_ref (object));
            }
          else
            {
              GSList *delayed_properties;
              
              delayed_properties = g_hash_table_lookup (builder->priv->delayed_properties,
                                                        builder->priv->current_toplevel);
              property = g_slice_new (DelayedProperty);
              property->object = g_strdup (object_name);
              property->name = g_strdup (prop->name);
              property->value = g_strdup (prop->data);
              delayed_properties = g_slist_prepend (delayed_properties, property);
              g_hash_table_insert (builder->priv->delayed_properties,
                                   g_strdup (builder->priv->current_toplevel),
                                   delayed_properties);
              continue;
            }
        }
      else if (!gtk_builder_value_from_string (pspec, prop->data, &parameter.value))
        {
          g_warning ("failed to set property %s.%s to %s",
                     g_type_name (object_type), prop->name, prop->data);
          continue;
        }

      if (pspec->flags & G_PARAM_CONSTRUCT_ONLY)
        g_array_append_val (*construct_parameters, parameter);
      else
        g_array_append_val (*parameters, parameter);
    }

  g_type_class_unref (oclass);
}

static GObject *
gtk_builder_get_internal_child (GtkBuilder  *builder,
                                ObjectInfo  *info,
                                const gchar *childname)
{
  GObject *obj = NULL;

  while (!obj)
    {
      if (!info->parent)
        break;

      info = (ObjectInfo*)((ChildInfo*)info->parent)->parent;
      if (!info)
        break;

      GTK_NOTE (BUILDER,
                g_print ("Trying to get internal child %s from %s\n",
                         childname,
                         gtk_buildable_get_name (GTK_BUILDABLE (info->object))));

      if (GTK_IS_BUILDABLE (info->object))
          obj = gtk_buildable_get_internal_child (GTK_BUILDABLE (info->object),
                                                  builder,
                                                  childname);
    };

  if (!obj)
    g_error ("Unknown internal child: %s\n", childname);

  return obj;
}

GObject *
_gtk_builder_construct (GtkBuilder *builder,
                        ObjectInfo *info)
{
  GArray *parameters, *construct_parameters;
  GType object_type;
  GObject *obj;
  int i;
  GtkBuildableIface *iface;
  gboolean custom_set_property;
  GtkBuildable *buildable;

  g_assert (info->class_name != NULL);
  object_type = gtk_builder_get_type_from_name (builder, info->class_name);
  if (object_type == G_TYPE_INVALID)
    g_error ("Invalid type: %s", info->class_name);

  gtk_builder_get_parameters (builder, object_type,
                              info->id,
                              info->properties,
                              &parameters,
                              &construct_parameters);

  if (info->constructor)
    {
      GObject *constructor;

      constructor = gtk_builder_get_object (builder, info->constructor);
      if (constructor == NULL)
        g_error ("Unknown constructor for %s: %s\n", info->id,
                 info->constructor);

      obj = gtk_buildable_construct_child (GTK_BUILDABLE (constructor),
                                           builder,
                                           info->id);
      g_assert (obj != NULL);
      if (construct_parameters->len)
        g_warning ("Can't pass in construct-only parameters to %s", info->id);

    }
  else if (info->parent && ((ChildInfo*)info->parent)->internal_child != NULL)
    {
      gchar *childname = ((ChildInfo*)info->parent)->internal_child;
      obj = gtk_builder_get_internal_child (builder, info, childname);
      if (construct_parameters->len)
        g_warning ("Can't pass in construct-only parameters to %s", childname);
    }
  else
    {
      obj = g_object_newv (object_type,
                           construct_parameters->len,
                           (GParameter *)construct_parameters->data);

      GTK_NOTE (BUILDER,
                g_print ("created %s of type %s\n", info->id, info->class_name));

      for (i = 0; i < construct_parameters->len; i++)
        {
          GParameter *param = &g_array_index (construct_parameters,
                                              GParameter, i);
          g_value_unset (&param->value);
        }
    }
  g_array_free (construct_parameters, TRUE);

  custom_set_property = FALSE;
  buildable = NULL;
  iface = NULL;
  if (GTK_IS_BUILDABLE (obj))
    {
      buildable = GTK_BUILDABLE (obj);
      iface = GTK_BUILDABLE_GET_IFACE (obj);
      if (iface->set_property)
        custom_set_property = TRUE;
    }

  for (i = 0; i < parameters->len; i++)
    {
      GParameter *param = &g_array_index (parameters, GParameter, i);
      if (custom_set_property)
        iface->set_property (buildable, builder, param->name, &param->value);
      else
        g_object_set_property (obj, param->name, &param->value);

#if G_ENABLE_DEBUG
      if (gtk_debug_flags & GTK_DEBUG_BUILDER)
        {
          gchar *str = g_strdup_value_contents ((const GValue*)&param->value);
          g_print ("set %s: %s = %s\n", info->id, param->name, str);
          g_free (str);
        }
#endif      
      g_value_unset (&param->value);
    }
  g_array_free (parameters, TRUE);
  
  if (GTK_IS_BUILDABLE (obj))
    gtk_buildable_set_name (buildable, info->id);
  else
    g_object_set_data_full (obj,
                            "gtk-builder-name",
                            g_strdup (info->id),
                            g_free);

  if (!info->parent)
    {
      g_free (builder->priv->current_toplevel);
      builder->priv->current_toplevel = g_strdup (info->id);
    }
  g_hash_table_insert (builder->priv->objects, g_strdup (info->id), obj);
  
  builder->priv->signals = g_slist_concat (builder->priv->signals,
                                           g_slist_copy (info->signals));
  return obj;
}


void
_gtk_builder_add (GtkBuilder *builder,
                  ChildInfo  *child_info)
{
  GObject *object;
  GObject *parent;

  /* Internal children are already added
   * Also prevent us from being called twice.
   */
  if (!child_info ||
      child_info->internal_child ||
      child_info->added)
    return;

  object = child_info->object;
  if (!object)
    return;

  if (!child_info->parent)
    {
      g_warning ("%s: Not adding, No parent\n",
                 gtk_buildable_get_name (GTK_BUILDABLE (object)));
      return;
    }

  g_assert (object != NULL);

  parent = ((ObjectInfo*)child_info->parent)->object;
  g_assert (GTK_IS_BUILDABLE (parent));

  GTK_NOTE (BUILDER,
            g_print ("adding %s to %s\n",
                     gtk_buildable_get_name (GTK_BUILDABLE (object)),
                     gtk_buildable_get_name (GTK_BUILDABLE (parent))));
  
  gtk_buildable_add (GTK_BUILDABLE (parent), builder, object,
                     child_info->type);

  child_info->added = TRUE;
}

static void
apply_delayed_properties (const gchar *window_name,
                          GSList      *props,
                          GtkBuilder  *builder)
{
  GSList *l;
  DelayedProperty *property;
  GObject *object;
  GType object_type;
  GObjectClass *oclass;
  GParamSpec *pspec;

  g_assert (props != NULL);
  props = g_slist_reverse (props);
  for (l = props; l; l = l->next)
    {
      property = (DelayedProperty*)l->data;
      object = g_hash_table_lookup (builder->priv->objects, property->object);
      g_assert (object != NULL);

      object_type = G_OBJECT_TYPE (object);
      g_assert (object_type != G_TYPE_INVALID);

      oclass = g_type_class_ref (object_type);
      g_assert (oclass != NULL);

      pspec = g_object_class_find_property (G_OBJECT_CLASS (oclass),
                                            property->name);
      if (!pspec)
        g_warning ("Unknown property: %s.%s\n", g_type_name (object_type),
                   property->name);
      else
        {
          GObject *obj;

          obj = g_hash_table_lookup (builder->priv->objects, property->value);
          if (!obj)
            g_warning ("No object called: %s\n", property->object);
          else
            g_object_set (object, property->name, obj, NULL);
        }
      g_free (property->value);
      g_free (property->object);
      g_free (property->name);
      g_slice_free (DelayedProperty, property);
      g_type_class_unref (oclass);
    }
  g_slist_free (props);
}

void
_gtk_builder_finish (GtkBuilder  *builder)
{
  if (builder->priv->delayed_properties)
    g_hash_table_foreach (builder->priv->delayed_properties,
                          (GHFunc)apply_delayed_properties, builder);
}

/**
 * gtk_builder_new:
 *
 * Creates a new builder object.
 *
 * Return value: a new builder object.
 *
 * Since: 2.12
 **/
GtkBuilder *
gtk_builder_new (void)
{
  return g_object_new (GTK_TYPE_BUILDER, NULL);
}

/**
 * gtk_builder_add_from_file:
 * @builder: a #GtkBuilder
 * @filename: the name of the file to parse
 * @error: return location for an error
 *
 * Parses a string containing a <link linkend="BUILDER-UI">GtkBuilder UI definition</link> and 
 * merges it with the current contents of @builder. 
 * 
 * Returns: A positive value on success, 0 if an error occurred
 *
 * Since: 2.12
 **/
guint
gtk_builder_add_from_file (GtkBuilder   *builder,
                           const gchar  *filename,
                           GError      **error)
{
  char *buffer;
  unsigned length;
  GError *tmp_error;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (filename != NULL, 0);

  tmp_error = NULL;

  if (!g_file_get_contents (filename, &buffer, &length, &tmp_error))
    {
      g_propagate_error (error, tmp_error);
      return 0;
    }
  
  _gtk_builder_parser_parse_buffer (builder, filename,
                                    buffer, length,
                                    &tmp_error);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return 0;
    }

  g_free (buffer);

  return 1;
}

/**
 * gtk_builder_add_from_string:
 * @builder: a #GtkBuilder
 * @buffer: the string to parse
 * @length: the length of @buffer (may be -1 if @buffer is nul-terminated)
 * @error: return location for an error
 *
 * Parses a file containing a <link linkend="BUILDER-UI">GtkBuilder UI definition</link> and 
 * merges it with the current contents of @builder. 
 * 
 * Returns: A positive value on success, 0 if an error occurred
 *
 * Since: 2.12
 **/
guint
gtk_builder_add_from_string (GtkBuilder   *builder,
                             const gchar  *buffer,
                             gsize         length,
                             GError      **error)
{
  GError *tmp_error;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (buffer != NULL, 0);

  tmp_error = NULL;

  _gtk_builder_parser_parse_buffer (builder, "<input>",
                                    buffer, length,
                                    &tmp_error);
  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return 0;
    }

  return 1;
}

/**
 * gtk_builder_get_object:
 * @builder: a #GtkBuilder
 * @name: name of object to get
 *
 * Return value: GObject or %NULL if it could not be found in the object tree.
 *
 * Since: 2.12
 **/
GObject *
gtk_builder_get_object (GtkBuilder  *builder,
                        const gchar *name)
{
  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_hash_table_lookup (builder->priv->objects, name);
}

static void
object_add_to_list (gchar *object_id,
                    GObject *object,
                    GSList **list)
{
  *list = g_slist_prepend (*list, object);
}

/**
 * gtk_builder_get_objects:
 * @builder: a #GtkBuilder
 *
 * Return value: a newly-allocated #GSList containing all the objects
 *   constructed by GtkBuilder instance.
 *
 * Since: 2.12
 **/
GSList *
gtk_builder_get_objects (GtkBuilder *builder)
{
  GSList *objects = NULL;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);

  g_hash_table_foreach (builder->priv->objects, (GHFunc)object_add_to_list, &objects);

  return g_slist_reverse (objects);
}

/**
 * gtk_builder_set_translation_domain:
 * @builder: a #GtkBuilder
 * @domain: the translation domain or %NULL
 *
 * Sets the translation domain and uses dgettext() for translating the
 * property values marked as translatable from an interface description.
 * You can also pass in %NULL to this method to use gettext() instead of
 * dgettext().
 *
 * Since: 2.12
 **/
void
gtk_builder_set_translation_domain (GtkBuilder  *builder,
                                    const gchar *domain)
{
  gchar *new_domain;
    
  g_return_if_fail (GTK_IS_BUILDER (builder));

  new_domain = g_strdup (domain);
  g_free (builder->priv->domain);
  builder->priv->domain = new_domain;

  g_object_notify (G_OBJECT (builder), "translation-domain");
}

/**
 * gtk_builder_get_translation_domain:
 * @builder: a #GtkBuilder
 *
 * Return value : the translation domain. This string is owned
 * by the builder object and must not be modified or freed.
 *
 * Since: 2.12
 **/
const gchar *
gtk_builder_get_translation_domain (GtkBuilder *builder)
{
  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);

  return builder->priv->domain;
}

typedef struct {
  GModule *module;
  gpointer data;
} connect_args;

static void
gtk_builder_connect_signals_default (GtkBuilder    *builder,
				     GObject       *object,
				     const gchar   *signal_name,
				     const gchar   *handler_name,
				     GObject       *connect_object,
				     GConnectFlags  flags,
				     gpointer       user_data)
{
  GCallback func;
  connect_args *args = (connect_args*)user_data;
  
  if (!g_module_symbol (args->module, handler_name, (gpointer)&func))
    {
      g_warning ("could not find signal handler '%s'", handler_name);
      return;
    }

  if (connect_object)
    g_signal_connect_object (object, signal_name, func, connect_object, flags);
  else
    g_signal_connect_data (object, signal_name, func, args->data, NULL, flags);
}


/**
 * gtk_builder_connect_signals:
 * @builder: a #GtkBuilder
 * @user_data: a pointer to a structure sent in as user data to all signals
 *
 * This method is a simpler variation of gtk_builder_connect_signals_full().
 * It uses #GModule's introspective features (by opening the module %NULL) to
 * look at the application's symbol table.  From here it tries to match
 * the signal handler names given in the interface description with
 * symbols in the application and connects the signals.
 * 
 * Note that this function will not work correctly if #GModule is not
 * supported on the platform.
 *
 * Since: 2.12
 **/
void
gtk_builder_connect_signals (GtkBuilder *builder,
			     gpointer    user_data)
{
  connect_args *args;
  
  g_return_if_fail (GTK_IS_BUILDER (builder));
  
  if (!g_module_supported ())
    g_error ("gtk_builder_connect_signals requires working GModule");

  args = g_slice_new0 (connect_args);
  args->module = g_module_open (NULL, G_MODULE_BIND_LAZY);
  args->data = user_data;
  
  gtk_builder_connect_signals_full (builder,
                                    gtk_builder_connect_signals_default,
                                    args);
  g_module_close (args->module);

  g_slice_free (connect_args, args);
}

/**
 * GtkBuilderConnectFunc:
 * @builder: a #GtkBuilder
 * @object: a GObject subclass to connect a signal to
 * @signal_name: name of the signal
 * @handler_name: name of the handler
 * @connect_object: GObject, if non-%NULL, use g_signal_connect_object.
 * @flags: #GConnectFlags to use
 * @user_data: user data
 *
 * This is the signature of a function used to connect signals.  It is used
 * by the gtk_builder_connect_signals() and gtk_builder_connect_signals_full()
 * methods.  It is mainly intended for interpreted language bindings, but
 * could be useful where the programmer wants more control over the signal
 * connection process.
 *
 * Since: 2.12
 */

/**
 * gtk_builder_connect_signals_full:
 * @builder: a #GtkBuilder
 * @func: the function used to connect the signals.
 * @user_data: arbitrary data that will be passed to the connection function.
 *
 * This function can be thought of the interpreted language binding
 * version of gtk_builder_signal_autoconnect(), except that it does not
 * require gmodule to function correctly.
 *
 * Since: 2.12
 */
void
gtk_builder_connect_signals_full (GtkBuilder            *builder,
                                  GtkBuilderConnectFunc  func,
                                  gpointer               user_data)
{
  GSList *l;
  GObject *object;
  GObject *connect_object;
  
  g_return_if_fail (GTK_IS_BUILDER (builder));
  g_return_if_fail (func != NULL);
  
  if (!builder->priv->signals)
    return;

  builder->priv->signals = g_slist_reverse (builder->priv->signals);
  for (l = builder->priv->signals; l; l = l->next)
    {
      SignalInfo *signal = (SignalInfo*)l->data;

      g_assert (signal != NULL);
      g_assert (signal->name != NULL);

      object = g_hash_table_lookup (builder->priv->objects,
				    signal->object_name);
      g_assert (object != NULL);

      connect_object = NULL;
      
      if (signal->connect_object_name)
	{
	  connect_object = g_hash_table_lookup (builder->priv->objects,
						signal->connect_object_name);
	  if (!connect_object)
	      g_warning ("could not lookup object %s on signal %s of object %s",
			 signal->connect_object_name, signal->name,
			 signal->object_name);
	}
						  
      func (builder, object, signal->name, signal->handler, 
	    connect_object, signal->flags, user_data);
    }

  g_slist_foreach (builder->priv->signals, (GFunc)_free_signal_info, NULL);
  g_slist_free (builder->priv->signals);
  builder->priv->signals = NULL;
}

/**
 * gtk_builder_value_from_string
 * @pspec: the GParamSpec for the property
 * @string: the string representation of the value.
 * @value: the GValue to store the result in.
 *
 * This function demarshals a value from a string.  This function
 * calls g_value_init() on the @value argument, so it need not be
 * initialised beforehand.
 *
 * This function can handle char, uchar, boolean, int, uint, long,
 * ulong, enum, flags, float, double, string, GdkColor and
 * GtkAdjustment type values.  Support for GtkWidget type values is
 * still to come.
 *
 * Returns: %TRUE on success.
 *
 * Since: 2.12
 */
gboolean
gtk_builder_value_from_string (GParamSpec  *pspec,
                               const gchar *string,
                               GValue      *value)
{
  /*
   * GParamSpecUnichar has the internal type G_TYPE_UINT,
   * so we cannot handle this in the switch, do it separately
   */
  if (G_IS_PARAM_SPEC_UNICHAR (pspec))
    {
      gunichar c;
      g_value_init (value, G_TYPE_UINT);
      c = g_utf8_get_char_validated (string, strlen (string));
      if (c > 0)
        g_value_set_uint (value, c);
      return TRUE;
    }

  return gtk_builder_value_from_string_type (G_PARAM_SPEC_VALUE_TYPE (pspec),
                                             string, value);
}

/**
 * gtk_builder_value_from_string_type
 * @type: the GType of the value
 * @string: the string representation of the value.
 * @value: the GValue to store the result in.
 *
 * Like gtk_builder_value_from_string(), but takes a #GType instead of #GParamSpec.
 *
 * Returns: %TRUE on success.
 *
 * Since: 2.12
 */
gboolean
gtk_builder_value_from_string_type (GType        type,
                                    const gchar *string,
                                    GValue      *value)
{
  gboolean ret = TRUE;

  g_return_val_if_fail (type != G_TYPE_INVALID, FALSE);
  g_return_val_if_fail (string != NULL, FALSE);

  g_value_init (value, type);

  switch (G_TYPE_FUNDAMENTAL (type))
    {
    case G_TYPE_CHAR:
      g_value_set_char (value, string[0]);
      break;
    case G_TYPE_UCHAR:
      g_value_set_uchar (value, (guchar)string[0]);
      break;
    case G_TYPE_BOOLEAN:
      {
        gboolean b;

        if (g_ascii_tolower (string[0]) == 't')
          b = TRUE;
        else if (g_ascii_tolower (string[0]) == 'y')
          b = FALSE;
        else {
          errno = 0;
          b = strtol (string, NULL, 0);
          if (errno) {
            g_warning ("could not parse int `%s'", string);
            break;
          }
        }
        g_value_set_boolean (value, b);
        break;
      }
    case G_TYPE_INT:
    case G_TYPE_LONG:
      {
        long l;
        errno = 0;
        l = strtol (string, NULL, 0);
        if (errno) {
          g_warning ("could not parse long `%s'", string);
          break;
        }
        if (G_VALUE_HOLDS_INT (value))
          g_value_set_int (value, l);
        else
          g_value_set_long (value, l);
        break;
      }
    case G_TYPE_UINT:
    case G_TYPE_ULONG:
      {
        ulong ul;
        errno = 0;
        ul = strtoul (string, NULL, 0);
        if (errno)
          {
            g_warning ("could not parse ulong `%s'", string);
            break;
          }
        if (G_VALUE_HOLDS_UINT (value))
          g_value_set_uint (value, strtoul (string, NULL, 0));
        else 
          g_value_set_ulong (value, strtoul (string, NULL, 0));
        break;
      }
    case G_TYPE_ENUM:
      g_value_set_enum (value, _gtk_builder_enum_from_string (type, string));
      break;
    case G_TYPE_FLAGS:
      g_value_set_flags (value, _gtk_builder_flags_from_string (type, string));
      break;
    case G_TYPE_FLOAT:
    case G_TYPE_DOUBLE:
      {
        double d;
        errno = 0;
        d = g_ascii_strtod (string, NULL);
        if (errno)
          {
            g_warning ("could not parse double `%s'", string);
            break;
          }
        if (G_VALUE_HOLDS_FLOAT (value))
          g_value_set_float (value, d);
        else
          g_value_set_double (value, d);
        break;
      }
    case G_TYPE_STRING:
      g_value_set_string (value, string);
      break;
    case G_TYPE_BOXED:
      if (G_VALUE_HOLDS (value, GDK_TYPE_COLOR))
        {
          GdkColor colour = { 0, };

          if (gdk_color_parse (string, &colour) &&
              gdk_colormap_alloc_color (gtk_widget_get_default_colormap (),
                                        &colour, FALSE, TRUE))
            g_value_set_boxed (value, &colour);
          else
            {
              g_warning ("could not parse colour name `%s'", string);
              ret = FALSE;
            }
        }
      else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
        {
          char **vector = g_strsplit (string, "\n", 0);
          g_value_take_boxed (value, vector);
        }
      else
        ret = FALSE;
      break;
    case G_TYPE_OBJECT:
#if 0
        if (G_VALUE_HOLDS (value, GDK_TYPE_PIXBUF))
      {
        gchar *filename;
        GError *error = NULL;
        GdkPixbuf *pixbuf;

        filename = gtk_xml_relative_file (xml, string);
        pixbuf = gdk_pixbuf_new_from_file (filename, &error);
        if (pixbuf)
          {
            g_value_set_object (value, pixbuf);
            g_object_unref (G_OBJECT (pixbuf));
          }
        else
          {
            g_warning ("Error loading image: %s", error->message);
            g_error_free (error);
            ret = FALSE;
          }
        g_free (filename);
      }
        else
#endif
          ret = FALSE;
      break;
    default:
      ret = FALSE;
      break;
    }

  return ret;
}

static gint
_gtk_builder_enum_from_string (GType type, const char *string)
{
  GEnumClass *eclass;
  GEnumValue *ev;
  gchar *endptr;
  gint ret = 0;

  g_return_val_if_fail (G_TYPE_IS_ENUM (type), 0);
  g_return_val_if_fail (string != NULL, 0);
  
  ret = strtoul (string, &endptr, 0);
  if (endptr != string) /* parsed a number */
    return ret;

  eclass = g_type_class_ref (type);
  ev = g_enum_get_value_by_name (eclass, string);
  if (!ev)
    ev = g_enum_get_value_by_nick (eclass, string);

  if (ev)
    ret = ev->value;

  g_type_class_unref (eclass);

  return ret;
}

guint
_gtk_builder_flags_from_string (GType type, const char *string)
{
  GFlagsClass *fclass;
  gchar *endptr, *prevptr;
  guint i, j, ret;
  char *flagstr;
  GFlagsValue *fv;
  const char  *flag;
  gunichar ch;
  gboolean eos;

  g_return_val_if_fail (G_TYPE_IS_FLAGS (type), 0);
  g_return_val_if_fail (string != 0, 0);

  ret = strtoul (string, &endptr, 0);
  if (endptr != string) /* parsed a number */
    return ret;

  fclass = g_type_class_ref (type);

  flagstr = g_strdup (string);
  for (ret = i = j = 0; ; i++)
    {

      eos = flagstr[i] == '\0';

      if (!eos && flagstr[i] != '|')
        continue;

      flag = &flagstr[j];
      endptr = &flagstr[i];

      if (!eos)
        {
          flagstr[i++] = '\0';
          j = i;
        }

      /* trim spaces */
      for (;;)
        {
          ch = g_utf8_get_char (flag);
          if (!g_unichar_isspace (ch))
            break;
          flag = g_utf8_next_char (flag);
        }

      while (endptr > flag)
        {
          prevptr = g_utf8_prev_char (endptr);
          ch = g_utf8_get_char (prevptr);
          if (!g_unichar_isspace (ch))
            break;
          endptr = prevptr;
        }

      if (endptr > flag)
        {
          *endptr = '\0';
          fv = g_flags_get_value_by_name (fclass, flag);

          if (!fv)
            fv = g_flags_get_value_by_nick (fclass, flag);

          if (fv)
            ret |= fv->value;
          else
            g_warning ("Unknown flag: '%s'", flag);
        }

      if (eos)
        break;
    }

  g_free (flagstr);

  g_type_class_unref (fclass);

  return ret;
}

/**
 * gtk_builder_get_type_from_name:
 * @builder: a #GtkBuilder
 * @typename: Type name to lookup.
 *
 * This method is used to lookup a type. It can be implemented in a subclass to
 * override the #GType of an object created by the builder.
 *
 * Since 2.12
 */
GType
gtk_builder_get_type_from_name (GtkBuilder *builder, const gchar *typename)
{
  g_return_val_if_fail (GTK_IS_BUILDER (builder), G_TYPE_INVALID);
  g_return_val_if_fail (typename != NULL, G_TYPE_INVALID);

  return GTK_BUILDER_GET_CLASS (builder)->get_type_from_name (builder, typename);
}

/**
 * gtk_builder_error_quark:
 *
 * Registers an error quark for #GtkBuilder if necessary.
 * 
 * Return value: The error quark used for #GtkBuilder errors.
 *
 * Since: 2.12
 **/
GQuark
gtk_builder_error_quark (void)
{
  return g_quark_from_static_string ("gtk-builder-error-quark");
}


#define __GTK_BUILDER_C__
#include "gtkaliasdef.c"
