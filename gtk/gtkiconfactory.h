/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_ICON_FACTORY_H__
#define __GTK_ICON_FACTORY_H__

#include <gdk/gdk.h>
#include <gtk/gtkrc.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GtkIconFactoryClass GtkIconFactoryClass;

#define GTK_TYPE_ICON_FACTORY              (gtk_icon_factory_get_type ())
#define GTK_ICON_FACTORY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GTK_TYPE_ICON_FACTORY, GtkIconFactory))
#define GTK_ICON_FACTORY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_ICON_FACTORY, GtkIconFactoryClass))
#define GTK_IS_ICON_FACTORY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_ICON_FACTORY))
#define GTK_IS_ICON_FACTORY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ICON_FACTORY))
#define GTK_ICON_FACTORY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ICON_FACTORY, GtkIconFactoryClass))

struct _GtkIconFactory
{
  GObject parent_instance;

  GHashTable *icons;
};

struct _GtkIconFactoryClass
{
  GObjectClass parent_class;

  
};

GType           gtk_icon_factory_get_type (void);
GtkIconFactory* gtk_icon_factory_new      (void);
void            gtk_icon_factory_add      (GtkIconFactory *factory,
                                           const gchar    *stock_id,
                                           GtkIconSet     *icon_set);
GtkIconSet*     gtk_icon_factory_lookup   (GtkIconFactory *factory,
                                           const gchar    *stock_id);

/* Manage the default icon factory stack */

void        gtk_icon_factory_add_default     (GtkIconFactory  *factory);
void        gtk_icon_factory_remove_default  (GtkIconFactory  *factory);
GtkIconSet* gtk_icon_factory_lookup_default  (const gchar     *stock_id);

/* Get preferred real size from registered semantic size.  Note that
 * themes SHOULD use this size, but they aren't required to; for size
 * requests and such, you should get the actual pixbuf from the icon
 * set and see what size was rendered.
 *
 * This function is intended for people who are scaling icons,
 * rather than for people who are displaying already-scaled icons.
 * That is, if you are displaying an icon, you should get the
 * size from the rendered pixbuf, not from here.
 */

gboolean gtk_icon_size_lookup         (const gchar *alias,
                                       gint        *width,
                                       gint        *height);
void     gtk_icon_size_register       (const gchar *alias,
                                       gint         width,
                                       gint         height);
void     gtk_icon_size_register_alias (const gchar *alias,
                                       const gchar *target);


/* Standard sizes */

#define GTK_ICON_SIZE_MENU          "gtk-menu"
#define GTK_ICON_SIZE_SMALL_TOOLBAR "gtk-small-toolbar"
#define GTK_ICON_SIZE_LARGE_TOOLBAR "gtk-large-toolbar"
#define GTK_ICON_SIZE_BUTTON        "gtk-button"
#define GTK_ICON_SIZE_DIALOG        "gtk-dialog"

/* Icon sets */

GtkIconSet* gtk_icon_set_new             (void);
GtkIconSet* gtk_icon_set_new_from_pixbuf (GdkPixbuf       *pixbuf);

GtkIconSet* gtk_icon_set_ref             (GtkIconSet      *icon_set);
void        gtk_icon_set_unref           (GtkIconSet      *icon_set);
GtkIconSet* gtk_icon_set_copy            (GtkIconSet      *icon_set);

/* Get one of the icon variants in the set, creating the variant if
 * necessary.
 */
GdkPixbuf*  gtk_icon_set_render_icon     (GtkIconSet      *icon_set,
                                          GtkStyle        *style,
                                          GtkTextDirection direction,
                                          GtkStateType     state,
                                          const gchar     *size,
                                          GtkWidget       *widget,
                                          const char      *detail);


void           gtk_icon_set_add_source   (GtkIconSet          *icon_set,
                                          const GtkIconSource *source);

/* INTERNAL */
void _gtk_icon_set_invalidate_caches (void);

struct _GtkIconSource
{
  /* Either filename or pixbuf can be NULL. If both are non-NULL,
   * the pixbuf is assumed to be the already-loaded contents of the
   * file.
   */
  gchar *filename;
  GdkPixbuf *pixbuf;

  GtkTextDirection direction;
  GtkStateType state;
  gchar *size;

  /* If TRUE, then the parameter is wildcarded, and the above
   * fields should be ignored. If FALSE, the parameter is
   * specified, and the above fields should be valid.
   */
  guint any_direction : 1;
  guint any_state : 1;
  guint any_size : 1;
};


GtkIconSource* gtk_icon_source_copy    (const GtkIconSource *source);
void           gtk_icon_source_free    (GtkIconSource       *source);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_ICON_FACTORY_H__ */
