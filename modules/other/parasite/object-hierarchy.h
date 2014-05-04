/*
 * Copyright (c) 2013 Intel Corporation
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

#ifndef _GTKPARASITE_OBJECT_HIERARCHY_H_
#define _GTKPARASITE_OBJECT_HIERARCHY_H_

#include <gtk/gtk.h>

#define PARASITE_TYPE_OBJECT_HIERARCHY            (parasite_object_hierarchy_get_type())
#define PARASITE_OBJECT_HIERARCHY(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PARASITE_TYPE_OBJECT_HIERARCHY, ParasiteObjectHierarchy))
#define PARASITE_OBJECT_HIERARCHY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PARASITE_TYPE_OBJECT_HIERARCHY, ParasiteObjectHierarchyClass))
#define PARASITE_IS_OBJECT_HIERARCHY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PARASITE_TYPE_OBJECT_HIERARCHY))
#define PARASITE_IS_OBJECT_HIERARCHY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PARASITE_TYPE_OBJECT_HIERARCHY))
#define PARASITE_OBJECT_HIERARCHY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PARASITE_TYPE_OBJECT_HIERARCHY, ParasiteObjectHierarchyClass))


typedef struct _ParasiteObjectHierarchyPrivate ParasiteObjectHierarchyPrivate;

typedef struct _ParasiteObjectHierarchy
{
  GtkBox parent;
  ParasiteObjectHierarchyPrivate *priv;
} ParasiteObjectHierarchy;

typedef struct _ParasiteObjectHierarchyClass
{
  GtkBoxClass parent;
} ParasiteObjectHierarchyClass;

G_BEGIN_DECLS

GType      parasite_object_hierarchy_get_type   (void);
GtkWidget *parasite_object_hierarchy_new        (void);
void       parasite_object_hierarchy_set_object (ParasiteObjectHierarchy *oh,
                                                 GObject                 *object);

G_END_DECLS

#endif // _GTKPARASITE_OBJECT_HIERARCHY_H_

// vim: set et sw=2 ts=2:
