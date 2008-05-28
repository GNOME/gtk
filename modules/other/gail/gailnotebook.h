/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
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

#ifndef __GAIL_NOTEBOOK_H__
#define __GAIL_NOTEBOOK_H__

#include <gail/gailcontainer.h>

G_BEGIN_DECLS

#define GAIL_TYPE_NOTEBOOK                   (gail_notebook_get_type ())
#define GAIL_NOTEBOOK(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_NOTEBOOK, GailNotebook))
#define GAIL_NOTEBOOK_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_NOTEBOOK, GailNotebookClass))
#define GAIL_IS_NOTEBOOK(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_NOTEBOOK))
#define GAIL_IS_NOTEBOOK_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_NOTEBOOK))
#define GAIL_NOTEBOOK_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_NOTEBOOK, GailNotebookClass))

typedef struct _GailNotebook              GailNotebook;
typedef struct _GailNotebookClass         GailNotebookClass;

struct _GailNotebook
{
  GailContainer parent;

  /*
   * page_cache maintains a list of pre-ref'd Notebook Pages.
   * This cache is queried by gail_notebook_ref_child().
   * If the page is found in the list then a new page does not
   * need to be created
   */
  GList*       page_cache;
  gint         selected_page;
  gint         focus_tab_page;
  gint         page_count;
  guint        idle_focus_id;

  gint         remove_index;
};

GType gail_notebook_get_type (void);

struct _GailNotebookClass
{
  GailContainerClass parent_class;
};

G_END_DECLS

#endif /* __GAIL_NOTEBOOK_H__ */
