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

#include	"gtkitemfactory.h"
#include	"gtk/gtksignal.h"
#include	"gtk/gtkoptionmenu.h"
#include	"gtk/gtkmenubar.h"
#include	"gtk/gtkmenu.h"
#include	"gtk/gtkmenuitem.h"
#include	"gtk/gtkradiomenuitem.h"
#include	"gtk/gtkcheckmenuitem.h"
#include	"gtk/gtktearoffmenuitem.h"
#include	"gtk/gtkaccellabel.h"
#include        "gdk/gdkkeysyms.h"
#include	<string.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<stdio.h>



/* --- defines --- */
#define		ITEM_FACTORY_STRING	((gchar*) item_factory_string)
#define		ITEM_BLOCK_SIZE		(128)


/* --- structures --- */
typedef struct	_GtkIFCBData		GtkIFCBData;
typedef struct  _GtkIFDumpData		GtkIFDumpData;
struct _GtkIFCBData
{
  GtkItemFactoryCallback  func;
  guint			  callback_type;
  gpointer		  func_data;
  guint			  callback_action;
};
struct _GtkIFDumpData
{
  GtkPrintFunc	         print_func;
  gpointer	         func_data;
  guint		         modified_only : 1;
  GtkPatternSpec	*pspec;
};


/* --- prototypes --- */
static void	gtk_item_factory_class_init		(GtkItemFactoryClass  *klass);
static void	gtk_item_factory_init			(GtkItemFactory	      *ifactory);
static void	gtk_item_factory_destroy		(GtkObject	      *object);
static void	gtk_item_factory_finalize		(GtkObject	      *object);


/* --- static variables --- */
static GtkItemFactoryClass *gtk_item_factory_class = NULL;
static GtkObjectClass	*parent_class = NULL;
static const gchar	*item_factory_string = "Gtk-<ItemFactory>";
static GMemChunk	*ifactory_item_chunks = NULL;
static GMemChunk	*ifactory_cb_data_chunks = NULL;
static GQuark		 quark_popup_data = 0;
static GQuark		 quark_if_menu_pos = 0;
static GQuark		 quark_item_factory = 0;
static GQuark		 quark_item_path = 0;
static GQuark		 quark_action = 0;
static GQuark		 quark_accel_group = 0;
static GQuark		 quark_type_item = 0;
static GQuark		 quark_type_title = 0;
static GQuark		 quark_type_radio_item = 0;
static GQuark		 quark_type_check_item = 0;
static GQuark		 quark_type_toggle_item = 0;
static GQuark		 quark_type_tearoff_item = 0;
static GQuark		 quark_type_separator_item = 0;
static GQuark		 quark_type_branch = 0;
static GQuark		 quark_type_last_branch = 0;
static	GScannerConfig	ifactory_scanner_config =
{
  (
   " \t\n"
   )			/* cset_skip_characters */,
  (
   G_CSET_a_2_z
   "_"
   G_CSET_A_2_Z
   )			/* cset_identifier_first */,
  (
   G_CSET_a_2_z
   "-+_0123456789"
   G_CSET_A_2_Z
   G_CSET_LATINS
   G_CSET_LATINC
   )			/* cset_identifier_nth */,
  ( ";\n" )		/* cpair_comment_single */,
  
  FALSE			/* case_sensitive */,
  
  TRUE			/* skip_comment_multi */,
  TRUE			/* skip_comment_single */,
  FALSE			/* scan_comment_multi */,
  TRUE			/* scan_identifier */,
  FALSE			/* scan_identifier_1char */,
  FALSE			/* scan_identifier_NULL */,
  TRUE			/* scan_symbols */,
  TRUE			/* scan_binary */,
  TRUE			/* scan_octal */,
  TRUE			/* scan_float */,
  TRUE			/* scan_hex */,
  FALSE			/* scan_hex_dollar */,
  TRUE			/* scan_string_sq */,
  TRUE			/* scan_string_dq */,
  TRUE			/* numbers_2_int */,
  FALSE			/* int_2_float */,
  FALSE			/* identifier_2_string */,
  TRUE			/* char_2_token */,
  FALSE			/* symbol_2_token */,
};


/* --- functions --- */
GtkType
gtk_item_factory_get_type (void)
{
  static GtkType item_factory_type = 0;
  
  if (!item_factory_type)
    {
      static const GtkTypeInfo item_factory_info =
      {
	"GtkItemFactory",
	sizeof (GtkItemFactory),
	sizeof (GtkItemFactoryClass),
	(GtkClassInitFunc) gtk_item_factory_class_init,
	(GtkObjectInitFunc) gtk_item_factory_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      item_factory_type = gtk_type_unique (GTK_TYPE_OBJECT, &item_factory_info);
    }
  
  return item_factory_type;
}

static void
gtk_item_factory_class_init (GtkItemFactoryClass  *class)
{
  GtkObjectClass *object_class;

  gtk_item_factory_class = class;

  parent_class = gtk_type_class (GTK_TYPE_OBJECT);

  object_class = (GtkObjectClass*) class;

  object_class->destroy = gtk_item_factory_destroy;
  object_class->finalize = gtk_item_factory_finalize;

  class->cpair_comment_single = g_strdup (";\n");

  class->item_ht = g_hash_table_new (g_str_hash, g_str_equal);
  class->dummy = NULL;
  ifactory_item_chunks =
    g_mem_chunk_new ("GtkItemFactoryItem",
		     sizeof (GtkItemFactoryItem),
		     sizeof (GtkItemFactoryItem) * ITEM_BLOCK_SIZE,
		     G_ALLOC_ONLY);
  ifactory_cb_data_chunks =
    g_mem_chunk_new ("GtkIFCBData",
		     sizeof (GtkIFCBData),
		     sizeof (GtkIFCBData) * ITEM_BLOCK_SIZE,
		     G_ALLOC_AND_FREE);

  quark_popup_data		= g_quark_from_static_string ("GtkItemFactory-popup-data");
  quark_if_menu_pos		= g_quark_from_static_string ("GtkItemFactory-menu-position");
  quark_item_factory		= g_quark_from_static_string ("GtkItemFactory");
  quark_item_path		= g_quark_from_static_string ("GtkItemFactory-path");
  quark_action			= g_quark_from_static_string ("GtkItemFactory-action");
  quark_accel_group		= g_quark_from_static_string ("GtkAccelGroup");
  quark_type_item		= g_quark_from_static_string ("<Item>");
  quark_type_title		= g_quark_from_static_string ("<Title>");
  quark_type_radio_item		= g_quark_from_static_string ("<RadioItem>");
  quark_type_check_item		= g_quark_from_static_string ("<CheckItem>");
  quark_type_toggle_item	= g_quark_from_static_string ("<ToggleItem>");
  quark_type_tearoff_item	= g_quark_from_static_string ("<Tearoff>");
  quark_type_separator_item	= g_quark_from_static_string ("<Separator>");
  quark_type_branch		= g_quark_from_static_string ("<Branch>");
  quark_type_last_branch	= g_quark_from_static_string ("<LastBranch>");
}

static void
gtk_item_factory_init (GtkItemFactory	    *ifactory)
{
  GtkObject *object;

  object = GTK_OBJECT (ifactory);

  ifactory->path = NULL;
  ifactory->accel_group = NULL;
  ifactory->widget = NULL;
  ifactory->items = NULL;
  ifactory->translate_func = NULL;
  ifactory->translate_data = NULL;
  ifactory->translate_notify = NULL;
}

GtkItemFactory*
gtk_item_factory_new (GtkType	     container_type,
		      const gchar   *path,
		      GtkAccelGroup *accel_group)
{
  GtkItemFactory *ifactory;

  g_return_val_if_fail (path != NULL, NULL);

  ifactory = gtk_type_new (GTK_TYPE_ITEM_FACTORY);
  gtk_item_factory_construct (ifactory, container_type, path, accel_group);

  return ifactory;
}

static void
gtk_item_factory_callback_marshal (GtkWidget *widget,
				   gpointer   func_data)
{
  GtkIFCBData *data;

  data = func_data;

  if (data->callback_type == 1)
    {
      GtkItemFactoryCallback1 func1 = data->func;
      func1 (data->func_data, data->callback_action, widget);
    }
  else if (data->callback_type == 2)
    {
      GtkItemFactoryCallback2 func2 = data->func;
      func2 (widget, data->func_data, data->callback_action);
    }
}

static void
gtk_item_factory_propagate_accelerator (GtkItemFactoryItem *item,
					GtkWidget          *exclude)
{
  GSList *widget_list;
  GSList *slist;
  
  if (item->in_propagation)
    return;
  
  item->in_propagation = TRUE;
  
  widget_list = NULL;
  for (slist = item->widgets; slist; slist = slist->next)
    {
      GtkWidget *widget;
      
      widget = slist->data;
      
      if (widget != exclude)
	{
	  gtk_widget_ref (widget);
	  widget_list = g_slist_prepend (widget_list, widget);
	}
    }
  
  for (slist = widget_list; slist; slist = slist->next)
    {
      GtkWidget *widget;
      GtkAccelGroup *accel_group;
      guint signal_id;
      
      widget = slist->data;
      
      accel_group = gtk_object_get_data_by_id (GTK_OBJECT (widget), quark_accel_group);
      
      signal_id = gtk_signal_lookup ("activate", GTK_OBJECT_TYPE (widget));
      if (signal_id && accel_group)
	{
	  if (item->accelerator_key)
	    gtk_widget_add_accelerator (widget,
					"activate",
					accel_group,
					item->accelerator_key,
					item->accelerator_mods,
					GTK_ACCEL_VISIBLE);
	  else
	    {
	      GSList *work;
	      
	      work = gtk_accel_group_entries_from_object (GTK_OBJECT (widget));
	      while (work)
		{
		  GtkAccelEntry *ac_entry;
		  
		  ac_entry = work->data;
		  work = work->next;
		  if (ac_entry->accel_flags & GTK_ACCEL_VISIBLE &&
		      ac_entry->accel_group == accel_group &&
		      ac_entry->signal_id == signal_id)
		    gtk_widget_remove_accelerator (GTK_WIDGET (widget),
						   ac_entry->accel_group,
						   ac_entry->accelerator_key,
						   ac_entry->accelerator_mods);
		}
	    }
	}

      gtk_widget_unref (widget);
    }
  
  g_slist_free (widget_list);
  
  item->in_propagation = FALSE;
}

static gint
gtk_item_factory_item_add_accelerator (GtkWidget	  *widget,
				       guint               accel_signal_id,
				       GtkAccelGroup      *accel_group,
				       guint               accel_key,
				       guint               accel_mods,
				       GtkAccelFlags       accel_flags,
				       GtkItemFactoryItem *item)
{
  if (!item->in_propagation &&
      g_slist_find (item->widgets, widget) &&
      accel_signal_id == gtk_signal_lookup ("activate", GTK_OBJECT_TYPE (widget)))
    {
      item->accelerator_key = accel_key;
      item->accelerator_mods = accel_mods;
      item->modified = TRUE;
      
      gtk_item_factory_propagate_accelerator (item, widget);
    }

  return TRUE;
}

static void
gtk_item_factory_item_remove_accelerator (GtkWidget	     *widget,
					  GtkAccelGroup      *accel_group,
					  guint               accel_key,
					  guint               accel_mods,
					  GtkItemFactoryItem *item)
{
  if (!item->in_propagation &&
      g_slist_find (item->widgets, widget) &&
      item->accelerator_key == accel_key &&
      item->accelerator_mods == accel_mods)
    {
      item->accelerator_key = 0;
      item->accelerator_mods = 0;
      item->modified = TRUE;
      
      gtk_item_factory_propagate_accelerator (item, widget);
    }
}

static void
gtk_item_factory_item_remove_widget (GtkWidget		*widget,
				     GtkItemFactoryItem *item)
{
  item->widgets = g_slist_remove (item->widgets, widget);
  gtk_object_remove_data_by_id (GTK_OBJECT (widget), quark_item_factory);
  gtk_object_remove_data_by_id (GTK_OBJECT (widget), quark_item_path);
}

void
gtk_item_factory_add_foreign (GtkWidget      *accel_widget,
			      const gchar    *full_path,
			      GtkAccelGroup  *accel_group,
			      guint           keyval,
			      GdkModifierType modifiers)
{
  GtkItemFactoryClass *class;
  GtkItemFactoryItem *item;

  g_return_if_fail (GTK_IS_WIDGET (accel_widget));
  g_return_if_fail (full_path != NULL);

  class = gtk_type_class (GTK_TYPE_ITEM_FACTORY);

  keyval = keyval != GDK_VoidSymbol ? keyval : 0;

  item = g_hash_table_lookup (class->item_ht, full_path);
  if (!item)
    {
      item = g_chunk_new (GtkItemFactoryItem, ifactory_item_chunks);

      item->path = g_strdup (full_path);
      item->accelerator_key = keyval;
      item->accelerator_mods = modifiers;
      item->modified = FALSE;
      item->in_propagation = FALSE;
      item->dummy = NULL;
      item->widgets = NULL;
      
      g_hash_table_insert (class->item_ht, item->path, item);
    }

  item->widgets = g_slist_prepend (item->widgets, accel_widget);
  gtk_signal_connect (GTK_OBJECT (accel_widget),
		      "destroy",
		      GTK_SIGNAL_FUNC (gtk_item_factory_item_remove_widget),
		      item);

  /* set the item path for the widget
   */
  gtk_object_set_data_by_id (GTK_OBJECT (accel_widget), quark_item_path, item->path);
  gtk_widget_set_name (accel_widget, item->path);
  if (accel_group)
    {
      gtk_accel_group_ref (accel_group);
      gtk_object_set_data_by_id_full (GTK_OBJECT (accel_widget),
				      quark_accel_group,
				      accel_group,
				      (GtkDestroyNotify) gtk_accel_group_unref);
    }
  else
    gtk_object_set_data_by_id (GTK_OBJECT (accel_widget), quark_accel_group, NULL);

  /* install defined accelerators
   */
  if (gtk_signal_lookup ("activate", GTK_OBJECT_TYPE (accel_widget)))
    {
      if (item->accelerator_key && accel_group)
	gtk_widget_add_accelerator (accel_widget,
				    "activate",
				    accel_group,
				    item->accelerator_key,
				    item->accelerator_mods,
				    GTK_ACCEL_VISIBLE);
      else
	gtk_widget_remove_accelerators (accel_widget,
					"activate",
					TRUE);
    }

  /* keep track of accelerator changes
   */
  gtk_signal_connect_after (GTK_OBJECT (accel_widget),
			    "add-accelerator",
			    GTK_SIGNAL_FUNC (gtk_item_factory_item_add_accelerator),
			    item);
  gtk_signal_connect_after (GTK_OBJECT (accel_widget),
			    "remove-accelerator",
			    GTK_SIGNAL_FUNC (gtk_item_factory_item_remove_accelerator),
			    item);
}

static void
ifactory_cb_data_free (gpointer mem)
{
  g_mem_chunk_free (ifactory_cb_data_chunks, mem);
}

static void
gtk_item_factory_add_item (GtkItemFactory		*ifactory,
			   const gchar			*path,
			   const gchar			*accelerator,
			   GtkItemFactoryCallback	callback,
			   guint			callback_action,
			   gpointer			callback_data,
			   guint			callback_type,
			   gchar			*item_type,
			   GtkWidget			*widget)
{
  GtkItemFactoryClass *class;
  GtkItemFactoryItem *item;
  gchar *fpath;
  guint keyval, mods;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (item_type != NULL);

  class = GTK_ITEM_FACTORY_CLASS (GTK_OBJECT (ifactory)->klass);

  /* set accelerator group on menu widgets
   */
  if (GTK_IS_MENU (widget))
    gtk_menu_set_accel_group ((GtkMenu*) widget, ifactory->accel_group);

  /* connect callback if neccessary
   */
  if (callback)
    {
      GtkIFCBData *data;

      data = g_chunk_new (GtkIFCBData, ifactory_cb_data_chunks);
      data->func = callback;
      data->callback_type = callback_type;
      data->func_data = callback_data;
      data->callback_action = callback_action;

      gtk_object_weakref (GTK_OBJECT (widget),
			  ifactory_cb_data_free,
			  data);
      gtk_signal_connect (GTK_OBJECT (widget),
			  "activate",
			  GTK_SIGNAL_FUNC (gtk_item_factory_callback_marshal),
			  data);
    }

  /* link the widget into its item-entry
   * and keep back pointer on both the item factory and the widget
   */
  gtk_object_set_data_by_id (GTK_OBJECT (widget), quark_action, GUINT_TO_POINTER (callback_action));
  gtk_object_set_data_by_id (GTK_OBJECT (widget), quark_item_factory, ifactory);
  if (accelerator)
    gtk_accelerator_parse (accelerator, &keyval, &mods);
  else
    {
      keyval = 0;
      mods = 0;
    }
  fpath = g_strconcat (ifactory->path, path, NULL);
  gtk_item_factory_add_foreign (widget, fpath, ifactory->accel_group, keyval, mods);
  item = g_hash_table_lookup (class->item_ht, fpath);
  g_free (fpath);

  g_return_if_fail (item != NULL);

  if (!g_slist_find (ifactory->items, item))
    ifactory->items = g_slist_prepend (ifactory->items, item);
}

void
gtk_item_factory_construct (GtkItemFactory	*ifactory,
			    GtkType		 container_type,
			    const gchar		*path,
			    GtkAccelGroup	*accel_group)
{
  guint len;

  g_return_if_fail (ifactory != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  g_return_if_fail (ifactory->accel_group == NULL);
  g_return_if_fail (path != NULL);
  if (!gtk_type_is_a (container_type, GTK_TYPE_OPTION_MENU))
    g_return_if_fail (gtk_type_is_a (container_type, GTK_TYPE_MENU_SHELL));

  len = strlen (path);

  if (path[0] != '<' && path[len - 1] != '>')
    {
      g_warning ("GtkItemFactory: invalid factory path `%s'", path);
      return;
    }

  if (accel_group)
    {
      ifactory->accel_group = accel_group;
      gtk_accel_group_ref (ifactory->accel_group);
    }
  else
    ifactory->accel_group = gtk_accel_group_new ();

  ifactory->path = g_strdup (path);
  ifactory->widget =
    gtk_widget_new (container_type,
		    "GtkObject::signal::destroy", gtk_widget_destroyed, &ifactory->widget,
		    NULL);
  gtk_object_ref (GTK_OBJECT (ifactory));
  gtk_object_sink (GTK_OBJECT (ifactory));

  gtk_item_factory_add_item (ifactory,
			     "", NULL,
			     NULL, 0, NULL, 0,
			     ITEM_FACTORY_STRING,
			     ifactory->widget);
}

GtkItemFactory*
gtk_item_factory_from_path (const gchar      *path)
{
  GtkItemFactoryClass *class;
  GtkItemFactoryItem *item;
  gchar *fname;
  guint i;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (path[0] == '<', NULL);

  class = gtk_type_class (GTK_TYPE_ITEM_FACTORY);

  i = 0;
  while (path[i] && path[i] != '>')
    i++;
  if (path[i] != '>')
    {
      g_warning ("gtk_item_factory_from_path(): invalid factory path \"%s\"",
		 path);
      return NULL;
    }
  fname = g_new (gchar, i + 2);
  g_memmove (fname, path, i + 1);
  fname[i + 1] = 0;

  item = g_hash_table_lookup (class->item_ht, fname);

  g_free (fname);

  if (item && item->widgets)
    return gtk_item_factory_from_widget (item->widgets->data);

  return NULL;
}

static void
gtk_item_factory_destroy (GtkObject *object)
{
  GtkItemFactory *ifactory;
  GSList *slist;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (object));

  ifactory = (GtkItemFactory*) object;

  if (ifactory->widget)
    {
      GtkObject *dobj;

      dobj = GTK_OBJECT (ifactory->widget);

      gtk_object_ref (dobj);
      gtk_object_sink (dobj);
      gtk_object_destroy (dobj);
      gtk_object_unref (dobj);

      ifactory->widget = NULL;
    }

  for (slist = ifactory->items; slist; slist = slist->next)
    {
      GtkItemFactoryItem *item = slist->data;
      GSList *link;
      
      for (link = item->widgets; link; link = link->next)
	if (gtk_object_get_data_by_id (link->data, quark_item_factory) == ifactory)
	  gtk_object_remove_data_by_id (link->data, quark_item_factory);
    }
  g_slist_free (ifactory->items);
  ifactory->items = NULL;

  parent_class->destroy (object);
}

static void
gtk_item_factory_finalize (GtkObject		  *object)
{
  GtkItemFactory *ifactory;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (object));

  ifactory = GTK_ITEM_FACTORY (object);

  gtk_accel_group_unref (ifactory->accel_group);
  g_free (ifactory->path);
  g_assert (ifactory->widget == NULL);

  if (ifactory->translate_notify)
    ifactory->translate_notify (ifactory->translate_data);
  
  parent_class->finalize (object);
}

GtkItemFactory*
gtk_item_factory_from_widget (GtkWidget	       *widget)
{
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gtk_object_get_data_by_id (GTK_OBJECT (widget), quark_item_factory);
}

gchar*
gtk_item_factory_path_from_widget (GtkWidget	    *widget)
{
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gtk_object_get_data_by_id (GTK_OBJECT (widget), quark_item_path);
}

static void
gtk_item_factory_foreach (gpointer hash_key,
			  gpointer value,
			  gpointer user_data)
{
  GtkItemFactoryItem *item;
  GtkIFDumpData *data;
  gchar *string;
  gchar *name;
  gchar comment_prefix[2] = "\000\000";

  item = value;
  data = user_data;

  if (data->pspec && !gtk_pattern_match_string (data->pspec, item->path))
    return;

  comment_prefix[0] = gtk_item_factory_class->cpair_comment_single[0];

  name = gtk_accelerator_name (item->accelerator_key, item->accelerator_mods);
  string = g_strconcat (item->modified ? "" : comment_prefix,
			"(menu-path \"",
			hash_key,
			"\" \"",
			name,
			"\")",
			NULL);
  g_free (name);

  data->print_func (data->func_data, string);

  g_free (string);
}

void
gtk_item_factory_dump_items (GtkPatternSpec	 *path_pspec,
			     gboolean		  modified_only,
			     GtkPrintFunc	  print_func,
			     gpointer		  func_data)
{
  GtkIFDumpData data;

  g_return_if_fail (print_func != NULL);

  if (!gtk_item_factory_class)
    gtk_type_class (GTK_TYPE_ITEM_FACTORY);

  data.print_func = print_func;
  data.func_data = func_data;
  data.modified_only = (modified_only != FALSE);
  data.pspec = path_pspec;

  g_hash_table_foreach (gtk_item_factory_class->item_ht, gtk_item_factory_foreach, &data);
}

void
gtk_item_factory_print_func (gpointer FILE_pointer,
			     gchar   *string)
{
  FILE *f_out = FILE_pointer;

  g_return_if_fail (FILE_pointer != NULL);
  g_return_if_fail (string != NULL);
  
  fputs (string, f_out);
  fputc ('\n', f_out);
}

void
gtk_item_factory_dump_rc (const gchar            *file_name,
			  GtkPatternSpec         *path_pspec,
			  gboolean                modified_only)
{
  FILE *f_out;

  g_return_if_fail (file_name != NULL);

  f_out = fopen (file_name, "w");
  if (!f_out)
    return;

  fputs ("; ", f_out);
  if (g_get_prgname ())
    fputs (g_get_prgname (), f_out);
  fputs (" GtkItemFactory rc-file         -*- scheme -*-\n", f_out);
  fputs ("; this file is an automated menu-path dump\n", f_out);
  fputs (";\n", f_out);

  gtk_item_factory_dump_items (path_pspec,
			       modified_only,
			       gtk_item_factory_print_func,
			       f_out);

  fclose (f_out);
}

void
gtk_item_factory_create_items (GtkItemFactory	   *ifactory,
			       guint		    n_entries,
			       GtkItemFactoryEntry *entries,
			       gpointer		    callback_data)
{
  gtk_item_factory_create_items_ac (ifactory, n_entries, entries, callback_data, 1);
}

void
gtk_item_factory_create_items_ac (GtkItemFactory      *ifactory,
				  guint		       n_entries,
				  GtkItemFactoryEntry *entries,
				  gpointer	       callback_data,
				  guint		       callback_type)
{
  guint i;

  g_return_if_fail (ifactory != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  g_return_if_fail (callback_type >= 1 && callback_type <= 2);

  if (n_entries == 0)
    return;

  g_return_if_fail (entries != NULL);

  for (i = 0; i < n_entries; i++)
    gtk_item_factory_create_item (ifactory, entries + i, callback_data, callback_type);
}

GtkWidget*
gtk_item_factory_get_widget (GtkItemFactory *ifactory,
			     const gchar    *path)
{
  GtkItemFactoryClass *class;
  GtkItemFactoryItem *item;

  g_return_val_if_fail (GTK_IS_ITEM_FACTORY (ifactory), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  class = GTK_ITEM_FACTORY_CLASS (GTK_OBJECT (ifactory)->klass);

  if (path[0] == '<')
    item = g_hash_table_lookup (class->item_ht, (gpointer) path);
  else
    {
      gchar *fpath;

      fpath = g_strconcat (ifactory->path, path, NULL);
      item = g_hash_table_lookup (class->item_ht, fpath);
      g_free (fpath);
    }

  if (item)
    {
      GSList *slist;

      for (slist = item->widgets; slist; slist = slist->next)
	{
	  if (gtk_item_factory_from_widget (slist->data) == ifactory)
	    return slist->data;
	}
    }

  return NULL;
}

GtkWidget*
gtk_item_factory_get_widget_by_action (GtkItemFactory *ifactory,
				       guint	       action)
{
  GSList *slist;

  g_return_val_if_fail (GTK_IS_ITEM_FACTORY (ifactory), NULL);

  for (slist = ifactory->items; slist; slist = slist->next)
    {
      GtkItemFactoryItem *item = slist->data;
      GSList *link;

      for (link = item->widgets; link; link = link->next)
	if (gtk_object_get_data_by_id (link->data, quark_item_factory) == ifactory &&
	    gtk_object_get_data_by_id (link->data, quark_action) == GUINT_TO_POINTER (action))
	  return link->data;
    }

  return NULL;
}

GtkWidget*
gtk_item_factory_get_item (GtkItemFactory *ifactory,
			   const gchar    *path)
{
  GtkWidget *widget;

  g_return_val_if_fail (GTK_IS_ITEM_FACTORY (ifactory), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  widget = gtk_item_factory_get_widget (ifactory, path);

  if (GTK_IS_MENU (widget))
    widget = gtk_menu_get_attach_widget (GTK_MENU (widget));

  return GTK_IS_ITEM (widget) ? widget : NULL;
}

GtkWidget*
gtk_item_factory_get_item_by_action (GtkItemFactory *ifactory,
				     guint	     action)
{
  GtkWidget *widget;

  g_return_val_if_fail (GTK_IS_ITEM_FACTORY (ifactory), NULL);

  widget = gtk_item_factory_get_widget_by_action (ifactory, action);

  if (GTK_IS_MENU (widget))
    widget = gtk_menu_get_attach_widget (GTK_MENU (widget));

  return GTK_IS_ITEM (widget) ? widget : NULL;
}

static gboolean
gtk_item_factory_parse_path (GtkItemFactory *ifactory,
			     gchar          *str,
			     gchar         **path,
			     gchar         **parent_path,
			     gchar         **item)
{
  gchar *translation;
  gchar *p, *q;

  *path = g_strdup (str);

  p = q = *path;
  while (*p)
    {
      if (*p != '_')
	{
	  *q++ = *p;
	}
      p++;
    }
  *q = 0;

  *parent_path = g_strdup (*path);
  p = strrchr (*parent_path, '/');
  if (!p)
    {
      g_warning ("GtkItemFactory: invalid entry path `%s'", str);
      return FALSE;
    }
  *p = 0;

  if (ifactory->translate_func)
    translation = ifactory->translate_func (str, ifactory->translate_data);
  else
    translation = str;
			      
  p = strrchr (translation, '/');
  if (p)
    p++;
  else
    p = translation;

  *item = g_strdup (p);

  return TRUE;
}

void
gtk_item_factory_create_item (GtkItemFactory	     *ifactory,
			      GtkItemFactoryEntry    *entry,
			      gpointer		      callback_data,
			      guint		      callback_type)
{
  GtkOptionMenu *option_menu = NULL;
  GtkWidget *parent;
  GtkWidget *widget;
  GSList *radio_group;
  gchar *name;
  gchar *parent_path;
  gchar *path;
  guint accel_key;
  guint type_id;
  GtkType type;
  gchar *item_type_path;

  g_return_if_fail (ifactory != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  g_return_if_fail (entry != NULL);
  g_return_if_fail (entry->path != NULL);
  g_return_if_fail (entry->path[0] == '/');
  g_return_if_fail (callback_type >= 1 && callback_type <= 2);

  if (!entry->item_type ||
      entry->item_type[0] == 0)
    {
      item_type_path = "<Item>";
      type_id = quark_type_item;
    }
  else
    {
      item_type_path = entry->item_type;
      type_id = gtk_object_data_try_key (item_type_path);
    }

  radio_group = NULL;
  if (type_id == quark_type_item)
    type = GTK_TYPE_MENU_ITEM;
  else if (type_id == quark_type_title)
    type = GTK_TYPE_MENU_ITEM;
  else if (type_id == quark_type_radio_item)
    type = GTK_TYPE_RADIO_MENU_ITEM;
  else if (type_id == quark_type_check_item)
    type = GTK_TYPE_CHECK_MENU_ITEM;
  else if (type_id == quark_type_tearoff_item)
    type = GTK_TYPE_TEAROFF_MENU_ITEM;
  else if (type_id == quark_type_toggle_item)
    type = GTK_TYPE_CHECK_MENU_ITEM;
  else if (type_id == quark_type_separator_item)
    type = GTK_TYPE_MENU_ITEM;
  else if (type_id == quark_type_branch)
    type = GTK_TYPE_MENU_ITEM;
  else if (type_id == quark_type_last_branch)
    type = GTK_TYPE_MENU_ITEM;
  else
    {
      GtkWidget *radio_link;

      radio_link = gtk_item_factory_get_widget (ifactory, item_type_path);
      if (radio_link && GTK_IS_RADIO_MENU_ITEM (radio_link))
	{
	  type = GTK_TYPE_RADIO_MENU_ITEM;
	  radio_group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (radio_link));
	}
      else
	{
	  g_warning ("GtkItemFactory: entry path `%s' has invalid type `%s'",
		     entry->path,
		     item_type_path);
	  return;
	}
    }

  if (!gtk_item_factory_parse_path (ifactory, entry->path, 
				    &path, &parent_path, &name))
    return;

  parent = gtk_item_factory_get_widget (ifactory, parent_path);
  if (!parent)
    {
      GtkItemFactoryEntry pentry;
      gchar *ppath, *p;

      ppath = g_strdup (entry->path);
      p = strrchr (ppath, '/');
      g_return_if_fail (p != NULL);
      *p = 0;
      pentry.path = ppath;
      pentry.accelerator = NULL;
      pentry.callback = NULL;
      pentry.callback_action = 0;
      pentry.item_type = "<Branch>";

      gtk_item_factory_create_item (ifactory, &pentry, NULL, 1);
      g_free (ppath);

      parent = gtk_item_factory_get_widget (ifactory, parent_path);
      g_return_if_fail (parent != NULL);
    }
  g_free (parent_path);

  if (GTK_IS_OPTION_MENU (parent))
    {
      option_menu = GTK_OPTION_MENU (parent);
      if (!option_menu->menu)
	gtk_option_menu_set_menu (option_menu, gtk_widget_new (GTK_TYPE_MENU, NULL));
      parent = option_menu->menu;
    }
			      
  g_return_if_fail (GTK_IS_CONTAINER (parent));

  widget = gtk_widget_new (type,
			   "GtkWidget::visible", TRUE,
			   "GtkWidget::sensitive", (type_id != quark_type_separator_item &&
						    type_id != quark_type_title),
			   "GtkWidget::parent", parent,
			   NULL);
  if (option_menu && !option_menu->menu_item)
    gtk_option_menu_set_history (option_menu, 0);

  if (type == GTK_TYPE_RADIO_MENU_ITEM)
    gtk_radio_menu_item_set_group (GTK_RADIO_MENU_ITEM (widget), radio_group);
  if (GTK_IS_CHECK_MENU_ITEM (widget))
    gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (widget), TRUE);

  /* install underline accelerators for this item
   */
  if (type_id != quark_type_separator_item && 
      type_id != quark_type_tearoff_item &&
      *name)
    {
      GtkWidget *label;
      
      label = gtk_widget_new (GTK_TYPE_ACCEL_LABEL,
			      "GtkWidget::visible", TRUE,
			      "GtkWidget::parent", widget,
			      "GtkAccelLabel::accel_widget", widget,
			      "GtkMisc::xalign", 0.0,
			      NULL);
      
      accel_key = gtk_label_parse_uline (GTK_LABEL (label), name);
      
      if (accel_key != GDK_VoidSymbol)
	{
	  if (GTK_IS_MENU_BAR (parent))
	    gtk_widget_add_accelerator (widget,
					"activate_item",
					ifactory->accel_group,
					accel_key, GDK_MOD1_MASK,
					GTK_ACCEL_LOCKED);

	  if (GTK_IS_MENU (parent))
	    gtk_widget_add_accelerator (widget,
					"activate_item",
					gtk_menu_ensure_uline_accel_group (GTK_MENU (parent)),
					accel_key, 0,
					GTK_ACCEL_LOCKED);
	}
    }
  
  g_free (name);
  
  if (type_id == quark_type_branch ||
      type_id == quark_type_last_branch)
    {
      if (entry->callback)
	g_warning ("gtk_item_factory_create_item(): Can't specify a callback on a branch: \"%s\"",
		   entry->path);
      
      if (type_id == quark_type_last_branch)
	gtk_menu_item_right_justify (GTK_MENU_ITEM (widget));
      
      parent = widget;
      widget = gtk_widget_new (GTK_TYPE_MENU,
			       NULL);
      
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (parent), widget);
    }	   
  
  gtk_item_factory_add_item (ifactory,
			     path, entry->accelerator,
			     (type_id == quark_type_branch ||
			      type_id == quark_type_last_branch) ?
			      (GtkItemFactoryCallback) NULL : entry->callback,
			     entry->callback_action, callback_data,
			     callback_type,
			     item_type_path,
			     widget);

  g_free (path);
}

void
gtk_item_factory_create_menu_entries (guint              n_entries,
				      GtkMenuEntry      *entries)
{
  static GtkPatternSpec pspec_separator = { 42, 0 };
  static GtkPatternSpec pspec_check = { 42, 0 };
  guint i;

  if (!n_entries)
    return;
  g_return_if_fail (entries != NULL);

  if (pspec_separator.pattern_length == 0)
    {
      gtk_pattern_spec_init (&pspec_separator, "*<separator>*");
      gtk_pattern_spec_init (&pspec_check, "*<check>*");
    }

  for (i = 0; i < n_entries; i++)
    {
      GtkItemFactory *ifactory;
      GtkItemFactoryEntry entry;
      gchar *path;
      gchar *cpath;

      path = entries[i].path;
      ifactory = gtk_item_factory_from_path (path);
      if (!ifactory)
	{
	  g_warning ("gtk_item_factory_create_menu_entries(): "
		     "entry[%u] refers to unknown item factory: \"%s\"",
		     i, entries[i].path);
	  continue;
	}

      while (*path != '>')
	path++;
      path++;
      cpath = NULL;

      entry.path = path;
      entry.accelerator = entries[i].accelerator;
      entry.callback = entries[i].callback;
      entry.callback_action = 0;
      if (gtk_pattern_match_string (&pspec_separator, path))
	entry.item_type = "<Separator>";
      else if (!gtk_pattern_match_string (&pspec_check, path))
	entry.item_type = NULL;
      else
	{
	  gboolean in_brace = FALSE;
	  gchar *c;
	  
	  cpath = g_new (gchar, strlen (path));
	  c = cpath;
	  while (*path != 0)
	    {
	      if (*path == '<')
		in_brace = TRUE;
	      else if (*path == '>')
		in_brace = FALSE;
	      else if (!in_brace)
		*(c++) = *path;
	      path++;
	    }
	  *c = 0;
	  entry.item_type = "<ToggleItem>";
	  entry.path = cpath;
	}
      
      gtk_item_factory_create_item (ifactory, &entry, entries[i].callback_data, 2);
      entries[i].widget = gtk_item_factory_get_widget (ifactory, entries[i].path);
      g_free (cpath);
    }
}

void
gtk_item_factories_path_delete (const gchar *ifactory_path,
				const gchar *path)
{
  GtkItemFactoryClass *class;
  GtkItemFactoryItem *item;

  g_return_if_fail (path != NULL);

  class = gtk_type_class (GTK_TYPE_ITEM_FACTORY);

  if (path[0] == '<')
    item = g_hash_table_lookup (class->item_ht, (gpointer) path);
  else
    {
      gchar *fpath;

      g_return_if_fail (ifactory_path != NULL);
      
      fpath = g_strconcat (ifactory_path, path, NULL);
      item = g_hash_table_lookup (class->item_ht, fpath);
      g_free (fpath);
    }
  
  if (item)
    {
      GSList *widget_list;
      GSList *slist;

      widget_list = NULL;
      for (slist = item->widgets; slist; slist = slist->next)
	{
	  GtkWidget *widget;

	  widget = slist->data;
	  widget_list = g_slist_prepend (widget_list, widget);
	  gtk_widget_ref (widget);
	}

      for (slist = widget_list; slist; slist = slist->next)
	{
	  GtkWidget *widget;

	  widget = slist->data;
	  gtk_widget_destroy (widget);
	  gtk_widget_unref (widget);
	}
      g_slist_free (widget_list);
    }
}

void
gtk_item_factory_delete_item (GtkItemFactory         *ifactory,
			      const gchar            *path)
{
  GtkItemFactoryClass *class;
  GtkWidget *widget;

  g_return_if_fail (ifactory != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  g_return_if_fail (path != NULL);

  class = GTK_ITEM_FACTORY_CLASS (GTK_OBJECT (ifactory)->klass);

  widget = gtk_item_factory_get_widget (ifactory, path);

  if (widget)
    {
      if (GTK_IS_MENU (widget))
	widget = gtk_menu_get_attach_widget (GTK_MENU (widget));

      gtk_widget_destroy (widget);
    }
}

void
gtk_item_factory_delete_entry (GtkItemFactory         *ifactory,
			       GtkItemFactoryEntry    *entry)
{
  g_return_if_fail (ifactory != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  g_return_if_fail (entry != NULL);

  gtk_item_factory_delete_item (ifactory, entry->path);
}

void
gtk_item_factory_delete_entries (GtkItemFactory         *ifactory,
				 guint                   n_entries,
				 GtkItemFactoryEntry    *entries)
{
  guint i;

  g_return_if_fail (ifactory != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  if (n_entries > 0)
    g_return_if_fail (entries != NULL);

  for (i = 0; i < n_entries; i++)
    gtk_item_factory_delete_item (ifactory, (entries + i)->path);
}

typedef struct
{
  guint x;
  guint y;
} MenuPos;

static void
gtk_item_factory_menu_pos (GtkMenu  *menu,
			   gint     *x,
			   gint     *y,
			   gpointer  func_data)
{
  MenuPos *mpos = func_data;

  *x = mpos->x;
  *y = mpos->y;
}

gpointer
gtk_item_factory_popup_data_from_widget (GtkWidget     *widget)
{
  GtkItemFactory *ifactory;
  
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  ifactory = gtk_item_factory_from_widget (widget);
  if (ifactory)
    return gtk_object_get_data_by_id (GTK_OBJECT (ifactory), quark_popup_data);

  return NULL;
}

gpointer
gtk_item_factory_popup_data (GtkItemFactory *ifactory)
{
  g_return_val_if_fail (ifactory != NULL, NULL);
  g_return_val_if_fail (GTK_IS_ITEM_FACTORY (ifactory), NULL);

  return gtk_object_get_data_by_id (GTK_OBJECT (ifactory), quark_popup_data);
}

static void
ifactory_delete_popup_data (GtkObject	   *object,
			    GtkItemFactory *ifactory)
{
  gtk_signal_disconnect_by_func (object,
				 GTK_SIGNAL_FUNC (ifactory_delete_popup_data),
				 ifactory);
  gtk_object_remove_data_by_id (GTK_OBJECT (ifactory), quark_popup_data);
}

void
gtk_item_factory_popup (GtkItemFactory		*ifactory,
			guint			 x,
			guint			 y,
			guint			 mouse_button,
			guint32			 time)
{
  gtk_item_factory_popup_with_data (ifactory, NULL, NULL, x, y, mouse_button, time);
}

void
gtk_item_factory_popup_with_data (GtkItemFactory	*ifactory,
				  gpointer		 popup_data,
				  GtkDestroyNotify	 destroy,
				  guint			 x,
				  guint			 y,
				  guint			 mouse_button,
				  guint32		 time)
{
  MenuPos *mpos;

  g_return_if_fail (ifactory != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  g_return_if_fail (GTK_IS_MENU (ifactory->widget));
  
  mpos = gtk_object_get_data_by_id (GTK_OBJECT (ifactory->widget), quark_if_menu_pos);
  
  if (!mpos)
    {
      mpos = g_new0 (MenuPos, 1);
      gtk_object_set_data_by_id_full (GTK_OBJECT (ifactory->widget),
				      quark_if_menu_pos,
				      mpos,
				      g_free);
    }
  
  mpos->x = x;
  mpos->y = y;
  
  if (popup_data != NULL)
    {
      gtk_object_set_data_by_id_full (GTK_OBJECT (ifactory),
				      quark_popup_data,
				      popup_data,
				      destroy);
      gtk_signal_connect (GTK_OBJECT (ifactory->widget),
			  "selection-done",
			  GTK_SIGNAL_FUNC (ifactory_delete_popup_data),
			  ifactory);
    }
  
  gtk_menu_popup (GTK_MENU (ifactory->widget),
		  NULL, NULL,
		  gtk_item_factory_menu_pos, mpos,
		  mouse_button, time);
}

static guint
gtk_item_factory_parse_menu_path (GScanner            *scanner,
				  GtkItemFactoryClass *class)
{
  GtkItemFactoryItem *item;
  
  g_scanner_get_next_token (scanner);
  if (scanner->token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  g_scanner_peek_next_token (scanner);
  if (scanner->next_token != G_TOKEN_STRING)
    {
      g_scanner_get_next_token (scanner);
      return G_TOKEN_STRING;
    }

  item = g_hash_table_lookup (class->item_ht, scanner->value.v_string);
  if (!item)
    {
      item = g_chunk_new (GtkItemFactoryItem, ifactory_item_chunks);

      item->path = g_strdup (scanner->value.v_string);
      item->accelerator_key = 0;
      item->accelerator_mods = 0;
      item->modified = TRUE;
      item->in_propagation = FALSE;
      item->dummy = NULL;
      item->widgets = NULL;

      g_hash_table_insert (class->item_ht, item->path, item);
    }
  g_scanner_get_next_token (scanner);
  
  if (!item->in_propagation)
    {
      guint old_keyval;
      guint old_mods;
      
      old_keyval = item->accelerator_key;
      old_mods = item->accelerator_mods;
      gtk_accelerator_parse (scanner->value.v_string,
			     &item->accelerator_key,
			     &item->accelerator_mods);
      if (old_keyval != item->accelerator_key ||
	  old_mods != item->accelerator_mods)
	{
	  item->modified = TRUE;
	  gtk_item_factory_propagate_accelerator (item, NULL);
	}
    }
  
  g_scanner_get_next_token (scanner);
  if (scanner->token != ')')
    return ')';
  else
    return G_TOKEN_NONE;
}

static void
gtk_item_factory_parse_statement (GScanner            *scanner,
				  GtkItemFactoryClass *class)
{
  guint expected_token;
  
  g_scanner_get_next_token (scanner);
  
  if (scanner->token == G_TOKEN_SYMBOL)
    {
      guint (*parser_func) (GScanner*, GtkItemFactoryClass*);

      parser_func = (guint (*) (GScanner *, GtkItemFactoryClass*)) scanner->value.v_symbol;

      /* check whether this is a GtkItemFactory symbol.
       */
      if (parser_func == gtk_item_factory_parse_menu_path)
	expected_token = parser_func (scanner, class);
      else
	expected_token = G_TOKEN_SYMBOL;
    }
  else
    expected_token = G_TOKEN_SYMBOL;

  /* skip rest of statement on errrors
   */
  if (expected_token != G_TOKEN_NONE)
    {
      register guint level;

      level = 1;
      if (scanner->token == ')')
	level--;
      if (scanner->token == '(')
	level++;
      
      while (!g_scanner_eof (scanner) && level > 0)
	{
	  g_scanner_get_next_token (scanner);
	  
	  if (scanner->token == '(')
	    level++;
	  else if (scanner->token == ')')
	    level--;
	}
    }
}

void
gtk_item_factory_parse_rc_string (const gchar	 *rc_string)
{
  GScanner *scanner;

  g_return_if_fail (rc_string != NULL);

  if (!gtk_item_factory_class)
    gtk_type_class (GTK_TYPE_ITEM_FACTORY);

  ifactory_scanner_config.cpair_comment_single = gtk_item_factory_class->cpair_comment_single;
  scanner = g_scanner_new (&ifactory_scanner_config);

  g_scanner_input_text (scanner, rc_string, strlen (rc_string));

  gtk_item_factory_parse_rc_scanner (scanner);

  g_scanner_destroy (scanner);
}

void
gtk_item_factory_parse_rc_scanner (GScanner *scanner)
{
  gpointer saved_symbol;

  g_return_if_fail (scanner != NULL);

  if (!gtk_item_factory_class)
    gtk_type_class (GTK_TYPE_ITEM_FACTORY);

  saved_symbol = g_scanner_lookup_symbol (scanner, "menu-path");
  g_scanner_remove_symbol (scanner, "menu-path");
  g_scanner_add_symbol (scanner, "menu-path", (gpointer)gtk_item_factory_parse_menu_path);

  g_scanner_peek_next_token (scanner);

  while (scanner->next_token == '(')
    {
      g_scanner_get_next_token (scanner);

      gtk_item_factory_parse_statement (scanner, gtk_item_factory_class);

      g_scanner_peek_next_token (scanner);
    }

  g_scanner_remove_symbol (scanner, "menu-path");
  g_scanner_add_symbol (scanner, "menu-path", saved_symbol);
}

void
gtk_item_factory_parse_rc (const gchar	  *file_name)
{
  gint fd;
  GScanner *scanner;

  g_return_if_fail (file_name != NULL);

  if (!S_ISREG (g_scanner_stat_mode (file_name)))
    return;

  fd = open (file_name, O_RDONLY);
  if (fd < 0)
    return;

  if (!gtk_item_factory_class)
    gtk_type_class (GTK_TYPE_ITEM_FACTORY);

  ifactory_scanner_config.cpair_comment_single = gtk_item_factory_class->cpair_comment_single;
  scanner = g_scanner_new (&ifactory_scanner_config);

  g_scanner_input_file (scanner, fd);

  gtk_item_factory_parse_rc_scanner (scanner);

  g_scanner_destroy (scanner);

  close (fd);
}

void
gtk_item_factory_set_translate_func (GtkItemFactory      *ifactory,
				     GtkTranslateFunc     func,
				     gpointer             data,
				     GtkDestroyNotify     notify)
{
  g_return_if_fail (ifactory != NULL);
  
  if (ifactory->translate_notify)
    ifactory->translate_notify (ifactory->translate_data);
      
  ifactory->translate_func = func;
  ifactory->translate_data = data;
  ifactory->translate_notify = notify;
}
