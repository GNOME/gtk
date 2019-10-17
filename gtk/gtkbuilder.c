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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:gtkbuilder
 * @Short_description: Build an interface from an XML UI definition
 * @Title: GtkBuilder
 *
 * A GtkBuilder is an auxiliary object that reads textual descriptions
 * of a user interface and instantiates the described objects. To create
 * a GtkBuilder from a user interface description, call
 * gtk_builder_new_from_file(), gtk_builder_new_from_resource() or
 * gtk_builder_new_from_string().
 *
 * In the (unusual) case that you want to add user interface
 * descriptions from multiple sources to the same GtkBuilder you can
 * call gtk_builder_new() to get an empty builder and populate it by
 * (multiple) calls to gtk_builder_add_from_file(),
 * gtk_builder_add_from_resource() or gtk_builder_add_from_string().
 *
 * A GtkBuilder holds a reference to all objects that it has constructed
 * and drops these references when it is finalized. This finalization can
 * cause the destruction of non-widget objects or widgets which are not
 * contained in a toplevel window. For toplevel windows constructed by a
 * builder, it is the responsibility of the user to call gtk_widget_destroy()
 * to get rid of them and all the widgets they contain.
 *
 * The functions gtk_builder_get_object() and gtk_builder_get_objects()
 * can be used to access the widgets in the interface by the names assigned
 * to them inside the UI description. Toplevel windows returned by these
 * functions will stay around until the user explicitly destroys them
 * with gtk_widget_destroy(). Other widgets will either be part of a
 * larger hierarchy constructed by the builder (in which case you should
 * not have to worry about their lifecycle), or without a parent, in which
 * case they have to be added to some container to make use of them.
 * Non-widget objects need to be reffed with g_object_ref() to keep them
 * beyond the lifespan of the builder.
 *
 * The function gtk_builder_connect_signals() and variants thereof can be
 * used to connect handlers to the named signals in the description.
 *
 * # GtkBuilder UI Definitions # {#BUILDER-UI}
 *
 * GtkBuilder parses textual descriptions of user interfaces which are
 * specified in an XML format which can be roughly described by the
 * RELAX NG schema below. We refer to these descriptions as “GtkBuilder
 * UI definitions” or just “UI definitions” if the context is clear.
 *
 * [RELAX NG Compact Syntax](https://gitlab.gnome.org/GNOME/gtk/tree/master/gtk/gtkbuilder.rnc)
 *
 * The toplevel element is <interface>. It optionally takes a “domain”
 * attribute, which will make the builder look for translated strings
 * using dgettext() in the domain specified. This can also be done by
 * calling gtk_builder_set_translation_domain() on the builder.
 * Objects are described by <object> elements, which can contain
 * <property> elements to set properties, <signal> elements which
 * connect signals to handlers, and <child> elements, which describe
 * child objects (most often widgets inside a container, but also e.g.
 * actions in an action group, or columns in a tree model). A <child>
 * element contains an <object> element which describes the child object.
 * The target toolkit version(s) are described by <requires> elements,
 * the “lib” attribute specifies the widget library in question (currently
 * the only supported value is “gtk+”) and the “version” attribute specifies
 * the target version in the form “<major>.<minor>”. The builder will error
 * out if the version requirements are not met.
 *
 * Typically, the specific kind of object represented by an <object>
 * element is specified by the “class” attribute. If the type has not
 * been loaded yet, GTK+ tries to find the get_type() function from the
 * class name by applying heuristics. This works in most cases, but if
 * necessary, it is possible to specify the name of the get_type() function
 * explictly with the "type-func" attribute. As a special case, GtkBuilder
 * allows to use an object that has been constructed by a #GtkUIManager in
 * another part of the UI definition by specifying the id of the #GtkUIManager
 * in the “constructor” attribute and the name of the object in the “id”
 * attribute.
 *
 * Objects may be given a name with the “id” attribute, which allows the
 * application to retrieve them from the builder with gtk_builder_get_object().
 * An id is also necessary to use the object as property value in other
 * parts of the UI definition. GTK+ reserves ids starting and ending
 * with ___ (3 underscores) for its own purposes.
 *
 * Setting properties of objects is pretty straightforward with the
 * <property> element: the “name” attribute specifies the name of the
 * property, and the content of the element specifies the value.
 * If the “translatable” attribute is set to a true value, GTK+ uses
 * gettext() (or dgettext() if the builder has a translation domain set)
 * to find a translation for the value. This happens before the value
 * is parsed, so it can be used for properties of any type, but it is
 * probably most useful for string properties. It is also possible to
 * specify a context to disambiguate short strings, and comments which
 * may help the translators.
 *
 * GtkBuilder can parse textual representations for the most common
 * property types: characters, strings, integers, floating-point numbers,
 * booleans (strings like “TRUE”, “t”, “yes”, “y”, “1” are interpreted
 * as %TRUE, strings like “FALSE”, “f”, “no”, “n”, “0” are interpreted
 * as %FALSE), enumerations (can be specified by their name, nick or
 * integer value), flags (can be specified by their name, nick, integer
 * value, optionally combined with “|”, e.g. “GTK_VISIBLE|GTK_REALIZED”)
 * and colors (in a format understood by gdk_rgba_parse()).
 *
 * GVariants can be specified in the format understood by g_variant_parse(),
 * and pixbufs can be specified as a filename of an image file to load.
 *
 * Objects can be referred to by their name and by default refer to
 * objects declared in the local xml fragment and objects exposed via
 * gtk_builder_expose_object(). In general, GtkBuilder allows forward
 * references to objects — declared in the local xml; an object doesn’t
 * have to be constructed before it can be referred to. The exception
 * to this rule is that an object has to be constructed before it can
 * be used as the value of a construct-only property.
 *
 * It is also possible to bind a property value to another object's
 * property value using the attributes
 * "bind-source" to specify the source object of the binding, and
 * optionally, "bind-property" and "bind-flags" to specify the
 * source property and source binding flags respectively.
 * Internally builder implements this using GBinding objects.
 * For more information see g_object_bind_property()
 *
 * Signal handlers are set up with the <signal> element. The “name”
 * attribute specifies the name of the signal, and the “handler” attribute
 * specifies the function to connect to the signal. By default, GTK+ tries
 * to find the handler using g_module_symbol(), but this can be changed by
 * passing a custom #GtkBuilderConnectFunc to
 * gtk_builder_connect_signals_full(). The remaining attributes, “after”,
 * “swapped” and “object”, have the same meaning as the corresponding
 * parameters of the g_signal_connect_object() or
 * g_signal_connect_data() functions. A “last_modification_time”
 * attribute is also allowed, but it does not have a meaning to the
 * builder.
 *
 * Sometimes it is necessary to refer to widgets which have implicitly
 * been constructed by GTK+ as part of a composite widget, to set
 * properties on them or to add further children (e.g. the @vbox of
 * a #GtkDialog). This can be achieved by setting the “internal-child”
 * property of the <child> element to a true value. Note that GtkBuilder
 * still requires an <object> element for the internal child, even if it
 * has already been constructed.
 *
 * A number of widgets have different places where a child can be added
 * (e.g. tabs vs. page content in notebooks). This can be reflected in
 * a UI definition by specifying the “type” attribute on a <child>
 * The possible values for the “type” attribute are described in the
 * sections describing the widget-specific portions of UI definitions.
 *
 * # A GtkBuilder UI Definition
 *
 * |[
 * <interface>
 *   <object class="GtkDialog" id="dialog1">
 *     <child internal-child="vbox">
 *       <object class="GtkBox" id="vbox1">
 *         <child internal-child="action_area">
 *           <object class="GtkBox" id="hbuttonbox1">
 *             <child>
 *               <object class="GtkButton" id="ok_button">
 *                 <property name="label">gtk-ok</property>
 *                 <signal name="clicked" handler="ok_button_clicked"/>
 *               </object>
 *             </child>
 *           </object>
 *         </child>
 *       </object>
 *     </child>
 *   </object>
 * </interface>
 * ]|
 *
 * Beyond this general structure, several object classes define their
 * own XML DTD fragments for filling in the ANY placeholders in the DTD
 * above. Note that a custom element in a <child> element gets parsed by
 * the custom tag handler of the parent object, while a custom element in
 * an <object> element gets parsed by the custom tag handler of the object.
 *
 * These XML fragments are explained in the documentation of the
 * respective objects.
 *
 * Additionally, since 3.10 a special <template> tag has been added
 * to the format allowing one to define a widget class’s components.
 * See the [GtkWidget documentation][composite-templates] for details.
 */

#include "config.h"
#include <errno.h> /* errno */
#include <stdlib.h>
#include <string.h> /* strlen */

#include "gtkbuilder.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkdebug.h"
#include "gtkmain.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkicontheme.h"
#include "gtktestutils.h"


static void gtk_builder_finalize       (GObject         *object);
static void gtk_builder_set_property   (GObject         *object,
                                        guint            prop_id,
                                        const GValue    *value,
                                        GParamSpec      *pspec);
static void gtk_builder_get_property   (GObject         *object,
                                        guint            prop_id,
                                        GValue          *value,
                                        GParamSpec      *pspec);
static GType gtk_builder_real_get_type_from_name (GtkBuilder  *builder,
                                                  const gchar *type_name);

enum {
  PROP_0,
  PROP_TRANSLATION_DOMAIN,
  LAST_PROP
};

static GParamSpec *builder_props[LAST_PROP];

typedef struct
{
  gchar *domain;
  GHashTable *objects;
  GHashTable *callbacks;
  GSList *delayed_properties;
  GSList *signals;
  GSList *bindings;
  gchar *filename;
  gchar *resource_prefix;
  GType template_type;
  GtkApplication *application;
} GtkBuilderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkBuilder, gtk_builder, G_TYPE_OBJECT)

static void
gtk_builder_class_init (GtkBuilderClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_builder_finalize;
  gobject_class->set_property = gtk_builder_set_property;
  gobject_class->get_property = gtk_builder_get_property;

  klass->get_type_from_name = gtk_builder_real_get_type_from_name;

 /**
  * GtkBuilder:translation-domain:
  *
  * The translation domain used when translating property values that
  * have been marked as translatable in interface descriptions.
  * If the translation domain is %NULL, #GtkBuilder uses gettext(),
  * otherwise g_dgettext().
  */
  builder_props[PROP_TRANSLATION_DOMAIN] =
      g_param_spec_string ("translation-domain",
                           P_("Translation Domain"),
                           P_("The translation domain used by gettext"),
                           NULL,
                           GTK_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, LAST_PROP, builder_props);
}

static void
gtk_builder_init (GtkBuilder *builder)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  priv->domain = NULL;
  priv->objects = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, g_object_unref);
}


/*
 * GObject virtual methods
 */

static void
gtk_builder_finalize (GObject *object)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (GTK_BUILDER (object));

  g_free (priv->domain);
  g_free (priv->filename);
  g_free (priv->resource_prefix);

  g_hash_table_destroy (priv->objects);
  if (priv->callbacks)
    g_hash_table_destroy (priv->callbacks);

  g_slist_free_full (priv->signals, (GDestroyNotify)_free_signal_info);

  G_OBJECT_CLASS (gtk_builder_parent_class)->finalize (object);
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
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  switch (prop_id)
    {
    case PROP_TRANSLATION_DOMAIN:
      g_value_set_string (value, priv->domain);
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
 * GWeatherLocation -> gweather_location_get_type
 *
 * Keep in sync with testsuite/gtk/typename.c !
 */
static gchar *
type_name_mangle (const gchar *name)
{
  GString *symbol_name = g_string_new ("");
  gint i;

  for (i = 0; name[i] != '\0'; i++)
    {
      /* skip if uppercase, first or previous is uppercase */
      if ((name[i] == g_ascii_toupper (name[i]) &&
           i > 0 && name[i-1] != g_ascii_toupper (name[i-1])) ||
           (i > 2 && name[i]   == g_ascii_toupper (name[i]) &&
           name[i-1] == g_ascii_toupper (name[i-1]) &&
           name[i-2] == g_ascii_toupper (name[i-2])))
        g_string_append_c (symbol_name, '_');
      g_string_append_c (symbol_name, g_ascii_tolower (name[i]));
    }
  g_string_append (symbol_name, "_get_type");

  return g_string_free (symbol_name, FALSE);
}

static GType
_gtk_builder_resolve_type_lazily (const gchar *name)
{
  static GModule *module = NULL;
  GTypeGetFunc func;
  gchar *symbol;
  GType gtype = G_TYPE_INVALID;

  if (!module)
    module = g_module_open (NULL, 0);

  symbol = type_name_mangle (name);

  if (g_module_symbol (module, symbol, (gpointer)&func))
    gtype = func ();

  g_free (symbol);

  return gtype;
}

/*
 * GtkBuilder virtual methods
 */

static GType
gtk_builder_real_get_type_from_name (GtkBuilder  *builder,
                                     const gchar *type_name)
{
  GType gtype;

  gtype = g_type_from_name (type_name);
  if (gtype != G_TYPE_INVALID)
    return gtype;

  gtype = _gtk_builder_resolve_type_lazily (type_name);
  if (gtype != G_TYPE_INVALID)
    return gtype;

  gtk_test_register_all_types ();
  return g_type_from_name (type_name);
}

typedef struct
{
  gchar *object;
  GParamSpec *pspec;
  gchar *value;
  gint line;
  gint col;
} DelayedProperty;

typedef struct
{
  GPtrArray *names;
  GArray *values;
  guint len;
} ObjectProperties;

static ObjectProperties *
object_properties_new (void)
{
  ObjectProperties *res = g_new (ObjectProperties, 1);

  res->names = g_ptr_array_new ();

  res->values = g_array_new (FALSE, FALSE, sizeof (GValue));
  g_array_set_clear_func (res->values, (GDestroyNotify) g_value_unset);

  res->len = 0;

  return res;
}

static void
object_properties_free (ObjectProperties *properties)
{
  if (properties == NULL)
    return;

  g_ptr_array_unref (properties->names);
  g_array_unref (properties->values);

  g_free (properties);
}

static void
object_properties_add (ObjectProperties *properties,
                       const char       *name,
                       const GValue     *value)
{
  g_ptr_array_add (properties->names, (char *) name);
  g_array_append_vals (properties->values, value, 1);

  g_assert (properties->names->len == properties->values->len);

  properties->len += 1;
}

static const char *
object_properties_get_name (ObjectProperties *properties,
                            guint             idx)
{
  return g_ptr_array_index (properties->names, idx);
}

static GValue *
object_properties_get_value (ObjectProperties *properties,
                             guint             idx)
{
  return &g_array_index (properties->values, GValue, idx);
}

static void
gtk_builder_get_parameters (GtkBuilder         *builder,
                            GType               object_type,
                            const gchar        *object_name,
                            GSList             *properties,
                            GParamFlags         filter_flags,
                            ObjectProperties  **parameters,
                            ObjectProperties  **filtered_parameters)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GSList *l;
  DelayedProperty *property;
  GError *error = NULL;

  if (parameters)
    *parameters = object_properties_new ();
  if (filtered_parameters)
    *filtered_parameters = object_properties_new ();

  for (l = properties; l; l = l->next)
    {
      PropertyInfo *prop = (PropertyInfo*)l->data;
      const char *property_name = g_intern_string (prop->pspec->name);
      GValue property_value = G_VALUE_INIT;

      if (G_IS_PARAM_SPEC_OBJECT (prop->pspec) &&
          (G_PARAM_SPEC_VALUE_TYPE (prop->pspec) != GDK_TYPE_PIXBUF) &&
          (G_PARAM_SPEC_VALUE_TYPE (prop->pspec) != GDK_TYPE_TEXTURE) &&
          (G_PARAM_SPEC_VALUE_TYPE (prop->pspec) != GDK_TYPE_PAINTABLE) &&
          (G_PARAM_SPEC_VALUE_TYPE (prop->pspec) != G_TYPE_FILE))
        {
          GObject *object = g_hash_table_lookup (priv->objects,
                                                 g_strstrip (prop->text->str));

          if (object)
            {
              g_value_init (&property_value, G_OBJECT_TYPE (object));
              g_value_set_object (&property_value, object);
            }
          else
            {
              if (prop->pspec->flags & G_PARAM_CONSTRUCT_ONLY)
                {
                  g_warning ("Failed to get construct only property "
                             "%s of %s with value '%s'",
                             prop->pspec->name, object_name, prop->text->str);
                  continue;
                }
              /* Delay setting property */
              property = g_slice_new (DelayedProperty);
              property->pspec = prop->pspec;
              property->object = g_strdup (object_name);
              property->value = g_strdup (prop->text->str);
              property->line = prop->line;
              property->col = prop->col;
              priv->delayed_properties = g_slist_prepend (priv->delayed_properties,
                                                          property);
              continue;
            }
        }
      else if (prop->bound && (!prop->text || prop->text->len == 0))
        {
          /* Ignore properties with a binding and no value since they are
           * only there for to express the binding.
           */
          continue;
        }
      else if (!gtk_builder_value_from_string (builder, prop->pspec,
                                               prop->text->str,
                                               &property_value,
                                               &error))
        {
          g_warning ("Failed to set property %s.%s to %s: %s",
                     g_type_name (object_type), prop->pspec->name, prop->text->str,
                     error->message);
          g_error_free (error);
          error = NULL;
          continue;
        }

      if (prop->pspec->flags & filter_flags)
        {
          if (filtered_parameters)
            object_properties_add (*filtered_parameters, property_name, &property_value);
        }
      else
        {
          if (parameters)
            object_properties_add (*parameters, property_name, &property_value);
        }
    }
}

static const gchar *
object_get_name (GObject *object)
{
  if (GTK_IS_BUILDABLE (object))
    return gtk_buildable_get_name (GTK_BUILDABLE (object));
  else
    return g_object_get_data (object, "gtk-builder-name");
}

static GObject *
gtk_builder_get_internal_child (GtkBuilder   *builder,
                                ObjectInfo   *info,
                                const gchar  *childname,
                                GError      **error)
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
                g_message ("Trying to get internal child %s from %s",
                           childname, object_get_name (info->object)));

      if (GTK_IS_BUILDABLE (info->object))
          obj = gtk_buildable_get_internal_child (GTK_BUILDABLE (info->object),
                                                  builder,
                                                  childname);
    };

  if (!obj)
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_INVALID_VALUE,
                   "Unknown internal child: %s", childname);
    }
  return obj;
}

static inline void
object_set_name (GObject     *object,
                 const gchar *name)
{
  if (GTK_IS_BUILDABLE (object))
    gtk_buildable_set_name (GTK_BUILDABLE (object), name);
  else
    g_object_set_data_full (object, "gtk-builder-name", g_strdup (name), g_free);
}

void
_gtk_builder_add_object (GtkBuilder  *builder,
                         const gchar *id,
                         GObject     *object)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  object_set_name (object, id);
  g_hash_table_insert (priv->objects, g_strdup (id), g_object_ref (object));
}

static void
gtk_builder_take_bindings (GtkBuilder *builder,
                           GObject    *target,
                           GSList     *bindings)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GSList *l;

  for (l = bindings; l; l = l->next)
    {
      BindingInfo *info = l->data;
      info->target = target;
    }

  priv->bindings = g_slist_concat (priv->bindings, bindings);
}

GObject *
_gtk_builder_construct (GtkBuilder  *builder,
                        ObjectInfo  *info,
                        GError     **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  ObjectProperties *parameters, *construct_parameters;
  GObject *obj;
  int i;
  GtkBuildableIface *iface;
  gboolean custom_set_property;
  GtkBuildable *buildable;
  GParamFlags param_filter_flags;

  g_assert (info->type != G_TYPE_INVALID);

  if (priv->template_type != 0 &&
      g_type_is_a (info->type, priv->template_type))
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_OBJECT_TYPE_REFUSED,
                   "Refused to build object of type '%s' because it "
                   "conforms to the template type '%s', avoiding infinite recursion.",
                   g_type_name (info->type), g_type_name (priv->template_type));
      return NULL;
    }

  /* If there is a manual constructor (like UIManager), or if this is a
   * reference to an internal child, then we filter out construct-only
   * and warn that they cannot be set.
   *
   * Otherwise if we are calling g_object_new_with_properties(), we want
   * to pass both G_PARAM_CONSTRUCT and G_PARAM_CONSTRUCT_ONLY to the
   * object's constructor.
   *
   * Passing all construct properties to g_object_new_with_properties()
   * slightly improves performance as the construct properties will only
   * be set once.
   */
  if (info->constructor ||
      (info->parent &&
       info->parent->tag_type == TAG_CHILD &&
       ((ChildInfo*)info->parent)->internal_child != NULL))
    param_filter_flags = G_PARAM_CONSTRUCT_ONLY;
  else
    param_filter_flags = G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY;

  gtk_builder_get_parameters (builder, info->type,
                              info->id,
                              info->properties,
                              param_filter_flags,
                              &parameters,
                              &construct_parameters);

  if (info->constructor)
    {
      GObject *constructor;

      constructor = g_hash_table_lookup (priv->objects, info->constructor);
      if (constructor == NULL)
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Unknown object constructor for %s: %s",
                       info->id,
                       info->constructor);
          object_properties_free (parameters);
          object_properties_free (construct_parameters);
          return NULL;
        }
      obj = gtk_buildable_construct_child (GTK_BUILDABLE (constructor),
                                           builder,
                                           info->id);
      g_assert (obj != NULL);
      if (construct_parameters->len)
        g_warning ("Can't pass in construct-only parameters to %s", info->id);
    }
  else if (info->parent &&
           info->parent->tag_type == TAG_CHILD &&
           ((ChildInfo*)info->parent)->internal_child != NULL)
    {
      gchar *childname = ((ChildInfo*)info->parent)->internal_child;
      obj = gtk_builder_get_internal_child (builder, info, childname, error);
      if (!obj)
        {
          object_properties_free (parameters);
          object_properties_free (construct_parameters);
          return NULL;
        }
      if (construct_parameters->len)
        g_warning ("Can't pass in construct-only parameters to %s", childname);
      g_object_ref (obj);
    }
  else
    {
      obj = g_object_new_with_properties (info->type,
                                          construct_parameters->len,
                                          (const char **) construct_parameters->names->pdata,
                                          (GValue *) construct_parameters->values->data);

      /* No matter what, make sure we have a reference.
       *
       * If it's an initially unowned object, sink it.
       * If it's not initially unowned then we have the reference already.
       *
       * In the case that this is a window it will be sunk already and
       * this is effectively a call to g_object_ref().  That's what
       * we want.
       */
      if (G_IS_INITIALLY_UNOWNED (obj))
        g_object_ref_sink (obj);

      GTK_NOTE (BUILDER,
                g_message ("created %s of type %s", info->id, g_type_name (info->type)));
    }
  object_properties_free (construct_parameters);

  custom_set_property = FALSE;
  buildable = NULL;
  iface = NULL;
  if (GTK_IS_BUILDABLE (obj))
    {
      buildable = GTK_BUILDABLE (obj);
      iface = GTK_BUILDABLE_GET_IFACE (obj);
      if (iface->set_buildable_property)
        custom_set_property = TRUE;
    }

  for (i = 0; i < parameters->len; i++)
    {
      const char *name = object_properties_get_name (parameters, i);
      const GValue *value = object_properties_get_value (parameters, i);

      if (custom_set_property)
        iface->set_buildable_property (buildable, builder, name, value);
      else
        g_object_set_property (obj, name, value);

#ifdef G_ENABLE_DEBUG
      if (GTK_DEBUG_CHECK (BUILDER))
        {
          gchar *str = g_strdup_value_contents (value);
          g_message ("set %s: %s = %s", info->id, name, str);
          g_free (str);
        }
#endif
    }
  object_properties_free (parameters);

  if (info->bindings)
    gtk_builder_take_bindings (builder, obj, info->bindings);

  /* put it in the hash table. */
  _gtk_builder_add_object (builder, info->id, obj);

  /* we already own a reference to obj. */
  g_object_unref (obj);

  return obj;
}

void
_gtk_builder_apply_properties (GtkBuilder  *builder,
                               ObjectInfo  *info,
                               GError     **error)
{
  ObjectProperties *parameters;
  GtkBuildableIface *iface;
  GtkBuildable *buildable;
  gboolean custom_set_property;
  gint i;

  g_assert (info->object != NULL);
  g_assert (info->type != G_TYPE_INVALID);

  /* Fetch all properties that are not construct-only */
  gtk_builder_get_parameters (builder, info->type,
                              info->id,
                              info->properties,
                              G_PARAM_CONSTRUCT_ONLY,
                              &parameters, NULL);

  custom_set_property = FALSE;
  buildable = NULL;
  iface = NULL;
  if (GTK_IS_BUILDABLE (info->object))
    {
      buildable = GTK_BUILDABLE (info->object);
      iface = GTK_BUILDABLE_GET_IFACE (info->object);
      if (iface->set_buildable_property)
        custom_set_property = TRUE;
    }

  for (i = 0; i < parameters->len; i++)
    {
      const char *name = object_properties_get_name (parameters, i);
      const GValue *value = object_properties_get_value (parameters, i);
      if (custom_set_property)
        iface->set_buildable_property (buildable, builder, name, value);
      else
        g_object_set_property (info->object, name, value);

#ifdef G_ENABLE_DEBUG
      if (GTK_DEBUG_CHECK (BUILDER))
        {
          gchar *str = g_strdup_value_contents (value);
          g_message ("set %s: %s = %s", info->id, name, str);
          g_free (str);
        }
#endif
    }
  object_properties_free (parameters);
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
      g_warning ("%s: Not adding, No parent", object_get_name (object));
      return;
    }

  g_assert (object != NULL);

  parent = ((ObjectInfo*)child_info->parent)->object;

  GTK_NOTE (BUILDER,
            g_message ("adding %s to %s", object_get_name (object), object_get_name (parent)));

  if (G_IS_LIST_STORE (parent))
    {
      if (child_info->type != NULL)
        {
          GTK_BUILDER_WARN_INVALID_CHILD_TYPE (parent, child_info->type);
        }
      else
        {
          g_list_store_append (G_LIST_STORE (parent), object);
        }
    }
  else
    {
      g_assert (GTK_IS_BUILDABLE (parent));
      gtk_buildable_add_child (GTK_BUILDABLE (parent), builder, object,
                               child_info->type);
    }

  child_info->added = TRUE;
}

void
_gtk_builder_add_signals (GtkBuilder *builder,
                          GSList     *signals)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  priv->signals = g_slist_concat (priv->signals,
                                  g_slist_copy (signals));
}

static void
gtk_builder_apply_delayed_properties (GtkBuilder *builder)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GSList *l, *props;

  /* take the list over from the builder->priv.
   *
   * g_slist_reverse does not copy the list, so the list now
   * belongs to us (and we free it at the end of this function).
   */
  props = g_slist_reverse (priv->delayed_properties);
  priv->delayed_properties = NULL;

  for (l = props; l; l = l->next)
    {
      DelayedProperty *property = l->data;
      GObject *object, *obj;

      object = g_hash_table_lookup (priv->objects, property->object);
      g_assert (object != NULL);

      obj = _gtk_builder_lookup_object (builder, property->value, property->line, property->col);
      if (obj)
        g_object_set (object, property->pspec->name, obj, NULL);

      g_free (property->value);
      g_free (property->object);
      g_slice_free (DelayedProperty, property);
    }
  g_slist_free (props);
}

static inline void
free_binding_info (gpointer data,
                   gpointer user)
{
  BindingInfo *info = data;

  g_free (info->source);
  g_free (info->source_property);
  g_slice_free (BindingInfo, data);
}

static inline void
gtk_builder_create_bindings (GtkBuilder *builder)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GSList *l;

  for (l = priv->bindings; l; l = l->next)
    {
      BindingInfo *info = l->data;
      GObject *source;

      source = _gtk_builder_lookup_object (builder, info->source, info->line, info->col);
      if (source)
        g_object_bind_property (source, info->source_property,
                                info->target, info->target_pspec->name,
                                info->flags);

      free_binding_info (info, NULL);
    }

  g_slist_free (priv->bindings);
  priv->bindings = NULL;
}

void
_gtk_builder_finish (GtkBuilder *builder)
{
  gtk_builder_apply_delayed_properties (builder);
  gtk_builder_create_bindings (builder);
}

/**
 * gtk_builder_new:
 *
 * Creates a new empty builder object.
 *
 * This function is only useful if you intend to make multiple calls
 * to gtk_builder_add_from_file(), gtk_builder_add_from_resource()
 * or gtk_builder_add_from_string() in order to merge multiple UI
 * descriptions into a single builder.
 *
 * Most users will probably want to use gtk_builder_new_from_file(),
 * gtk_builder_new_from_resource() or gtk_builder_new_from_string().
 *
 * Returns: a new (empty) #GtkBuilder object
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
 * @error: (allow-none): return location for an error, or %NULL
 *
 * Parses a file containing a [GtkBuilder UI definition][BUILDER-UI]
 * and merges it with the current contents of @builder.
 *
 * Most users will probably want to use gtk_builder_new_from_file().
 *
 * If an error occurs, 0 will be returned and @error will be assigned a
 * #GError from the #GTK_BUILDER_ERROR, #G_MARKUP_ERROR or #G_FILE_ERROR
 * domain.
 *
 * It’s not really reasonable to attempt to handle failures of this
 * call. You should not use this function with untrusted files (ie:
 * files that are not part of your application). Broken #GtkBuilder
 * files can easily crash your program, and it’s possible that memory
 * was leaked leading up to the reported failure. The only reasonable
 * thing to do when an error is detected is to call g_error().
 *
 * Returns: %TRUE on success, %FALSE if an error occurred
 **/
gboolean
gtk_builder_add_from_file (GtkBuilder   *builder,
                           const gchar  *filename,
                           GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  gchar *buffer;
  gsize length;
  GError *tmp_error;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (filename != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  tmp_error = NULL;

  if (!g_file_get_contents (filename, &buffer, &length, &tmp_error))
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  g_free (priv->filename);
  g_free (priv->resource_prefix);
  priv->filename = g_strdup (filename);
  priv->resource_prefix = NULL;

  _gtk_builder_parser_parse_buffer (builder, filename,
                                    buffer, (gssize)length,
                                    NULL,
                                    &tmp_error);

  g_free (buffer);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * gtk_builder_add_objects_from_file:
 * @builder: a #GtkBuilder
 * @filename: the name of the file to parse
 * @object_ids: (array zero-terminated=1) (element-type utf8): nul-terminated array of objects to build
 * @error: (allow-none): return location for an error, or %NULL
 *
 * Parses a file containing a [GtkBuilder UI definition][BUILDER-UI]
 * building only the requested objects and merges
 * them with the current contents of @builder.
 *
 * Upon errors 0 will be returned and @error will be assigned a
 * #GError from the #GTK_BUILDER_ERROR, #G_MARKUP_ERROR or #G_FILE_ERROR
 * domain.
 *
 * If you are adding an object that depends on an object that is not
 * its child (for instance a #GtkTreeView that depends on its
 * #GtkTreeModel), you have to explicitly list all of them in @object_ids.
 *
 * Returns: %TRUE on success, %FALSE if an error occurred
 **/
gboolean
gtk_builder_add_objects_from_file (GtkBuilder   *builder,
                                   const gchar  *filename,
                                   gchar       **object_ids,
                                   GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  gchar *buffer;
  gsize length;
  GError *tmp_error;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (filename != NULL, 0);
  g_return_val_if_fail (object_ids != NULL && object_ids[0] != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  tmp_error = NULL;

  if (!g_file_get_contents (filename, &buffer, &length, &tmp_error))
    {
      g_propagate_error (error, tmp_error);
      return 0;
    }

  g_free (priv->filename);
  g_free (priv->resource_prefix);
  priv->filename = g_strdup (filename);
  priv->resource_prefix = NULL;

  _gtk_builder_parser_parse_buffer (builder, filename,
                                    buffer, (gssize)length,
                                    object_ids,
                                    &tmp_error);

  g_free (buffer);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  return TRUE;
}


/**
 * gtk_builder_extend_with_template:
 * @builder: a #GtkBuilder
 * @widget: the widget that is being extended
 * @template_type: the type that the template is for
 * @buffer: the string to parse
 * @length: the length of @buffer (may be -1 if @buffer is nul-terminated)
 * @error: (allow-none): return location for an error, or %NULL
 *
 * Main private entry point for building composite container
 * components from template XML.
 *
 * This is exported purely to let gtk-builder-tool validate
 * templates, applications have no need to call this function.
 *
 * Returns: A positive value on success, 0 if an error occurred
 */
gboolean
gtk_builder_extend_with_template (GtkBuilder   *builder,
                                  GtkWidget    *widget,
                                  GType         template_type,
                                  const gchar  *buffer,
                                  gssize        length,
                                  GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GError *tmp_error;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  g_return_val_if_fail (g_type_name (template_type) != NULL, 0);
  g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (widget), template_type), 0);
  g_return_val_if_fail (buffer && buffer[0], 0);

  tmp_error = NULL;

  g_free (priv->filename);
  g_free (priv->resource_prefix);
  priv->filename = g_strdup (".");
  priv->resource_prefix = NULL;
  priv->template_type = template_type;

  gtk_builder_expose_object (builder, g_type_name (template_type), G_OBJECT (widget));
  _gtk_builder_parser_parse_buffer (builder, "<input>",
                                    buffer, length,
                                    NULL,
                                    &tmp_error);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * gtk_builder_add_from_resource:
 * @builder: a #GtkBuilder
 * @resource_path: the path of the resource file to parse
 * @error: (allow-none): return location for an error, or %NULL
 *
 * Parses a resource file containing a [GtkBuilder UI definition][BUILDER-UI]
 * and merges it with the current contents of @builder.
 *
 * Most users will probably want to use gtk_builder_new_from_resource().
 *
 * If an error occurs, 0 will be returned and @error will be assigned a
 * #GError from the #GTK_BUILDER_ERROR, #G_MARKUP_ERROR or #G_RESOURCE_ERROR
 * domain.
 *
 * It’s not really reasonable to attempt to handle failures of this
 * call.  The only reasonable thing to do when an error is detected is
 * to call g_error().
 *
 * Returns: %TRUE on success, %FALSE if an error occurred
 **/
gboolean
gtk_builder_add_from_resource (GtkBuilder   *builder,
                               const gchar  *resource_path,
                               GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GError *tmp_error;
  GBytes *data;
  char *filename_for_errors;
  char *slash;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (resource_path != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  tmp_error = NULL;

  data = g_resources_lookup_data (resource_path, 0, &tmp_error);
  if (data == NULL)
    {
      g_propagate_error (error, tmp_error);
      return 0;
    }

  g_free (priv->filename);
  g_free (priv->resource_prefix);
  priv->filename = g_strdup (".");

  slash = strrchr (resource_path, '/');
  if (slash != NULL)
    priv->resource_prefix = g_strndup (resource_path, slash - resource_path + 1);
  else
    priv->resource_prefix = g_strdup ("/");

  filename_for_errors = g_strconcat ("<resource>", resource_path, NULL);

  _gtk_builder_parser_parse_buffer (builder, filename_for_errors,
                                    g_bytes_get_data (data, NULL), g_bytes_get_size (data),
                                    NULL,
                                    &tmp_error);

  g_free (filename_for_errors);
  g_bytes_unref (data);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * gtk_builder_add_objects_from_resource:
 * @builder: a #GtkBuilder
 * @resource_path: the path of the resource file to parse
 * @object_ids: (array zero-terminated=1) (element-type utf8): nul-terminated array of objects to build
 * @error: (allow-none): return location for an error, or %NULL
 *
 * Parses a resource file containing a [GtkBuilder UI definition][BUILDER-UI]
 * building only the requested objects and merges
 * them with the current contents of @builder.
 *
 * Upon errors 0 will be returned and @error will be assigned a
 * #GError from the #GTK_BUILDER_ERROR, #G_MARKUP_ERROR or #G_RESOURCE_ERROR
 * domain.
 *
 * If you are adding an object that depends on an object that is not
 * its child (for instance a #GtkTreeView that depends on its
 * #GtkTreeModel), you have to explicitly list all of them in @object_ids.
 *
 * Returns: %TRUE on success, %FALSE if an error occurred
 **/
gboolean
gtk_builder_add_objects_from_resource (GtkBuilder   *builder,
                                       const gchar  *resource_path,
                                       gchar       **object_ids,
                                       GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GError *tmp_error;
  GBytes *data;
  char *filename_for_errors;
  char *slash;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (resource_path != NULL, 0);
  g_return_val_if_fail (object_ids != NULL && object_ids[0] != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  tmp_error = NULL;

  data = g_resources_lookup_data (resource_path, 0, &tmp_error);
  if (data == NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  g_free (priv->filename);
  g_free (priv->resource_prefix);
  priv->filename = g_strdup (".");

  slash = strrchr (resource_path, '/');
  if (slash != NULL)
    priv->resource_prefix = g_strndup (resource_path, slash - resource_path + 1);
  else
    priv->resource_prefix = g_strdup ("/");

  filename_for_errors = g_strconcat ("<resource>", resource_path, NULL);

  _gtk_builder_parser_parse_buffer (builder, filename_for_errors,
                                    g_bytes_get_data (data, NULL), g_bytes_get_size (data),
                                    object_ids,
                                    &tmp_error);
  g_free (filename_for_errors);
  g_bytes_unref (data);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * gtk_builder_add_from_string:
 * @builder: a #GtkBuilder
 * @buffer: the string to parse
 * @length: the length of @buffer (may be -1 if @buffer is nul-terminated)
 * @error: (allow-none): return location for an error, or %NULL
 *
 * Parses a string containing a [GtkBuilder UI definition][BUILDER-UI]
 * and merges it with the current contents of @builder.
 *
 * Most users will probably want to use gtk_builder_new_from_string().
 *
 * Upon errors %FALSE will be returned and @error will be assigned a
 * #GError from the #GTK_BUILDER_ERROR, #G_MARKUP_ERROR or
 * #G_VARIANT_PARSE_ERROR domain.
 *
 * It’s not really reasonable to attempt to handle failures of this
 * call.  The only reasonable thing to do when an error is detected is
 * to call g_error().
 *
 * Returns: %TRUE on success, %FALSE if an error occurred
 **/
gboolean
gtk_builder_add_from_string (GtkBuilder   *builder,
                             const gchar  *buffer,
                             gssize        length,
                             GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GError *tmp_error;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (buffer != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  tmp_error = NULL;

  g_free (priv->filename);
  g_free (priv->resource_prefix);
  priv->filename = g_strdup (".");
  priv->resource_prefix = NULL;

  _gtk_builder_parser_parse_buffer (builder, "<input>",
                                    buffer, length,
                                    NULL,
                                    &tmp_error);
  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * gtk_builder_add_objects_from_string:
 * @builder: a #GtkBuilder
 * @buffer: the string to parse
 * @length: the length of @buffer (may be -1 if @buffer is nul-terminated)
 * @object_ids: (array zero-terminated=1) (element-type utf8): nul-terminated array of objects to build
 * @error: (allow-none): return location for an error, or %NULL
 *
 * Parses a string containing a [GtkBuilder UI definition][BUILDER-UI]
 * building only the requested objects and merges
 * them with the current contents of @builder.
 *
 * Upon errors %FALSE will be returned and @error will be assigned a
 * #GError from the #GTK_BUILDER_ERROR or #G_MARKUP_ERROR domain.
 *
 * If you are adding an object that depends on an object that is not
 * its child (for instance a #GtkTreeView that depends on its
 * #GtkTreeModel), you have to explicitly list all of them in @object_ids.
 *
 * Returns: %TRUE on success, %FALSE if an error occurred
 **/
gboolean
gtk_builder_add_objects_from_string (GtkBuilder   *builder,
                                     const gchar  *buffer,
                                     gssize        length,
                                     gchar       **object_ids,
                                     GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GError *tmp_error;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (buffer != NULL, 0);
  g_return_val_if_fail (object_ids != NULL && object_ids[0] != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  tmp_error = NULL;

  g_free (priv->filename);
  g_free (priv->resource_prefix);
  priv->filename = g_strdup (".");
  priv->resource_prefix = NULL;

  _gtk_builder_parser_parse_buffer (builder, "<input>",
                                    buffer, length,
                                    object_ids,
                                    &tmp_error);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * gtk_builder_get_object:
 * @builder: a #GtkBuilder
 * @name: name of object to get
 *
 * Gets the object named @name. Note that this function does not
 * increment the reference count of the returned object.
 *
 * Returns: (nullable) (transfer none): the object named @name or %NULL if
 *    it could not be found in the object tree.
 **/
GObject *
gtk_builder_get_object (GtkBuilder  *builder,
                        const gchar *name)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_hash_table_lookup (priv->objects, name);
}

/**
 * gtk_builder_get_objects:
 * @builder: a #GtkBuilder
 *
 * Gets all objects that have been constructed by @builder. Note that
 * this function does not increment the reference counts of the returned
 * objects.
 *
 * Returns: (element-type GObject) (transfer container): a newly-allocated #GSList containing all the objects
 *   constructed by the #GtkBuilder instance. It should be freed by
 *   g_slist_free()
 **/
GSList *
gtk_builder_get_objects (GtkBuilder *builder)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GSList *objects = NULL;
  GObject *object;
  GHashTableIter iter;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);

  g_hash_table_iter_init (&iter, priv->objects);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&object))
    objects = g_slist_prepend (objects, object);

  return g_slist_reverse (objects);
}

/**
 * gtk_builder_set_translation_domain:
 * @builder: a #GtkBuilder
 * @domain: (nullable): the translation domain or %NULL
 *
 * Sets the translation domain of @builder.
 * See #GtkBuilder:translation-domain.
 **/
void
gtk_builder_set_translation_domain (GtkBuilder  *builder,
                                    const gchar *domain)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  gchar *new_domain;

  g_return_if_fail (GTK_IS_BUILDER (builder));

  new_domain = g_strdup (domain);
  g_free (priv->domain);
  priv->domain = new_domain;

  g_object_notify_by_pspec (G_OBJECT (builder), builder_props[PROP_TRANSLATION_DOMAIN]);
}

/**
 * gtk_builder_get_translation_domain:
 * @builder: a #GtkBuilder
 *
 * Gets the translation domain of @builder.
 *
 * Returns: (transfer none) (nullable): the translation domain or %NULL
 *   in case it was never set or explicitly unset via gtk_builder_set_translation_domain().
 *   This string is owned by the builder object and must not be modified or freed.
 **/
const gchar *
gtk_builder_get_translation_domain (GtkBuilder *builder)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);

  return priv->domain;
}

/**
 * gtk_builder_expose_object:
 * @builder: a #GtkBuilder
 * @name: the name of the object exposed to the builder
 * @object: the object to expose
 *
 * Add @object to the @builder object pool so it can be referenced just like any
 * other object built by builder.
 **/
void
gtk_builder_expose_object (GtkBuilder    *builder,
                           const gchar   *name,
                           GObject       *object)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  g_return_if_fail (GTK_IS_BUILDER (builder));
  g_return_if_fail (name && name[0]);
  g_return_if_fail (!g_hash_table_contains (priv->objects, name));

  object_set_name (object, name);
  g_hash_table_insert (priv->objects,
                       g_strdup (name),
                       g_object_ref (object));
}


typedef struct {
  GModule *module;
  gpointer data;
} ConnectArgs;

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
  ConnectArgs *args = (ConnectArgs*) user_data;

  func = gtk_builder_lookup_callback_symbol (builder, handler_name);

  if (!func)
    {
      /* Only error out for missing GModule support if we've not
       * found the symbols explicitly added with gtk_builder_add_callback_symbol()
       */
      if (args->module == NULL)
        g_error ("gtk_builder_connect_signals() requires working GModule");

      if (!g_module_symbol (args->module, handler_name, (gpointer)&func))
        {
          g_warning ("Could not find signal handler '%s'.  Did you compile with -rdynamic?", handler_name);
          return;
        }
    }

  if (connect_object)
    g_signal_connect_object (object, signal_name, func, connect_object, flags);
  else
    g_signal_connect_data (object, signal_name, func, args->data, NULL, flags);
}


/**
 * gtk_builder_connect_signals:
 * @builder: a #GtkBuilder
 * @user_data: user data to pass back with all signals
 *
 * This method is a simpler variation of gtk_builder_connect_signals_full().
 * It uses symbols explicitly added to @builder with prior calls to
 * gtk_builder_add_callback_symbol(). In the case that symbols are not
 * explicitly added; it uses #GModule’s introspective features (by opening the module %NULL)
 * to look at the application’s symbol table. From here it tries to match
 * the signal handler names given in the interface description with
 * symbols in the application and connects the signals. Note that this
 * function can only be called once, subsequent calls will do nothing.
 *
 * Note that unless gtk_builder_add_callback_symbol() is called for
 * all signal callbacks which are referenced by the loaded XML, this
 * function will require that #GModule be supported on the platform.
 *
 * If you rely on #GModule support to lookup callbacks in the symbol table,
 * the following details should be noted:
 *
 * When compiling applications for Windows, you must declare signal callbacks
 * with #G_MODULE_EXPORT, or they will not be put in the symbol table.
 * On Linux and Unices, this is not necessary; applications should instead
 * be compiled with the -Wl,--export-dynamic CFLAGS, and linked against
 * gmodule-export-2.0.
 **/
void
gtk_builder_connect_signals (GtkBuilder *builder,
                             gpointer    user_data)
{
  ConnectArgs args;

  g_return_if_fail (GTK_IS_BUILDER (builder));

  args.data = user_data;

  if (g_module_supported ())
    args.module = g_module_open (NULL, G_MODULE_BIND_LAZY);
  else
    args.module = NULL;

  gtk_builder_connect_signals_full (builder,
                                    gtk_builder_connect_signals_default,
                                    &args);
  if (args.module)
    g_module_close (args.module);
}

/**
 * GtkBuilderConnectFunc:
 * @builder: a #GtkBuilder
 * @object: object to connect a signal to
 * @signal_name: name of the signal
 * @handler_name: name of the handler
 * @connect_object: (nullable): a #GObject, if non-%NULL, use g_signal_connect_object()
 * @flags: #GConnectFlags to use
 * @user_data: user data
 *
 * This is the signature of a function used to connect signals.  It is used
 * by the gtk_builder_connect_signals() and gtk_builder_connect_signals_full()
 * methods.  It is mainly intended for interpreted language bindings, but
 * could be useful where the programmer wants more control over the signal
 * connection process. Note that this function can only be called once,
 * subsequent calls will do nothing.
 */

/**
 * gtk_builder_connect_signals_full:
 * @builder: a #GtkBuilder
 * @func: (scope call): the function used to connect the signals
 * @user_data: arbitrary data that will be passed to the connection function
 *
 * This function can be thought of the interpreted language binding
 * version of gtk_builder_connect_signals(), except that it does not
 * require GModule to function correctly.
 */
void
gtk_builder_connect_signals_full (GtkBuilder            *builder,
                                  GtkBuilderConnectFunc  func,
                                  gpointer               user_data)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GSList *l;
  GObject *object;
  GObject *connect_object;
  GString *detailed_id = NULL;

  g_return_if_fail (GTK_IS_BUILDER (builder));
  g_return_if_fail (func != NULL);

  if (!priv->signals)
    return;

  priv->signals = g_slist_reverse (priv->signals);
  for (l = priv->signals; l; l = l->next)
    {
      SignalInfo *signal = (SignalInfo*)l->data;
      const gchar *signal_name;

      g_assert (signal != NULL);
      g_assert (signal->id != 0);

      signal_name = g_signal_name (signal->id);

      object = g_hash_table_lookup (priv->objects, signal->object_name);
      g_assert (object != NULL);

      connect_object = NULL;

      if (signal->connect_object_name)
        {
          connect_object = g_hash_table_lookup (priv->objects,
                                                signal->connect_object_name);
          if (!connect_object)
              g_warning ("Could not lookup object %s on signal %s of object %s",
                         signal->connect_object_name, signal_name,
                         signal->object_name);
        }

      if (signal->detail)
        {
          if (detailed_id == NULL)
            detailed_id = g_string_new ("");

          g_string_printf (detailed_id, "%s::%s", signal_name,
                           g_quark_to_string (signal->detail));
          signal_name = detailed_id->str;
        }

      func (builder, object, signal_name, signal->handler,
            connect_object, signal->flags, user_data);
    }

  g_slist_free_full (priv->signals, (GDestroyNotify)_free_signal_info);
  priv->signals = NULL;

  if (detailed_id)
    g_string_free (detailed_id, TRUE);
}

/**
 * gtk_builder_value_from_string:
 * @builder: a #GtkBuilder
 * @pspec: the #GParamSpec for the property
 * @string: the string representation of the value
 * @value: (out): the #GValue to store the result in
 * @error: (allow-none): return location for an error, or %NULL
 *
 * This function demarshals a value from a string. This function
 * calls g_value_init() on the @value argument, so it need not be
 * initialised beforehand.
 *
 * This function can handle char, uchar, boolean, int, uint, long,
 * ulong, enum, flags, float, double, string, #GdkRGBA and
 * #GtkAdjustment type values. Support for #GtkWidget type values is
 * still to come.
 *
 * Upon errors %FALSE will be returned and @error will be assigned a
 * #GError from the #GTK_BUILDER_ERROR domain.
 *
 * Returns: %TRUE on success
 */
gboolean
gtk_builder_value_from_string (GtkBuilder   *builder,
                               GParamSpec   *pspec,
                               const gchar  *string,
                               GValue       *value,
                               GError      **error)
{
  g_return_val_if_fail (GTK_IS_BUILDER (builder), FALSE);
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (string != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /*
   * GParamSpecUnichar has the internal type G_TYPE_UINT,
   * so we cannot handle this in the switch, do it separately
   */
  if (G_IS_PARAM_SPEC_UNICHAR (pspec))
    {
      gunichar c;
      g_value_init (value, G_TYPE_UINT);
      c = g_utf8_get_char_validated (string, strlen (string));
      if (c != 0 && c != (gunichar)-1 && c != (gunichar)-2)
        g_value_set_uint (value, c);
      return TRUE;
    }

  /*
   * GParamSpecVariant can specify a GVariantType which can help with
   * parsing, so we need to take care of that here.
   */
  if (G_IS_PARAM_SPEC_VARIANT (pspec))
    {
      GParamSpecVariant *variant_pspec = G_PARAM_SPEC_VARIANT (pspec);
      const GVariantType *type;
      GVariant *variant;

      g_value_init (value, G_TYPE_VARIANT);

      /* The GVariant parser doesn't deal with indefinite types */
      if (g_variant_type_is_definite (variant_pspec->type))
        type = variant_pspec->type;
      else
        type = NULL;

      variant = g_variant_parse (type, string, NULL, NULL, error);
      if (variant == NULL)
        return FALSE;
      g_value_take_variant (value, variant);
      return TRUE;
    }

  return gtk_builder_value_from_string_type (builder,
                                             G_PARAM_SPEC_VALUE_TYPE (pspec),
                                             string, value, error);
}

/**
 * gtk_builder_value_from_string_type:
 * @builder: a #GtkBuilder
 * @type: the #GType of the value
 * @string: the string representation of the value
 * @value: (out): the #GValue to store the result in
 * @error: (allow-none): return location for an error, or %NULL
 *
 * Like gtk_builder_value_from_string(), this function demarshals
 * a value from a string, but takes a #GType instead of #GParamSpec.
 * This function calls g_value_init() on the @value argument, so it
 * need not be initialised beforehand.
 *
 * Upon errors %FALSE will be returned and @error will be assigned a
 * #GError from the #GTK_BUILDER_ERROR domain.
 *
 * Returns: %TRUE on success
 */
gboolean
gtk_builder_value_from_string_type (GtkBuilder   *builder,
                                    GType         type,
                                    const gchar  *string,
                                    GValue       *value,
                                    GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  gboolean ret = TRUE;

  g_return_val_if_fail (string != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  g_value_init (value, type);

  switch (G_TYPE_FUNDAMENTAL (type))
    {
    case G_TYPE_CHAR:
      g_value_set_schar (value, string[0]);
      break;
    case G_TYPE_UCHAR:
      g_value_set_uchar (value, (guchar)string[0]);
      break;
    case G_TYPE_BOOLEAN:
      {
        gboolean b;

        if (!_gtk_builder_boolean_from_string (string, &b, error))
          {
            ret = FALSE;
            break;
          }
        g_value_set_boolean (value, b);
        break;
      }
    case G_TYPE_INT:
    case G_TYPE_LONG:
    case G_TYPE_INT64:
      {
        gint64 l;
        gchar *endptr = NULL;

        errno = 0;
        l = g_ascii_strtoll (string, &endptr, 0);
        if (errno || endptr == string)
          {
            g_set_error (error,
                         GTK_BUILDER_ERROR,
                         GTK_BUILDER_ERROR_INVALID_VALUE,
                         "Could not parse integer '%s'",
                         string);
            ret = FALSE;
            break;
          }
        if (G_VALUE_HOLDS_INT (value))
          g_value_set_int (value, l);
        else if (G_VALUE_HOLDS_LONG (value))
          g_value_set_long (value, l);
        else
          g_value_set_int64 (value, l);
        break;
      }
    case G_TYPE_UINT:
    case G_TYPE_ULONG:
    case G_TYPE_UINT64:
      {
        guint64 ul;
        gchar *endptr = NULL;
        errno = 0;
        ul = g_ascii_strtoull (string, &endptr, 0);
        if (errno || endptr == string)
          {
            g_set_error (error,
                         GTK_BUILDER_ERROR,
                         GTK_BUILDER_ERROR_INVALID_VALUE,
                         "Could not parse unsigned integer '%s'",
                         string);
            ret = FALSE;
            break;
          }
        if (G_VALUE_HOLDS_UINT (value))
          g_value_set_uint (value, ul);
        else if (G_VALUE_HOLDS_ULONG (value))
          g_value_set_ulong (value, ul);
        else
          g_value_set_uint64 (value, ul);
        break;
      }
    case G_TYPE_ENUM:
      {
        gint enum_value;
        if (!_gtk_builder_enum_from_string (type, string, &enum_value, error))
          {
            ret = FALSE;
            break;
          }
        g_value_set_enum (value, enum_value);
        break;
      }
    case G_TYPE_FLAGS:
      {
        guint flags_value;

        if (!_gtk_builder_flags_from_string (type, NULL, string, &flags_value, error))
          {
            ret = FALSE;
            break;
          }
        g_value_set_flags (value, flags_value);
        break;
      }
    case G_TYPE_FLOAT:
    case G_TYPE_DOUBLE:
      {
        gdouble d;
        gchar *endptr = NULL;
        errno = 0;
        d = g_ascii_strtod (string, &endptr);
        if (errno || endptr == string)
          {
            g_set_error (error,
                         GTK_BUILDER_ERROR,
                         GTK_BUILDER_ERROR_INVALID_VALUE,
                         "Could not parse double '%s'",
                         string);
            ret = FALSE;
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
    case G_TYPE_VARIANT:
      {
        GVariant *variant;

        variant = g_variant_parse (NULL, string, NULL, NULL, error);
        if (value != NULL)
          g_value_take_variant (value, variant);
        else
          ret = FALSE;
      }
      break;
    case G_TYPE_BOXED:
      if (G_VALUE_HOLDS (value, GDK_TYPE_RGBA))
        {
          GdkRGBA rgba = { 0 };

          if (gdk_rgba_parse (&rgba, string))
            g_value_set_boxed (value, &rgba);
          else
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_VALUE,
                           "Could not parse RGBA color '%s'",
                           string);
              ret = FALSE;
            }
        }
      else if (G_VALUE_HOLDS (value, GSK_TYPE_TRANSFORM))
        {
          GskTransform *transform;

          if (gsk_transform_parse (string, &transform))
            g_value_take_boxed (value, transform);
          else
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_VALUE,
                           "Could not parse transform '%s'",
                           string);
              ret = FALSE;
            }
        }
      else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
        {
          gchar **vector = g_strsplit (string, "\n", 0);
          g_value_take_boxed (value, vector);
        }
      else
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Could not parse '%s' as a %s",
                       string, G_VALUE_TYPE_NAME (value));
          ret = FALSE;
        }
      break;
    case G_TYPE_OBJECT:
    case G_TYPE_INTERFACE:
      if (G_VALUE_HOLDS (value, GDK_TYPE_PIXBUF) ||
          G_VALUE_HOLDS (value, GDK_TYPE_PAINTABLE) ||
          G_VALUE_HOLDS (value, GDK_TYPE_TEXTURE))
        {
          gchar *filename;
          GError *tmp_error = NULL;
          GdkPixbuf *pixbuf = NULL;
          GObject *object;

          object = g_hash_table_lookup (priv->objects, string);

          if (object)
            {
              if (g_type_is_a (G_OBJECT_TYPE (object), G_VALUE_TYPE (value)))
                {
                  g_value_set_object (value, object);
                  return TRUE;
                }
              else
                {
                  g_set_error (error,
                               GTK_BUILDER_ERROR,
                               GTK_BUILDER_ERROR_INVALID_VALUE,
                               "Could not load image '%s': "
                               " '%s' is already used as object id for a %s",
                               string, string, G_OBJECT_TYPE_NAME (object));
                  return FALSE;
                }
            }

          filename = _gtk_builder_get_resource_path (builder, string);
          if (filename != NULL)
            {
              GInputStream *stream = g_resources_open_stream (filename, 0, &tmp_error);
              if (stream != NULL)
                {
                  pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, &tmp_error);
                  g_object_unref (stream);
                }
            }
          else
            {
              filename = _gtk_builder_get_absolute_filename (builder, string);
              pixbuf = gdk_pixbuf_new_from_file (filename, &tmp_error);
            }

          if (pixbuf == NULL)
            {
              GtkIconTheme *theme;
              GdkPaintable *texture;

              g_warning ("Could not load image '%s': %s",
                         string, tmp_error->message);
              g_error_free (tmp_error);

              /* fall back to a missing image */
              theme = gtk_icon_theme_get_default ();
              texture = gtk_icon_theme_load_icon (theme,
                                                 "image-missing",
                                                 16,
                                                 GTK_ICON_LOOKUP_USE_BUILTIN,
                                                 NULL);
              pixbuf = gdk_pixbuf_get_from_texture (GDK_TEXTURE (texture));
              g_object_unref (texture);
            }

          if (pixbuf)
            {
              if (G_VALUE_HOLDS (value, GDK_TYPE_TEXTURE) ||
                  G_VALUE_HOLDS (value, GDK_TYPE_PAINTABLE))
                {
                  GdkTexture *texture = gdk_texture_new_for_pixbuf (pixbuf);
                  g_value_set_object (value, texture);
                  g_object_unref (texture);
                }
              else
                {
                  g_value_set_object (value, pixbuf);
                }
              g_object_unref (G_OBJECT (pixbuf));
            }

          g_free (filename);

          ret = TRUE;
        }
      else if (G_VALUE_HOLDS (value, G_TYPE_FILE))
        {
          GFile *file;

          if (g_hash_table_contains (priv->objects, string))
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_VALUE,
                           "Could not create file '%s': "
                           " '%s' is already used as object id",
                           string, string);
              return FALSE;
            }

          file = g_file_new_for_uri (string);
          g_value_set_object (value, file);
          g_object_unref (G_OBJECT (file));

          ret = TRUE;
        }
      else
        ret = FALSE;
      break;
    case G_TYPE_POINTER:
      if (G_VALUE_HOLDS (value, G_TYPE_GTYPE))
        {
          GType resolved_type;

          resolved_type = gtk_builder_get_type_from_name (builder, string);
          if (resolved_type == G_TYPE_INVALID)
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_VALUE,
                           "Unsupported GType '%s' for value of type 'GType'", string);
              return FALSE;
            }
          g_value_set_gtype (value, resolved_type);

          ret = TRUE;
        }
      else
        ret = FALSE;
      break;
    default:
      ret = FALSE;
      break;
    }

  /* Catch unassigned error for object types as well as any unsupported types.
   * While parsing GtkBuilder; object types are deserialized
   * without calling gtk_builder_value_from_string_type().
   */
  if (!ret && error && *error == NULL)
    g_set_error (error,
                 GTK_BUILDER_ERROR,
                 GTK_BUILDER_ERROR_INVALID_VALUE,
                 "Unsupported GType '%s'", g_type_name (type));

  return ret;
}

gboolean
_gtk_builder_enum_from_string (GType         type,
                               const gchar  *string,
                               gint         *enum_value,
                               GError      **error)
{
  GEnumClass *eclass;
  GEnumValue *ev;
  gchar *endptr;
  gint value;
  gboolean ret;

  g_return_val_if_fail (G_TYPE_IS_ENUM (type), FALSE);
  g_return_val_if_fail (string != NULL, FALSE);

  ret = TRUE;

  endptr = NULL;
  errno = 0;
  value = g_ascii_strtoull (string, &endptr, 0);
  if (errno == 0 && endptr != string) /* parsed a number */
    *enum_value = value;
  else
    {
      eclass = g_type_class_ref (type);
      ev = g_enum_get_value_by_name (eclass, string);
      if (!ev)
        ev = g_enum_get_value_by_nick (eclass, string);

      if (ev)
        *enum_value = ev->value;
      else
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Could not parse enum: '%s'",
                       string);
          ret = FALSE;
        }

      g_type_class_unref (eclass);
    }

  return ret;
}

gboolean
_gtk_builder_flags_from_string (GType         type,
                                GFlagsValue  *aliases,
                                const gchar  *string,
                                guint        *flags_value,
                                GError      **error)
{
  GFlagsClass *fclass;
  gchar *endptr, *prevptr;
  guint i, j, k, value;
  gchar *flagstr;
  GFlagsValue *fv;
  const gchar *flag;
  gunichar ch;
  gboolean eos, ret;

  g_return_val_if_fail (G_TYPE_IS_FLAGS (type), FALSE);
  g_return_val_if_fail (string != 0, FALSE);

  ret = TRUE;

  endptr = NULL;
  errno = 0;
  value = g_ascii_strtoull (string, &endptr, 0);
  if (errno == 0 && endptr != string) /* parsed a number */
    *flags_value = value;
  else
    {
      fclass = g_type_class_ref (type);

      flagstr = g_strdup (string);
      for (value = i = j = 0; ; i++)
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

              fv = NULL;

              if (aliases)
                {
                  for (k = 0; aliases[k].value_nick; k++)
                    {
                      if (g_ascii_strcasecmp (aliases[k].value_nick, flag) == 0)
                        {
                          fv = &aliases[k];
                          break;
                        }
                    }
                }

              if (!fv)
                fv = g_flags_get_value_by_name (fclass, flag);

              if (!fv)
                fv = g_flags_get_value_by_nick (fclass, flag);

              if (fv)
                value |= fv->value;
              else
                {
                  g_set_error (error,
                               GTK_BUILDER_ERROR,
                               GTK_BUILDER_ERROR_INVALID_VALUE,
                               "Unknown flag: '%s'",
                               flag);
                  ret = FALSE;
                  break;
                }
            }

          if (eos)
            {
              *flags_value = value;
              break;
            }
        }

      g_free (flagstr);

      g_type_class_unref (fclass);
    }

  return ret;
}

gboolean
_gtk_builder_boolean_from_string (const gchar  *string,
                                  gboolean     *value,
                                  GError      **error)
{
  if (string[0] == '\0')
    goto error;
  else if (string[1] == '\0')
    {
      gchar c;

      c = string[0];
      if (c == '1' ||
          c == 'y' || c == 't' ||
          c == 'Y' || c == 'T')
        *value = TRUE;
      else if (c == '0' ||
               c == 'n' || c == 'f' ||
               c == 'N' || c == 'F')
        *value = FALSE;
      else
        goto error;
    }
  else
    {
      if (g_ascii_strcasecmp (string, "true") == 0 ||
          g_ascii_strcasecmp (string, "yes") == 0)
        *value = TRUE;
      else if (g_ascii_strcasecmp (string, "false") == 0 ||
               g_ascii_strcasecmp (string, "no") == 0)
        *value = FALSE;
      else
        goto error;
    }

  return TRUE;

error:
  g_set_error (error,
               GTK_BUILDER_ERROR,
               GTK_BUILDER_ERROR_INVALID_VALUE,
               "Could not parse boolean '%s'",
               string);
  return FALSE;
}

/**
 * gtk_builder_get_type_from_name:
 * @builder: a #GtkBuilder
 * @type_name: type name to lookup
 *
 * Looks up a type by name, using the virtual function that
 * #GtkBuilder has for that purpose. This is mainly used when
 * implementing the #GtkBuildable interface on a type.
 *
 * Returns: the #GType found for @type_name or #G_TYPE_INVALID
 *   if no type was found
 */
GType
gtk_builder_get_type_from_name (GtkBuilder  *builder,
                                const gchar *type_name)
{
  GType type;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), G_TYPE_INVALID);
  g_return_val_if_fail (type_name != NULL, G_TYPE_INVALID);

  type = GTK_BUILDER_GET_CLASS (builder)->get_type_from_name (builder, type_name);

  if (G_TYPE_IS_CLASSED (type))
    g_type_class_unref (g_type_class_ref (type));

  return type;
}

GQuark
gtk_builder_error_quark (void)
{
  return g_quark_from_static_string ("gtk-builder-error-quark");
}

gchar *
_gtk_builder_get_resource_path (GtkBuilder *builder, const gchar *string)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  if (g_str_has_prefix (string, "resource:///"))
    return g_uri_unescape_string (string + 11, "/");

  if (g_path_is_absolute (string) ||
      priv->resource_prefix == NULL)
    return NULL;

  return g_build_path ("/", priv->resource_prefix, string, NULL);
}

gchar *
_gtk_builder_get_absolute_filename (GtkBuilder  *builder,
                                    const gchar *string)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  gchar *filename;
  gchar *dirname = NULL;

  if (g_path_is_absolute (string))
    return g_strdup (string);

  if (priv->filename &&
      strcmp (priv->filename, ".") != 0)
    {
      dirname = g_path_get_dirname (priv->filename);

      if (strcmp (dirname, ".") == 0)
        {
          g_free (dirname);
          dirname = g_get_current_dir ();
        }
    }
  else
    dirname = g_get_current_dir ();

  filename = g_build_filename (dirname, string, NULL);
  g_free (dirname);

  return filename;
}

GType
_gtk_builder_get_template_type (GtkBuilder *builder)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  return priv->template_type;
}

/**
 * gtk_builder_add_callback_symbol:
 * @builder: a #GtkBuilder
 * @callback_name: The name of the callback, as expected in the XML
 * @callback_symbol: (scope async): The callback pointer
 *
 * Adds the @callback_symbol to the scope of @builder under the given @callback_name.
 *
 * Using this function overrides the behavior of gtk_builder_connect_signals()
 * for any callback symbols that are added. Using this method allows for better
 * encapsulation as it does not require that callback symbols be declared in
 * the global namespace.
 */
void
gtk_builder_add_callback_symbol (GtkBuilder  *builder,
                                 const gchar *callback_name,
                                 GCallback    callback_symbol)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  g_return_if_fail (GTK_IS_BUILDER (builder));
  g_return_if_fail (callback_name && callback_name[0]);
  g_return_if_fail (callback_symbol != NULL);

  if (!priv->callbacks)
    priv->callbacks = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, NULL);

  g_hash_table_insert (priv->callbacks, g_strdup (callback_name), callback_symbol);
}

/**
 * gtk_builder_add_callback_symbols:
 * @builder: a #GtkBuilder
 * @first_callback_name: The name of the callback, as expected in the XML
 * @first_callback_symbol: (scope async): The callback pointer
 * @...: A list of callback name and callback symbol pairs terminated with %NULL
 *
 * A convenience function to add many callbacks instead of calling
 * gtk_builder_add_callback_symbol() for each symbol.
 */
void
gtk_builder_add_callback_symbols (GtkBuilder  *builder,
                                  const gchar *first_callback_name,
                                  GCallback    first_callback_symbol,
                                  ...)
{
  va_list var_args;
  const gchar *callback_name;
  GCallback callback_symbol;

  g_return_if_fail (GTK_IS_BUILDER (builder));
  g_return_if_fail (first_callback_name && first_callback_name[0]);
  g_return_if_fail (first_callback_symbol != NULL);

  callback_name = first_callback_name;
  callback_symbol = first_callback_symbol;

  va_start (var_args, first_callback_symbol);

  do {

    gtk_builder_add_callback_symbol (builder, callback_name, callback_symbol);

    callback_name = va_arg (var_args, const gchar*);

    if (callback_name)
      callback_symbol = va_arg (var_args, GCallback);

  } while (callback_name != NULL);

  va_end (var_args);
}

/**
 * gtk_builder_lookup_callback_symbol: (skip)
 * @builder: a #GtkBuilder
 * @callback_name: The name of the callback
 *
 * Fetches a symbol previously added to @builder
 * with gtk_builder_add_callback_symbols()
 *
 * This function is intended for possible use in language bindings
 * or for any case that one might be cusomizing signal connections
 * using gtk_builder_connect_signals_full()
 *
 * Returns: (nullable): The callback symbol in @builder for @callback_name, or %NULL
 */
GCallback
gtk_builder_lookup_callback_symbol (GtkBuilder  *builder,
                                    const gchar *callback_name)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (callback_name && callback_name[0], NULL);

  if (!priv->callbacks)
    return NULL;

  return g_hash_table_lookup (priv->callbacks, callback_name);
}

/**
 * gtk_builder_new_from_file:
 * @filename: filename of user interface description file
 *
 * Builds the [GtkBuilder UI definition][BUILDER-UI]
 * in the file @filename.
 *
 * If there is an error opening the file or parsing the description then
 * the program will be aborted.  You should only ever attempt to parse
 * user interface descriptions that are shipped as part of your program.
 *
 * Returns: a #GtkBuilder containing the described interface
 **/
GtkBuilder *
gtk_builder_new_from_file (const gchar *filename)
{
  GError *error = NULL;
  GtkBuilder *builder;

  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_file (builder, filename, &error))
    g_error ("failed to add UI from file %s: %s", filename, error->message);

  return builder;
}

/**
 * gtk_builder_new_from_resource:
 * @resource_path: a #GResource resource path
 *
 * Builds the [GtkBuilder UI definition][BUILDER-UI]
 * at @resource_path.
 *
 * If there is an error locating the resource or parsing the
 * description, then the program will be aborted.
 *
 * Returns: a #GtkBuilder containing the described interface
 **/
GtkBuilder *
gtk_builder_new_from_resource (const gchar *resource_path)
{
  GError *error = NULL;
  GtkBuilder *builder;

  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_resource (builder, resource_path, &error))
    g_error ("failed to add UI from resource %s: %s", resource_path, error->message);

  return builder;
}

/**
 * gtk_builder_new_from_string:
 * @string: a user interface (XML) description
 * @length: the length of @string, or -1
 *
 * Builds the user interface described by @string (in the
 * [GtkBuilder UI definition][BUILDER-UI] format).
 *
 * If @string is %NULL-terminated, then @length should be -1.
 * If @length is not -1, then it is the length of @string.
 *
 * If there is an error parsing @string then the program will be
 * aborted. You should not attempt to parse user interface description
 * from untrusted sources.
 *
 * Returns: a #GtkBuilder containing the interface described by @string
 **/
GtkBuilder *
gtk_builder_new_from_string (const gchar *string,
                             gssize       length)
{
  GError *error = NULL;
  GtkBuilder *builder;

  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_string (builder, string, length, &error))
    g_error ("failed to add UI: %s", error->message);

  return builder;
}

/**
 * gtk_builder_set_application:
 * @builder: a #GtkBuilder
 * @application: a #GtkApplication
 *
 * Sets the application associated with @builder.
 *
 * You only need this function if there is more than one #GApplication
 * in your process. @application cannot be %NULL.
 **/
void
gtk_builder_set_application (GtkBuilder     *builder,
                             GtkApplication *application)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  g_return_if_fail (GTK_IS_BUILDER (builder));
  g_return_if_fail (GTK_IS_APPLICATION (application));

  if (priv->application)
    g_object_unref (priv->application);

  priv->application = g_object_ref (application);
}

/**
 * gtk_builder_get_application:
 * @builder: a #GtkBuilder
 *
 * Gets the #GtkApplication associated with the builder.
 *
 * The #GtkApplication is used for creating action proxies as requested
 * from XML that the builder is loading.
 *
 * By default, the builder uses the default application: the one from
 * g_application_get_default(). If you want to use another application
 * for constructing proxies, use gtk_builder_set_application().
 *
 * Returns: (nullable) (transfer none): the application being used by the builder,
 *     or %NULL
 **/
GtkApplication *
gtk_builder_get_application (GtkBuilder *builder)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);

  if (!priv->application)
    {
      GApplication *application;

      application = g_application_get_default ();
      if (application && GTK_IS_APPLICATION (application))
        priv->application = g_object_ref (GTK_APPLICATION (application));
    }

  return priv->application;
}

/*< private >
 * _gtk_builder_prefix_error:
 * @builder: a #GtkBuilder
 * @context: the #GtkBuildableParseContext
 * @error: an error
 *
 * Calls g_prefix_error() to prepend a filename:line:column marker
 * to the given error. The filename is taken from @builder, and
 * the line and column are obtained by calling
 * g_markup_parse_context_get_position().
 *
 * This is intended to be called on errors returned by
 * g_markup_collect_attributes() in a start_element vfunc.
 */
void
_gtk_builder_prefix_error (GtkBuilder                *builder,
                           GtkBuildableParseContext  *context,
                           GError                   **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  gint line, col;

  gtk_buildable_parse_context_get_position (context, &line, &col);
  g_prefix_error (error, "%s:%d:%d ", priv->filename, line, col);
}

/*< private >
 * _gtk_builder_error_unhandled_tag:
 * @builder: a #GtkBuilder
 * @context: the #GtkBuildableParseContext
 * @object: name of the object that is being handled
 * @element_name: name of the element whose start tag is being handled
 * @error: return location for the error
 *
 * Sets @error to a suitable error indicating that an @element_name
 * tag is not expected in the custom markup for @object.
 *
 * This is intended to be called in a start_element vfunc.
 */
void
_gtk_builder_error_unhandled_tag (GtkBuilder                *builder,
                                  GtkBuildableParseContext  *context,
                                  const gchar               *object,
                                  const gchar               *element_name,
                                  GError                   **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  gint line, col;

  gtk_buildable_parse_context_get_position (context, &line, &col);
  g_set_error (error,
               GTK_BUILDER_ERROR,
               GTK_BUILDER_ERROR_UNHANDLED_TAG,
               "%s:%d:%d Unsupported tag for %s: <%s>",
               priv->filename, line, col,
               object, element_name);
}

/*< private >
 * @builder: a #GtkBuilder
 * @context: the #GtkBuildableParseContext
 * @parent_name: the name of the expected parent element
 * @error: return location for an error
 *
 * Checks that the parent element of the currently handled
 * start tag is @parent_name and set @error if it isn't.
 *
 * This is intended to be called in start_element vfuncs to
 * ensure that element nesting is as intended.
 *
 * Returns: %TRUE if @parent_name is the parent element
 */
gboolean
_gtk_builder_check_parent (GtkBuilder                *builder,
                           GtkBuildableParseContext  *context,
                           const gchar               *parent_name,
                           GError                   **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GPtrArray *stack;
  gint line, col;
  const gchar *parent;
  const gchar *element;

  stack = gtk_buildable_parse_context_get_element_stack (context);

  element = g_ptr_array_index (stack, stack->len - 1);
  parent = stack->len > 1 ? g_ptr_array_index (stack, stack->len - 2) : "";

  if (g_str_equal (parent_name, parent) ||
      (g_str_equal (parent_name, "object") && g_str_equal (parent, "template")))
    return TRUE;

  gtk_buildable_parse_context_get_position (context, &line, &col);
  g_set_error (error,
               GTK_BUILDER_ERROR,
               GTK_BUILDER_ERROR_INVALID_TAG,
               "%s:%d:%d Can't use <%s> here",
               priv->filename, line, col, element);

  return FALSE;
}

/*< private >
 * @builder: a #GtkBuilder
 * @name: object name to look up
 * @line: line number where @name was encountered
 * @col: column number where @name was encountered
 *
 * Looks up an object by name. Similar to gtk_builder_get_object(),
 * but sets an error if lookup fails during custom_tag_end,
 * custom_finished or parser_finished vfuncs.
 *
 * The reason for doing things this way is that these vfuncs don't
 * take a GError** parameter to return an error.
 *
 * Returns: the found object
 */
GObject *
_gtk_builder_lookup_object (GtkBuilder  *builder,
                            const gchar *name,
                            gint         line,
                            gint         col)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GObject *obj;
  GError *error = NULL;

  obj = g_hash_table_lookup (priv->objects, name);
  error = (GError *) g_object_get_data (G_OBJECT (builder), "lookup-error");

  if (!obj && !error)
    {
      g_set_error (&error,
                   GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_ID,
                   "%s:%d:%d Object with ID %s not found",
                   priv->filename, line, col, name);
      g_object_set_data_full (G_OBJECT (builder), "lookup-error",
                              error, (GDestroyNotify)g_error_free);
    }

  return obj;
}

/*< private >
 * _gtk_builder_lookup_failed:
 * @GtkBuilder: a #GtkBuilder
 * @error: return location for error
 *
 * Finds whether any object lookups have failed.
 *
 * Returns: %TRUE if @error has been set
 */
gboolean
_gtk_builder_lookup_failed (GtkBuilder  *builder,
                            GError     **error)
{
  GError *lookup_error;

  lookup_error = (GError*) g_object_steal_data (G_OBJECT (builder), "lookup-error");
  if (lookup_error)
    {
      g_propagate_error (error, lookup_error);
      return TRUE;
    }

  return FALSE;
}
