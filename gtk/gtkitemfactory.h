/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkItemFactory: Flexible item factory with automatic rc handling
 * Copyright (C) 1998 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_ITEM_FACTORY_H__
#define	__GTK_ITEM_FACTORY_H__


#include <gtk/gtkwidget.h>
#include <gtk/gtkmenufactory.h>	/* for GtkMenuEntry */
#include <gtk/gtkbindings.h>	/* for GtkPatternSpec */


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef void	(*GtkPrintFunc)		   (gpointer		 func_data,
					    gchar		*str);
typedef gchar * (*GtkTranslateFunc)	   (const gchar		*path,
					    gpointer             func_data);
typedef	void	(*GtkItemFactoryCallback)  ();
typedef	void	(*GtkItemFactoryCallback1) (gpointer		 callback_data,
					    guint		 callback_action,
					    GtkWidget		*widget);

#define	GTK_TYPE_ITEM_FACTORY		 (gtk_item_factory_get_type ())
#define	GTK_ITEM_FACTORY(object)	 (GTK_CHECK_CAST (object, GTK_TYPE_ITEM_FACTORY, GtkItemFactory))
#define	GTK_ITEM_FACTORY_CLASS(klass)	 (GTK_CHECK_CLASS_CAST (klass, GTK_TYPE_ITEM_FACTORY, GtkItemFactoryClass))
#define	GTK_IS_ITEM_FACTORY(object)	 (GTK_CHECK_TYPE (object, GTK_TYPE_ITEM_FACTORY))
#define	GTK_IS_ITEM_FACTORY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ITEM_FACTORY))


typedef	struct	_GtkItemFactory			GtkItemFactory;
typedef	struct	_GtkItemFactoryClass		GtkItemFactoryClass;
typedef	struct	_GtkItemFactoryEntry		GtkItemFactoryEntry;
typedef	struct	_GtkItemFactoryItem		GtkItemFactoryItem;

struct _GtkItemFactory
{
  GtkObject		 object;

  gchar			*path;
  GtkAccelGroup		*accel_group;
  GtkWidget		*widget;
  GSList		*items;

  GtkTranslateFunc       translate_func;
  gpointer               translate_data;
  GtkDestroyNotify       translate_notify;   
};

struct _GtkItemFactoryClass
{
  GtkObjectClass	 object_class;

  gchar			*cpair_comment_single;

  GHashTable		*item_ht;

  gpointer               dummy;
};

struct _GtkItemFactoryEntry
{
  gchar *path;
  gchar *accelerator;

  GtkItemFactoryCallback callback;
  guint			 callback_action;

  /* possible values:
   * NULL		-> "<Item>"
   * ""			-> "<Item>"
   * "<Title>"		-> create a title item
   * "<Item>"		-> create a simple item
   * "<CheckItem>"	-> create a check item
   * "<ToggleItem>"	-> create a toggle item
   * "<RadioItem>"	-> create a radio item
   * <path>		-> path of a radio item to link against
   * "<Separator>"	-> create a separator
   * "<Branch>"		-> create an item to hold sub items
   * "<LastBranch>"	-> create a right justified item to hold sub items
   */
  gchar		 *item_type;
};

struct _GtkItemFactoryItem
{
  gchar *path;
  guint  accelerator_key;
  guint  accelerator_mods;
  guint	 modified : 1;
  guint  in_propagation : 1;
  gchar *dummy;

  GSList *widgets;
};


GtkType		gtk_item_factory_get_type	    (void);

/* `container_type' must be of GTK_TYPE_MENU_BAR, GTK_TYPE_MENU,
 * or GTK_TYPE_OPTION_MENU.
 */
GtkItemFactory*	gtk_item_factory_new	   (GtkType		 container_type,
					    const gchar		*path,
					    GtkAccelGroup       *accel_group);
void		gtk_item_factory_construct (GtkItemFactory	*ifactory,
					    GtkType		 container_type,
					    const gchar		*path,
					    GtkAccelGroup       *accel_group);
     
/* These functions operate on GtkItemFactoryClass basis.
 */
void		gtk_item_factory_parse_rc	    (const gchar    *file_name);
void		gtk_item_factory_parse_rc_string    (const gchar    *rc_string);
void		gtk_item_factory_parse_rc_scanner   (GScanner	    *scanner);
void		gtk_item_factory_add_foreign        (GtkWidget	    *accel_widget,
						     const gchar    *full_path,
						     GtkAccelGroup  *accel_group,
						     guint	     keyval,
						     GdkModifierType modifiers);
     
GtkItemFactory*	gtk_item_factory_from_widget	    (GtkWidget	      *widget);
gchar*		gtk_item_factory_path_from_widget   (GtkWidget	      *widget);

GtkWidget*	gtk_item_factory_get_item	      (GtkItemFactory *ifactory,
						       const gchar    *path);
GtkWidget*	gtk_item_factory_get_widget	      (GtkItemFactory *ifactory,
						       const gchar    *path);
GtkWidget*	gtk_item_factory_get_widget_by_action (GtkItemFactory *ifactory,
						       guint	       action);
GtkWidget*	gtk_item_factory_get_item_by_action   (GtkItemFactory *ifactory,
						       guint	       action);

/* If `path_pspec' is passed as `NULL', this function will iterate over
 * all hash entries. otherwise only those entries will be dumped for which
 * the pattern matches, e.g. "<Image>*...".
 */
void	gtk_item_factory_dump_items	(GtkPatternSpec		*path_pspec,
					 gboolean		 modified_only,
					 GtkPrintFunc		 print_func,
					 gpointer		 func_data);
void	gtk_item_factory_dump_rc	(const gchar		*file_name,
					 GtkPatternSpec		*path_pspec,
					 gboolean		 modified_only);
void	gtk_item_factory_print_func	(gpointer		 FILE_pointer,
					 gchar			*string);
void	gtk_item_factory_create_item	(GtkItemFactory		*ifactory,
					 GtkItemFactoryEntry	*entry,
					 gpointer		 callback_data,
					 guint			 callback_type);
void	gtk_item_factory_create_items	(GtkItemFactory		*ifactory,
					 guint			 n_entries,
					 GtkItemFactoryEntry	*entries,
					 gpointer		 callback_data);
void	gtk_item_factory_delete_item	(GtkItemFactory		*ifactory,
					 const gchar		*path);
void	gtk_item_factory_delete_entry	(GtkItemFactory		*ifactory,
					 GtkItemFactoryEntry	*entry);
void	gtk_item_factory_delete_entries	(GtkItemFactory		*ifactory,
					 guint			 n_entries,
					 GtkItemFactoryEntry	*entries);
void	gtk_item_factory_popup		(GtkItemFactory		*ifactory,
					 guint			 x,
					 guint			 y,
					 guint			 mouse_button,
					 guint32		 time);
void	gtk_item_factory_popup_with_data(GtkItemFactory		*ifactory,
					 gpointer		 popup_data,
					 GtkDestroyNotify	 destroy,
					 guint			 x,
					 guint			 y,
					 guint			 mouse_button,
					 guint32		 time);
gpointer gtk_item_factory_popup_data	(GtkItemFactory		*ifactory);
gpointer gtk_item_factory_popup_data_from_widget (GtkWidget	*widget);
void   gtk_item_factory_set_translate_func (GtkItemFactory      *ifactory,
					    GtkTranslateFunc     func,
					    gpointer             data,
					    GtkDestroyNotify     notify);

/* Compatibility functions for deprecated GtkMenuFactory code
 */
GtkItemFactory*	gtk_item_factory_from_path   (const gchar       *path);
void	gtk_item_factory_create_menu_entries (guint		 n_entries,
					      GtkMenuEntry      *entries);
void	gtk_item_factories_path_delete	   (const gchar		*ifactory_path,
					    const gchar		*path);
typedef	void	(*GtkItemFactoryCallback2) (GtkWidget		*widget,
					    gpointer		 callback_data,
					    guint		 callback_action);
void	gtk_item_factory_create_items_ac (GtkItemFactory	*ifactory,
					  guint			 n_entries,
					  GtkItemFactoryEntry	*entries,
					  gpointer		 callback_data,
					  guint			 callback_type);



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif	/* __GTK_ITEM_FACTORY_H__ */
