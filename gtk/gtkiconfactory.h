/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
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
GtkIconSet *    gtk_icon_factory_lookup   (GtkIconFactory *factory,
                                           const gchar    *stock_id);

/* Manage the default icon factory stack */

void        gtk_push_default_icon_factory (GtkIconFactory  *factory);
GtkIconSet *gtk_default_icon_lookup       (const gchar     *stock_id);

/* Get real size from semantic size (eventually user-configurable) */
void        gtk_get_icon_size             (GtkIconSizeType  semantic_size,
                                           gint            *width,
                                           gint            *height);


/* Icon sets */

GtkIconSet* gtk_icon_set_ref             (GtkIconSet      *icon_set);
void        gtk_icon_set_unref           (GtkIconSet      *icon_set);
GtkIconSet* gtk_icon_set_copy            (GtkIconSet      *icon_set);

/* Get one of the icon variants in the set, creating the variant if
 * necessary.
 */
GdkPixbuf*  gtk_icon_set_get_icon        (GtkIconSet      *icon_set,
                                          GtkStyle        *style,
                                          GtkTextDirection direction,
                                          GtkStateType     state,
                                          GtkIconSizeType  size,
                                          GtkWidget       *widget,
                                          const char      *detail);

GtkIconSet* gtk_icon_set_new             (void);

struct _GtkIconSource
{
  /* Either filename or pixbuf can be NULL. If both are non-NULL,
   * the filename gets ignored.
   */
  const char *filename;
  GdkPixbuf *pixbuf;

  GtkTextDirection direction;
  GtkStateType state;
  GtkIconSizeType size;

  /* If TRUE, then the parameter is wildcarded, and the above
   * fields should be ignored. If FALSE, the parameter is
   * specified, and the above fields should be valid.
   */
  guint any_direction : 1;
  guint any_state : 1;
  guint any_size : 1;
};

void gtk_icon_set_add_source (GtkIconSet *icon_set,
                              const GtkIconSource *source);

GtkIconSource *gtk_icon_source_copy (const GtkIconSource *source);
void           gtk_icon_source_free (GtkIconSource *source);

/* Clear icon set contents, drop references to all contained
 * GdkPixbuf objects and forget all GtkIconSources. Used to
 * recycle an icon set.
 */
void        gtk_icon_set_clear           (GtkIconSet      *icon_set);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_ICON_FACTORY_H__ */
