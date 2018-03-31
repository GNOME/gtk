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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkcontainerprivate.h"

#include "gtkadjustment.h"
#include "gtkassistant.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkintl.h"
#include "gtkpopovermenu.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtkshortcutssection.h"
#include "gtkshortcutswindow.h"
#include "gtksizerequest.h"
#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkwindow.h"

#include "a11y/gtkcontaineraccessibleprivate.h"

#include <gobject/gobjectnotifyqueue.c>
#include <gobject/gvaluecollector.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* A handful of containers inside GTK+ are cheating and widgets
 * inside internal structure as direct children for the purpose
 * of forall().
 */
#define SPECIAL_CONTAINER(x) (GTK_IS_ASSISTANT (x) || \
                              GTK_IS_POPOVER_MENU (x) || \
                              GTK_IS_SHORTCUTS_SECTION (x) || \
                              GTK_IS_SHORTCUTS_WINDOW (x))

/**
 * SECTION:gtkcontainer
 * @Short_description: Base class for widgets which contain other widgets
 * @Title: GtkContainer
 *
 * A GTK+ user interface is constructed by nesting widgets inside widgets.
 * Container widgets are the inner nodes in the resulting tree of widgets:
 * they contain other widgets. So, for example, you might have a #GtkWindow
 * containing a #GtkFrame containing a #GtkLabel. If you wanted an image instead
 * of a textual label inside the frame, you might replace the #GtkLabel widget
 * with a #GtkImage widget.
 *
 * There are two major kinds of container widgets in GTK+. Both are subclasses
 * of the abstract GtkContainer base class.
 *
 * The first type of container widget has a single child widget and derives
 * from #GtkBin. These containers are decorators, which
 * add some kind of functionality to the child. For example, a #GtkButton makes
 * its child into a clickable button; a #GtkFrame draws a frame around its child
 * and a #GtkWindow places its child widget inside a top-level window.
 *
 * The second type of container can have more than one child; its purpose is to
 * manage layout. This means that these containers assign
 * sizes and positions to their children. For example, a horizontal #GtkBox arranges its
 * children in a horizontal row, and a #GtkGrid arranges the widgets it contains
 * in a two-dimensional grid.
 *
 * For implementations of #GtkContainer the virtual method #GtkContainerClass.forall()
 * is always required, since it's used for drawing and other internal operations
 * on the children.
 * If the #GtkContainer implementation expect to have non internal children
 * it's needed to implement both #GtkContainerClass.add() and #GtkContainerClass.remove().
 * If the GtkContainer implementation has internal children, they should be added
 * with gtk_widget_set_parent() on init() and removed with gtk_widget_unparent()
 * in the #GtkWidgetClass.destroy() implementation.
 * See more about implementing custom widgets at https://wiki.gnome.org/HowDoI/CustomWidgets
 *
 * # Child properties
 *
 * GtkContainer introduces child properties.
 * These are object properties that are not specific
 * to either the container or the contained widget, but rather to their relation.
 * Typical examples of child properties are the position or pack-type of a widget
 * which is contained in a #GtkBox.
 *
 * Use gtk_container_class_install_child_property() to install child properties
 * for a container class and gtk_container_class_find_child_property() or
 * gtk_container_class_list_child_properties() to get information about existing
 * child properties.
 *
 * To set the value of a child property, use gtk_container_child_set_property(),
 * gtk_container_child_set() or gtk_container_child_set_valist().
 * To obtain the value of a child property, use
 * gtk_container_child_get_property(), gtk_container_child_get() or
 * gtk_container_child_get_valist(). To emit notification about child property
 * changes, use gtk_widget_child_notify().
 *
 * # GtkContainer as GtkBuildable
 *
 * The GtkContainer implementation of the GtkBuildable interface supports
 * a <packing> element for children, which can contain multiple <property>
 * elements that specify child properties for the child.
 * 
 * Since 2.16, child properties can also be marked as translatable using
 * the same “translatable”, “comments” and “context” attributes that are used
 * for regular properties.
 *
 * Since 3.16, containers can have a <focus-chain> element containing multiple
 * <widget> elements, one for each child that should be added to the focus
 * chain. The ”name” attribute gives the id of the widget.
 *
 * An example of these properties in UI definitions:
 * |[
 * <object class="GtkBox">
 *   <child>
 *     <object class="GtkEntry" id="entry1"/>
 *     <packing>
 *       <property name="pack-type">start</property>
 *     </packing>
 *   </child>
 *   <child>
 *     <object class="GtkEntry" id="entry2"/>
 *   </child>
 *   <focus-chain>
 *     <widget name="entry1"/>
 *     <widget name="entry2"/>
 *   </focus-chain>
 * </object>
 * ]|
 *
 */


struct _GtkContainerPrivate
{
  GdkFrameClock *resize_clock;
  guint resize_handler;

  guint has_focus_chain    : 1;
  guint restyle_pending    : 1;
};

enum {
  ADD,
  REMOVE,
  CHECK_RESIZE,
  SET_FOCUS_CHILD,
  LAST_SIGNAL
};

#define PARAM_SPEC_PARAM_ID(pspec)              ((pspec)->param_id)
#define PARAM_SPEC_SET_PARAM_ID(pspec, id)      ((pspec)->param_id = (id))


/* --- prototypes --- */
static void     gtk_container_base_class_init      (GtkContainerClass *klass);
static void     gtk_container_base_class_finalize  (GtkContainerClass *klass);
static void     gtk_container_class_init           (GtkContainerClass *klass);
static void     gtk_container_init                 (GtkContainer      *container);
static void     gtk_container_destroy              (GtkWidget         *widget);
static void     gtk_container_add_unimplemented    (GtkContainer      *container,
                                                    GtkWidget         *widget);
static void     gtk_container_remove_unimplemented (GtkContainer      *container,
                                                    GtkWidget         *widget);
static void     gtk_container_real_check_resize    (GtkContainer      *container);
static void     gtk_container_compute_expand       (GtkWidget         *widget,
                                                    gboolean          *hexpand_p,
                                                    gboolean          *vexpand_p);
static gboolean gtk_container_focus                (GtkWidget         *widget,
                                                    GtkDirectionType   direction);
static void     gtk_container_real_set_focus_child (GtkContainer      *container,
                                                    GtkWidget         *widget);

static void     gtk_container_children_callback    (GtkWidget         *widget,
                                                    gpointer           client_data);
static GtkSizeRequestMode gtk_container_get_request_mode (GtkWidget   *widget);

static GtkWidgetPath * gtk_container_real_get_path_for_child (GtkContainer *container,
                                                              GtkWidget    *child);

/* GtkBuildable */
static void gtk_container_buildable_init           (GtkBuildableIface *iface);
static void gtk_container_buildable_add_child      (GtkBuildable *buildable,
                                                    GtkBuilder   *builder,
                                                    GObject      *child,
                                                    const gchar  *type);
static gboolean gtk_container_buildable_custom_tag_start (GtkBuildable  *buildable,
                                                          GtkBuilder    *builder,
                                                          GObject       *child,
                                                          const gchar   *tagname,
                                                          GMarkupParser *parser,
                                                          gpointer      *data);
static void    gtk_container_buildable_custom_tag_end (GtkBuildable *buildable,
                                                       GtkBuilder   *builder,
                                                       GObject      *child,
                                                       const gchar  *tagname,
                                                       gpointer     *data);
static void    gtk_container_buildable_custom_finished (GtkBuildable *buildable,
                                                        GtkBuilder   *builder,
                                                        GObject      *child,
                                                        const gchar  *tagname,
                                                        gpointer      data);

/* --- variables --- */
static GQuark                vadjustment_key_id;
static GQuark                hadjustment_key_id;
static GQuark                quark_focus_chain;
static guint                 container_signals[LAST_SIGNAL] = { 0 };
static gint                  GtkContainer_private_offset;
static GtkWidgetClass       *gtk_container_parent_class = NULL;
extern GParamSpecPool       *_gtk_widget_child_property_pool;
extern GObjectNotifyContext *_gtk_widget_child_property_notify_context;
static GtkBuildableIface    *parent_buildable_iface;


/* --- functions --- */
static inline gpointer
gtk_container_get_instance_private (GtkContainer *self)
{
  return G_STRUCT_MEMBER_P (self, GtkContainer_private_offset);
}

GType
gtk_container_get_type (void)
{
  static GType container_type = 0;

  if (!container_type)
    {
      const GTypeInfo container_info =
      {
        sizeof (GtkContainerClass),
        (GBaseInitFunc) gtk_container_base_class_init,
        (GBaseFinalizeFunc) gtk_container_base_class_finalize,
        (GClassInitFunc) gtk_container_class_init,
        NULL        /* class_finalize */,
        NULL        /* class_data */,
        sizeof (GtkContainer),
        0           /* n_preallocs */,
        (GInstanceInitFunc) gtk_container_init,
        NULL,       /* value_table */
      };

      const GInterfaceInfo buildable_info =
      {
        (GInterfaceInitFunc) gtk_container_buildable_init,
        NULL,
        NULL
      };

      container_type =
        g_type_register_static (GTK_TYPE_WIDGET, I_("GtkContainer"),
                                &container_info, G_TYPE_FLAG_ABSTRACT);

      GtkContainer_private_offset =
        g_type_add_instance_private (container_type, sizeof (GtkContainerPrivate));

      g_type_add_interface_static (container_type,
                                   GTK_TYPE_BUILDABLE,
                                   &buildable_info);

    }

  return container_type;
}

static void
gtk_container_base_class_init (GtkContainerClass *class)
{
  /* reset instance specifc class fields that don't get inherited */
  class->set_child_property = NULL;
  class->get_child_property = NULL;
}

static void
gtk_container_base_class_finalize (GtkContainerClass *class)
{
  GList *list, *node;

  list = g_param_spec_pool_list_owned (_gtk_widget_child_property_pool, G_OBJECT_CLASS_TYPE (class));
  for (node = list; node; node = node->next)
    {
      GParamSpec *pspec = node->data;

      g_param_spec_pool_remove (_gtk_widget_child_property_pool, pspec);
      PARAM_SPEC_SET_PARAM_ID (pspec, 0);
      g_param_spec_unref (pspec);
    }
  g_list_free (list);
}

static void
gtk_container_class_init (GtkContainerClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gtk_container_parent_class = g_type_class_peek_parent (class);

  vadjustment_key_id = g_quark_from_static_string ("gtk-vadjustment");
  hadjustment_key_id = g_quark_from_static_string ("gtk-hadjustment");
  quark_focus_chain = g_quark_from_static_string ("gtk-container-focus-chain");

  widget_class->destroy = gtk_container_destroy;
  widget_class->compute_expand = gtk_container_compute_expand;
  widget_class->focus = gtk_container_focus;
  widget_class->get_request_mode = gtk_container_get_request_mode;

  class->add = gtk_container_add_unimplemented;
  class->remove = gtk_container_remove_unimplemented;
  class->check_resize = gtk_container_real_check_resize;
  class->forall = NULL;
  class->set_focus_child = gtk_container_real_set_focus_child;
  class->child_type = NULL;
  class->get_path_for_child = gtk_container_real_get_path_for_child;

  container_signals[ADD] =
    g_signal_new (I_("add"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkContainerClass, add),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_WIDGET);
  container_signals[REMOVE] =
    g_signal_new (I_("remove"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkContainerClass, remove),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_WIDGET);
  container_signals[CHECK_RESIZE] =
    g_signal_new (I_("check-resize"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkContainerClass, check_resize),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
  container_signals[SET_FOCUS_CHILD] =
    g_signal_new (I_("set-focus-child"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkContainerClass, set_focus_child),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_WIDGET);

  if (GtkContainer_private_offset != 0)
    g_type_class_adjust_private_offset (class, &GtkContainer_private_offset);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_CONTAINER_ACCESSIBLE);
}

static void
gtk_container_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = gtk_container_buildable_add_child;
  iface->custom_tag_start = gtk_container_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_container_buildable_custom_tag_end;
  iface->custom_finished = gtk_container_buildable_custom_finished;
}

static void
gtk_container_buildable_add_child (GtkBuildable  *buildable,
                                   GtkBuilder    *builder,
                                   GObject       *child,
                                   const gchar   *type)
{
  if (type)
    {
      GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
    }
  else if (GTK_IS_WIDGET (child) &&
           _gtk_widget_get_parent (GTK_WIDGET (child)) == NULL)
    {
      gtk_container_add (GTK_CONTAINER (buildable), GTK_WIDGET (child));
    }
  else
    g_warning ("Cannot add an object of type %s to a container of type %s",
               g_type_name (G_OBJECT_TYPE (child)), g_type_name (G_OBJECT_TYPE (buildable)));
}

static inline void
container_set_child_property (GtkContainer       *container,
                              GtkWidget          *child,
                              GParamSpec         *pspec,
                              const GValue       *value,
                              GObjectNotifyQueue *nqueue)
{
  GValue tmp_value = G_VALUE_INIT;
  GtkContainerClass *class = g_type_class_peek (pspec->owner_type);

  /* provide a copy to work from, convert (if necessary) and validate */
  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  if (!g_value_transform (value, &tmp_value))
    g_warning ("unable to set child property '%s' of type '%s' from value of type '%s'",
               pspec->name,
               g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
               G_VALUE_TYPE_NAME (value));
  else if (g_param_value_validate (pspec, &tmp_value) && !(pspec->flags & G_PARAM_LAX_VALIDATION))
    {
      gchar *contents = g_strdup_value_contents (value);

      g_warning ("value \"%s\" of type '%s' is invalid for property '%s' of type '%s'",
                 contents,
                 G_VALUE_TYPE_NAME (value),
                 pspec->name,
                 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
      g_free (contents);
    }
  else
    {
      class->set_child_property (container, child, PARAM_SPEC_PARAM_ID (pspec), &tmp_value, pspec);
      g_object_notify_queue_add (G_OBJECT (child), nqueue, pspec);
    }
  g_value_unset (&tmp_value);
}

static void
gtk_container_buildable_set_child_property (GtkContainer *container,
                                            GtkBuilder   *builder,
                                            GtkWidget    *child,
                                            gchar        *name,
                                            const gchar  *value)
{
  GParamSpec *pspec;
  GValue gvalue = G_VALUE_INIT;
  GError *error = NULL;
  GObjectNotifyQueue *nqueue;

  if (SPECIAL_CONTAINER (container))
    {
      /* This can happen with internal children of complex widgets.
       * Silently ignore the child properties in this case. We explicitly
       * allow it for GtkAssistant, since that is how it works.
       */
      return;
    }

  pspec = gtk_container_class_find_child_property (G_OBJECT_GET_CLASS (container), name);
  if (!pspec)
    {
      g_warning ("%s does not have a child property called %s",
                 G_OBJECT_TYPE_NAME (container), name);
      return;
    }
  else if (!(pspec->flags & G_PARAM_WRITABLE))
    {
      g_warning ("Child property '%s' of container class '%s' is not writable",
                 name, G_OBJECT_TYPE_NAME (container));
      return;
    }

  if (!gtk_builder_value_from_string (builder, pspec, value, &gvalue, &error))
    {
      g_warning ("Could not read property %s:%s with value %s of type %s: %s",
                 g_type_name (G_OBJECT_TYPE (container)),
                 name,
                 value,
                 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
                 error->message);
      g_error_free (error);
      return;
    }

  g_object_ref (container);
  g_object_ref (child);
  nqueue = g_object_notify_queue_freeze (G_OBJECT (child), _gtk_widget_child_property_notify_context);
  container_set_child_property (container, child, pspec, &gvalue, nqueue);
  g_object_notify_queue_thaw (G_OBJECT (child), nqueue);
  g_object_unref (container);
  g_object_unref (child);
  g_value_unset (&gvalue);
}

typedef struct {
  GtkBuilder   *builder;
  GtkContainer *container;
  GtkWidget    *child;
  GString      *string;
  gchar        *child_prop_name;
  gchar        *context;
  gboolean      translatable;
} PackingData;

static void
packing_start_element (GMarkupParseContext  *context,
                       const gchar          *element_name,
                       const gchar         **names,
                       const gchar         **values,
                       gpointer              user_data,
                       GError              **error)
{
  PackingData *data = (PackingData*)user_data;

  if (strcmp (element_name, "property") == 0)
    {
      const gchar *name;
      gboolean translatable = FALSE;
      const gchar *ctx = NULL;

      if (!_gtk_builder_check_parent (data->builder, context, "packing", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "translatable", &translatable,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "comments", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "context", &ctx,
                                        G_MARKUP_COLLECT_INVALID))
       {
         _gtk_builder_prefix_error (data->builder, context, error);
         return;
       }

     data->child_prop_name = g_strdup (name);
     data->translatable = translatable;
     data->context = g_strdup (ctx);
    }
  else if (strcmp (element_name, "packing") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "child", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkContainer", element_name,
                                        error);
    }
}

static void
packing_text_element (GMarkupParseContext  *context,
                      const gchar          *text,
                      gsize                 text_len,
                      gpointer              user_data,
                      GError              **error)
{
  PackingData *data = (PackingData*)user_data;

  if (data->child_prop_name)
    g_string_append_len (data->string, text, text_len);
}

static void
packing_end_element (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     gpointer              user_data,
                     GError              **error)
{
  PackingData *data = (PackingData*)user_data;

  /* translate the string */
  if (data->string->len && data->translatable)
    {
      const gchar *translated;
      const gchar *domain;

      domain = gtk_builder_get_translation_domain (data->builder);

      translated = _gtk_builder_parser_translate (domain,
                                                  data->context,
                                                  data->string->str);
      g_string_assign (data->string, translated);
    }

  if (data->child_prop_name)
    gtk_container_buildable_set_child_property (data->container,
                                                data->builder,
                                                data->child,
                                                data->child_prop_name,
                                                data->string->str);

  g_string_set_size (data->string, 0);
  g_clear_pointer (&data->child_prop_name, g_free);
  g_clear_pointer (&data->context, g_free);
  data->translatable = FALSE;
}

static const GMarkupParser packing_parser =
  {
    packing_start_element,
    packing_end_element,
    packing_text_element,
  };

typedef struct
  {
    gchar *name;
    gint line;
    gint col;
  } FocusChainWidget;

static void
focus_chain_widget_free (gpointer data)
{
  FocusChainWidget *fcw = data;

  g_free (fcw->name);
  g_free (fcw);
}

typedef struct
  {
    GSList *items;
    GObject *object;
    GtkBuilder *builder;
    gint line;
    gint col;
  } FocusChainData;

static void
focus_chain_start_element (GMarkupParseContext  *context,
                           const gchar          *element_name,
                           const gchar         **names,
                           const gchar         **values,
                           gpointer              user_data,
                           GError              **error)
{
  FocusChainData *data = (FocusChainData*)user_data;

  if (strcmp (element_name, "widget") == 0)
    {
      const gchar *name;
      FocusChainWidget *fcw;

      if (!_gtk_builder_check_parent (data->builder, context, "focus-chain", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      fcw = g_new (FocusChainWidget, 1);
      fcw->name = g_strdup (name);
      g_markup_parse_context_get_position (context, &fcw->line, &fcw->col);
      data->items = g_slist_prepend (data->items, fcw);
    }
  else if (strcmp (element_name, "focus-chain") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, "", NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkContainer", element_name,
                                        error);
    }
}

static const GMarkupParser focus_chain_parser =
  {
    focus_chain_start_element
  };

static gboolean
gtk_container_buildable_custom_tag_start (GtkBuildable  *buildable,
                                          GtkBuilder    *builder,
                                          GObject       *child,
                                          const gchar   *tagname,
                                          GMarkupParser *parser,
                                          gpointer      *parser_data)
{
  if (parent_buildable_iface->custom_tag_start (buildable, builder, child,
                                                tagname, parser, parser_data))
    return TRUE;

  if (child && strcmp (tagname, "packing") == 0)
    {
      PackingData *data;

      data = g_slice_new0 (PackingData);
      data->string = g_string_new ("");
      data->builder = builder;
      data->container = GTK_CONTAINER (buildable);
      data->child = GTK_WIDGET (child);
      data->child_prop_name = NULL;

      *parser = packing_parser;
      *parser_data = data;

      return TRUE;
    }
  else if (!child && strcmp (tagname, "focus-chain") == 0)
    {
      FocusChainData *data;

      data = g_slice_new0 (FocusChainData);
      data->items = NULL;
      data->object = G_OBJECT (buildable);
      data->builder = builder;

      *parser = focus_chain_parser;
      *parser_data = data;

      return TRUE;
    }

  return FALSE;
}

static void
gtk_container_buildable_custom_tag_end (GtkBuildable *buildable,
                                        GtkBuilder   *builder,
                                        GObject      *child,
                                        const gchar  *tagname,
                                        gpointer     *parser_data)
{
  if (strcmp (tagname, "packing") == 0)
    {
      PackingData *data = (PackingData*)parser_data;

      g_string_free (data->string, TRUE);
      g_slice_free (PackingData, data);

      return;
    }

  if (parent_buildable_iface->custom_tag_end)
    parent_buildable_iface->custom_tag_end (buildable, builder,
                                            child, tagname, parser_data);
}

static void
gtk_container_buildable_custom_finished (GtkBuildable *buildable,
                                         GtkBuilder   *builder,
                                         GObject      *child,
                                         const gchar  *tagname,
                                         gpointer      parser_data)
{
   if (strcmp (tagname, "focus-chain") == 0)
    {
      FocusChainData *data = (FocusChainData*)parser_data;
      FocusChainWidget *fcw;
      GSList *l;
      GList *chain;
      GObject *object;

      chain = NULL;
      for (l = data->items; l; l = l->next)
        {
          fcw = l->data;
          object = _gtk_builder_lookup_object (builder, fcw->name, fcw->line, fcw->col);
          if (!object)
            continue;
          chain = g_list_prepend (chain, object);
        }

      gtk_container_set_focus_chain (GTK_CONTAINER (data->object), chain);
      g_list_free (chain);

      g_slist_free_full (data->items, focus_chain_widget_free);
      g_slice_free (FocusChainData, data);

      return;
    }

  if (parent_buildable_iface->custom_finished)
    parent_buildable_iface->custom_finished (buildable, builder,
                                             child, tagname, parser_data);
}

/**
 * gtk_container_child_type:
 * @container: a #GtkContainer
 *
 * Returns the type of the children supported by the container.
 *
 * Note that this may return %G_TYPE_NONE to indicate that no more
 * children can be added, e.g. for a #GtkPaned which already has two
 * children.
 *
 * Returns: a #GType.
 **/
GType
gtk_container_child_type (GtkContainer *container)
{
  GType slot;
  GtkContainerClass *class;

  g_return_val_if_fail (GTK_IS_CONTAINER (container), 0);

  class = GTK_CONTAINER_GET_CLASS (container);
  if (class->child_type)
    slot = class->child_type (container);
  else
    slot = G_TYPE_NONE;

  return slot;
}

/* --- GtkContainer child property mechanism --- */

/**
 * gtk_container_child_notify:
 * @container: the #GtkContainer
 * @child: the child widget
 * @child_property: the name of a child property installed on
 *     the class of @container
 *
 * Emits a #GtkWidget::child-notify signal for the
 * [child property][child-properties]
 * @child_property on the child.
 *
 * This is an analogue of g_object_notify() for child properties.
 *
 * Also see gtk_widget_child_notify().
 */
void
gtk_container_child_notify (GtkContainer *container,
                            GtkWidget    *child,
                            const gchar  *child_property)
{
  GObject *obj;
  GParamSpec *pspec;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child_property != NULL);

  obj = G_OBJECT (child);

  if (obj->ref_count == 0)
    return;

  g_object_ref (obj);

  pspec = g_param_spec_pool_lookup (_gtk_widget_child_property_pool,
                                    child_property,
                                    G_OBJECT_TYPE (container),
                                    TRUE);

  if (pspec == NULL)
    {
      g_warning ("%s: container class '%s' has no child property named '%s'",
                 G_STRLOC,
                 G_OBJECT_TYPE_NAME (container),
                 child_property);
    }
  else
    {
      GObjectNotifyQueue *nqueue;

      nqueue = g_object_notify_queue_freeze (obj, _gtk_widget_child_property_notify_context);

      g_object_notify_queue_add (obj, nqueue, pspec);
      g_object_notify_queue_thaw (obj, nqueue);
    }

  g_object_unref (obj);
}

/**
 * gtk_container_child_notify_by_pspec:
 * @container: the #GtkContainer
 * @child: the child widget
 * @pspec: the #GParamSpec of a child property instealled on
 *     the class of @container
 *
 * Emits a #GtkWidget::child-notify signal for the
 * [child property][child-properties] specified by
 * @pspec on the child.
 *
 * This is an analogue of g_object_notify_by_pspec() for child properties.
 */
void
gtk_container_child_notify_by_pspec (GtkContainer *container,
                                     GtkWidget    *child,
                                     GParamSpec   *pspec)
{
  GObject *obj = G_OBJECT (child);
  GObjectNotifyQueue *nqueue;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  if (obj->ref_count == 0)
    return;

  g_object_ref (obj);

  nqueue = g_object_notify_queue_freeze (obj, _gtk_widget_child_property_notify_context);

  g_object_notify_queue_add (obj, nqueue, pspec);
  g_object_notify_queue_thaw (obj, nqueue);

  g_object_unref (obj);
}

static inline void
container_get_child_property (GtkContainer *container,
                              GtkWidget    *child,
                              GParamSpec   *pspec,
                              GValue       *value)
{
  GtkContainerClass *class = g_type_class_peek (pspec->owner_type);

  class->get_child_property (container, child, PARAM_SPEC_PARAM_ID (pspec), value, pspec);
}

/**
 * gtk_container_child_get_valist:
 * @container: a #GtkContainer
 * @child: a widget which is a child of @container
 * @first_property_name: the name of the first property to get
 * @var_args: return location for the first property, followed
 *     optionally by more name/return location pairs, followed by %NULL
 *
 * Gets the values of one or more child properties for @child and @container.
 **/
void
gtk_container_child_get_valist (GtkContainer *container,
                                GtkWidget    *child,
                                const gchar  *first_property_name,
                                va_list       var_args)
{
  const gchar *name;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));

  g_object_ref (container);
  g_object_ref (child);

  name = first_property_name;
  while (name)
    {
      GValue value = G_VALUE_INIT;
      GParamSpec *pspec;
      gchar *error;

      pspec = g_param_spec_pool_lookup (_gtk_widget_child_property_pool,
                                        name,
                                        G_OBJECT_TYPE (container),
                                        TRUE);
      if (!pspec)
        {
          g_warning ("%s: container class '%s' has no child property named '%s'",
                     G_STRLOC,
                     G_OBJECT_TYPE_NAME (container),
                     name);
          break;
        }
      if (!(pspec->flags & G_PARAM_READABLE))
        {
          g_warning ("%s: child property '%s' of container class '%s' is not readable",
                     G_STRLOC,
                     pspec->name,
                     G_OBJECT_TYPE_NAME (container));
          break;
        }
      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      container_get_child_property (container, child, pspec, &value);
      G_VALUE_LCOPY (&value, var_args, 0, &error);
      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          g_value_unset (&value);
          break;
        }
      g_value_unset (&value);
      name = va_arg (var_args, gchar*);
    }

  g_object_unref (child);
  g_object_unref (container);
}

/**
 * gtk_container_child_get_property:
 * @container: a #GtkContainer
 * @child: a widget which is a child of @container
 * @property_name: the name of the property to get
 * @value: a location to return the value
 *
 * Gets the value of a child property for @child and @container.
 **/
void
gtk_container_child_get_property (GtkContainer *container,
                                  GtkWidget    *child,
                                  const gchar  *property_name,
                                  GValue       *value)
{
  GParamSpec *pspec;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (G_IS_VALUE (value));

  g_object_ref (container);
  g_object_ref (child);
  pspec = g_param_spec_pool_lookup (_gtk_widget_child_property_pool, property_name,
                                    G_OBJECT_TYPE (container), TRUE);
  if (!pspec)
    g_warning ("%s: container class '%s' has no child property named '%s'",
               G_STRLOC,
               G_OBJECT_TYPE_NAME (container),
               property_name);
  else if (!(pspec->flags & G_PARAM_READABLE))
    g_warning ("%s: child property '%s' of container class '%s' is not readable",
               G_STRLOC,
               pspec->name,
               G_OBJECT_TYPE_NAME (container));
  else
    {
      GValue *prop_value, tmp_value = G_VALUE_INIT;

      /* auto-conversion of the callers value type
       */
      if (G_VALUE_TYPE (value) == G_PARAM_SPEC_VALUE_TYPE (pspec))
        {
          g_value_reset (value);
          prop_value = value;
        }
      else if (!g_value_type_transformable (G_PARAM_SPEC_VALUE_TYPE (pspec), G_VALUE_TYPE (value)))
        {
          g_warning ("can't retrieve child property '%s' of type '%s' as value of type '%s'",
                     pspec->name,
                     g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
                     G_VALUE_TYPE_NAME (value));
          g_object_unref (child);
          g_object_unref (container);
          return;
        }
      else
        {
          g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
          prop_value = &tmp_value;
        }
      container_get_child_property (container, child, pspec, prop_value);
      if (prop_value != value)
        {
          g_value_transform (prop_value, value);
          g_value_unset (&tmp_value);
        }
    }
  g_object_unref (child);
  g_object_unref (container);
}

/**
 * gtk_container_child_set_valist:
 * @container: a #GtkContainer
 * @child: a widget which is a child of @container
 * @first_property_name: the name of the first property to set
 * @var_args: a %NULL-terminated list of property names and values, starting
 *           with @first_prop_name
 *
 * Sets one or more child properties for @child and @container.
 **/
void
gtk_container_child_set_valist (GtkContainer *container,
                                GtkWidget    *child,
                                const gchar  *first_property_name,
                                va_list       var_args)
{
  GObjectNotifyQueue *nqueue;
  const gchar *name;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));

  g_object_ref (container);
  g_object_ref (child);

  nqueue = g_object_notify_queue_freeze (G_OBJECT (child), _gtk_widget_child_property_notify_context);
  name = first_property_name;
  while (name)
    {
      GValue value = G_VALUE_INIT;
      gchar *error = NULL;
      GParamSpec *pspec = g_param_spec_pool_lookup (_gtk_widget_child_property_pool,
                                                    name,
                                                    G_OBJECT_TYPE (container),
                                                    TRUE);
      if (!pspec)
        {
          g_warning ("%s: container class '%s' has no child property named '%s'",
                     G_STRLOC,
                     G_OBJECT_TYPE_NAME (container),
                     name);
          break;
        }
      if (!(pspec->flags & G_PARAM_WRITABLE))
        {
          g_warning ("%s: child property '%s' of container class '%s' is not writable",
                     G_STRLOC,
                     pspec->name,
                     G_OBJECT_TYPE_NAME (container));
          break;
        }

      G_VALUE_COLLECT_INIT (&value, G_PARAM_SPEC_VALUE_TYPE (pspec),
                            var_args, 0, &error);
      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);

          /* we purposely leak the value here, it might not be
           * in a sane state if an error condition occoured
           */
          break;
        }
      container_set_child_property (container, child, pspec, &value, nqueue);
      g_value_unset (&value);
      name = va_arg (var_args, gchar*);
    }
  g_object_notify_queue_thaw (G_OBJECT (child), nqueue);

  g_object_unref (container);
  g_object_unref (child);
}

/**
 * gtk_container_child_set_property:
 * @container: a #GtkContainer
 * @child: a widget which is a child of @container
 * @property_name: the name of the property to set
 * @value: the value to set the property to
 *
 * Sets a child property for @child and @container.
 **/
void
gtk_container_child_set_property (GtkContainer *container,
                                  GtkWidget    *child,
                                  const gchar  *property_name,
                                  const GValue *value)
{
  GObjectNotifyQueue *nqueue;
  GParamSpec *pspec;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (G_IS_VALUE (value));

  g_object_ref (container);
  g_object_ref (child);

  nqueue = g_object_notify_queue_freeze (G_OBJECT (child), _gtk_widget_child_property_notify_context);
  pspec = g_param_spec_pool_lookup (_gtk_widget_child_property_pool, property_name,
                                    G_OBJECT_TYPE (container), TRUE);
  if (!pspec)
    g_warning ("%s: container class '%s' has no child property named '%s'",
               G_STRLOC,
               G_OBJECT_TYPE_NAME (container),
               property_name);
  else if (!(pspec->flags & G_PARAM_WRITABLE))
    g_warning ("%s: child property '%s' of container class '%s' is not writable",
               G_STRLOC,
               pspec->name,
               G_OBJECT_TYPE_NAME (container));
  else
    {
      container_set_child_property (container, child, pspec, value, nqueue);
    }
  g_object_notify_queue_thaw (G_OBJECT (child), nqueue);
  g_object_unref (container);
  g_object_unref (child);
}

/**
 * gtk_container_add_with_properties:
 * @container: a #GtkContainer
 * @widget: a widget to be placed inside @container
 * @first_prop_name: the name of the first child property to set
 * @...: a %NULL-terminated list of property names and values, starting
 *     with @first_prop_name
 *
 * Adds @widget to @container, setting child properties at the same time.
 * See gtk_container_add() and gtk_container_child_set() for more details.
 */
void
gtk_container_add_with_properties (GtkContainer *container,
                                   GtkWidget    *widget,
                                   const gchar  *first_prop_name,
                                   ...)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (_gtk_widget_get_parent (widget) == NULL);

  g_object_ref (container);
  g_object_ref (widget);
  gtk_widget_freeze_child_notify (widget);

  g_signal_emit (container, container_signals[ADD], 0, widget);
  if (_gtk_widget_get_parent (widget))
    {
      va_list var_args;

      va_start (var_args, first_prop_name);
      gtk_container_child_set_valist (container, widget, first_prop_name, var_args);
      va_end (var_args);
    }

  gtk_widget_thaw_child_notify (widget);
  g_object_unref (widget);
  g_object_unref (container);
}

/**
 * gtk_container_child_set:
 * @container: a #GtkContainer
 * @child: a widget which is a child of @container
 * @first_prop_name: the name of the first property to set
 * @...: a %NULL-terminated list of property names and values, starting
 *     with @first_prop_name
 *
 * Sets one or more child properties for @child and @container.
 */
void
gtk_container_child_set (GtkContainer      *container,
                         GtkWidget         *child,
                         const gchar       *first_prop_name,
                         ...)
{
  va_list var_args;

  va_start (var_args, first_prop_name);
  gtk_container_child_set_valist (container, child, first_prop_name, var_args);
  va_end (var_args);
}

/**
 * gtk_container_child_get:
 * @container: a #GtkContainer
 * @child: a widget which is a child of @container
 * @first_prop_name: the name of the first property to get
 * @...: return location for the first property, followed
 *     optionally by more name/return location pairs, followed by %NULL
 *
 * Gets the values of one or more child properties for @child and @container.
 */
void
gtk_container_child_get (GtkContainer      *container,
                         GtkWidget         *child,
                         const gchar       *first_prop_name,
                         ...)
{
  va_list var_args;

  va_start (var_args, first_prop_name);
  gtk_container_child_get_valist (container, child, first_prop_name, var_args);
  va_end (var_args);
}

static inline void
install_child_property_internal (GType       g_type,
                                 guint       property_id,
                                 GParamSpec *pspec)
{
  if (g_param_spec_pool_lookup (_gtk_widget_child_property_pool, pspec->name, g_type, FALSE))
    {
      g_warning ("Class '%s' already contains a child property named '%s'",
                 g_type_name (g_type),
                 pspec->name);
      return;
    }
  g_param_spec_ref (pspec);
  g_param_spec_sink (pspec);
  PARAM_SPEC_SET_PARAM_ID (pspec, property_id);
  g_param_spec_pool_insert (_gtk_widget_child_property_pool, pspec, g_type);
}

/**
 * gtk_container_class_install_child_property:
 * @cclass: a #GtkContainerClass
 * @property_id: the id for the property
 * @pspec: the #GParamSpec for the property
 *
 * Installs a child property on a container class.
 **/
void
gtk_container_class_install_child_property (GtkContainerClass *cclass,
                                            guint              property_id,
                                            GParamSpec        *pspec)
{
  g_return_if_fail (GTK_IS_CONTAINER_CLASS (cclass));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  if (pspec->flags & G_PARAM_WRITABLE)
    g_return_if_fail (cclass->set_child_property != NULL);
  if (pspec->flags & G_PARAM_READABLE)
    g_return_if_fail (cclass->get_child_property != NULL);
  g_return_if_fail (property_id > 0);
  g_return_if_fail (PARAM_SPEC_PARAM_ID (pspec) == 0);  /* paranoid */
  if (pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY))
    g_return_if_fail ((pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)) == 0);

  install_child_property_internal (G_OBJECT_CLASS_TYPE (cclass), property_id, pspec);
}

/**
 * gtk_container_class_install_child_properties:
 * @cclass: a #GtkContainerClass
 * @n_pspecs: the length of the #GParamSpec array
 * @pspecs: (array length=n_pspecs): the #GParamSpec array defining the new
 *     child properties
 *
 * Installs child properties on a container class.
 */
void
gtk_container_class_install_child_properties (GtkContainerClass  *cclass,
                                              guint               n_pspecs,
                                              GParamSpec        **pspecs)
{
  gint i;

  g_return_if_fail (GTK_IS_CONTAINER_CLASS (cclass));
  g_return_if_fail (n_pspecs > 1);
  g_return_if_fail (pspecs[0] == NULL);

  /* we skip the first element of the array as it would have a 0 prop_id */
  for (i = 1; i < n_pspecs; i++)
    {
      GParamSpec *pspec = pspecs[i];

      g_return_if_fail (G_IS_PARAM_SPEC (pspec));
      if (pspec->flags & G_PARAM_WRITABLE)
        g_return_if_fail (cclass->set_child_property != NULL);
      if (pspec->flags & G_PARAM_READABLE)
        g_return_if_fail (cclass->get_child_property != NULL);
      g_return_if_fail (PARAM_SPEC_PARAM_ID (pspec) == 0);  /* paranoid */
      if (pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY))
        g_return_if_fail ((pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)) == 0);

      install_child_property_internal (G_OBJECT_CLASS_TYPE (cclass), i, pspec);
    }
}

/**
 * gtk_container_class_find_child_property:
 * @cclass: (type GtkContainerClass): a #GtkContainerClass
 * @property_name: the name of the child property to find
 *
 * Finds a child property of a container class by name.
 *
 * Returns: (nullable) (transfer none): the #GParamSpec of the child
 *     property or %NULL if @class has no child property with that
 *     name.
 */
GParamSpec*
gtk_container_class_find_child_property (GObjectClass *cclass,
                                         const gchar  *property_name)
{
  g_return_val_if_fail (GTK_IS_CONTAINER_CLASS (cclass), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  return g_param_spec_pool_lookup (_gtk_widget_child_property_pool,
                                   property_name,
                                   G_OBJECT_CLASS_TYPE (cclass),
                                   TRUE);
}

/**
 * gtk_container_class_list_child_properties:
 * @cclass: (type GtkContainerClass): a #GtkContainerClass
 * @n_properties: location to return the number of child properties found
 *
 * Returns all child properties of a container class.
 *
 * Returns: (array length=n_properties) (transfer container):
 *     a newly allocated %NULL-terminated array of #GParamSpec*.
 *     The array must be freed with g_free().
 */
GParamSpec**
gtk_container_class_list_child_properties (GObjectClass *cclass,
                                           guint        *n_properties)
{
  GParamSpec **pspecs;
  guint n;

  g_return_val_if_fail (GTK_IS_CONTAINER_CLASS (cclass), NULL);

  pspecs = g_param_spec_pool_list (_gtk_widget_child_property_pool,
                                   G_OBJECT_CLASS_TYPE (cclass),
                                   &n);
  if (n_properties)
    *n_properties = n;

  return pspecs;
}

static void
gtk_container_add_unimplemented (GtkContainer     *container,
                                 GtkWidget        *widget)
{
  g_warning ("GtkContainerClass::add not implemented for '%s'", g_type_name (G_TYPE_FROM_INSTANCE (container)));
}

static void
gtk_container_remove_unimplemented (GtkContainer     *container,
                                    GtkWidget        *widget)
{
  g_warning ("GtkContainerClass::remove not implemented for '%s'", g_type_name (G_TYPE_FROM_INSTANCE (container)));
}

static void
gtk_container_init (GtkContainer *container)
{
}

static void
gtk_container_destroy (GtkWidget *widget)
{
  GtkContainer *container = GTK_CONTAINER (widget);
  GtkContainerPrivate *priv = gtk_container_get_instance_private (container);

  if (priv->restyle_pending)
    priv->restyle_pending = FALSE;

  /* do this before walking child widgets, to avoid
   * removing children from focus chain one by one.
   */
  if (priv->has_focus_chain)
    gtk_container_unset_focus_chain (container);

  gtk_container_foreach (container, (GtkCallback) gtk_widget_destroy, NULL);

  GTK_WIDGET_CLASS (gtk_container_parent_class)->destroy (widget);
}

/**
 * gtk_container_add:
 * @container: a #GtkContainer
 * @widget: a widget to be placed inside @container
 *
 * Adds @widget to @container. Typically used for simple containers
 * such as #GtkWindow, #GtkFrame, or #GtkButton; for more complicated
 * layout containers such as #GtkBox or #GtkGrid, this function will
 * pick default packing parameters that may not be correct.  So
 * consider functions such as gtk_box_pack_start() and
 * gtk_grid_attach() as an alternative to gtk_container_add() in
 * those cases. A widget may be added to only one container at a time;
 * you can’t place the same widget inside two different containers.
 *
 * Note that some containers, such as #GtkScrolledWindow or #GtkListBox,
 * may add intermediate children between the added widget and the
 * container.
 */
void
gtk_container_add (GtkContainer *container,
                   GtkWidget    *widget)
{
  GtkWidget *parent;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  parent = _gtk_widget_get_parent (widget);

  if (parent != NULL)
    {
      g_warning ("Attempting to add a widget with type %s to a container of "
                 "type %s, but the widget is already inside a container of type %s, "
                 "please remove the widget from its existing container first." ,
                 g_type_name (G_OBJECT_TYPE (widget)),
                 g_type_name (G_OBJECT_TYPE (container)),
                 g_type_name (G_OBJECT_TYPE (parent)));
      return;
    }

  g_signal_emit (container, container_signals[ADD], 0, widget);

  _gtk_container_accessible_add (GTK_WIDGET (container), widget);
}

/**
 * gtk_container_remove:
 * @container: a #GtkContainer
 * @widget: a current child of @container
 *
 * Removes @widget from @container. @widget must be inside @container.
 * Note that @container will own a reference to @widget, and that this
 * may be the last reference held; so removing a widget from its
 * container can destroy that widget. If you want to use @widget
 * again, you need to add a reference to it before removing it from
 * a container, using g_object_ref(). If you don’t want to use @widget
 * again it’s usually more efficient to simply destroy it directly
 * using gtk_widget_destroy() since this will remove it from the
 * container and help break any circular reference count cycles.
 **/
void
gtk_container_remove (GtkContainer *container,
                      GtkWidget    *widget)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_ref (container);
  g_object_ref (widget);

  g_signal_emit (container, container_signals[REMOVE], 0, widget);

  _gtk_container_accessible_remove (GTK_WIDGET (container), widget);

  g_object_unref (widget);
  g_object_unref (container);
}

static gboolean
gtk_container_needs_idle_sizer (GtkContainer *container)
{
  GtkContainerPrivate *priv = gtk_container_get_instance_private (container);

  if (priv->restyle_pending)
    return TRUE;

  return gtk_widget_needs_allocate (GTK_WIDGET (container));
}

static void
gtk_container_idle_sizer (GdkFrameClock *clock,
			  GtkContainer  *container)
{
  GtkContainerPrivate *priv = gtk_container_get_instance_private (container);

  /* We validate the style contexts in a single loop before even trying
   * to handle resizes instead of doing validations inline.
   * This is mostly necessary for compatibility reasons with old code,
   * because both style_updated and size_allocate functions often change
   * styles and so could cause infinite loops in this function.
   *
   * It's important to note that even an invalid style context returns
   * sane values. So the result of an invalid style context will never be
   * a program crash, but only a wrong layout or rendering.
   */
  if (priv->restyle_pending)
    {
      priv->restyle_pending = FALSE;
      gtk_css_node_validate (gtk_widget_get_css_node (GTK_WIDGET (container)));
    }

  /* we may be invoked with a container_resize_queue of NULL, because
   * queue_resize could have been adding an extra idle function while
   * the queue still got processed. we better just ignore such case
   * than trying to explicitly work around them with some extra flags,
   * since it doesn't cause any actual harm.
   */
  if (gtk_widget_needs_allocate (GTK_WIDGET (container)))
    {
      gtk_container_check_resize (container);
    }

  if (!gtk_container_needs_idle_sizer (container))
    {
      _gtk_container_stop_idle_sizer (container);
    }
  else
    {
      gdk_frame_clock_request_phase (clock,
                                     GDK_FRAME_CLOCK_PHASE_LAYOUT);
    }
}

static void
gtk_container_start_idle_sizer (GtkContainer *container)
{
  GtkContainerPrivate *priv = gtk_container_get_instance_private (container);
  GdkFrameClock *clock;

  if (priv->resize_handler != 0)
    return;

  clock = gtk_widget_get_frame_clock (GTK_WIDGET (container));
  if (clock == NULL)
    return;

  priv->resize_clock = clock;
  priv->resize_handler = g_signal_connect (clock, "layout",
                                           G_CALLBACK (gtk_container_idle_sizer), container);
  gdk_frame_clock_request_phase (clock,
                                 GDK_FRAME_CLOCK_PHASE_LAYOUT);
}

void
_gtk_container_stop_idle_sizer (GtkContainer *container)
{
  GtkContainerPrivate *priv = gtk_container_get_instance_private (container);

  if (priv->resize_handler == 0)
    return;

  g_signal_handler_disconnect (priv->resize_clock,
                               priv->resize_handler);
  priv->resize_handler = 0;
  priv->resize_clock = NULL;
}

void
gtk_container_queue_resize_handler (GtkContainer *container)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (container);

  if (_gtk_widget_get_visible (widget) &&
      gtk_widget_needs_allocate (widget) &&
      _gtk_widget_is_toplevel (widget))
    {
      gtk_container_start_idle_sizer (container);
    }
}

void
_gtk_container_queue_restyle (GtkContainer *container)
{
  GtkContainerPrivate *priv = gtk_container_get_instance_private (container);

  g_return_if_fail (GTK_CONTAINER (container));

  if (priv->restyle_pending)
    return;

  gtk_container_start_idle_sizer (container);
  priv->restyle_pending = TRUE;
}

void
_gtk_container_maybe_start_idle_sizer (GtkContainer *container)
{
  if (gtk_container_needs_idle_sizer (container))
    gtk_container_start_idle_sizer (container);
}

void
gtk_container_check_resize (GtkContainer *container)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));

  g_signal_emit (container, container_signals[CHECK_RESIZE], 0);
}

static void
gtk_container_real_check_resize (GtkContainer *container)
{
  GtkWidget *widget = GTK_WIDGET (container);
  GtkAllocation allocation;
  GtkRequisition requisition;
  int baseline;

  if (_gtk_widget_get_alloc_needed (widget))
    {
      if (!_gtk_widget_is_toplevel (widget))
        {
          gtk_widget_get_preferred_size (widget, &requisition, NULL);
          gtk_widget_get_allocated_size (widget, &allocation, &baseline);

          if (allocation.width < requisition.width)
            allocation.width = requisition.width;
          if (allocation.height < requisition.height)
            allocation.height = requisition.height;
          gtk_widget_size_allocate (widget, &allocation, baseline);
        }
      else
        gtk_widget_queue_resize (widget);
    }
  else
    {
      gtk_widget_ensure_allocate (widget);
    }
}

static GtkSizeRequestMode 
gtk_container_get_request_mode (GtkWidget *widget)
{
  GtkWidget *w;
  int wfh = 0, hfw = 0;

  for (w = gtk_widget_get_first_child (widget);
       w != NULL;
       w = gtk_widget_get_next_sibling (w))
    {
      GtkSizeRequestMode mode = gtk_widget_get_request_mode (w);

      switch (mode)
        {
        case GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH:
          hfw ++;
          break;
        case GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT:
          wfh ++;
          break;
        case GTK_SIZE_REQUEST_CONSTANT_SIZE:
        default:
          break;
        }
    }

  if (hfw == 0 && wfh == 0)
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
  else
    return wfh > hfw ?
        GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT :
        GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

/**
 * gtk_container_forall: (virtual forall)
 * @container: a #GtkContainer
 * @callback: (scope call) (closure callback_data): a callback
 * @callback_data: callback user data
 *
 * Invokes @callback on each direct child of @container, including
 * children that are considered “internal” (implementation details
 * of the container). “Internal” children generally weren’t added
 * by the user of the container, but were added by the container
 * implementation itself.
 *
 * Most applications should use gtk_container_foreach(), rather
 * than gtk_container_forall().
 **/
void
gtk_container_forall (GtkContainer *container,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (callback != NULL);

  gtk_widget_forall (GTK_WIDGET (container),
                     callback,
                     callback_data);
}

/**
 * gtk_container_foreach:
 * @container: a #GtkContainer
 * @callback: (scope call):  a callback
 * @callback_data: callback user data
 *
 * Invokes @callback on each non-internal child of @container.
 * See gtk_container_forall() for details on what constitutes
 * an “internal” child. For all practical purposes, this function
 * should iterate over precisely those child widgets that were
 * added to the container by the application with explicit add()
 * calls.
 *
 * Most applications should use gtk_container_foreach(),
 * rather than gtk_container_forall().
 **/
void
gtk_container_foreach (GtkContainer *container,
                       GtkCallback   callback,
                       gpointer      callback_data)
{
  GtkContainerClass *class;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (callback != NULL);

  class = GTK_CONTAINER_GET_CLASS (container);

  if (class->forall)
    class->forall (container, callback, callback_data);
}

/**
 * gtk_container_set_focus_child:
 * @container: a #GtkContainer
 * @child: (allow-none): a #GtkWidget, or %NULL
 *
 * Sets, or unsets if @child is %NULL, the focused child of @container.
 *
 * This function emits the GtkContainer::set_focus_child signal of
 * @container. Implementations of #GtkContainer can override the
 * default behaviour by overriding the class closure of this signal.
 *
 * This is function is mostly meant to be used by widgets. Applications can use
 * gtk_widget_grab_focus() to manually set the focus to a specific widget.
 */
void
gtk_container_set_focus_child (GtkContainer *container,
                               GtkWidget    *child)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  if (child)
    g_return_if_fail (GTK_IS_WIDGET (child));

  g_signal_emit (container, container_signals[SET_FOCUS_CHILD], 0, child);
}

/**
 * gtk_container_get_children:
 * @container: a #GtkContainer
 *
 * Returns the container’s non-internal children. See
 * gtk_container_forall() for details on what constitutes an "internal" child.
 *
 * Returns: (element-type GtkWidget) (transfer container): a newly-allocated list of the container’s non-internal children.
 **/
GList*
gtk_container_get_children (GtkContainer *container)
{
  GList *children = NULL;

  gtk_container_foreach (container,
                         gtk_container_children_callback,
                         &children);

  return g_list_reverse (children);
}

static void
gtk_container_compute_expand (GtkWidget *widget,
                              gboolean  *hexpand_p,
                              gboolean  *vexpand_p)
{
  GtkWidget *w;
  gboolean hexpand = FALSE;
  gboolean vexpand = FALSE;

  for (w = gtk_widget_get_first_child (widget);
       w != NULL;
       w = gtk_widget_get_next_sibling (w))
    {
      /* note that we don't get_expand on the child if we already know we
       * have to expand, so we only recurse into children until we find
       * one that expands and then we basically don't do any more
       * work. This means that we can leave some children in a
       * need_compute_expand state, which is fine, as long as GtkWidget
       * doesn't rely on an invariant that "if a child has
       * need_compute_expand, its parents also do"
       *
       * gtk_widget_compute_expand() always returns FALSE if the
       * child is !visible so that's taken care of.
       */
      hexpand = hexpand || gtk_widget_compute_expand (w, GTK_ORIENTATION_HORIZONTAL);
      vexpand = vexpand || gtk_widget_compute_expand (w, GTK_ORIENTATION_VERTICAL);
    }

  *hexpand_p = hexpand;
  *vexpand_p = vexpand;
}

static void
gtk_container_real_set_focus_child (GtkContainer *container,
                                    GtkWidget    *focus_child)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (focus_child == NULL || GTK_IS_WIDGET (focus_child));

  /* Check for h/v adjustments and scroll to show the focus child if possible */
  if (focus_child)
    {
      GtkAdjustment *hadj;
      GtkAdjustment *vadj;
      GtkAllocation allocation;
      gint x, y;

      hadj = g_object_get_qdata (G_OBJECT (container), hadjustment_key_id);
      vadj = g_object_get_qdata (G_OBJECT (container), vadjustment_key_id);
      if (hadj || vadj)
        {
          GtkWidget *child = focus_child;

          while (gtk_widget_get_focus_child (child))
            child = gtk_widget_get_focus_child (child);

          if (!gtk_widget_translate_coordinates (child, focus_child,
                                                 0, 0, &x, &y))
            return;

          gtk_widget_get_outer_allocation (child, &allocation);

          if (vadj)
            gtk_adjustment_clamp_page (vadj, y, y + allocation.height);

          if (hadj)
            gtk_adjustment_clamp_page (hadj, x, x + allocation.width);
        }
    }
}

static GList*
get_focus_chain (GtkContainer *container)
{
  return g_object_get_qdata (G_OBJECT (container), quark_focus_chain);
}

static GtkWidgetPath *
gtk_container_real_get_path_for_child (GtkContainer *container,
                                       GtkWidget    *child)
{
  GtkWidgetPath *path;
  GtkWidget *widget = GTK_WIDGET (container);

  path = _gtk_widget_create_path (widget);

  gtk_widget_path_append_for_widget (path, child);

  return path;
}

/* Utility function, equivalent to g_list_reverse */
static void
reverse_ptr_array (GPtrArray *arr)
{
  int i;

  for (i = 0; i < arr->len / 2; i ++)
    {
      void *a = g_ptr_array_index (arr, i);
      void *b = g_ptr_array_index (arr, arr->len - 1 - i);

      arr->pdata[i] = b;
      arr->pdata[arr->len - 1 - i] = a;
    }
}

static gboolean
gtk_container_focus (GtkWidget        *widget,
                     GtkDirectionType  direction)
{
  GtkContainerPrivate *priv;
  gint return_val = FALSE;
  GtkContainer *container;

  g_return_val_if_fail (GTK_IS_CONTAINER (widget), FALSE);

  container = GTK_CONTAINER (widget);
  priv = gtk_container_get_instance_private (container);

  if (!priv->has_focus_chain)
    {
      return_val = GTK_WIDGET_CLASS (gtk_container_parent_class)->focus (widget, direction);
    }
  else
    {
      GList *focus_chain;
      GList *l;
      GPtrArray *child_array;

      /* Here we compute the next focus child based on the focus chain set on @container.
       * If we move via TAB, we consider the focus chain widgets in the order given to us (or in reverse).
       * If UP/DOWN/LEFT/RIGHT are used, we sort the focus chain like normal, using their allocation. */
      focus_chain = get_focus_chain (container);
      child_array = g_ptr_array_sized_new (g_list_length (focus_chain));

      for (l = focus_chain; l; l = l->next)
        g_ptr_array_add (child_array, l->data);

      if (direction == GTK_DIR_TAB_BACKWARD)
        {
          /* Reverse order for gtk_widget_focus_move to consider */
          reverse_ptr_array (child_array);
        }
      else if (direction != GTK_DIR_TAB_FORWARD)
        {
          /* Nothing to be done. We don't sort the children, and we don't reverse them either. */
          gtk_widget_focus_sort (widget, direction, child_array);
        }

      return_val = gtk_widget_focus_move (widget, direction, child_array);

      g_ptr_array_free (child_array, TRUE);
    }

  return return_val;
}

static void
gtk_container_children_callback (GtkWidget *widget,
                                 gpointer   client_data)
{
  GList **children;

  children = (GList**) client_data;
  *children = g_list_prepend (*children, widget);
}

static void
chain_widget_destroyed (GtkWidget *widget,
                        gpointer   user_data)
{
  GtkContainer *container;
  GList *chain;

  container = GTK_CONTAINER (user_data);

  chain = g_object_get_qdata (G_OBJECT (container), quark_focus_chain);

  chain = g_list_remove (chain, widget);

  g_signal_handlers_disconnect_by_func (widget,
                                        chain_widget_destroyed,
                                        user_data);

  g_object_set_qdata (G_OBJECT (container), quark_focus_chain, chain);
}

/**
 * gtk_container_set_focus_chain:
 * @container: a #GtkContainer
 * @focusable_widgets: (transfer none) (element-type GtkWidget):
 *     the new focus chain
 *
 * Sets a focus chain, overriding the one computed automatically by GTK+.
 *
 * In principle each widget in the chain should be a descendant of the
 * container, but this is not enforced by this method, since it’s allowed
 * to set the focus chain before you pack the widgets, or have a widget
 * in the chain that isn’t always packed. The necessary checks are done
 * when the focus chain is actually traversed.
 **/
void
gtk_container_set_focus_chain (GtkContainer *container,
                               GList        *focusable_widgets)
{
  GtkContainerPrivate *priv = gtk_container_get_instance_private (container);
  GList *chain;
  GList *tmp_list;

  g_return_if_fail (GTK_IS_CONTAINER (container));

  if (priv->has_focus_chain)
    gtk_container_unset_focus_chain (container);

  priv->has_focus_chain = TRUE;

  chain = NULL;
  tmp_list = focusable_widgets;
  while (tmp_list != NULL)
    {
      g_return_if_fail (GTK_IS_WIDGET (tmp_list->data));

      /* In principle each widget in the chain should be a descendant
       * of the container, but we don't want to check that here. It's
       * expensive and also it's allowed to set the focus chain before
       * you pack the widgets, or have a widget in the chain that isn't
       * always packed. So we check for ancestor during actual traversal.
       */

      chain = g_list_prepend (chain, tmp_list->data);

      g_signal_connect (tmp_list->data,
                        "destroy",
                        G_CALLBACK (chain_widget_destroyed),
                        container);

      tmp_list = tmp_list->next;
    }

  chain = g_list_reverse (chain);

  g_object_set_qdata (G_OBJECT (container), quark_focus_chain, chain);
}

/**
 * gtk_container_get_focus_chain:
 * @container:         a #GtkContainer
 * @focusable_widgets: (element-type GtkWidget) (out) (transfer container): location
 *                     to store the focus chain of the
 *                     container, or %NULL. You should free this list
 *                     using g_list_free() when you are done with it, however
 *                     no additional reference count is added to the
 *                     individual widgets in the focus chain.
 *
 * Retrieves the focus chain of the container, if one has been
 * set explicitly. If no focus chain has been explicitly
 * set, GTK+ computes the focus chain based on the positions
 * of the children. In that case, GTK+ stores %NULL in
 * @focusable_widgets and returns %FALSE.
 *
 * Returns: %TRUE if the focus chain of the container
 * has been set explicitly.
 **/
gboolean
gtk_container_get_focus_chain (GtkContainer *container,
                               GList       **focus_chain)
{
  GtkContainerPrivate *priv = gtk_container_get_instance_private (container);

  g_return_val_if_fail (GTK_IS_CONTAINER (container), FALSE);

  if (focus_chain)
    {
      if (priv->has_focus_chain)
        *focus_chain = g_list_copy (get_focus_chain (container));
      else
        *focus_chain = NULL;
    }

  return priv->has_focus_chain;
}

/**
 * gtk_container_unset_focus_chain:
 * @container: a #GtkContainer
 *
 * Removes a focus chain explicitly set with gtk_container_set_focus_chain().
 **/
void
gtk_container_unset_focus_chain (GtkContainer  *container)
{
  GtkContainerPrivate *priv = gtk_container_get_instance_private (container);

  g_return_if_fail (GTK_IS_CONTAINER (container));

  if (priv->has_focus_chain)
    {
      GList *chain;
      GList *tmp_list;

      chain = get_focus_chain (container);

      priv->has_focus_chain = FALSE;

      g_object_set_qdata (G_OBJECT (container), quark_focus_chain, NULL);

      tmp_list = chain;
      while (tmp_list != NULL)
        {
          g_signal_handlers_disconnect_by_func (tmp_list->data,
                                                chain_widget_destroyed,
                                                container);

          tmp_list = tmp_list->next;
        }

      g_list_free (chain);
    }
}

/**
 * gtk_container_set_focus_vadjustment:
 * @container: a #GtkContainer
 * @adjustment: an adjustment which should be adjusted when the focus
 *   is moved among the descendents of @container
 *
 * Hooks up an adjustment to focus handling in a container, so when a
 * child of the container is focused, the adjustment is scrolled to
 * show that widget. This function sets the vertical alignment. See
 * gtk_scrolled_window_get_vadjustment() for a typical way of obtaining
 * the adjustment and gtk_container_set_focus_hadjustment() for setting
 * the horizontal adjustment.
 *
 * The adjustments have to be in pixel units and in the same coordinate
 * system as the allocation for immediate children of the container.
 */
void
gtk_container_set_focus_vadjustment (GtkContainer  *container,
                                     GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  if (adjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (adjustment)
    g_object_ref (adjustment);

  g_object_set_qdata_full (G_OBJECT (container),
                           vadjustment_key_id,
                           adjustment,
                           g_object_unref);
}

/**
 * gtk_container_get_focus_vadjustment:
 * @container: a #GtkContainer
 *
 * Retrieves the vertical focus adjustment for the container. See
 * gtk_container_set_focus_vadjustment().
 *
 * Returns: (nullable) (transfer none): the vertical focus adjustment, or
 *   %NULL if none has been set.
 **/
GtkAdjustment *
gtk_container_get_focus_vadjustment (GtkContainer *container)
{
  GtkAdjustment *vadjustment;

  g_return_val_if_fail (GTK_IS_CONTAINER (container), NULL);

  vadjustment = g_object_get_qdata (G_OBJECT (container), vadjustment_key_id);

  return vadjustment;
}

/**
 * gtk_container_set_focus_hadjustment:
 * @container: a #GtkContainer
 * @adjustment: an adjustment which should be adjusted when the focus is
 *   moved among the descendents of @container
 *
 * Hooks up an adjustment to focus handling in a container, so when a child
 * of the container is focused, the adjustment is scrolled to show that
 * widget. This function sets the horizontal alignment.
 * See gtk_scrolled_window_get_hadjustment() for a typical way of obtaining
 * the adjustment and gtk_container_set_focus_vadjustment() for setting
 * the vertical adjustment.
 *
 * The adjustments have to be in pixel units and in the same coordinate
 * system as the allocation for immediate children of the container.
 */
void
gtk_container_set_focus_hadjustment (GtkContainer  *container,
                                     GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  if (adjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (adjustment)
    g_object_ref (adjustment);

  g_object_set_qdata_full (G_OBJECT (container),
                           hadjustment_key_id,
                           adjustment,
                           g_object_unref);
}

/**
 * gtk_container_get_focus_hadjustment:
 * @container: a #GtkContainer
 *
 * Retrieves the horizontal focus adjustment for the container. See
 * gtk_container_set_focus_hadjustment ().
 *
 * Returns: (nullable) (transfer none): the horizontal focus adjustment, or %NULL if
 *   none has been set.
 **/
GtkAdjustment *
gtk_container_get_focus_hadjustment (GtkContainer *container)
{
  GtkAdjustment *hadjustment;

  g_return_val_if_fail (GTK_IS_CONTAINER (container), NULL);

  hadjustment = g_object_get_qdata (G_OBJECT (container), hadjustment_key_id);

  return hadjustment;
}

/**
 * gtk_container_get_path_for_child:
 * @container: a #GtkContainer
 * @child: a child of @container
 *
 * Returns a newly created widget path representing all the widget hierarchy
 * from the toplevel down to and including @child.
 *
 * Returns: A newly created #GtkWidgetPath
 **/
GtkWidgetPath *
gtk_container_get_path_for_child (GtkContainer *container,
                                  GtkWidget    *child)
{
  GtkWidgetPath *path;

  g_return_val_if_fail (GTK_IS_CONTAINER (container), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);
  g_return_val_if_fail (container == (GtkContainer *) _gtk_widget_get_parent (child), NULL);

  path = GTK_CONTAINER_GET_CLASS (container)->get_path_for_child (container, child);
  if (gtk_widget_path_get_object_type (path) != G_OBJECT_TYPE (child))
    {
      g_critical ("%s %p returned a widget path for type %s, but child is %s",
                  G_OBJECT_TYPE_NAME (container),
                  container,
                  g_type_name (gtk_widget_path_get_object_type (path)),
                  G_OBJECT_TYPE_NAME (child));
    }

  return path;
}

