/*
 * Copyright (c) 2008-2009  Christian Hammond
 * Copyright (c) 2008-2009  David Trowbridge
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef _GTKPARASITE_PROP_LIST_H_
#define _GTKPARASITE_PROP_LIST_H_


#include <gtk/gtk.h>

#define PARASITE_TYPE_PROP_LIST            (parasite_prop_list_get_type())
#define PARASITE_PROP_LIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PARASITE_TYPE_PROP_LIST, ParasitePropList))
#define PARASITE_PROP_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PARASITE_TYPE_PROP_LIST, ParasitePropListClass))
#define PARASITE_IS_PROP_LIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PARASITE_TYPE_PROP_LIST))
#define PARASITE_IS_PROP_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PARASITE_TYPE_PROP_LIST))
#define PARASITE_PROP_LIST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PARASITE_TYPE_PROP_LIST, ParasitePropListClass))


typedef struct _ParasitePropListPrivate ParasitePropListPrivate;

typedef struct _ParasitePropList
{
  GtkTreeView parent;
  ParasitePropListPrivate *priv;
} ParasitePropList;

typedef struct _ParasitePropListClass
{
  GtkTreeViewClass parent;
} ParasitePropListClass;


G_BEGIN_DECLS

GType      parasite_prop_list_get_type   (void);
GtkWidget *parasite_prop_list_new        (GtkWidget        *widget_tree,
                                          gboolean          child_properties);
gboolean   parasite_prop_list_set_object (ParasitePropList *pl,
                                          GObject          *object);

G_END_DECLS

#endif // _GTKPARASITE_PROP_LIST_H_

// vim: set et sw=2 ts=2:
