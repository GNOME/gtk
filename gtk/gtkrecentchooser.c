/* GTK - The GIMP Toolkit
 * gtkrecentchooser.c - Abstract interface for recent file selectors GUIs
 *
 * Copyright (C) 2006, Emmanuele Bassi
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

#include "gtkrecentchooser.h"
#include "gtkrecentchooserprivate.h"
#include "gtkrecentmanager.h"
#include "deprecated/gtkrecentaction.h"
#include "deprecated/gtkactivatable.h"
#include "gtkintl.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"

/**
 * SECTION:gtkrecentchooser
 * @Short_description: Interface implemented by widgets displaying recently
 *   used files
 * @Title: GtkRecentChooser
 * @See_also: #GtkRecentManager, #GtkRecentChooserDialog,
 *   #GtkRecentChooserWidget, #GtkRecentChooserMenu
 *
 * #GtkRecentChooser is an interface that can be implemented by widgets
 * displaying the list of recently used files.  In GTK+, the main objects
 * that implement this interface are #GtkRecentChooserWidget,
 * #GtkRecentChooserDialog and #GtkRecentChooserMenu.
 *
 * Recently used files are supported since GTK+ 2.10.
 */


enum
{
  ITEM_ACTIVATED,
  SELECTION_CHANGED,

  LAST_SIGNAL
};

static gboolean recent_chooser_has_show_numbers       (GtkRecentChooser *chooser);

static GQuark      quark_gtk_related_action               = 0;
static GQuark      quark_gtk_use_action_appearance        = 0;
static const gchar gtk_related_action_key[]               = "gtk-related-action";
static const gchar gtk_use_action_appearance_key[]        = "gtk-use-action-appearance";


static guint chooser_signals[LAST_SIGNAL] = { 0, };


typedef GtkRecentChooserIface GtkRecentChooserInterface;
G_DEFINE_INTERFACE (GtkRecentChooser, gtk_recent_chooser, G_TYPE_OBJECT);


static void
gtk_recent_chooser_default_init (GtkRecentChooserInterface *iface)
{
  GType iface_type = G_TYPE_FROM_INTERFACE (iface);

  quark_gtk_related_action        = g_quark_from_static_string (gtk_related_action_key);
  quark_gtk_use_action_appearance = g_quark_from_static_string (gtk_use_action_appearance_key);
  
  /**
   * GtkRecentChooser::selection-changed:
   * @chooser: the object which received the signal
   *
   * This signal is emitted when there is a change in the set of
   * selected recently used resources.  This can happen when a user
   * modifies the selection with the mouse or the keyboard, or when
   * explicitly calling functions to change the selection.
   *
   * Since: 2.10
   */
  chooser_signals[SELECTION_CHANGED] =
    g_signal_new (I_("selection-changed"),
                  iface_type,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkRecentChooserIface, selection_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GtkRecentChooser::item-activated:
   * @chooser: the object which received the signal
   *
   * This signal is emitted when the user "activates" a recent item
   * in the recent chooser.  This can happen by double-clicking on an item
   * in the recently used resources list, or by pressing
   * `Enter`.
   *
   * Since: 2.10
   */
  chooser_signals[ITEM_ACTIVATED] =
    g_signal_new (I_("item-activated"),
                  iface_type,
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkRecentChooserIface, item_activated),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkRecentChooser:recent-manager:
   *
   * The #GtkRecentManager instance used by the #GtkRecentChooser to
   * display the list of recently used resources.
   *
   * Since: 2.10
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("recent-manager",
                                                            P_("Recent Manager"),
                                                            P_("The RecentManager object to use"),
                                                            GTK_TYPE_RECENT_MANAGER,
                                                            GTK_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));

  /**
   * GtkRecentManager:show-private:
   *
   * Whether this #GtkRecentChooser should display recently used resources
   * marked with the "private" flag. Such resources should be considered
   * private to the applications and groups that have added them.
   *
   * Since: 2.10
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("show-private",
                                                             P_("Show Private"),
                                                             P_("Whether the private items should be displayed"),
                                                             FALSE,
                                                             GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkRecentChooser:show-tips:
   *
   * Whether this #GtkRecentChooser should display a tooltip containing the
   * full path of the recently used resources.
   *
   * Since: 2.10
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("show-tips",
                                                             P_("Show Tooltips"),
                                                             P_("Whether there should be a tooltip on the item"),
                                                             FALSE,
                                                             GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkRecentChooser:show-icons:
   *
   * Whether this #GtkRecentChooser should display an icon near the item.
   *
   * Since: 2.10
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("show-icons",
                                                             P_("Show Icons"),
                                                             P_("Whether there should be an icon near the item"),
                                                             TRUE,
                                                             GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkRecentChooser:show-not-found:
   *
   * Whether this #GtkRecentChooser should display the recently used resources
   * even if not present anymore. Setting this to %FALSE will perform a
   * potentially expensive check on every local resource (every remote
   * resource will always be displayed).
   *
   * Since: 2.10
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("show-not-found",
                                                             P_("Show Not Found"),
                                                             P_("Whether the items pointing to unavailable resources should be displayed"),
                                                             TRUE,
                                                             GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkRecentChooser:select-multiple:
   *
   * Allow the user to select multiple resources.
   *
   * Since: 2.10
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("select-multiple",
                                                             P_("Select Multiple"),
                                                             P_("Whether to allow multiple items to be selected"),
                                                             FALSE,
                                                             GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkRecentChooser:local-only:
   *
   * Whether this #GtkRecentChooser should display only local (file:)
   * resources.
   *
   * Since: 2.10
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("local-only",
                                                             P_("Local only"),
                                                             P_("Whether the selected resource(s) should be limited to local file: URIs"),
                                                             TRUE,
                                                             GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkRecentChooser:limit:
   *
   * The maximum number of recently used resources to be displayed,
   * or -1 to display all items.
   *
   * Since: 2.10
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_int ("limit",
                                                         P_("Limit"),
                                                         P_("The maximum number of items to be displayed"),
                                                         -1, G_MAXINT, 50,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkRecentChooser:sort-type:
   *
   * Sorting order to be used when displaying the recently used resources.
   *
   * Since: 2.10
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_enum ("sort-type",
                                                          P_("Sort Type"),
                                                          P_("The sorting order of the items displayed"),
                                                          GTK_TYPE_RECENT_SORT_TYPE,
                                                          GTK_RECENT_SORT_NONE,
                                                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkRecentChooser:filter:
   *
   * The #GtkRecentFilter object to be used when displaying
   * the recently used resources.
   *
   * Since: 2.10
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("filter",
                                                            P_("Filter"),
                                                            P_("The current filter for selecting which resources are displayed"),
                                                            GTK_TYPE_RECENT_FILTER,
                                                            GTK_PARAM_READWRITE));
}

GQuark
gtk_recent_chooser_error_quark (void)
{
  return g_quark_from_static_string ("gtk-recent-chooser-error-quark");
}

/**
 * _gtk_recent_chooser_get_recent_manager:
 * @chooser: a #GtkRecentChooser
 *
 * Gets the #GtkRecentManager used by @chooser.
 *
 * Returns: the recent manager for @chooser.
 *
 * Since: 2.10
 */
GtkRecentManager *
_gtk_recent_chooser_get_recent_manager (GtkRecentChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), NULL);
  
  return GTK_RECENT_CHOOSER_GET_IFACE (chooser)->get_recent_manager (chooser);
}

/**
 * gtk_recent_chooser_set_show_private:
 * @chooser: a #GtkRecentChooser
 * @show_private: %TRUE to show private items, %FALSE otherwise
 *
 * Whether to show recently used resources marked registered as private.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_show_private (GtkRecentChooser *chooser,
				     gboolean          show_private)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_object_set (chooser, "show-private", show_private, NULL);
}

/**
 * gtk_recent_chooser_get_show_private:
 * @chooser: a #GtkRecentChooser
 *
 * Returns whether @chooser should display recently used resources
 * registered as private.
 *
 * Returns: %TRUE if the recent chooser should show private items,
 *   %FALSE otherwise.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_get_show_private (GtkRecentChooser *chooser)
{
  gboolean show_private;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);
  
  g_object_get (chooser, "show-private", &show_private, NULL);
  
  return show_private;
}

/**
 * gtk_recent_chooser_set_show_not_found:
 * @chooser: a #GtkRecentChooser
 * @show_not_found: whether to show the local items we didn’t find
 *
 * Sets whether @chooser should display the recently used resources that
 * it didn’t find.  This only applies to local resources.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_show_not_found (GtkRecentChooser *chooser,
				       gboolean          show_not_found)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_object_set (chooser, "show-not-found", show_not_found, NULL);
}

/**
 * gtk_recent_chooser_get_show_not_found:
 * @chooser: a #GtkRecentChooser
 *
 * Retrieves whether @chooser should show the recently used resources that
 * were not found.
 *
 * Returns: %TRUE if the resources not found should be displayed, and
 *   %FALSE otheriwse.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_get_show_not_found (GtkRecentChooser *chooser)
{
  gboolean show_not_found;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);
  
  g_object_get (chooser, "show-not-found", &show_not_found, NULL);
  
  return show_not_found;
}

/**
 * gtk_recent_chooser_set_show_icons:
 * @chooser: a #GtkRecentChooser
 * @show_icons: whether to show an icon near the resource
 *
 * Sets whether @chooser should show an icon near the resource when
 * displaying it.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_show_icons (GtkRecentChooser *chooser,
				   gboolean          show_icons)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_object_set (chooser, "show-icons", show_icons, NULL);
}

/**
 * gtk_recent_chooser_get_show_icons:
 * @chooser: a #GtkRecentChooser
 *
 * Retrieves whether @chooser should show an icon near the resource.
 *
 * Returns: %TRUE if the icons should be displayed, %FALSE otherwise.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_get_show_icons (GtkRecentChooser *chooser)
{
  gboolean show_icons;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);
  
  g_object_get (chooser, "show-icons", &show_icons, NULL);
  
  return show_icons;
}

/**
 * gtk_recent_chooser_set_select_multiple:
 * @chooser: a #GtkRecentChooser
 * @select_multiple: %TRUE if @chooser can select more than one item
 *
 * Sets whether @chooser can select multiple items.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_select_multiple (GtkRecentChooser *chooser,
					gboolean          select_multiple)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_object_set (chooser, "select-multiple", select_multiple, NULL);
}

/**
 * gtk_recent_chooser_get_select_multiple:
 * @chooser: a #GtkRecentChooser
 *
 * Gets whether @chooser can select multiple items.
 *
 * Returns: %TRUE if @chooser can select more than one item.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_get_select_multiple (GtkRecentChooser *chooser)
{
  gboolean select_multiple;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);
  
  g_object_get (chooser, "select-multiple", &select_multiple, NULL);
  
  return select_multiple;
}

/**
 * gtk_recent_chooser_set_local_only:
 * @chooser: a #GtkRecentChooser
 * @local_only: %TRUE if only local files can be shown
 * 
 * Sets whether only local resources, that is resources using the file:// URI
 * scheme, should be shown in the recently used resources selector.  If
 * @local_only is %TRUE (the default) then the shown resources are guaranteed
 * to be accessible through the operating system native file system.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_local_only (GtkRecentChooser *chooser,
				   gboolean          local_only)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));

  g_object_set (chooser, "local-only", local_only, NULL);
}

/**
 * gtk_recent_chooser_get_local_only:
 * @chooser: a #GtkRecentChooser
 *
 * Gets whether only local resources should be shown in the recently used
 * resources selector.  See gtk_recent_chooser_set_local_only()
 *
 * Returns: %TRUE if only local resources should be shown.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_get_local_only (GtkRecentChooser *chooser)
{
  gboolean local_only;

  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "local-only", &local_only, NULL);

  return local_only;
}

/**
 * gtk_recent_chooser_set_limit:
 * @chooser: a #GtkRecentChooser
 * @limit: a positive integer, or -1 for all items
 *
 * Sets the number of items that should be returned by
 * gtk_recent_chooser_get_items() and gtk_recent_chooser_get_uris().
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_limit (GtkRecentChooser *chooser,
			      gint              limit)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_object_set (chooser, "limit", limit, NULL);
}

/**
 * gtk_recent_chooser_get_limit:
 * @chooser: a #GtkRecentChooser
 *
 * Gets the number of items returned by gtk_recent_chooser_get_items()
 * and gtk_recent_chooser_get_uris().
 *
 * Returns: A positive integer, or -1 meaning that all items are
 *   returned.
 *
 * Since: 2.10
 */
gint
gtk_recent_chooser_get_limit (GtkRecentChooser *chooser)
{
  gint limit;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), 10);
  
  g_object_get (chooser, "limit", &limit, NULL);
  
  return limit;
}

/**
 * gtk_recent_chooser_set_show_tips:
 * @chooser: a #GtkRecentChooser
 * @show_tips: %TRUE if tooltips should be shown
 *
 * Sets whether to show a tooltips containing the full path of each
 * recently used resource in a #GtkRecentChooser widget.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_show_tips (GtkRecentChooser *chooser,
				  gboolean          show_tips)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_object_set (chooser, "show-tips", show_tips, NULL);
}

/**
 * gtk_recent_chooser_get_show_tips:
 * @chooser: a #GtkRecentChooser
 *
 * Gets whether @chooser should display tooltips containing the full path
 * of a recently user resource.
 *
 * Returns: %TRUE if the recent chooser should show tooltips,
 *   %FALSE otherwise.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_get_show_tips (GtkRecentChooser *chooser)
{
  gboolean show_tips;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);
  
  g_object_get (chooser, "show-tips", &show_tips, NULL);
  
  return show_tips;
}

static gboolean
recent_chooser_has_show_numbers (GtkRecentChooser *chooser)
{
  GParamSpec *pspec;
  
  /* This is the result of a minor screw up: the "show-numbers" property
   * was removed from the GtkRecentChooser interface, but the accessors
   * remained in the interface API; now we need to check whether the
   * implementation of the RecentChooser interface has a "show-numbers"
   * boolean property installed before accessing it, and avoid an
   * assertion failure using a more graceful warning.  This should really
   * go away as soon as we can break API and remove these accessors.
   */
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (chooser),
		  			"show-numbers");

  return (pspec && pspec->value_type == G_TYPE_BOOLEAN);
}

/**
 * gtk_recent_chooser_set_sort_type:
 * @chooser: a #GtkRecentChooser
 * @sort_type: sort order that the chooser should use
 *
 * Changes the sorting order of the recently used resources list displayed by
 * @chooser.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_sort_type (GtkRecentChooser  *chooser,
				  GtkRecentSortType  sort_type)
{  
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_object_set (chooser, "sort-type", sort_type, NULL);
}

/**
 * gtk_recent_chooser_get_sort_type:
 * @chooser: a #GtkRecentChooser
 *
 * Gets the value set by gtk_recent_chooser_set_sort_type().
 *
 * Returns: the sorting order of the @chooser.
 *
 * Since: 2.10
 */
GtkRecentSortType
gtk_recent_chooser_get_sort_type (GtkRecentChooser *chooser)
{
  GtkRecentSortType sort_type;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), GTK_RECENT_SORT_NONE);
  
  g_object_get (chooser, "sort-type", &sort_type, NULL);

  return sort_type;
}

/**
 * gtk_recent_chooser_set_sort_func:
 * @chooser: a #GtkRecentChooser
 * @sort_func: the comparison function
 * @sort_data: (allow-none): user data to pass to @sort_func, or %NULL
 * @data_destroy: (allow-none): destroy notifier for @sort_data, or %NULL
 *
 * Sets the comparison function used when sorting to be @sort_func.  If
 * the @chooser has the sort type set to #GTK_RECENT_SORT_CUSTOM then
 * the chooser will sort using this function.
 *
 * To the comparison function will be passed two #GtkRecentInfo structs and
 * @sort_data;  @sort_func should return a positive integer if the first
 * item comes before the second, zero if the two items are equal and
 * a negative integer if the first item comes after the second.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_sort_func  (GtkRecentChooser  *chooser,
				   GtkRecentSortFunc  sort_func,
				   gpointer           sort_data,
				   GDestroyNotify     data_destroy)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  GTK_RECENT_CHOOSER_GET_IFACE (chooser)->set_sort_func (chooser,
  							 sort_func,
  							 sort_data,
  							 data_destroy);
}

/**
 * gtk_recent_chooser_set_current_uri:
 * @chooser: a #GtkRecentChooser
 * @uri: a URI
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Sets @uri as the current URI for @chooser.
 *
 * Returns: %TRUE if the URI was found.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_set_current_uri (GtkRecentChooser  *chooser,
				    const gchar       *uri,
				    GError           **error)
{
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);
  
  return GTK_RECENT_CHOOSER_GET_IFACE (chooser)->set_current_uri (chooser, uri, error);
}

/**
 * gtk_recent_chooser_get_current_uri:
 * @chooser: a #GtkRecentChooser
 *
 * Gets the URI currently selected by @chooser.
 *
 * Returns: a newly allocated string holding a URI.
 *
 * Since: 2.10
 */
gchar *
gtk_recent_chooser_get_current_uri (GtkRecentChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), NULL);
  
  return GTK_RECENT_CHOOSER_GET_IFACE (chooser)->get_current_uri (chooser);
}

/**
 * gtk_recent_chooser_get_current_item:
 * @chooser: a #GtkRecentChooser
 * 
 * Gets the #GtkRecentInfo currently selected by @chooser.
 *
 * Returns: a #GtkRecentInfo.  Use gtk_recent_info_unref() when
 *   when you have finished using it.
 *
 * Since: 2.10
 */
GtkRecentInfo *
gtk_recent_chooser_get_current_item (GtkRecentChooser *chooser)
{
  GtkRecentManager *manager;
  GtkRecentInfo *retval;
  gchar *uri;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), NULL);
  
  uri = gtk_recent_chooser_get_current_uri (chooser);
  if (!uri)
    return NULL;
  
  manager = _gtk_recent_chooser_get_recent_manager (chooser);
  retval = gtk_recent_manager_lookup_item (manager, uri, NULL);
  g_free (uri);
  
  return retval;
}

/**
 * gtk_recent_chooser_select_uri:
 * @chooser: a #GtkRecentChooser
 * @uri: a URI
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Selects @uri inside @chooser.
 *
 * Returns: %TRUE if @uri was found.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_select_uri (GtkRecentChooser  *chooser,
			       const gchar       *uri,
			       GError           **error)
{
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);
  
  return GTK_RECENT_CHOOSER_GET_IFACE (chooser)->select_uri (chooser, uri, error);
}

/**
 * gtk_recent_chooser_unselect_uri:
 * @chooser: a #GtkRecentChooser
 * @uri: a URI
 *
 * Unselects @uri inside @chooser.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_unselect_uri (GtkRecentChooser *chooser,
				 const gchar      *uri)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  GTK_RECENT_CHOOSER_GET_IFACE (chooser)->unselect_uri (chooser, uri);
}

/**
 * gtk_recent_chooser_select_all:
 * @chooser: a #GtkRecentChooser
 *
 * Selects all the items inside @chooser, if the @chooser supports
 * multiple selection.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_select_all (GtkRecentChooser *chooser)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  GTK_RECENT_CHOOSER_GET_IFACE (chooser)->select_all (chooser);
}

/**
 * gtk_recent_chooser_unselect_all:
 * @chooser: a #GtkRecentChooser
 *
 * Unselects all the items inside @chooser.
 * 
 * Since: 2.10
 */
void
gtk_recent_chooser_unselect_all (GtkRecentChooser *chooser)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  GTK_RECENT_CHOOSER_GET_IFACE (chooser)->unselect_all (chooser);
}

/**
 * gtk_recent_chooser_get_items:
 * @chooser: a #GtkRecentChooser
 *
 * Gets the list of recently used resources in form of #GtkRecentInfo objects.
 *
 * The return value of this function is affected by the “sort-type” and
 * “limit” properties of @chooser.
 *
 * Returns:  (element-type GtkRecentInfo) (transfer full): A newly allocated
 *   list of #GtkRecentInfo objects.  You should
 *   use gtk_recent_info_unref() on every item of the list, and then free
 *   the list itself using g_list_free().
 *
 * Since: 2.10
 */
GList *
gtk_recent_chooser_get_items (GtkRecentChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), NULL);
  
  return GTK_RECENT_CHOOSER_GET_IFACE (chooser)->get_items (chooser);
}

/**
 * gtk_recent_chooser_get_uris:
 * @chooser: a #GtkRecentChooser
 * @length: (out) (allow-none): return location for a the length of the
 *     URI list, or %NULL
 *
 * Gets the URI of the recently used resources.
 *
 * The return value of this function is affected by the “sort-type” and “limit”
 * properties of @chooser.
 *
 * Since the returned array is %NULL terminated, @length may be %NULL.
 * 
 * Returns: (array length=length zero-terminated=1) (transfer full):
 *     A newly allocated, %NULL-terminated array of strings. Use
 *     g_strfreev() to free it.
 *
 * Since: 2.10
 */
gchar **
gtk_recent_chooser_get_uris (GtkRecentChooser *chooser,
                             gsize            *length)
{
  GList *items, *l;
  gchar **retval;
  gsize n_items, i;
  
  items = gtk_recent_chooser_get_items (chooser);
  
  n_items = g_list_length (items);
  retval = g_new0 (gchar *, n_items + 1);
  
  for (l = items, i = 0; l != NULL; l = l->next)
    {
      GtkRecentInfo *info = (GtkRecentInfo *) l->data;
      const gchar *uri;
      
      g_assert (info != NULL);
      
      uri = gtk_recent_info_get_uri (info);
      g_assert (uri != NULL);
      
      retval[i++] = g_strdup (uri);
    }
  retval[i] = NULL;
  
  if (length)
    *length = i;
  
  g_list_free_full (items, (GDestroyNotify) gtk_recent_info_unref);
  
  return retval;
}

/**
 * gtk_recent_chooser_add_filter:
 * @chooser: a #GtkRecentChooser
 * @filter: a #GtkRecentFilter
 *
 * Adds @filter to the list of #GtkRecentFilter objects held by @chooser.
 *
 * If no previous filter objects were defined, this function will call
 * gtk_recent_chooser_set_filter().
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_add_filter (GtkRecentChooser *chooser,
			       GtkRecentFilter  *filter)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  g_return_if_fail (GTK_IS_RECENT_FILTER (filter));
  
  GTK_RECENT_CHOOSER_GET_IFACE (chooser)->add_filter (chooser, filter);
}

/**
 * gtk_recent_chooser_remove_filter:
 * @chooser: a #GtkRecentChooser
 * @filter: a #GtkRecentFilter
 *
 * Removes @filter from the list of #GtkRecentFilter objects held by @chooser.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_remove_filter (GtkRecentChooser *chooser,
				  GtkRecentFilter  *filter)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  g_return_if_fail (GTK_IS_RECENT_FILTER (filter));
  
  GTK_RECENT_CHOOSER_GET_IFACE (chooser)->remove_filter (chooser, filter);
}

/**
 * gtk_recent_chooser_list_filters:
 * @chooser: a #GtkRecentChooser
 *
 * Gets the #GtkRecentFilter objects held by @chooser.
 *
 * Returns: (element-type GtkRecentFilter) (transfer container): A singly linked list
 *   of #GtkRecentFilter objects.  You
 *   should just free the returned list using g_slist_free().
 *
 * Since: 2.10
 */
GSList *
gtk_recent_chooser_list_filters (GtkRecentChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), NULL);
  
  return GTK_RECENT_CHOOSER_GET_IFACE (chooser)->list_filters (chooser);
}

/**
 * gtk_recent_chooser_set_filter:
 * @chooser: a #GtkRecentChooser
 * @filter: (allow-none): a #GtkRecentFilter
 *
 * Sets @filter as the current #GtkRecentFilter object used by @chooser
 * to affect the displayed recently used resources.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_filter (GtkRecentChooser *chooser,
			       GtkRecentFilter  *filter)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  g_return_if_fail (filter == NULL || GTK_IS_RECENT_FILTER (filter));
  
  g_object_set (G_OBJECT (chooser), "filter", filter, NULL);
}

/**
 * gtk_recent_chooser_get_filter:
 * @chooser: a #GtkRecentChooser
 *
 * Gets the #GtkRecentFilter object currently used by @chooser to affect
 * the display of the recently used resources.
 *
 * Returns: (transfer none): a #GtkRecentFilter object.
 *
 * Since: 2.10
 */
GtkRecentFilter *
gtk_recent_chooser_get_filter (GtkRecentChooser *chooser)
{
  GtkRecentFilter *filter;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), NULL);
  
  g_object_get (G_OBJECT (chooser), "filter", &filter, NULL);
  
  /* we need this hack because g_object_get() increases the refcount
   * of the returned object; see also gtk_file_chooser_get_filter()
   * inside gtkfilechooser.c
   */
  if (filter)
    g_object_unref (filter);
  
  return filter;
}

void
_gtk_recent_chooser_item_activated (GtkRecentChooser *chooser)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_signal_emit (chooser, chooser_signals[ITEM_ACTIVATED], 0);
}

void
_gtk_recent_chooser_selection_changed (GtkRecentChooser *chooser)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));

  g_signal_emit (chooser, chooser_signals[SELECTION_CHANGED], 0);
}

void
_gtk_recent_chooser_update (GtkActivatable *activatable,
			    GtkAction      *action,
			    const gchar    *property_name)
{
  GtkRecentChooser *recent_chooser;
  GtkRecentChooser *action_chooser;
  GtkRecentAction  *recent_action;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  recent_chooser = GTK_RECENT_CHOOSER (activatable);
  action_chooser = GTK_RECENT_CHOOSER (action);
  recent_action  = GTK_RECENT_ACTION (action);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (strcmp (property_name, "show-numbers") == 0 && recent_chooser_has_show_numbers (recent_chooser))
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      g_object_set (recent_chooser, "show-numbers",
                    gtk_recent_action_get_show_numbers (recent_action), NULL);
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
  else if (strcmp (property_name, "show-private") == 0)
    gtk_recent_chooser_set_show_private (recent_chooser, gtk_recent_chooser_get_show_private (action_chooser));
  else if (strcmp (property_name, "show-not-found") == 0)
    gtk_recent_chooser_set_show_not_found (recent_chooser, gtk_recent_chooser_get_show_not_found (action_chooser));
  else if (strcmp (property_name, "show-tips") == 0)
    gtk_recent_chooser_set_show_tips (recent_chooser, gtk_recent_chooser_get_show_tips (action_chooser));
  else if (strcmp (property_name, "show-icons") == 0)
    gtk_recent_chooser_set_show_icons (recent_chooser, gtk_recent_chooser_get_show_icons (action_chooser));
  else if (strcmp (property_name, "limit") == 0)
    gtk_recent_chooser_set_limit (recent_chooser, gtk_recent_chooser_get_limit (action_chooser));
  else if (strcmp (property_name, "local-only") == 0)
    gtk_recent_chooser_set_local_only (recent_chooser, gtk_recent_chooser_get_local_only (action_chooser));
  else if (strcmp (property_name, "sort-type") == 0)
    gtk_recent_chooser_set_sort_type (recent_chooser, gtk_recent_chooser_get_sort_type (action_chooser));
  else if (strcmp (property_name, "filter") == 0)
    gtk_recent_chooser_set_filter (recent_chooser, gtk_recent_chooser_get_filter (action_chooser));
}

void
_gtk_recent_chooser_sync_action_properties (GtkActivatable *activatable,
			                    GtkAction      *action)
{
  GtkRecentChooser *recent_chooser;
  GtkRecentChooser *action_chooser;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  recent_chooser = GTK_RECENT_CHOOSER (activatable);
  action_chooser = GTK_RECENT_CHOOSER (action);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (!action)
    return;

  if (recent_chooser_has_show_numbers (recent_chooser))
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      g_object_set (recent_chooser, "show-numbers", 
                    gtk_recent_action_get_show_numbers (GTK_RECENT_ACTION (action)),
                    NULL);
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
  gtk_recent_chooser_set_show_private (recent_chooser, gtk_recent_chooser_get_show_private (action_chooser));
  gtk_recent_chooser_set_show_not_found (recent_chooser, gtk_recent_chooser_get_show_not_found (action_chooser));
  gtk_recent_chooser_set_show_tips (recent_chooser, gtk_recent_chooser_get_show_tips (action_chooser));
  gtk_recent_chooser_set_show_icons (recent_chooser, gtk_recent_chooser_get_show_icons (action_chooser));
  gtk_recent_chooser_set_limit (recent_chooser, gtk_recent_chooser_get_limit (action_chooser));
  gtk_recent_chooser_set_local_only (recent_chooser, gtk_recent_chooser_get_local_only (action_chooser));
  gtk_recent_chooser_set_sort_type (recent_chooser, gtk_recent_chooser_get_sort_type (action_chooser));
  gtk_recent_chooser_set_filter (recent_chooser, gtk_recent_chooser_get_filter (action_chooser));
}

void
_gtk_recent_chooser_set_related_action (GtkRecentChooser *recent_chooser,
					GtkAction        *action)
{
  GtkAction *prev_action;

  prev_action = g_object_get_qdata (G_OBJECT (recent_chooser), quark_gtk_related_action);

  if (prev_action == action)
    return;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (recent_chooser), action);
  G_GNUC_END_IGNORE_DEPRECATIONS;
  g_object_set_qdata (G_OBJECT (recent_chooser), quark_gtk_related_action, action);
}

GtkAction *
_gtk_recent_chooser_get_related_action (GtkRecentChooser *recent_chooser)
{
  return g_object_get_qdata (G_OBJECT (recent_chooser), quark_gtk_related_action);
}

/* The default for use-action-appearance is TRUE, so we try to set the
 * qdata backwards for this case.
 */
void
_gtk_recent_chooser_set_use_action_appearance (GtkRecentChooser *recent_chooser, 
					       gboolean          use_appearance)
{
  GtkAction *action;
  gboolean   use_action_appearance;

  action                = g_object_get_qdata (G_OBJECT (recent_chooser), quark_gtk_related_action);
  use_action_appearance = !GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (recent_chooser), quark_gtk_use_action_appearance));

  if (use_action_appearance != use_appearance)
    {

      g_object_set_qdata (G_OBJECT (recent_chooser), quark_gtk_use_action_appearance, GINT_TO_POINTER (!use_appearance));
 
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_activatable_sync_action_properties (GTK_ACTIVATABLE (recent_chooser), action);
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
}

gboolean
_gtk_recent_chooser_get_use_action_appearance (GtkRecentChooser *recent_chooser)
{
  return !GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (recent_chooser), quark_gtk_use_action_appearance));
}
