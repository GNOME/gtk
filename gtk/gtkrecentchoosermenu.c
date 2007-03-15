/* GTK - The GIMP Toolkit
 * gtkrecentchoosermenu.c - Recently used items menu widget
 * Copyright (C) 2005, Emmanuele Bassi
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

#include "config.h"

#include <string.h>

#include <gdk/gdkscreen.h>

#include "gtkrecentmanager.h"
#include "gtkrecentfilter.h"
#include "gtkrecentchooser.h"
#include "gtkrecentchooserutils.h"
#include "gtkrecentchooserprivate.h"
#include "gtkrecentchoosermenu.h"

#include "gtkstock.h"
#include "gtkicontheme.h"
#include "gtkiconfactory.h"
#include "gtkintl.h"
#include "gtksettings.h"
#include "gtkmenushell.h"
#include "gtkmenuitem.h"
#include "gtkimagemenuitem.h"
#include "gtkseparatormenuitem.h"
#include "gtkmenu.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkobject.h"
#include "gtktooltips.h"
#include "gtktypebuiltins.h"

#include "gtkrecentmanager.h"
#include "gtkrecentfilter.h"
#include "gtkrecentchooser.h"
#include "gtkrecentchooserutils.h"
#include "gtkrecentchooserprivate.h"
#include "gtkrecentchoosermenu.h"

#include "gtkprivate.h"
#include "gtkalias.h"

struct _GtkRecentChooserMenuPrivate
{
  /* the recent manager object */
  GtkRecentManager *manager;
  
  /* size of the icons of the menu items */  
  gint icon_size;

  /* max size of the menu item label */
  gint label_width;

  /* RecentChooser properties */
  gint limit;  
  guint show_private : 1;
  guint show_not_found : 1;
  guint show_tips : 1;
  guint show_icons : 1;
  guint local_only : 1;
  
  guint show_numbers : 1;
  
  GtkRecentSortType sort_type;
  GtkRecentSortFunc sort_func;
  gpointer sort_data;
  GDestroyNotify sort_data_destroy;
  
  GSList *filters;
  GtkRecentFilter *current_filter;
 
  guint local_manager : 1;
  gulong manager_changed_id;

  gulong populate_id;

  /* tooltips for our bookmark items*/
  GtkTooltips *tooltips;
};

enum {
  PROP_0,

  PROP_SHOW_NUMBERS
};

#define FALLBACK_ICON_SIZE 	32
#define FALLBACK_ITEM_LIMIT 	10
#define DEFAULT_LABEL_WIDTH     30

#define GTK_RECENT_CHOOSER_MENU_GET_PRIVATE(obj)	(G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_RECENT_CHOOSER_MENU, GtkRecentChooserMenuPrivate))

static void     gtk_recent_chooser_menu_finalize    (GObject                   *object);
static void     gtk_recent_chooser_menu_dispose     (GObject                   *object);
static GObject *gtk_recent_chooser_menu_constructor (GType                      type,
						     guint                      n_construct_properties,
						     GObjectConstructParam     *construct_params);

static void gtk_recent_chooser_iface_init      (GtkRecentChooserIface     *iface);

static void gtk_recent_chooser_menu_set_property (GObject      *object,
						  guint         prop_id,
						  const GValue *value,
						  GParamSpec   *pspec);
static void gtk_recent_chooser_menu_get_property (GObject      *object,
						  guint         prop_id,
						  GValue       *value,
						  GParamSpec   *pspec);

static gboolean          gtk_recent_chooser_menu_set_current_uri    (GtkRecentChooser  *chooser,
							             const gchar       *uri,
							             GError           **error);
static gchar *           gtk_recent_chooser_menu_get_current_uri    (GtkRecentChooser  *chooser);
static gboolean          gtk_recent_chooser_menu_select_uri         (GtkRecentChooser  *chooser,
								     const gchar       *uri,
								     GError           **error);
static void              gtk_recent_chooser_menu_unselect_uri       (GtkRecentChooser  *chooser,
								     const gchar       *uri);
static void              gtk_recent_chooser_menu_select_all         (GtkRecentChooser  *chooser);
static void              gtk_recent_chooser_menu_unselect_all       (GtkRecentChooser  *chooser);
static GList *           gtk_recent_chooser_menu_get_items          (GtkRecentChooser  *chooser);
static GtkRecentManager *gtk_recent_chooser_menu_get_recent_manager (GtkRecentChooser  *chooser);
static void              gtk_recent_chooser_menu_set_sort_func      (GtkRecentChooser  *chooser,
								     GtkRecentSortFunc  sort_func,
								     gpointer           sort_data,
								     GDestroyNotify     data_destroy);
static void              gtk_recent_chooser_menu_add_filter         (GtkRecentChooser  *chooser,
								     GtkRecentFilter   *filter);
static void              gtk_recent_chooser_menu_remove_filter      (GtkRecentChooser  *chooser,
								     GtkRecentFilter   *filter);
static GSList *          gtk_recent_chooser_menu_list_filters       (GtkRecentChooser  *chooser);
static void              gtk_recent_chooser_menu_set_current_filter (GtkRecentChooserMenu *menu,
								     GtkRecentFilter      *filter);

static void              gtk_recent_chooser_menu_populate           (GtkRecentChooserMenu *menu);
static void              gtk_recent_chooser_menu_set_show_tips      (GtkRecentChooserMenu *menu,
								     gboolean              show_tips);

static void     set_recent_manager (GtkRecentChooserMenu *menu,
				    GtkRecentManager     *manager);

static void     chooser_set_sort_type (GtkRecentChooserMenu *menu,
				       GtkRecentSortType     sort_type);

static gint     get_icon_size_for_widget (GtkWidget *widget);

static void     item_activate_cb   (GtkWidget        *widget,
			            gpointer          user_data);
static void     manager_changed_cb (GtkRecentManager *manager,
				    gpointer          user_data);

G_DEFINE_TYPE_WITH_CODE (GtkRecentChooserMenu,
			 gtk_recent_chooser_menu,
			 GTK_TYPE_MENU,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_RECENT_CHOOSER,
				 		gtk_recent_chooser_iface_init))


static void
gtk_recent_chooser_iface_init (GtkRecentChooserIface *iface)
{
  iface->set_current_uri = gtk_recent_chooser_menu_set_current_uri;
  iface->get_current_uri = gtk_recent_chooser_menu_get_current_uri;
  iface->select_uri = gtk_recent_chooser_menu_select_uri;
  iface->unselect_uri = gtk_recent_chooser_menu_unselect_uri;
  iface->select_all = gtk_recent_chooser_menu_select_all;
  iface->unselect_all = gtk_recent_chooser_menu_unselect_all;
  iface->get_items = gtk_recent_chooser_menu_get_items;
  iface->get_recent_manager = gtk_recent_chooser_menu_get_recent_manager;
  iface->set_sort_func = gtk_recent_chooser_menu_set_sort_func;
  iface->add_filter = gtk_recent_chooser_menu_add_filter;
  iface->remove_filter = gtk_recent_chooser_menu_remove_filter;
  iface->list_filters = gtk_recent_chooser_menu_list_filters;
}

static void
gtk_recent_chooser_menu_class_init (GtkRecentChooserMenuClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructor = gtk_recent_chooser_menu_constructor;
  gobject_class->finalize = gtk_recent_chooser_menu_finalize;
  gobject_class->dispose = gtk_recent_chooser_menu_dispose;
  gobject_class->set_property = gtk_recent_chooser_menu_set_property;
  gobject_class->get_property = gtk_recent_chooser_menu_get_property;

  _gtk_recent_chooser_install_properties (gobject_class);

  /**
   * GtkRecentChooserMenu:show-numbers
   *
   * Whether the first ten items in the menu should be prepended by
   * a number acting as a unique mnemonic.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
		  		   PROP_SHOW_NUMBERS,
				   g_param_spec_boolean ("show-numbers",
							 P_("Show Numbers"),
							 P_("Whether the items should be displayed with a number"),
							 FALSE,
							 GTK_PARAM_READWRITE));
  
  g_type_class_add_private (klass, sizeof (GtkRecentChooserMenuPrivate));
}

static void
gtk_recent_chooser_menu_init (GtkRecentChooserMenu *menu)
{
  GtkRecentChooserMenuPrivate *priv;
  
  priv = GTK_RECENT_CHOOSER_MENU_GET_PRIVATE (menu);
  
  menu->priv = priv;
  
  priv->show_icons= TRUE;
  priv->show_numbers = FALSE;
  priv->show_tips = FALSE;
  priv->show_not_found = TRUE;
  priv->show_private = FALSE;
  priv->local_only = TRUE;
  
  priv->limit = FALLBACK_ITEM_LIMIT;

  priv->sort_type = GTK_RECENT_SORT_NONE;
  
  priv->icon_size = FALLBACK_ICON_SIZE;

  priv->label_width = DEFAULT_LABEL_WIDTH;
  
  priv->current_filter = NULL;
    
  priv->tooltips = gtk_tooltips_new ();
  g_object_ref_sink (priv->tooltips);
}

static void
gtk_recent_chooser_menu_finalize (GObject *object)
{
  GtkRecentChooserMenu *menu = GTK_RECENT_CHOOSER_MENU (object);
  GtkRecentChooserMenuPrivate *priv = menu->priv;
  
  priv->manager = NULL;
  
  if (priv->sort_data_destroy)
    {
      priv->sort_data_destroy (priv->sort_data);
      priv->sort_data_destroy = NULL;
    }
  
  priv->sort_data = NULL;
  priv->sort_func = NULL;
  
  G_OBJECT_CLASS (gtk_recent_chooser_menu_parent_class)->finalize (object);
}

static void
gtk_recent_chooser_menu_dispose (GObject *object)
{
  GtkRecentChooserMenu *menu = GTK_RECENT_CHOOSER_MENU (object);
  GtkRecentChooserMenuPrivate *priv = menu->priv;

  if (priv->manager_changed_id)
    {
      if (priv->manager)
        g_signal_handler_disconnect (priv->manager, priv->manager_changed_id);

      priv->manager_changed_id = 0;
    }

  if (priv->populate_id)
    {
      g_source_remove (priv->populate_id);
      priv->populate_id = 0;
    }

  if (priv->tooltips)
    {
      g_object_unref (priv->tooltips);
      priv->tooltips = NULL;
    }

  if (priv->current_filter)
    {
      g_object_unref (priv->current_filter);
      priv->current_filter = NULL;
    }

  G_OBJECT_CLASS (gtk_recent_chooser_menu_parent_class)->dispose (object);
}

static GObject *
gtk_recent_chooser_menu_constructor (GType                  type,
				     guint                  n_construct_properties,
				     GObjectConstructParam *construct_params)
{
  GtkRecentChooserMenu *menu;
  GObject *object;
  
  object = G_OBJECT_CLASS (gtk_recent_chooser_menu_parent_class)->constructor (type,
		  							       n_construct_properties,
									       construct_params);
  menu = GTK_RECENT_CHOOSER_MENU (object);
  
  g_assert (menu->priv->manager);
  
  return object;
}

static void
gtk_recent_chooser_menu_set_property (GObject      *object,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
  GtkRecentChooserMenu *menu = GTK_RECENT_CHOOSER_MENU (object);
  GtkRecentChooserMenuPrivate *priv = menu->priv;
  
  switch (prop_id)
    {
    case PROP_SHOW_NUMBERS:
      priv->show_numbers = g_value_get_boolean (value);
      break;
    case GTK_RECENT_CHOOSER_PROP_RECENT_MANAGER:
      set_recent_manager (menu, g_value_get_object (value));
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_PRIVATE:
      priv->show_private = g_value_get_boolean (value);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_NOT_FOUND:
      priv->show_not_found = g_value_get_boolean (value);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_TIPS:
      gtk_recent_chooser_menu_set_show_tips (menu, g_value_get_boolean (value));
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_ICONS:
      priv->show_icons = g_value_get_boolean (value);
      break;
    case GTK_RECENT_CHOOSER_PROP_SELECT_MULTIPLE:
      g_warning ("%s: Choosers of type `%s' do not support selecting multiple items.",
                 G_STRFUNC,
                 G_OBJECT_TYPE_NAME (object));
      break;
    case GTK_RECENT_CHOOSER_PROP_LOCAL_ONLY:
      priv->local_only = g_value_get_boolean (value);
      break;
    case GTK_RECENT_CHOOSER_PROP_LIMIT:
      priv->limit = g_value_get_int (value);
      break;
    case GTK_RECENT_CHOOSER_PROP_SORT_TYPE:
      chooser_set_sort_type (menu, g_value_get_enum (value));
      break;
    case GTK_RECENT_CHOOSER_PROP_FILTER:
      gtk_recent_chooser_menu_set_current_filter (menu, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_recent_chooser_menu_get_property (GObject    *object,
				      guint       prop_id,
				      GValue     *value,
				      GParamSpec *pspec)
{
  GtkRecentChooserMenu *menu = GTK_RECENT_CHOOSER_MENU (object);
  GtkRecentChooserMenuPrivate *priv = menu->priv;
  
  switch (prop_id)
    {
    case PROP_SHOW_NUMBERS:
      g_value_set_boolean (value, priv->show_numbers);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_TIPS:
      g_value_set_boolean (value, priv->show_tips);
      break;
    case GTK_RECENT_CHOOSER_PROP_LIMIT:
      g_value_set_int (value, priv->limit);
      break;
    case GTK_RECENT_CHOOSER_PROP_LOCAL_ONLY:
      g_value_set_boolean (value, priv->local_only);
      break;
    case GTK_RECENT_CHOOSER_PROP_SORT_TYPE:
      g_value_set_enum (value, priv->sort_type);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_PRIVATE:
      g_value_set_boolean (value, priv->show_private);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_NOT_FOUND:
      g_value_set_boolean (value, priv->show_not_found);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_ICONS:
      g_value_set_boolean (value, priv->show_icons);
      break;
    case GTK_RECENT_CHOOSER_PROP_SELECT_MULTIPLE:
      g_warning ("%s: Choosers of type `%s' do not support selecting multiple items.",
                 G_STRFUNC,
                 G_OBJECT_TYPE_NAME (object));
      break;
    case GTK_RECENT_CHOOSER_PROP_FILTER:
      g_value_set_object (value, priv->current_filter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gtk_recent_chooser_menu_set_current_uri (GtkRecentChooser  *chooser,
					 const gchar       *uri,
					 GError           **error)
{
  GtkRecentChooserMenu *menu = GTK_RECENT_CHOOSER_MENU (chooser);
  GList *children, *l;
  GtkWidget *menu_item = NULL;
  gboolean found = FALSE;
  
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  
  for (l = children; l != NULL; l = l->next)
    {
      GtkRecentInfo *info;
      
      menu_item = GTK_WIDGET (l->data);
      
      info = g_object_get_data (G_OBJECT (menu_item), "gtk-recent-info");
      if (!info)
        continue;
      
      if (strcmp (uri, gtk_recent_info_get_uri (info)) == 0)
        {
          gtk_menu_shell_activate_item (GTK_MENU_SHELL (menu),
	                                menu_item,
					TRUE);
          found = TRUE;

	  break;
	}
    }

  g_list_free (children);
  
  if (!found)
    {
      g_set_error (error, GTK_RECENT_CHOOSER_ERROR,
      		   GTK_RECENT_CHOOSER_ERROR_NOT_FOUND,
      		   _("No recently used resource found with URI `%s'"),
      		   uri);
    }

  return found;
}

static gchar *
gtk_recent_chooser_menu_get_current_uri (GtkRecentChooser  *chooser)
{
  GtkRecentChooserMenu *menu = GTK_RECENT_CHOOSER_MENU (chooser);
  GtkWidget *menu_item;
  GtkRecentInfo *info;
  
  menu_item = gtk_menu_get_active (GTK_MENU (menu));
  if (!menu_item)
    return NULL;
  
  info = g_object_get_data (G_OBJECT (menu_item), "gtk-recent-info");
  if (!info)
    return NULL;
  
  return g_strdup (gtk_recent_info_get_uri (info));
}

static gboolean
gtk_recent_chooser_menu_select_uri (GtkRecentChooser  *chooser,
				    const gchar       *uri,
				    GError           **error)
{
  GtkRecentChooserMenu *menu = GTK_RECENT_CHOOSER_MENU (chooser);
  GList *children, *l;
  GtkWidget *menu_item = NULL;
  gboolean found = FALSE;
  
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  for (l = children; l != NULL; l = l->next)
    {
      GtkRecentInfo *info;
      
      menu_item = GTK_WIDGET (l->data);
      
      info = g_object_get_data (G_OBJECT (menu_item), "gtk-recent-info");
      if (!info)
        continue;
      
      if (0 == strcmp (uri, gtk_recent_info_get_uri (info)))
        found = TRUE;
    }

  g_list_free (children);
  
  if (!found)  
    {
      g_set_error (error, GTK_RECENT_CHOOSER_ERROR,
      		   GTK_RECENT_CHOOSER_ERROR_NOT_FOUND,
      		   _("No recently used resource found with URI `%s'"),
      		   uri);
      return FALSE;
    }
  else
    {
      gtk_menu_shell_select_item (GTK_MENU_SHELL (menu), menu_item);

      return TRUE;
    }
}

static void
gtk_recent_chooser_menu_unselect_uri (GtkRecentChooser *chooser,
				       const gchar     *uri)
{
  GtkRecentChooserMenu *menu = GTK_RECENT_CHOOSER_MENU (chooser);
  
  gtk_menu_shell_deselect (GTK_MENU_SHELL (menu));
}

static void
gtk_recent_chooser_menu_select_all (GtkRecentChooser *chooser)
{
  g_warning (_("This function is not implemented for "
               "widgets of class '%s'"),
             g_type_name (G_OBJECT_TYPE (chooser)));
}

static void
gtk_recent_chooser_menu_unselect_all (GtkRecentChooser *chooser)
{
  g_warning (_("This function is not implemented for "
               "widgets of class '%s'"),
             g_type_name (G_OBJECT_TYPE (chooser)));
}

static void
gtk_recent_chooser_menu_set_sort_func (GtkRecentChooser  *chooser,
				       GtkRecentSortFunc  sort_func,
				       gpointer           sort_data,
				       GDestroyNotify     data_destroy)
{
  GtkRecentChooserMenu *menu = GTK_RECENT_CHOOSER_MENU (chooser);
  GtkRecentChooserMenuPrivate *priv = menu->priv;
  
  if (priv->sort_data_destroy)
    {
      priv->sort_data_destroy (priv->sort_data);
      priv->sort_data_destroy = NULL;
    }
      
  priv->sort_func = NULL;
  priv->sort_data = NULL;
  priv->sort_data_destroy = NULL;
  
  if (sort_func)
    {
      priv->sort_func = sort_func;
      priv->sort_data = sort_data;
      priv->sort_data_destroy = data_destroy;
    }
}

static void
chooser_set_sort_type (GtkRecentChooserMenu *menu,
		       GtkRecentSortType     sort_type)
{
  if (menu->priv->sort_type == sort_type)
    return;

  menu->priv->sort_type = sort_type;
}


static GList *
gtk_recent_chooser_menu_get_items (GtkRecentChooser *chooser)
{
  GtkRecentChooserMenu *menu = GTK_RECENT_CHOOSER_MENU (chooser);
  GtkRecentChooserMenuPrivate *priv = menu->priv;

  return _gtk_recent_chooser_get_items (chooser,
                                        priv->current_filter,
                                        priv->sort_func,
                                        priv->sort_data);
}

static GtkRecentManager *
gtk_recent_chooser_menu_get_recent_manager (GtkRecentChooser *chooser)
{
  GtkRecentChooserMenuPrivate *priv;
 
  priv = GTK_RECENT_CHOOSER_MENU (chooser)->priv;
  
  return priv->manager;
}

static void
gtk_recent_chooser_menu_add_filter (GtkRecentChooser *chooser,
				    GtkRecentFilter  *filter)
{
  GtkRecentChooserMenu *menu;

  menu = GTK_RECENT_CHOOSER_MENU (chooser);
  
  gtk_recent_chooser_menu_set_current_filter (menu, filter);
}

static void
gtk_recent_chooser_menu_remove_filter (GtkRecentChooser *chooser,
				       GtkRecentFilter  *filter)
{
  GtkRecentChooserMenu *menu;

  menu = GTK_RECENT_CHOOSER_MENU (chooser);
  
  if (filter == menu->priv->current_filter)
    {
      g_object_unref (menu->priv->current_filter);
      menu->priv->current_filter = NULL;

      g_object_notify (G_OBJECT (menu), "filter");
    }
}

static GSList *
gtk_recent_chooser_menu_list_filters (GtkRecentChooser *chooser)
{
  GtkRecentChooserMenu *menu;
  GSList *retval = NULL;

  menu = GTK_RECENT_CHOOSER_MENU (chooser);
 
  if (menu->priv->current_filter)
    retval = g_slist_prepend (retval, menu->priv->current_filter);

  return retval;
}

static void
gtk_recent_chooser_menu_set_current_filter (GtkRecentChooserMenu *menu,
					    GtkRecentFilter      *filter)
{
  GtkRecentChooserMenuPrivate *priv;

  priv = menu->priv;
  
  if (priv->current_filter)
    g_object_unref (G_OBJECT (priv->current_filter));
  
  if (filter)
    {
      priv->current_filter = filter;
      g_object_ref_sink (priv->current_filter);
    }

  gtk_recent_chooser_menu_populate (menu);
  
  g_object_notify (G_OBJECT (menu), "filter");
}

/* taken from libeel/eel-strings.c */
static gchar *
escape_underscores (const gchar *string)
{
  gint underscores;
  const gchar *p;
  gchar *q;
  gchar *escaped;

  if (!string)
    return NULL;
	
  underscores = 0;
  for (p = string; *p != '\0'; p++)
    underscores += (*p == '_');

  if (underscores == 0)
    return g_strdup (string);

  escaped = g_new (char, strlen (string) + underscores + 1);
  for (p = string, q = escaped; *p != '\0'; p++, q++)
    {
      /* Add an extra underscore. */
      if (*p == '_')
        *q++ = '_';
      
      *q = *p;
    }
  
  *q = '\0';
	
  return escaped;
}

static void
gtk_recent_chooser_menu_add_tip (GtkRecentChooserMenu *menu,
				 GtkRecentInfo        *info,
				 GtkWidget            *item)
{
  GtkRecentChooserMenuPrivate *priv;
  gchar *path, *tip_text;

  g_assert (info != NULL);
  g_assert (item != NULL);

  priv = menu->priv;
  
  if (!priv->tooltips)
    return;
  
  path = gtk_recent_info_get_uri_display (info);
  if (path)
    {
      tip_text = g_strdup_printf (_("Open '%s'"), path);

      gtk_tooltips_set_tip (priv->tooltips,
                            item,
                            tip_text,
                            NULL);
      
      g_free (path);
      g_free (tip_text);
    }
}

static GtkWidget *
gtk_recent_chooser_menu_create_item (GtkRecentChooserMenu *menu,
				     GtkRecentInfo        *info,
				     gint                  count)
{
  GtkRecentChooserMenuPrivate *priv;
  gchar *text;
  GtkWidget *item, *image, *label;
  GdkPixbuf *icon;

  g_assert (info != NULL);

  priv = menu->priv;

  if (priv->show_numbers)
    {
      gchar *name, *escaped;
      
      name = g_strdup (gtk_recent_info_get_display_name (info));
      if (!name)
        name = g_strdup (_("Unknown item"));
      
      escaped = escape_underscores (name);
      
      /* avoid clashing mnemonics */
      if (count <= 10)
        text = g_strdup_printf ("_%d. %s", count, escaped);
      else
        text = g_strdup_printf ("%d. %s", count, escaped);
      
      item = gtk_image_menu_item_new_with_mnemonic (text);
      
      g_free (escaped);
      g_free (name);
    }
  else
    {
      text = g_strdup (gtk_recent_info_get_display_name (info));
      item = gtk_image_menu_item_new_with_label (text);
    }

  g_free (text);

  label = GTK_BIN (item)->child;
  if (GTK_IS_LABEL (label))
    {
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
      gtk_label_set_max_width_chars (GTK_LABEL (label), priv->label_width);
    }
  
  if (priv->show_icons)
    {
      icon = gtk_recent_info_get_icon (info, priv->icon_size);
        
      image = gtk_image_new_from_pixbuf (icon);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

      g_object_unref (icon);
    }

  g_signal_connect (item, "activate",
  		    G_CALLBACK (item_activate_cb),
  		    menu);

  return item;
}

/* removes the items we own from the menu */
static void
gtk_recent_chooser_menu_dispose_items (GtkRecentChooserMenu *menu)
{
  GList *children, *l;
 
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  for (l = children; l != NULL; l = l->next)
    {
      GtkWidget *menu_item = GTK_WIDGET (l->data);
      gint mark = 0;
      
      /* check for our mark, in order to remove just the items we own */
      mark = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item),
                                                 "gtk-recent-menu-mark"));
      if (mark == 1)
        {
          GtkRecentInfo *info;
          
          /* destroy the attached RecentInfo struct, if found */
          info = g_object_get_data (G_OBJECT (menu_item), "gtk-recent-info");
          if (info)
            g_object_set_data_full (G_OBJECT (menu_item), "gtk-recent-info",
            			    NULL, NULL);
          
          /* and finally remove the item from the menu */
          gtk_container_remove (GTK_CONTAINER (menu), menu_item);
        }
    }

  g_list_free (children);
}

typedef struct
{
  GList *items;
  gint n_items;
  gint loaded_items;
  gint displayed_items;
  GtkRecentChooserMenu *menu;
} MenuPopulateData;

static gboolean
idle_populate_func (gpointer data)
{
  MenuPopulateData *pdata;
  GtkRecentChooserMenuPrivate *priv;
  GtkRecentInfo *info;
  gboolean retval;
  GtkWidget *item;

  GDK_THREADS_ENTER ();

  pdata = (MenuPopulateData *) data;
  priv = pdata->menu->priv;

  priv->populate_id = 0;

  if (!pdata->items)
    {
      pdata->items = gtk_recent_chooser_get_items (GTK_RECENT_CHOOSER (pdata->menu));
      if (!pdata->items)
        {
          item = gtk_menu_item_new_with_label (_("No items found"));
          gtk_widget_set_sensitive (item, FALSE);
      
          /* we also mark this item, so that it gets removed when rebuilding
           * the menu on the next map event
           */
          g_object_set_data (G_OBJECT (item), "gtk-recent-menu-mark",
      			     GINT_TO_POINTER (1));
      
          gtk_menu_shell_prepend (GTK_MENU_SHELL (pdata->menu), item);
          gtk_widget_show (item);

          pdata->displayed_items = 1;

	  /* no items: add a placeholder menu */
          GDK_THREADS_LEAVE ();

	  return FALSE;
	}
      
      /* reverse the list */
      pdata->items = g_list_reverse (pdata->items);
      
      pdata->n_items = g_list_length (pdata->items);
      pdata->loaded_items = 0;
    }

  info = g_list_nth_data (pdata->items, pdata->loaded_items);
  item = gtk_recent_chooser_menu_create_item (pdata->menu,
                                              info,
					      pdata->displayed_items);
  if (!item)
    goto check_and_return;
      
  gtk_recent_chooser_menu_add_tip (pdata->menu, info, item);
      
  /* FIXME
   *
   * We should really place our items taking into account user
   * defined menu items; this would also remove the need of
   * reverting the scan order.
   */
  gtk_menu_shell_prepend (GTK_MENU_SHELL (pdata->menu), item);
  gtk_widget_show (item);

  pdata->displayed_items += 1;
      
  /* mark the menu item as one of our own */
  g_object_set_data (G_OBJECT (item), "gtk-recent-menu-mark",
      		     GINT_TO_POINTER (1));
      
  /* attach the RecentInfo object to the menu item, and own a reference
   * to it, so that it will be destroyed with the menu item when it's
   * not needed anymore.
   */
  g_object_set_data_full (G_OBJECT (item), "gtk-recent-info",
      			  gtk_recent_info_ref (info),
      			  (GDestroyNotify) gtk_recent_info_unref);
  
check_and_return:
  pdata->loaded_items += 1;

  if (pdata->loaded_items == pdata->n_items)
    {
      g_list_foreach (pdata->items, (GFunc) gtk_recent_info_unref, NULL);
      g_list_free (pdata->items);

      retval = FALSE;
    }
  else
    retval = TRUE;

  GDK_THREADS_LEAVE ();

  return retval;
}

static void
idle_populate_clean_up (gpointer data)
{
  MenuPopulateData *pdata = data;

  if (!pdata->displayed_items)
    {
      GtkWidget *item;

      item = gtk_menu_item_new_with_label (_("No items found"));
      gtk_widget_set_sensitive (item, FALSE);

      /* we also mark this item, so that it gets removed when rebuilding
       * the menu on the next map event
       */
      g_object_set_data (G_OBJECT (item), "gtk-recent-menu-mark",
                         GINT_TO_POINTER (1));
      
      gtk_menu_shell_prepend (GTK_MENU_SHELL (pdata->menu), item);
      gtk_widget_show (item);
    }

  g_slice_free (MenuPopulateData, data);
}

static void
gtk_recent_chooser_menu_populate (GtkRecentChooserMenu *menu)
{
  MenuPopulateData *pdata;

  if (menu->priv->populate_id)
    return;

  pdata = g_slice_new (MenuPopulateData);
  pdata->items = NULL;
  pdata->n_items = 0;
  pdata->loaded_items = 0;
  pdata->displayed_items = 0;
  pdata->menu = menu;

  menu->priv->icon_size = get_icon_size_for_widget (GTK_WIDGET (menu));
  
  /* dispose our menu items first */
  gtk_recent_chooser_menu_dispose_items (menu);
  
  menu->priv->populate_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE + 30,
  					     idle_populate_func,
					     pdata,
					     idle_populate_clean_up);
}

/* bounce activate signal from the recent menu item widget 
 * to the recent menu widget
 */
static void
item_activate_cb (GtkWidget *widget,
		  gpointer   user_data)
{
  GtkRecentChooser *chooser = GTK_RECENT_CHOOSER (user_data);
  
  _gtk_recent_chooser_item_activated (chooser);
}

/* we force a redraw if the manager changes when we are showing */
static void
manager_changed_cb (GtkRecentManager *manager,
		    gpointer          user_data)
{
  GtkRecentChooserMenu *menu = GTK_RECENT_CHOOSER_MENU (user_data);

  gtk_recent_chooser_menu_populate (menu);
}

static void
set_recent_manager (GtkRecentChooserMenu *menu,
		    GtkRecentManager     *manager)
{
  GtkRecentChooserMenuPrivate *priv = menu->priv;

  if (priv->manager)
    {
      if (priv->manager_changed_id)
        g_signal_handler_disconnect (priv->manager, priv->manager_changed_id);

      priv->manager = NULL;
    }
  
  if (manager)
    priv->manager = manager;
  else
    priv->manager = gtk_recent_manager_get_default ();
  
  if (priv->manager)
    priv->manager_changed_id = g_signal_connect (priv->manager, "changed",
      						 G_CALLBACK (manager_changed_cb),
      						 menu);
  /* (re)populate the menu */
  gtk_recent_chooser_menu_populate (menu);
}

static gint
get_icon_size_for_widget (GtkWidget *widget)
{
  GtkSettings *settings;
  gint width, height;

  if (gtk_widget_has_screen (widget))
    settings = gtk_settings_get_for_screen (gtk_widget_get_screen (widget));
  else
    settings = gtk_settings_get_default ();

  if (gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU,
                                         &width, &height))
    return MAX (width, height);

  return FALLBACK_ICON_SIZE;
}

static void
gtk_recent_chooser_menu_set_show_tips (GtkRecentChooserMenu *menu,
				       gboolean              show_tips)
{
  if (menu->priv->show_tips == show_tips)
    return;
  
  g_assert (menu->priv->tooltips != NULL);
  
  if (show_tips)
    gtk_tooltips_enable (menu->priv->tooltips);
  else
    gtk_tooltips_disable (menu->priv->tooltips);
  
  menu->priv->show_tips = show_tips;
}

/*
 * Public API
 */

/**
 * gtk_recent_chooser_menu_new:
 *
 * Creates a new #GtkRecentChooserMenu widget.
 *
 * This kind of widget shows the list of recently used resources as
 * a menu, each item as a menu item.  Each item inside the menu might
 * have an icon, representing its MIME type, and a number, for mnemonic
 * access.
 *
 * This widget implements the #GtkRecentChooser interface.
 *
 * This widget creates its own #GtkRecentManager object.  See the
 * gtk_recent_chooser_menu_new_for_manager() function to know how to create
 * a #GtkRecentChooserMenu widget bound to another #GtkRecentManager object.
 *
 * Return value: a new #GtkRecentChooserMenu
 *
 * Since: 2.10
 */
GtkWidget *
gtk_recent_chooser_menu_new (void)
{
  return g_object_new (GTK_TYPE_RECENT_CHOOSER_MENU,
  		       "recent-manager", NULL,
  		       NULL);
}

/**
 * gtk_recent_chooser_menu_new_for_manager:
 * @manager: a #GtkRecentManager
 *
 * Creates a new #GtkRecentChooserMenu widget using @manager as
 * the underlying recently used resources manager.
 *
 * This is useful if you have implemented your own recent manager,
 * or if you have a customized instance of a #GtkRecentManager
 * object or if you wish to share a common #GtkRecentManager object
 * among multiple #GtkRecentChooser widgets.
 *
 * Return value: a new #GtkRecentChooserMenu, bound to @manager.
 *
 * Since: 2.10
 */
GtkWidget *
gtk_recent_chooser_menu_new_for_manager (GtkRecentManager *manager)
{
  g_return_val_if_fail (manager == NULL || GTK_IS_RECENT_MANAGER (manager), NULL);
  
  return g_object_new (GTK_TYPE_RECENT_CHOOSER_MENU,
  		       "recent-manager", manager,
  		       NULL);
}

/**
 * gtk_recent_chooser_menu_get_show_numbers:
 * @menu: a #GtkRecentChooserMenu
 *
 * Returns the value set by gtk_recent_chooser_menu_set_show_numbers().
 * 
 * Return value: %TRUE if numbers should be shown.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_menu_get_show_numbers (GtkRecentChooserMenu *menu)
{
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER_MENU (menu), FALSE);

  return menu->priv->show_numbers;
}

/**
 * gtk_recent_chooser_menu_set_show_numbers:
 * @menu: a #GtkRecentChooserMenu
 * @show_numbers: whether to show numbers
 *
 * Sets whether a number should be added to the items of @menu.  The
 * numbers are shown to provide a unique character for a mnemonic to
 * be used inside ten menu item's label.  Only the first the items
 * get a number to avoid clashes.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_menu_set_show_numbers (GtkRecentChooserMenu *menu,
					  gboolean              show_numbers)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER_MENU (menu));

  if (menu->priv->show_numbers == show_numbers)
    return;

  menu->priv->show_numbers = show_numbers;
  g_object_notify (G_OBJECT (menu), "show-numbers");
}

#define __GTK_RECENT_CHOOSER_MENU_C__
#include "gtkaliasdef.c"
