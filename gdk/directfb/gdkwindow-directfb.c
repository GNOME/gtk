/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-1999 Tor Lillqvist
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.
 */

/*
 * GTK+ DirectFB backend
 * Copyright (C) 2001-2002  convergence integrated media GmbH
 * Copyright (C) 2002-2004  convergence GmbH
 * Written by Denis Oliver Kropp <dok@convergence.de> and
 *            Sven Neumann <sven@convergence.de>
 */

#include <config.h>
#include "gdk.h"
#include "gdkwindow.h"

#include "gdkdirectfb.h"
#include "gdkprivate-directfb.h"
#include "gdkdisplay-directfb.h"

#include "gdkregion-generic.h"

#include "gdkinternals.h"
#include "gdkalias.h"
#include "cairo.h"
#include <assert.h>

static GdkRegion * gdk_window_impl_directfb_get_visible_region (GdkDrawable *drawable);
static void        gdk_window_impl_directfb_set_colormap       (GdkDrawable *drawable,
                                                                GdkColormap *colormap);
static void gdk_window_impl_directfb_init       (GdkWindowImplDirectFB      *window);
static void gdk_window_impl_directfb_class_init (GdkWindowImplDirectFBClass *klass);
static void gdk_window_impl_directfb_finalize   (GObject                    *object);

typedef struct
{
  GdkWindowChildChanged  changed;
  GdkWindowChildGetPos   get_pos;
  gpointer               user_data;
} GdkWindowChildHandlerData;


/* Code for dirty-region queueing
 */
static GSList *update_windows = NULL;
static guint update_idle = 0;

static void
gdk_window_directfb_process_all_updates (void)
{
  GSList *old_update_windows = update_windows;
  GSList *tmp_list = update_windows;

  if (update_idle)
    g_source_remove (update_idle);
  
  update_windows = NULL;
  update_idle = 0;

  g_slist_foreach (old_update_windows, (GFunc)g_object_ref, NULL);
  
  while (tmp_list)
    {
      GdkWindowObject *private = (GdkWindowObject *)tmp_list->data;
      
      if (private->update_freeze_count)
	update_windows = g_slist_prepend (update_windows, private);
      else
	gdk_window_process_updates(tmp_list->data,TRUE);
      
      g_object_unref (tmp_list->data);
      tmp_list = tmp_list->next;
    }

  g_slist_free (old_update_windows);

}

static gboolean
gdk_window_update_idle (gpointer data)
{
  GDK_THREADS_ENTER ();
  gdk_window_directfb_process_all_updates ();
  GDK_THREADS_LEAVE ();
  
  return FALSE;
}

static void
gdk_window_schedule_update (GdkWindow *window)
{
  if (window && GDK_WINDOW_OBJECT (window)->update_freeze_count)
    return;

  if (!update_idle)
    {
      update_idle = g_idle_add_full (GDK_PRIORITY_REDRAW,
				     gdk_window_update_idle, NULL, NULL);
    }
}


static GdkWindow *gdk_directfb_window_containing_pointer = NULL;
static GdkWindow *gdk_directfb_focused_window            = NULL;
static gpointer   parent_class                           = NULL;
GdkWindow * _gdk_parent_root = NULL;
static void
gdk_window_impl_directfb_paintable_init (GdkPaintableIface *iface);





GType
gdk_window_impl_directfb_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
        {
          sizeof (GdkWindowImplDirectFBClass),
          (GBaseInitFunc) NULL,
          (GBaseFinalizeFunc) NULL,
          (GClassInitFunc) gdk_window_impl_directfb_class_init,
          NULL,           /* class_finalize */
          NULL,           /* class_data */
          sizeof (GdkWindowImplDirectFB),
          0,              /* n_preallocs */
          (GInstanceInitFunc) gdk_window_impl_directfb_init,
        };

    static const GInterfaceInfo paintable_info =
      {
    (GInterfaceInitFunc) gdk_window_impl_directfb_paintable_init,
    NULL,
    NULL
      };

      object_type = g_type_register_static (GDK_TYPE_DRAWABLE_IMPL_DIRECTFB,
                                            "GdkWindowImplDirectFB",
                                            &object_info, 0);
       g_type_add_interface_static (object_type,
                   GDK_TYPE_PAINTABLE,
                   &paintable_info);

    }

  return object_type;
}

GType
_gdk_window_impl_get_type (void)
{
  return gdk_window_impl_directfb_get_type ();
}

static void
gdk_window_impl_directfb_init (GdkWindowImplDirectFB *impl)
{
  impl->drawable.width  = 1;
  impl->drawable.height = 1;
  //cannot use gdk_cursor_new here since gdk_display_get_default
  //does not work yet.
  impl->cursor          = gdk_cursor_new_for_display (GDK_DISPLAY_OBJECT(_gdk_display),GDK_LEFT_PTR);
  impl->opacity         = 255;
}

static void
gdk_window_impl_directfb_class_init (GdkWindowImplDirectFBClass *klass)
{
  GObjectClass     *object_class   = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_window_impl_directfb_finalize;

  drawable_class->set_colormap = gdk_window_impl_directfb_set_colormap;

  /* Visible and clip regions are the same */

  drawable_class->get_clip_region =
    gdk_window_impl_directfb_get_visible_region;

  drawable_class->get_visible_region =
    gdk_window_impl_directfb_get_visible_region;
}

static void
g_free_2nd (gpointer a,
            gpointer b,
            gpointer data)
{
  g_free (b);
}

static void
gdk_window_impl_directfb_finalize (GObject *object)
{
  GdkWindowImplDirectFB *impl = GDK_WINDOW_IMPL_DIRECTFB (object);

  if (GDK_WINDOW_IS_MAPPED (impl->drawable.wrapper))
    gdk_window_hide (impl->drawable.wrapper);

  if (impl->cursor)
    gdk_cursor_unref (impl->cursor);

  if (impl->properties)
    {
      g_hash_table_foreach (impl->properties, g_free_2nd, NULL);
      g_hash_table_destroy (impl->properties);
    }
  if (impl->window)
    {
      gdk_directfb_window_id_table_remove (impl->dfb_id);
	  /* native window resource must be release before we can finalize !*/
      impl->window = NULL;
    }

  if (G_OBJECT_CLASS (parent_class)->finalize)
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GdkRegion*
gdk_window_impl_directfb_get_visible_region (GdkDrawable *drawable)
{
  GdkDrawableImplDirectFB *priv = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);
  GdkRectangle             rect = { 0, 0, 0, 0 };
  DFBRectangle             drect = { 0, 0, 0, 0 };

  if (priv->surface)
  priv->surface->GetVisibleRectangle (priv->surface, &drect);
  rect.x= drect.x;
  rect.y= drect.y;
  rect.width=drect.w;
  rect.height=drect.h;
  return gdk_region_rectangle (&rect);
}

static void
gdk_window_impl_directfb_set_colormap (GdkDrawable *drawable,
                                       GdkColormap *colormap)
{
  GDK_DRAWABLE_CLASS (parent_class)->set_colormap (drawable, colormap);

  if (colormap)
    {
       GdkDrawableImplDirectFB *priv = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);

       if (priv->surface)
	 {
	   IDirectFBPalette *palette = gdk_directfb_colormap_get_palette (colormap);

           if (palette)
             priv->surface->SetPalette (priv->surface, palette);
         }
    }
}


static gboolean
create_directfb_window (GdkWindowImplDirectFB *impl,
                        DFBWindowDescription  *desc,
                        DFBWindowOptions       window_options)
{
  DFBResult        ret;
  IDirectFBWindow *window;

  ret = _gdk_display->layer->CreateWindow (_gdk_display->layer, desc, &window);

  if (ret != DFB_OK)
    {
      DirectFBError ("gdk_window_new: Layer->CreateWindow failed", ret);
      g_assert (0);
      return FALSE;
    }

  if ((desc->flags & DWDESC_CAPS) && (desc->caps & DWCAPS_INPUTONLY))
  {
    impl->drawable.surface = NULL;
  } else 
    window->GetSurface (window, &impl->drawable.surface);

  if (window_options)
    {
      DFBWindowOptions options;
      window->GetOptions (window, &options);
      window->SetOptions (window,  options | window_options);
    }

  impl->window = window;

  return TRUE;
}

void
_gdk_windowing_window_init (void)
{
  GdkWindowObject       *private;
  GdkWindowImplDirectFB *impl;
  DFBDisplayLayerConfig  dlc;

  g_assert (_gdk_parent_root == NULL);

  _gdk_display->layer->GetConfiguration( 
	_gdk_display->layer, &dlc );

  _gdk_parent_root = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = GDK_WINDOW_OBJECT (_gdk_parent_root);
  impl = GDK_WINDOW_IMPL_DIRECTFB (private->impl);

  private->window_type = GDK_WINDOW_ROOT;
  private->state       = 0;
  private->children    = NULL;
  impl->drawable.paint_region   = NULL;
  impl->gdkWindow      = _gdk_parent_root;
  impl->window           = NULL;
  impl->drawable.abs_x   = 0;
  impl->drawable.abs_y   = 0;
  impl->drawable.width   = dlc.width;
  impl->drawable.height  = dlc.height;
  impl->drawable.wrapper = GDK_DRAWABLE (private);
  /* custom root window init */
  {
    DFBWindowDescription   desc;
    desc.flags = 0;
	/*XXX I must do this now its a bug  ALPHA ROOT*/

    desc.flags = DWDESC_CAPS;
    desc.caps = 0;
    desc.caps  |= DWCAPS_NODECORATION;
    desc.caps  |= DWCAPS_ALPHACHANNEL;
    desc.flags |= ( DWDESC_WIDTH | DWDESC_HEIGHT |
                      DWDESC_POSX  | DWDESC_POSY );
    desc.posx   = 0;
    desc.posy   = 0;
    desc.width  = dlc.width;
    desc.height = dlc.height;
    create_directfb_window (impl,&desc,0);
	g_assert(impl->window != NULL);
    g_assert(impl->drawable.surface != NULL );
  }
  impl->drawable.surface->GetPixelFormat(impl->drawable.surface,&impl->drawable.format); 
  private->depth = DFB_BITS_PER_PIXEL(impl->drawable.format);
  /*
	Now we can set up the system colormap
  */
  gdk_drawable_set_colormap (GDK_DRAWABLE (_gdk_parent_root),gdk_colormap_get_system());
}



GdkWindow *
gdk_directfb_window_new (GdkWindow              *parent,
                         GdkWindowAttr          *attributes,
                         gint                    attributes_mask,
                         DFBWindowCapabilities   window_caps,
                         DFBWindowOptions        window_options,
                         DFBSurfaceCapabilities  surface_caps)
{
  GdkWindow             *window;
  GdkWindowObject       *private;
  GdkWindowObject       *parent_private;
  GdkWindowImplDirectFB *impl;
  GdkWindowImplDirectFB *parent_impl;
  GdkVisual             *visual;
  DFBWindowDescription   desc;
  gint x, y;

  g_return_val_if_fail (attributes != NULL, NULL);

  if (!parent || attributes->window_type != GDK_WINDOW_CHILD)
    parent = _gdk_parent_root;

  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = GDK_WINDOW_OBJECT (window);

  parent_private = GDK_WINDOW_OBJECT (parent);
  parent_impl = GDK_WINDOW_IMPL_DIRECTFB (parent_private->impl);
  private->parent = parent_private;

  x = (attributes_mask & GDK_WA_X) ? attributes->x : 0;
  y = (attributes_mask & GDK_WA_Y) ? attributes->y : 0;

  gdk_window_set_events (window, attributes->event_mask | GDK_STRUCTURE_MASK);

  impl = GDK_WINDOW_IMPL_DIRECTFB (private->impl);
  impl->drawable.wrapper = GDK_DRAWABLE (window);
  impl->gdkWindow      = window;

  private->x = x;
  private->y = y;

  _gdk_directfb_calc_abs (window);

  impl->drawable.width  = MAX (1, attributes->width);
  impl->drawable.height = MAX (1, attributes->height);

  private->window_type = attributes->window_type;

  desc.flags = 0;

  if (attributes_mask & GDK_WA_VISUAL)
    visual = attributes->visual;
  else
    visual = gdk_drawable_get_visual (parent);

  switch (attributes->wclass)
    {
    case GDK_INPUT_OUTPUT:
      private->input_only = FALSE;

      desc.flags |= DWDESC_PIXELFORMAT;
      desc.pixelformat = ((GdkVisualDirectFB *) visual)->format;

      if (DFB_PIXELFORMAT_HAS_ALPHA (desc.pixelformat))
        {
          desc.flags |= DWDESC_CAPS;
          desc.caps = DWCAPS_ALPHACHANNEL;
        }
      break;

    case GDK_INPUT_ONLY:
      private->input_only = TRUE;
      desc.flags |= DWDESC_CAPS;
      desc.caps = DWCAPS_INPUTONLY;
      break;

    default:
      g_warning ("gdk_window_new: unsupported window class\n");
      _gdk_window_destroy (window, FALSE);
      return NULL;
    }

  switch (private->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
      desc.flags |= ( DWDESC_WIDTH | DWDESC_HEIGHT |
                      DWDESC_POSX  | DWDESC_POSY );
      desc.posx   = x;
      desc.posy   = y;
      desc.width  = impl->drawable.width;
      desc.height = impl->drawable.height;
#if 0
      if (window_caps)
        {
          if (! (desc.flags & DWDESC_CAPS))
            {
              desc.flags |= DWDESC_CAPS;
              desc.caps   = DWCAPS_NONE;
            }

          desc.caps |= window_caps;
        }

      if (surface_caps)
        {
          desc.flags |= DWDESC_SURFACE_CAPS;
          desc.surface_caps = surface_caps;
        }
#endif

      if (!create_directfb_window (impl, &desc, window_options))
        {
		  g_assert(0);
          _gdk_window_destroy (window, FALSE);
          return NULL;
        }
      	if( desc.caps != DWCAPS_INPUTONLY )
			impl->window->SetOpacity(impl->window, 0x00 );
      break;

    case GDK_WINDOW_CHILD:
	   impl->window=NULL;
      if (!private->input_only && parent_impl->drawable.surface)
        {

          DFBRectangle rect =
          { x, y, impl->drawable.width, impl->drawable.height };
          parent_impl->drawable.surface->GetSubSurface (parent_impl->drawable.surface,
                                                        &rect,
                                                        &impl->drawable.surface);
        }
      break;

    default:
      g_warning ("gdk_window_new: unsupported window type: %d",
                 private->window_type);
      _gdk_window_destroy (window, FALSE);
      return NULL;
    }

  if (impl->drawable.surface)
    {
      GdkColormap *colormap;

      impl->drawable.surface->GetPixelFormat (impl->drawable.surface,
					      &impl->drawable.format);

  	  private->depth = DFB_BITS_PER_PIXEL(impl->drawable.format);

      if ((attributes_mask & GDK_WA_COLORMAP) && attributes->colormap)
	{
	  colormap = attributes->colormap;
	}
      else
	{
	  if (gdk_visual_get_system () == visual)
	    colormap = gdk_colormap_get_system ();
	  else
	    colormap =gdk_drawable_get_colormap (parent);
	}

      gdk_drawable_set_colormap (GDK_DRAWABLE (window), colormap);
    }
  else
    {
      impl->drawable.format = ((GdkVisualDirectFB *)visual)->format;
  	  private->depth = visual->depth;
    }

  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
                                  (attributes->cursor) : NULL));

  if (parent_private)
    parent_private->children = g_list_prepend (parent_private->children,
                                               window);

  /* we hold a reference count on ourselves */
  g_object_ref (window);

  if (impl->window)
    {
      impl->window->GetID (impl->window, &impl->dfb_id);
      gdk_directfb_window_id_table_insert (impl->dfb_id, window);
      gdk_directfb_event_windows_add (window);
    }

  if (attributes_mask & GDK_WA_TYPE_HINT)
    gdk_window_set_type_hint (window, attributes->type_hint);

  return window;
}

GdkWindow *
gdk_window_new (GdkWindow     *parent,
                GdkWindowAttr *attributes,
                gint           attributes_mask)
{
  g_return_val_if_fail (attributes != NULL, NULL);

  return gdk_directfb_window_new (parent, attributes, attributes_mask,
                                  DWCAPS_NONE, DWOP_NONE, DSCAPS_NONE);
}
void
_gdk_windowing_window_destroy_foreign (GdkWindow *window)
{
  /* It's somebody else's window, but in our heirarchy,
   * so reparent it to the root window, and then send
   * it a delete event, as if we were a WM
   */
	_gdk_windowing_window_destroy (window,TRUE,TRUE);
}


void
_gdk_windowing_window_destroy (GdkWindow *window,
                               gboolean   recursing,
                               gboolean   foreign_destroy)
{
  GdkWindowObject       *private;
  GdkWindowImplDirectFB *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_DIRECTFB (private->impl);

  _gdk_selection_window_destroyed (window);
#if (DIRECTFB_MAJOR_VERSION >= 1)
  gdk_directfb_event_windows_remove (window);
#endif
  if (window == _gdk_directfb_pointer_grab_window)
    gdk_pointer_ungrab (GDK_CURRENT_TIME);
  if (window == _gdk_directfb_keyboard_grab_window)
    gdk_keyboard_ungrab (GDK_CURRENT_TIME);

  if (window == gdk_directfb_focused_window)
    gdk_directfb_change_focus (NULL);


  if (impl->drawable.surface) {
    GdkDrawableImplDirectFB *dimpl = GDK_DRAWABLE_IMPL_DIRECTFB (private->impl);
    if(dimpl->cairo_surface) {
      cairo_surface_destroy(dimpl->cairo_surface);
      dimpl->cairo_surface= NULL;
    }
    impl->drawable.surface->Release (impl->drawable.surface);
    impl->drawable.surface = NULL;
  }

  if (!recursing && !foreign_destroy && impl->window ) {
    	impl->window->SetOpacity (impl->window,0);
   		impl->window->Close(impl->window);
      	impl->window->Release(impl->window);
  		impl->window = NULL;
	}
}

/* This function is called when the window is really gone.
 */
void
gdk_window_destroy_notify (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE(window) != GDK_WINDOW_FOREIGN)
	g_warning ("GdkWindow %p unexpectedly destroyed", window);

      _gdk_window_destroy (window, TRUE);
    }
   g_object_unref (window);
}

/* Focus follows pointer */
GdkWindow *
gdk_directfb_window_find_toplevel (GdkWindow *window)
{
  while (window && window != _gdk_parent_root)
    {
      GdkWindow *parent = (GdkWindow *) (GDK_WINDOW_OBJECT (window))->parent;

      if ((parent == _gdk_parent_root) && GDK_WINDOW_IS_MAPPED (window))
        return window;

      window = parent;
    }

  return _gdk_parent_root;
}

GdkWindow *
gdk_directfb_window_find_focus (void)
{
  if (_gdk_directfb_keyboard_grab_window)
    return _gdk_directfb_keyboard_grab_window;

  if (!gdk_directfb_focused_window)
    gdk_directfb_focused_window = g_object_ref (_gdk_parent_root);

  return gdk_directfb_focused_window;
}

void
gdk_directfb_change_focus (GdkWindow *new_focus_window)
{
  GdkEventFocus *event;
  GdkWindow     *old_win;
  GdkWindow     *new_win;
  GdkWindow     *event_win;


  /* No focus changes while the pointer is grabbed */
  if (_gdk_directfb_pointer_grab_window)
    return;

  old_win = gdk_directfb_focused_window;
  new_win = gdk_directfb_window_find_toplevel (new_focus_window);

  if (old_win == new_win)
    return;

  if (old_win)
    {
      event_win = gdk_directfb_keyboard_event_window (old_win,
                                                      GDK_FOCUS_CHANGE);
      if (event_win)
        {
          event = (GdkEventFocus *) gdk_directfb_event_make (event_win,
                                                             GDK_FOCUS_CHANGE);
          event->in = FALSE;
        }
    }

  event_win = gdk_directfb_keyboard_event_window (new_win,
                                                  GDK_FOCUS_CHANGE);
  if (event_win)
    {
      event = (GdkEventFocus *) gdk_directfb_event_make (event_win,
                                                         GDK_FOCUS_CHANGE);
      event->in = TRUE;
    }

  if (gdk_directfb_focused_window)
    g_object_unref (gdk_directfb_focused_window);
  gdk_directfb_focused_window = g_object_ref (new_win);
}

void
gdk_window_set_accept_focus (GdkWindow *window,
                             gboolean accept_focus)
{
  GdkWindowObject *private;
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
                                                                                                                      
  private = (GdkWindowObject *)window;
                                                                                                                      
  accept_focus = accept_focus != FALSE;
                                                                                                                      
  if (private->accept_focus != accept_focus)
    private->accept_focus = accept_focus;

}

void
gdk_window_set_focus_on_map (GdkWindow *window,
                             gboolean focus_on_map)
{
  GdkWindowObject *private;
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
                                                                                                                      
  private = (GdkWindowObject *)window;
                                                                                                                      
  focus_on_map = focus_on_map != FALSE;
                                                                                                                      
  if (private->focus_on_map != focus_on_map)
    private->focus_on_map = focus_on_map;
}

static gboolean
gdk_directfb_window_raise (GdkWindow *window)
{
  GdkWindowObject *parent;

  parent = GDK_WINDOW_OBJECT (window)->parent;

  if (parent->children->data == window)
    return FALSE;

  parent->children = g_list_remove (parent->children, window);
  parent->children = g_list_prepend (parent->children, window);

  return TRUE;
}

static void
gdk_directfb_window_lower (GdkWindow *window)
{
  GdkWindowObject *parent;

  parent = GDK_WINDOW_OBJECT (window)->parent;

  parent->children = g_list_remove (parent->children, window);
  parent->children = g_list_append (parent->children, window);
}

static gboolean
all_parents_shown (GdkWindowObject *private)
{
  while (GDK_WINDOW_IS_MAPPED (private))
    {
      if (private->parent)
        private = GDK_WINDOW_OBJECT (private)->parent;
      else
        return TRUE;
    }

  return FALSE;
}

static void
send_map_events (GdkWindowObject *private)
{
  GList     *list;
  GdkWindow *event_win;

  if (!GDK_WINDOW_IS_MAPPED (private))
    return;

  event_win = gdk_directfb_other_event_window ((GdkWindow *) private, GDK_MAP);
  if (event_win)
    gdk_directfb_event_make (event_win, GDK_MAP);

  for (list = private->children; list; list = list->next)
    send_map_events (list->data);
}

static GdkWindow *
gdk_directfb_find_common_ancestor (GdkWindow *win1,
                                   GdkWindow *win2)
{
  GdkWindowObject *a;
  GdkWindowObject *b;

  for (a = GDK_WINDOW_OBJECT (win1); a; a = a->parent)
    for (b = GDK_WINDOW_OBJECT (win2); b; b = b->parent)
      {
        if (a == b)
          return GDK_WINDOW (a);
      }

  return NULL;
}

void
gdk_directfb_window_send_crossing_events (GdkWindow       *src,
                                          GdkWindow       *dest,
                                          GdkCrossingMode  mode)
{
  GdkWindow       *c;
  GdkWindow       *win, *last, *next;
  GdkEvent        *event;
  gint             x, y, x_int, y_int;
  GdkModifierType  modifiers;
  GSList          *path, *list;
  gboolean         non_linear;
  GdkWindow       *a;
  GdkWindow       *b;
  GdkWindow       *event_win;

  /* Do a possible cursor change before checking if we need to
     generate crossing events so cursor changes due to pointer
     grabs work correctly. */
  {
    static GdkCursorDirectFB *last_cursor = NULL;

    GdkWindowObject       *private = GDK_WINDOW_OBJECT (dest);
    GdkWindowImplDirectFB *impl    = GDK_WINDOW_IMPL_DIRECTFB (private->impl);
    GdkCursorDirectFB     *cursor;

    if (_gdk_directfb_pointer_grab_cursor)
      cursor = (GdkCursorDirectFB*) _gdk_directfb_pointer_grab_cursor;
    else
      cursor = (GdkCursorDirectFB*) impl->cursor;

    if (cursor != last_cursor)
      {
        win     = gdk_directfb_window_find_toplevel (dest);
        private = GDK_WINDOW_OBJECT (win);
        impl    = GDK_WINDOW_IMPL_DIRECTFB (private->impl);

        if (impl->window)
          impl->window->SetCursorShape (impl->window,
                                        cursor->shape,
                                        cursor->hot_x, cursor->hot_y);
        last_cursor = cursor;
      }
  }

  if (dest == gdk_directfb_window_containing_pointer)
    return;

  if (gdk_directfb_window_containing_pointer == NULL)
    gdk_directfb_window_containing_pointer = g_object_ref (_gdk_parent_root);

  if (src)
    a = src;
  else
    a = gdk_directfb_window_containing_pointer;

  b = dest;

  if (a == b)
    return;

  /* gdk_directfb_window_containing_pointer might have been destroyed.
   * The refcount we hold on it should keep it, but it's parents
   * might have died.
   */
  if (GDK_WINDOW_DESTROYED (a))
    a = _gdk_parent_root;

  gdk_directfb_mouse_get_info (&x, &y, &modifiers);

  c = gdk_directfb_find_common_ancestor (a, b);

  non_linear = (c != a) && (c != b);

  event_win = gdk_directfb_pointer_event_window (a, GDK_LEAVE_NOTIFY);
  if (event_win)
    {
      event = gdk_directfb_event_make (event_win, GDK_LEAVE_NOTIFY);
      event->crossing.subwindow = NULL;

      gdk_window_get_origin (a, &x_int, &y_int);

      event->crossing.x      = x - x_int;
      event->crossing.y      = y - y_int;
      event->crossing.x_root = x;
      event->crossing.y_root = y;
      event->crossing.mode   = mode;

      if (non_linear)
        event->crossing.detail = GDK_NOTIFY_NONLINEAR;
      else if (c == a)
        event->crossing.detail = GDK_NOTIFY_INFERIOR;
      else
        event->crossing.detail = GDK_NOTIFY_ANCESTOR;

      event->crossing.focus = FALSE;
      event->crossing.state = modifiers;
    }

   /* Traverse up from a to (excluding) c */
  if (c != a)
    {
      last = a;
      win = GDK_WINDOW (GDK_WINDOW_OBJECT (a)->parent);
      while (win != c)
        {
          event_win =
            gdk_directfb_pointer_event_window (win, GDK_LEAVE_NOTIFY);

          if (event_win)
            {
              event = gdk_directfb_event_make (event_win, GDK_LEAVE_NOTIFY);

              event->crossing.subwindow = g_object_ref (last);

              gdk_window_get_origin (win, &x_int, &y_int);

              event->crossing.x      = x - x_int;
              event->crossing.y      = y - y_int;
              event->crossing.x_root = x;
              event->crossing.y_root = y;
              event->crossing.mode   = mode;

              if (non_linear)
                event->crossing.detail = GDK_NOTIFY_NONLINEAR_VIRTUAL;
              else
                event->crossing.detail = GDK_NOTIFY_VIRTUAL;

              event->crossing.focus = FALSE;
              event->crossing.state = modifiers;
            }

          last = win;
          win = GDK_WINDOW (GDK_WINDOW_OBJECT (win)->parent);
        }
    }

  /* Traverse down from c to b */
  if (c != b)
    {
      path = NULL;
      win = GDK_WINDOW (GDK_WINDOW_OBJECT (b)->parent);
      while (win != c)
        {
          path = g_slist_prepend (path, win);
          win = GDK_WINDOW (GDK_WINDOW_OBJECT (win)->parent);
        }

      list = path;
      while (list)
        {
          win = GDK_WINDOW (list->data);
          list = g_slist_next (list);

          if (list)
            next = GDK_WINDOW (list->data);
          else
            next = b;

          event_win =
            gdk_directfb_pointer_event_window (win, GDK_ENTER_NOTIFY);

          if (event_win)
            {
              event = gdk_directfb_event_make (event_win, GDK_ENTER_NOTIFY);

              event->crossing.subwindow = g_object_ref (next);

              gdk_window_get_origin (win, &x_int, &y_int);

              event->crossing.x      = x - x_int;
              event->crossing.y      = y - y_int;
              event->crossing.x_root = x;
              event->crossing.y_root = y;
              event->crossing.mode   = mode;

              if (non_linear)
                event->crossing.detail = GDK_NOTIFY_NONLINEAR_VIRTUAL;
              else
                event->crossing.detail = GDK_NOTIFY_VIRTUAL;

              event->crossing.focus = FALSE;
              event->crossing.state = modifiers;
            }
        }

      g_slist_free (path);
    }

  event_win = gdk_directfb_pointer_event_window (b, GDK_ENTER_NOTIFY);
  if (event_win)
    {
      event = gdk_directfb_event_make (event_win, GDK_ENTER_NOTIFY);

      event->crossing.subwindow = NULL;

      gdk_window_get_origin (b, &x_int, &y_int);

      event->crossing.x      = x - x_int;
      event->crossing.y      = y - y_int;
      event->crossing.x_root = x;
      event->crossing.y_root = y;
      event->crossing.mode   = mode;

      if (non_linear)
        event->crossing.detail = GDK_NOTIFY_NONLINEAR;
      else if (c==a)
        event->crossing.detail = GDK_NOTIFY_ANCESTOR;
      else
        event->crossing.detail = GDK_NOTIFY_INFERIOR;

      event->crossing.focus = FALSE;
      event->crossing.state = modifiers;
    }

  if (mode != GDK_CROSSING_GRAB)
    {
      //this seems to cause focus to change as the pointer moves yuck
      //gdk_directfb_change_focus (b);
      if (b != gdk_directfb_window_containing_pointer)
        {
          g_object_unref (gdk_directfb_window_containing_pointer);
          gdk_directfb_window_containing_pointer = g_object_ref (b);
        }
    }
}

static void
show_window_internal (GdkWindow *window,
                      gboolean   raise)
{
  GdkWindowObject       *private;
  GdkWindowImplDirectFB *impl;
  GdkWindow             *mousewin;

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_DIRECTFB (private->impl);

  if (!private->destroyed && !GDK_WINDOW_IS_MAPPED (private))
    {
      private->state &= ~GDK_WINDOW_STATE_WITHDRAWN;

      if (raise)
        gdk_window_raise (window);

      if (all_parents_shown (GDK_WINDOW_OBJECT (private)->parent))
        {
          send_map_events (private);

          mousewin = gdk_window_at_pointer (NULL, NULL);
          gdk_directfb_window_send_crossing_events (NULL, mousewin,
                                                    GDK_CROSSING_NORMAL);

          if (private->input_only)
            return;

          gdk_window_invalidate_rect (window, NULL, TRUE);
        }
    }

  if (impl->window)
    {
      if (gdk_directfb_apply_focus_opacity)
	impl->window->SetOpacity (impl->window,
				  (impl->opacity >> 1) + (impl->opacity >> 2));
      else
	impl->window->SetOpacity (impl->window, impl->opacity);
	  /* if its the first window focus it */
    }
}

void
gdk_window_show_unraised (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  show_window_internal (window, FALSE);
}

void
gdk_window_show (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  show_window_internal (window, TRUE);
}

void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowObject       *private;
  GdkWindowImplDirectFB *impl;
  GdkWindow             *mousewin;
  GdkWindow             *event_win;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = GDK_WINDOW_OBJECT (window);
  impl    = GDK_WINDOW_IMPL_DIRECTFB (private->impl);

  if (impl->window)
    impl->window->SetOpacity (impl->window, 0);

  if (!private->destroyed && GDK_WINDOW_IS_MAPPED (private))
    {
      GdkEvent *event;

      private->state |= GDK_WINDOW_STATE_WITHDRAWN;

      if (!private->input_only && private->parent)
        {
          _gdk_windowing_window_clear_area (GDK_WINDOW (private->parent),
                                            private->x,
                                            private->y,
                                            impl->drawable.width,
                                            impl->drawable.height);
        }

      event_win = gdk_directfb_other_event_window (window, GDK_UNMAP);
      if (event_win)
        event = gdk_directfb_event_make (event_win, GDK_UNMAP);

      mousewin = gdk_window_at_pointer (NULL, NULL);
      gdk_directfb_window_send_crossing_events (NULL,
                                                mousewin,
                                                GDK_CROSSING_NORMAL);

      if (window == _gdk_directfb_pointer_grab_window)
        gdk_pointer_ungrab (GDK_CURRENT_TIME);
      if (window == _gdk_directfb_keyboard_grab_window)
        gdk_keyboard_ungrab (GDK_CURRENT_TIME);
    }
}

void
gdk_window_withdraw (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* for now this should be enough */
  gdk_window_hide (window);
}

void
gdk_window_move (GdkWindow *window,
                 gint       x,
                 gint       y)
{
  GdkWindowObject       *private;
  GdkWindowImplDirectFB *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = GDK_WINDOW_OBJECT (window);
  impl    = GDK_WINDOW_IMPL_DIRECTFB (private->impl);

  if (impl->window)
    {
  	  private->x = x;
  	  private->y = y;
      impl->window->MoveTo (impl->window, x, y);
    }
  else
    {
	  gint width=impl->drawable.width;
	  gint height=impl->drawable.height;
      GdkRectangle  old =
      { private->x, private->y,width,height };

      _gdk_directfb_move_resize_child (window, x, y, width, height);
      _gdk_directfb_calc_abs (window);

      if (GDK_WINDOW_IS_MAPPED (private))
        {
          GdkWindow    *mousewin;
          GdkRectangle  new = { x, y, width, height };

          gdk_rectangle_union (&new, &old, &new);
          gdk_window_invalidate_rect (GDK_WINDOW (private->parent), &new,TRUE);

          /* The window the pointer is in might have changed */
          mousewin = gdk_window_at_pointer (NULL, NULL);
          gdk_directfb_window_send_crossing_events (NULL, mousewin,
                                                    GDK_CROSSING_NORMAL);
        }
    }
}

void
gdk_window_resize (GdkWindow *window,
                   gint       width,
                   gint       height)
{
  GdkWindowObject *private;
  gint             x, y;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = GDK_WINDOW_OBJECT (window);

  x = private->x;
  y = private->y;

  if (private->parent && (private->parent->window_type != GDK_WINDOW_CHILD))
    {
      GdkWindowChildHandlerData *data;

      data = g_object_get_data (G_OBJECT (private->parent),
				"gdk-window-child-handler");

      if (data)
	(*data->get_pos) (window, &x, &y, data->user_data);
    }

  gdk_window_move_resize (window, x, y, width, height);
}

void
_gdk_directfb_move_resize_child (GdkWindow *window,
                                 gint       x,
                                 gint       y,
                                 gint       width,
                                 gint       height)
{
  GdkWindowObject       *private;
  GdkWindowImplDirectFB *impl;
  GdkWindowImplDirectFB *parent_impl;
  GList                 *list;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = GDK_WINDOW_OBJECT (window);
  impl    = GDK_WINDOW_IMPL_DIRECTFB (private->impl);

  private->x = x;
  private->y = y;

  impl->drawable.width  = width;
  impl->drawable.height = height;

  if (!private->input_only)
    {
    if (impl->drawable.surface) {
      GdkDrawableImplDirectFB *dimpl = GDK_DRAWABLE_IMPL_DIRECTFB (private->impl);
      if(dimpl->cairo_surface) {
          cairo_surface_destroy(dimpl->cairo_surface);
          dimpl->cairo_surface= NULL;
        }
    impl->drawable.surface->Release (impl->drawable.surface);
    impl->drawable.surface = NULL;
  }

      parent_impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (private->parent)->impl);

      if (parent_impl->drawable.surface)
        {
          DFBRectangle rect = { x, y, width, height };

          parent_impl->drawable.surface->GetSubSurface (parent_impl->drawable.surface,
                                                        &rect,
                                                        &impl->drawable.surface);
        }
    }

  for (list = private->children; list; list = list->next)
    {
      private = GDK_WINDOW_OBJECT (list->data);
  	  impl  = GDK_WINDOW_IMPL_DIRECTFB (private->impl);
      _gdk_directfb_move_resize_child (list->data,
                                       private->x, private->y,
                                       impl->drawable.width, impl->drawable.height);
    }
}

void
gdk_window_move_resize (GdkWindow *window,
                        gint       x,
                        gint       y,
                        gint       width,
                        gint       height)
{
  GdkWindowObject       *private;
  GdkWindowImplDirectFB *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_DIRECTFB (private->impl);

  if (width < 1)
     width = 1;
  if (height < 1)
    height = 1;

  if (private->destroyed ||
      (private->x == x  &&  private->y == y  &&
       impl->drawable.width == width  &&  impl->drawable.height == height))
    return;

  if (private->parent && (private->parent->window_type != GDK_WINDOW_CHILD))
    {
      GdkWindowChildHandlerData *data;

      data = g_object_get_data (G_OBJECT (private->parent),
                                "gdk-window-child-handler");

      if (data &&
          (*data->changed) (window, x, y, width, height, data->user_data))
        return;
    }

  if (impl->drawable.width == width  &&  impl->drawable.height == height)
    {
      gdk_window_move (window, x, y);
    }
  else if (impl->window)
    {
  	  private->x = x;
  	  private->y = y;
      impl->window->MoveTo (impl->window, x, y);
      impl->window->Resize (impl->window, width, height);
    }
  else
    {
      GdkRectangle  old =
      { private->x, private->y, impl->drawable.width, impl->drawable.height };
      _gdk_directfb_move_resize_child (window, x, y, width, height);
      _gdk_directfb_calc_abs (window);

      if (GDK_WINDOW_IS_MAPPED (private))
        {
          GdkWindow    *mousewin;
          GdkRectangle  new = { x, y, width, height };

          gdk_rectangle_union (&new, &old, &new);
          gdk_window_invalidate_rect (GDK_WINDOW (private->parent), &new,TRUE);

          /* The window the pointer is in might have changed */
          mousewin = gdk_window_at_pointer (NULL, NULL);
          gdk_directfb_window_send_crossing_events (NULL, mousewin,
                                                    GDK_CROSSING_NORMAL);
        }
    }
}

void
gdk_window_reparent (GdkWindow *window,
                     GdkWindow *new_parent,
                     gint       x,
                     gint       y)
{
  GdkWindowObject *window_private;
  GdkWindowObject *parent_private;
  GdkWindowObject *old_parent_private;
  GdkWindowImplDirectFB *impl;
  GdkWindowImplDirectFB *parent_impl;
  GdkVisual             *visual;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (!new_parent)
    new_parent = _gdk_parent_root;

  window_private = (GdkWindowObject *) window;
  old_parent_private = (GdkWindowObject *) window_private->parent;
  parent_private = (GdkWindowObject *) new_parent;
  parent_impl = GDK_WINDOW_IMPL_DIRECTFB (parent_private->impl);
  visual = gdk_drawable_get_visual (window);

  /* already parented */
  if( window_private->parent == (GdkWindowObject *)new_parent )
          return;

  window_private->parent = (GdkWindowObject *) new_parent;

  if (old_parent_private)
    {
      old_parent_private->children =
        g_list_remove (old_parent_private->children, window);
    }

    parent_private->children = g_list_prepend (parent_private->children, window);

    impl = GDK_WINDOW_IMPL_DIRECTFB (window_private->impl);

    if( impl->drawable.surface ) {
        impl->drawable.surface->Release (impl->drawable.surface);
        impl->drawable.surface = NULL;
    }

    if( impl->window != NULL ) { 
        gdk_directfb_window_id_table_remove (impl->dfb_id);
    	impl->window->SetOpacity (impl->window,0);
   		impl->window->Close(impl->window);
      	impl->window->Release(impl->window);
        impl->window = NULL;
    }

    //create window were a child of the root now
    if( window_private->parent == (GdkWindowObject *)_gdk_parent_root)  {
        DFBWindowDescription  desc;
        DFBWindowOptions  window_options = DWOP_NONE;
        desc.flags = DWDESC_CAPS;
        if( window_private->input_only ) {
            desc.caps = DWCAPS_INPUTONLY;
        } else {
            desc.flags |= DWDESC_PIXELFORMAT;
            desc.pixelformat = ((GdkVisualDirectFB *) visual)->format;
            if (DFB_PIXELFORMAT_HAS_ALPHA (desc.pixelformat)) {
                desc.flags |= DWDESC_CAPS;
                desc.caps = DWCAPS_ALPHACHANNEL;
            }
       }
       if( window_private->window_type ==  GDK_WINDOW_CHILD )
           window_private->window_type = GDK_WINDOW_TOPLEVEL;
        desc.flags |= ( DWDESC_WIDTH | DWDESC_HEIGHT |
                      DWDESC_POSX  | DWDESC_POSY );
        desc.posx   = x;
        desc.posy   = y;
        desc.width  = impl->drawable.width;
        desc.height = impl->drawable.height;
        if (!create_directfb_window (impl, &desc, window_options))
        {
		  g_assert(0);
          _gdk_window_destroy (window, FALSE);
          return;
        }
        /* we hold a reference count on ourselves */
        g_object_ref (window);
        impl->window->GetID (impl->window, &impl->dfb_id);
        gdk_directfb_window_id_table_insert (impl->dfb_id, window);
        gdk_directfb_event_windows_add (window);
   } else {
          DFBRectangle rect = { x, y, impl->drawable.width,
                                     impl->drawable.height};
          impl->window = NULL;
          parent_impl->drawable.surface->GetSubSurface (
                          parent_impl->drawable.surface,
                          &rect,
                          &impl->drawable.surface);
   }
}

void
_gdk_windowing_window_clear_area (GdkWindow *window,
                                  gint       x,
                                  gint       y,
                                  gint       width,
                                  gint       height)
{
  GdkWindowObject         *private;
  GdkDrawableImplDirectFB *impl;
  GdkPixmap               *bg_pixmap;
  GdkWindowObject         *relative_to;
  GdkGC                   *gc = NULL;
  gint                     dx = 0;
  gint                     dy = 0;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  private = GDK_WINDOW_OBJECT (window);

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (private->impl);

  /**
	Follow XClearArea definition for zero height width
  **/
  if( width == 0 )  
		width = impl->width-x;
  if( height == 0 )  
		height = impl->height-y;

  bg_pixmap = private->bg_pixmap;

  for (relative_to = private;
       relative_to && bg_pixmap == GDK_PARENT_RELATIVE_BG;
       relative_to = relative_to->parent)
    {
      bg_pixmap = relative_to->bg_pixmap;
      dx += relative_to->x;
      dy += relative_to->y;
    }

  if (bg_pixmap == GDK_NO_BG)
    return;

  if (bg_pixmap && bg_pixmap != GDK_PARENT_RELATIVE_BG)
    {
      GdkGCValues  values;

      values.fill = GDK_TILED;
      values.tile = bg_pixmap;
      values.ts_x_origin = - dx;
      values.ts_y_origin = - dy;

      gc = gdk_gc_new_with_values (GDK_DRAWABLE (impl), &values,
                                   GDK_GC_FILL | GDK_GC_TILE |
                                   GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN);
    }
  else 
    {
      /* GDK_PARENT_RELATIVE_BG, but no pixmap,
         get the color from the parent window. */

      GdkGCValues  values;

      values.foreground = relative_to->bg_color;

      gc = gdk_gc_new_with_values (GDK_DRAWABLE (impl), &values,
                                   GDK_GC_FOREGROUND);
    }

  gdk_draw_rectangle (GDK_DRAWABLE (impl),
                                gc, TRUE, x, y, width, height);

  if (gc)
    g_object_unref (gc);
}

void
_gdk_windowing_window_clear_area_e (GdkWindow *window,
                                    gint       x,
                                    gint       y,
                                    gint       width,
                                    gint       height)
{
  GdkRectangle  rect;
  GdkWindowObject       *private;
  GdkWindowImplDirectFB *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = GDK_WINDOW_OBJECT (window);
  impl    = GDK_WINDOW_IMPL_DIRECTFB (private->impl);

  /**
	Follow XClearArea definition for zero height width
  **/
  if( width == 0 )  
		width = impl->drawable.width-x;
  if( height == 0 )  
		height = impl->drawable.height-y;

  rect.x = x;
  rect.y = y;
  rect.width = width;
  rect.height = height;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  _gdk_windowing_window_clear_area (window, x, y, width, height);

  gdk_window_invalidate_rect (window, &rect, TRUE);
}

void
gdk_window_raise (GdkWindow *window)
{
  GdkWindowImplDirectFB *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (window)->impl);

  if (impl->window)
    {
      DFBResult ret;

      ret = impl->window->RaiseToTop (impl->window);
      if (ret)
        DirectFBError ("gdkwindow-directfb.c: RaiseToTop", ret);
      else
        gdk_directfb_window_raise (window);
    }
  else
    {
      if (gdk_directfb_window_raise (window))
        gdk_window_invalidate_rect (window, NULL, TRUE);
    }
}

void
gdk_window_lower (GdkWindow *window)
{
  GdkWindowImplDirectFB *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (window)->impl);

  if (impl->window)
    {
      DFBResult ret;

      ret = impl->window->LowerToBottom (impl->window);
      if (ret)
        DirectFBError ("gdkwindow-directfb.c: LowerToBottom", ret);
      else
        gdk_directfb_window_lower (window);
    }
  else
    {
      gdk_directfb_window_lower (window);
      gdk_window_invalidate_rect (window, NULL, TRUE);
    }
}

void
gdk_window_set_hints (GdkWindow *window,
                      gint       x,
                      gint       y,
                      gint       min_width,
                      gint       min_height,
                      gint       max_width,
                      gint       max_height,
                      gint       flags)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* N/A */
}

void
gdk_window_set_geometry_hints (GdkWindow      *window,
                               GdkGeometry    *geometry,
                               GdkWindowHints  geom_mask)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* N/A */
}

void
gdk_window_set_title (GdkWindow   *window,
                      const gchar *title)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* N/A */
}

void
gdk_window_set_role (GdkWindow   *window,
                     const gchar *role)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* N/A */
}

void
gdk_window_set_transient_for (GdkWindow *window,
                              GdkWindow *parent)
{
  GdkWindowObject *private;
  GdkWindowObject *root;
  gint i;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_WINDOW (parent));

  private = GDK_WINDOW_OBJECT (window);
  root    = GDK_WINDOW_OBJECT (_gdk_parent_root);

  g_return_if_fail (GDK_WINDOW (private->parent) == _gdk_parent_root);
  g_return_if_fail (GDK_WINDOW (GDK_WINDOW_OBJECT (parent)->parent) == _gdk_parent_root);

  root->children = g_list_remove (root->children, window);

  i = g_list_index (root->children, parent);
  if (i < 0)
    root->children = g_list_prepend (root->children, window);
  else
    root->children = g_list_insert (root->children, window, i);
}

void
gdk_window_set_background (GdkWindow *window,
                           const GdkColor  *color)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  g_return_if_fail (color != NULL);

  private = GDK_WINDOW_OBJECT (window);
  private->bg_color = *color;

  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    g_object_unref (private->bg_pixmap);

  private->bg_pixmap = NULL;
}

void
gdk_window_set_back_pixmap (GdkWindow *window,
                            GdkPixmap *pixmap,
                            gint       parent_relative)
{
  GdkWindowObject *private;
  GdkPixmap       *old_pixmap;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (pixmap == NULL || !parent_relative);

  private = GDK_WINDOW_OBJECT (window);
  old_pixmap = private->bg_pixmap;

  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    {
      g_object_unref (private->bg_pixmap);
    }

  if (parent_relative)
    {
      private->bg_pixmap = GDK_PARENT_RELATIVE_BG;
    }
  else
    {
      if (pixmap && pixmap != GDK_NO_BG && pixmap != GDK_PARENT_RELATIVE_BG)
        g_object_ref (pixmap);

      private->bg_pixmap = pixmap;
    }
}

void
gdk_window_set_cursor (GdkWindow *window,
                       GdkCursor *cursor)
{
  GdkWindowImplDirectFB *impl;
  GdkCursor             *old_cursor;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (window)->impl);
  old_cursor = impl->cursor;

  impl->cursor = (cursor ?
                  gdk_cursor_ref (cursor) : gdk_cursor_new (GDK_LEFT_PTR));

  if (gdk_window_at_pointer (NULL, NULL) == window)
    {
      /* This is a bit evil but we want to keep all cursor changes in
         one place, so let gdk_directfb_window_send_crossing_events
         do the work for us. */

      gdk_directfb_window_send_crossing_events (window, window,
                                                GDK_CROSSING_NORMAL);
    }
  else if (impl->window)
    {
      GdkCursorDirectFB *dfb_cursor = (GdkCursorDirectFB *) impl->cursor;

      /* this branch takes care of setting the cursor for unmapped windows */

      impl->window->SetCursorShape (impl->window,
                                    dfb_cursor->shape,
                                    dfb_cursor->hot_x, dfb_cursor->hot_y);
    }

  if (old_cursor)
    gdk_cursor_unref (old_cursor);
}

void
gdk_window_get_geometry (GdkWindow *window,
                         gint      *x,
                         gint      *y,
                         gint      *width,
                         gint      *height,
                         gint      *depth)
{
  GdkWindowObject         *private;
  GdkDrawableImplDirectFB *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = GDK_WINDOW_OBJECT (window);
  impl    = GDK_DRAWABLE_IMPL_DIRECTFB (private->impl);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (x)
	*x = private->x;

      if (y)
	*y = private->y;

      if (width)
	*width = impl->width;

      if (height)
	*height = impl->height;

      if (depth)
	*depth = DFB_BITS_PER_PIXEL(impl->format);
    }
}

void
_gdk_directfb_calc_abs (GdkWindow *window)
{
  GdkWindowObject         *private;
  GdkDrawableImplDirectFB *impl;
  GList                   *list;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_DRAWABLE_IMPL_DIRECTFB (private->impl);

  impl->abs_x = private->x;
  impl->abs_y = private->y;

  if (private->parent)
    {
      GdkDrawableImplDirectFB *parent_impl =
        GDK_DRAWABLE_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (private->parent)->impl);

      impl->abs_x += parent_impl->abs_x;
      impl->abs_y += parent_impl->abs_y;
    }

  for (list = private->children; list; list = list->next)
    {
      _gdk_directfb_calc_abs (list->data);
    }
}

gboolean
gdk_window_get_origin (GdkWindow *window,
                       gint      *x,
                       gint      *y)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      GdkDrawableImplDirectFB *impl;

      impl = GDK_DRAWABLE_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (window)->impl);

      if (x)
	*x = impl->abs_x;
      if (y)
	*y = impl->abs_y;

      return TRUE;
    }

  return FALSE;
}

gboolean
gdk_window_get_deskrelative_origin (GdkWindow *window,
                                    gint      *x,
                                    gint      *y)
{
  return gdk_window_get_origin (window, x, y);
}

void
gdk_window_get_root_origin (GdkWindow *window,
                            gint      *x,
                            gint      *y)
{
  GdkWindowObject *rover;

  g_return_if_fail (GDK_IS_WINDOW (window));

  rover = (GdkWindowObject*) window;
  if (x)
    *x = 0;
  if (y)
    *y = 0;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  while (rover->parent && ((GdkWindowObject*) rover->parent)->parent)
    rover = (GdkWindowObject *) rover->parent;
  if (rover->destroyed)
    return;

  if (x)
    *x = rover->x;
  if (y)
    *y = rover->y;
}

GdkWindow *
_gdk_windowing_window_get_pointer (GdkDisplay      *display,
                                   GdkWindow       *window,
				   gint            *x,
				   gint            *y,
				   GdkModifierType *mask)
{
  GdkWindow               *retval = NULL;
  gint                     rx, ry, wx, wy;
  GdkDrawableImplDirectFB *impl;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), NULL);

  if (!window)
    window = _gdk_parent_root;

  gdk_directfb_mouse_get_info (&rx, &ry, mask);

  wx = rx;
  wy = ry;
  retval = gdk_directfb_child_at (_gdk_parent_root, &wx, &wy);

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (window)->impl);

  if (x)
    *x = rx - impl->abs_x;
  if (y)
    *y = ry - impl->abs_y;

  return retval;
}

GdkWindow *
_gdk_windowing_window_at_pointer (GdkDisplay *display,
                                  gint       *win_x,
				  gint       *win_y)
{
  GdkWindow *retval;
  gint       wx, wy;

  if (!win_x || !win_y)
  gdk_directfb_mouse_get_info (&wx, &wy, NULL);

  if (win_x)
    wx = *win_x;

  if (win_y)
    wy = *win_y;

  retval = gdk_directfb_child_at (_gdk_parent_root, &wx, &wy);

  if (win_x)
    *win_x = wx;

  if (win_y)
    *win_y = wy;

  return retval;
}

void
_gdk_windowing_get_pointer (GdkDisplay       *display,
                            GdkScreen       **screen,
                            gint             *x,
                            gint             *y,
                            GdkModifierType  *mask)
{
(void)screen;
if(screen) {
	*screen = gdk_display_get_default_screen  (display);
}
_gdk_windowing_window_get_pointer (display,
				   _gdk_windowing_window_at_pointer(display,NULL,NULL),x,y,mask);

}

GdkEventMask
gdk_window_get_events (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (GDK_WINDOW_DESTROYED (window))
    return 0;
  else
    return GDK_WINDOW_OBJECT (window)->event_mask;
}

void
gdk_window_set_events (GdkWindow    *window,
                       GdkEventMask  event_mask)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (event_mask & GDK_BUTTON_MOTION_MASK)
    event_mask |= (GDK_BUTTON1_MOTION_MASK |
                   GDK_BUTTON2_MOTION_MASK |
                   GDK_BUTTON3_MOTION_MASK);

  GDK_WINDOW_OBJECT (window)->event_mask = event_mask;
}

void
gdk_window_shape_combine_mask (GdkWindow *window,
                               GdkBitmap *mask,
                               gint       x,
                               gint       y)
{
}

void
gdk_window_input_shape_combine_mask (GdkWindow *window,
                                    GdkBitmap *mask,
                                    gint       x,
                                    gint       y)
{
}

void
gdk_window_shape_combine_region (GdkWindow *window,
                                 GdkRegion *shape_region,
                                 gint       offset_x,
                                 gint       offset_y)
{
}

void
gdk_window_input_shape_combine_region (GdkWindow *window,
                                      GdkRegion *shape_region,
                                      gint       offset_x,
                                      gint       offset_y)
{
}

void
gdk_window_set_override_redirect (GdkWindow *window,
                                  gboolean   override_redirect)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* N/A */
}

void
gdk_window_set_icon_list (GdkWindow *window,
                          GList     *pixbufs)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* N/A */
}

void
gdk_window_set_icon (GdkWindow *window,
                     GdkWindow *icon_window,
                     GdkPixmap *pixmap,
                     GdkBitmap *mask)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* N/A */
}

void
gdk_window_set_icon_name (GdkWindow   *window,
                          const gchar *name)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* N/A */
}

void
gdk_window_iconify (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  gdk_window_hide (window);
}

void
gdk_window_deiconify (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  gdk_window_show (window);
}

void
gdk_window_stick (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* N/A */
}

void
gdk_window_unstick (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* N/A */
}

void
gdk_directfb_window_set_opacity (GdkWindow *window,
                                 guchar     opacity)
{
  GdkWindowImplDirectFB *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (window)->impl);

  impl->opacity = opacity;

  if (impl->window && GDK_WINDOW_IS_MAPPED (window))
    {
      if (gdk_directfb_apply_focus_opacity &&
	  window == gdk_directfb_focused_window)
	impl->window->SetOpacity (impl->window,
				  (impl->opacity >> 1) + (impl->opacity >> 2));
      else
	impl->window->SetOpacity (impl->window, impl->opacity);
    }
}

void
gdk_window_focus (GdkWindow *window,
                  guint32    timestamp)
{
  GdkWindow *toplevel;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  toplevel = gdk_directfb_window_find_toplevel (window);
  if (toplevel != _gdk_parent_root)
    {
      GdkWindowImplDirectFB *impl;

      impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (toplevel)->impl);

      impl->window->RequestFocus (impl->window);
    }
}

void
gdk_window_maximize (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* N/A */
}

void
gdk_window_unmaximize (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* N/A */
}

void
gdk_window_set_type_hint (GdkWindow        *window,
                          GdkWindowTypeHint hint)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_set_type_hint: 0x%x: %d\n",
               GDK_WINDOW_DFB_ID (window), hint));

  ((GdkWindowImplDirectFB *)((GdkWindowObject *)window)->impl)->type_hint = hint;


  /* N/A */
}

GdkWindowTypeHint
gdk_window_get_type_hint (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), GDK_WINDOW_TYPE_HINT_NORMAL);
  
  if (GDK_WINDOW_DESTROYED (window))
    return GDK_WINDOW_TYPE_HINT_NORMAL;

  return GDK_WINDOW_IMPL_DIRECTFB (((GdkWindowObject *) window)->impl)->type_hint;
}

void
gdk_window_set_modal_hint (GdkWindow *window,
                           gboolean   modal)
{
  GdkWindowImplDirectFB *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (window)->impl);

  if (impl->window)
    {
      impl->window->SetStackingClass (impl->window,
                                      modal ? DWSC_UPPER : DWSC_MIDDLE);
    }
}

void
gdk_window_set_skip_taskbar_hint (GdkWindow *window,
				  gboolean   skips_taskbar)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
}

void
gdk_window_set_skip_pager_hint (GdkWindow *window,
				gboolean   skips_pager)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
}


void
gdk_window_set_group (GdkWindow *window,
                      GdkWindow *leader)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_WINDOW (leader));
 g_warning(" DirectFb set_group groups not supported \n");

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* N/A */
}

GdkWindow * gdk_window_get_group (GdkWindow *window)
{
 g_warning(" DirectFb get_group groups not supported \n");
 return window;	
}

void
gdk_fb_window_set_child_handler (GdkWindow             *window,
                                 GdkWindowChildChanged  changed,
                                 GdkWindowChildGetPos   get_pos,
                                 gpointer               user_data)
{
  GdkWindowChildHandlerData *data;

  g_return_if_fail (GDK_IS_WINDOW (window));

  data = g_new (GdkWindowChildHandlerData, 1);
  data->changed   = changed;
  data->get_pos   = get_pos;
  data->user_data = user_data;

  g_object_set_data_full (G_OBJECT (window), "gdk-window-child-handler",
                          data, (GDestroyNotify) g_free);
}

void
gdk_window_set_decorations (GdkWindow       *window,
                            GdkWMDecoration  decorations)
{
  GdkWMDecoration *dec;

  g_return_if_fail (GDK_IS_WINDOW (window));

  dec = g_new (GdkWMDecoration, 1);
  *dec = decorations;

  g_object_set_data_full (G_OBJECT (window), "gdk-window-decorations",
                          dec, (GDestroyNotify) g_free);
}

gboolean
gdk_window_get_decorations (GdkWindow       *window,
			    GdkWMDecoration *decorations)
{
  GdkWMDecoration *dec;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  dec = g_object_get_data (G_OBJECT (window), "gdk-window-decorations");
  if (dec)
    {
      *decorations = *dec;
      return TRUE;
    }
  return FALSE;
}

void
gdk_window_set_functions (GdkWindow     *window,
                          GdkWMFunction  functions)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* N/A */
  g_message("unimplemented %s", G_GNUC_FUNCTION);
}

void
gdk_window_set_child_shapes (GdkWindow *window)
{
}

void
gdk_window_merge_child_shapes (GdkWindow *window)
{
}

void
gdk_window_set_child_input_shapes (GdkWindow *window)
{
}

void
gdk_window_merge_child_input_shapes (GdkWindow *window)
{
}

gboolean
gdk_window_set_static_gravities (GdkWindow *window,
                                 gboolean   use_static)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;

  /* N/A */
  g_message("unimplemented %s", G_GNUC_FUNCTION);

  return FALSE;
}

void
gdk_window_begin_resize_drag (GdkWindow     *window,
                              GdkWindowEdge  edge,
                              gint           button,
                              gint           root_x,
                              gint           root_y,
                              guint32        timestamp)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  g_message("unimplemented %s", G_GNUC_FUNCTION);
}

void
gdk_window_begin_move_drag (GdkWindow *window,
                            gint       button,
                            gint       root_x,
                            gint       root_y,
                            guint32    timestamp)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  g_message("unimplemented %s", G_GNUC_FUNCTION);
}

/**
 * gdk_window_get_frame_extents:
 * @window: a #GdkWindow
 * @rect: rectangle to fill with bounding box of the window frame
 *
 * Obtains the bounding box of the window, including window manager
 * titlebar/borders if any. The frame position is given in root window
 * coordinates. To get the position of the window itself (rather than
 * the frame) in root window coordinates, use gdk_window_get_origin().
 *
 **/
void
gdk_window_get_frame_extents (GdkWindow    *window,
                              GdkRectangle *rect)
{
  GdkWindowObject         *private;
  GdkDrawableImplDirectFB *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (rect != NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  private = GDK_WINDOW_OBJECT (window);

  while (private->parent && ((GdkWindowObject*) private->parent)->parent)
    private = (GdkWindowObject*) private->parent;
  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (private->impl);

  rect->x      = impl->abs_x;
  rect->y      = impl->abs_x;
  rect->width  = impl->width;
  rect->height = impl->height;
}

/*
 * Given a directfb window and a subsurface of that window
 * create a gdkwindow child wrapper
 */
#if (DIRECTFB_MAJOR_VERSION >= 1)
GdkWindow *gdk_directfb_create_child_window(GdkWindow *parent,
                                IDirectFBSurface *subsurface)
{
  GdkWindow             *window;
  GdkWindowObject       *private;
  GdkWindowObject       *parent_private;
  GdkWindowImplDirectFB *impl;
  GdkWindowImplDirectFB *parent_impl;
  gint x,y,w,h;

  g_return_val_if_fail (parent != NULL, NULL);

  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = GDK_WINDOW_OBJECT (window);
  parent_private = GDK_WINDOW_OBJECT (parent);
  parent_impl = GDK_WINDOW_IMPL_DIRECTFB (parent_private->impl);
  private->parent = parent_private;

  subsurface->GetPosition(subsurface,&x,&y);
  subsurface->GetSize(subsurface,&w,&h);

  impl = GDK_WINDOW_IMPL_DIRECTFB (private->impl);
  impl->drawable.wrapper = GDK_DRAWABLE (window);

  private->x = x;
  private->y = y;

  _gdk_directfb_calc_abs (window);

  impl->drawable.width  = w;
  impl->drawable.height = h;
  private->window_type = GDK_WINDOW_CHILD;
  impl->drawable.surface = subsurface;
  impl->drawable.format = parent_impl->drawable.format;
  private->depth = parent_private->depth;
  gdk_drawable_set_colormap (GDK_DRAWABLE (window),
        gdk_drawable_get_colormap (parent));
  gdk_window_set_cursor (window, NULL);
  parent_private->children = g_list_prepend (parent_private->children,window);
  /*we hold a reference count on ourselves */
  g_object_ref (window);

  return window;

}
#endif

/*
 * The wrapping is not perfect since directfb does not give full access
 * to the current state of a window event mask etc need to fix dfb
 */
GdkWindow *
gdk_window_foreign_new_for_display (GdkDisplay* display,GdkNativeWindow anid)
{
    GdkWindow *window = NULL;
    GdkWindow              *parent =NULL;
    GdkWindowObject       *private =NULL;
    GdkWindowObject       *parent_private =NULL;
    GdkWindowImplDirectFB *parent_impl =NULL;
    GdkWindowImplDirectFB *impl =NULL;
    DFBWindowOptions options;
    DFBResult        ret;
    GdkDisplayDFB * gdkdisplay =  _gdk_display;
    IDirectFBWindow *dfbwindow;

    window = gdk_window_lookup (anid);

    if (window) {
        g_object_ref (window);
        return window;
    }
    if( display != NULL )
            gdkdisplay = GDK_DISPLAY_DFB(display);

    ret = gdkdisplay->layer->GetWindow (gdkdisplay->layer,
                    (DFBWindowID)anid,&dfbwindow);

    if (ret != DFB_OK) {
        DirectFBError ("gdk_window_new: Layer->GetWindow failed", ret);
        return NULL;
    }

    parent = _gdk_parent_root;

    if(parent) {
        parent_private = GDK_WINDOW_OBJECT (parent);
        parent_impl = GDK_WINDOW_IMPL_DIRECTFB (parent_private->impl);
    }

    window = g_object_new (GDK_TYPE_WINDOW, NULL);
    /* we hold a reference count on ourselves */
    g_object_ref (window);
    private = GDK_WINDOW_OBJECT (window);
    private->parent = parent_private;
    private->window_type = GDK_WINDOW_TOPLEVEL;
    impl = GDK_WINDOW_IMPL_DIRECTFB (private->impl);

    impl->drawable.wrapper = GDK_DRAWABLE (window);
    impl->window = dfbwindow;
    dfbwindow->GetOptions(dfbwindow,&options);
    dfbwindow->GetPosition(dfbwindow,&private->x,&private->y);
    dfbwindow->GetSize(dfbwindow,&impl->drawable.width,&impl->drawable.height);


    private->input_only = FALSE;

    if( dfbwindow->GetSurface (dfbwindow, &impl->drawable.surface) == DFB_UNSUPPORTED ){
        private->input_only = TRUE;
        impl->drawable.surface = NULL;
    }
    /*
     * Position ourselevs
     */
    _gdk_directfb_calc_abs (window);

    /* We default to all events least surprise to the user 
     * minus the poll for motion events 
     */
    gdk_window_set_events (window, (GDK_ALL_EVENTS_MASK ^ GDK_POINTER_MOTION_HINT_MASK));

    if (impl->drawable.surface)
    {
      impl->drawable.surface->GetPixelFormat (impl->drawable.surface,
					      &impl->drawable.format);

  	  private->depth = DFB_BITS_PER_PIXEL(impl->drawable.format);
      if( parent )
        gdk_drawable_set_colormap (GDK_DRAWABLE (window), gdk_drawable_get_colormap (parent));
      else
        gdk_drawable_set_colormap (GDK_DRAWABLE (window), gdk_colormap_get_system());
    }

    //can  be null for the soft cursor window itself when
    //running a gtk directfb wm
    if( gdk_display_get_default() != NULL ) {
        gdk_window_set_cursor (window,NULL);
    }

    if (parent_private)
        parent_private->children = g_list_prepend (parent_private->children,
                                               window);
    impl->dfb_id = (DFBWindowID)anid; 
    gdk_directfb_window_id_table_insert (impl->dfb_id, window);
    gdk_directfb_event_windows_add (window);

    return window;
}

GdkWindow *
gdk_window_lookup_for_display (GdkDisplay *display,GdkNativeWindow anid)
{
  return gdk_directfb_window_id_table_lookup ((DFBWindowID) anid);
}

GdkWindow *
gdk_window_lookup (GdkNativeWindow anid)
{
  return gdk_directfb_window_id_table_lookup ((DFBWindowID) anid);
}

IDirectFBWindow *gdk_directfb_window_lookup(GdkWindow *window )
{
  GdkWindowObject       *private;
  GdkWindowImplDirectFB *impl;
  g_return_val_if_fail (GDK_IS_WINDOW (window),NULL);
  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_DIRECTFB (private->impl);
  return impl->window;
}

IDirectFBSurface *gdk_directfb_surface_lookup(GdkWindow *window)
{
  GdkWindowObject       *private;
  GdkWindowImplDirectFB *impl;
  g_return_val_if_fail (GDK_IS_WINDOW (window),NULL);
  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_DIRECTFB (private->impl);
  return impl->drawable.surface;
}

void
gdk_window_fullscreen (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_warning ("gdk_window_fullscreen() not implemented.\n");
}

void
gdk_window_unfullscreen (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  /* g_warning ("gdk_window_unfullscreen() not implemented.\n");*/
}

void
gdk_window_set_keep_above (GdkWindow *window, gboolean setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  static gboolean first_call = TRUE;
  if (first_call) {
  g_warning ("gdk_window_set_keep_above() not implemented.\n");
	first_call=FALSE;
  }
	
}

void
gdk_window_set_keep_below (GdkWindow *window, gboolean setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  static gboolean first_call = TRUE;
  if (first_call) {
  g_warning ("gdk_window_set_keep_below() not implemented.\n");
  first_call=FALSE;
  }
  
}

void
gdk_window_enable_synchronized_configure (GdkWindow *window)
{
}

void
gdk_window_configure_finished (GdkWindow *window)
{
}

void
gdk_display_warp_pointer (GdkDisplay *display,
                          GdkScreen  *screen,
                          gint        x,
                          gint        y)
{
  g_warning ("gdk_display_warp_pointer() not implemented.\n");
}

void
gdk_window_set_urgency_hint (GdkWindow *window,
                             gboolean   urgent)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  g_warning ("gdk_window_set_urgency_hint() not implemented.\n");

}

static void
gdk_window_impl_directfb_invalidate_maybe_recurse (GdkPaintable *paintable,
                         GdkRegion    *region,
                         gboolean    (*child_func) (GdkWindow *, gpointer),
                         gpointer      user_data)
{
  GdkWindow *window;
  GdkWindowObject *private;
  GdkWindowImplDirectFB *wimpl;
  GdkDrawableImplDirectFB *impl;

  wimpl = GDK_WINDOW_IMPL_DIRECTFB (paintable);
  impl = (GdkDrawableImplDirectFB *)wimpl;
  window = wimpl->gdkWindow;
  private = (GdkWindowObject *)window;

  GdkRegion *visible_region;
  GList *tmp_list;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  if (private->input_only || !GDK_WINDOW_IS_MAPPED (window))
    return;

  visible_region = gdk_drawable_get_visible_region (window);
  gdk_region_intersect (visible_region, region);

  tmp_list = private->children;
  while (tmp_list)
    {
      GdkWindowObject *child = tmp_list->data;
      
      if (!child->input_only)
	{
	  GdkRegion *child_region;
	  GdkRectangle child_rect;
	  
	  gdk_window_get_position ((GdkWindow *)child,
				   &child_rect.x, &child_rect.y);
	  gdk_drawable_get_size ((GdkDrawable *)child,
				 &child_rect.width, &child_rect.height);

	  child_region = gdk_region_rectangle (&child_rect);
	  
	  /* remove child area from the invalid area of the parent */
	  if (GDK_WINDOW_IS_MAPPED (child) && !child->shaped)
	    gdk_region_subtract (visible_region, child_region);
	  
	  if (child_func && (*child_func) ((GdkWindow *)child, user_data))
	    {
	      gdk_region_offset (region, - child_rect.x, - child_rect.y);
	      gdk_region_offset (child_region, - child_rect.x, - child_rect.y);
	      gdk_region_intersect (child_region, region);
	      
	      gdk_window_invalidate_maybe_recurse ((GdkWindow *)child,
						   child_region, child_func, user_data);
	      
	      gdk_region_offset (region, child_rect.x, child_rect.y);
	    }

	  gdk_region_destroy (child_region);
	}

      tmp_list = tmp_list->next;
    }
  
  if (!gdk_region_empty (visible_region))
    {
      
      if (private->update_area)
	{
	  gdk_region_union (private->update_area, visible_region);
	}
      else
	{
	  update_windows = g_slist_prepend (update_windows, window);
	  private->update_area = gdk_region_copy (visible_region);
	  gdk_window_schedule_update (window);
	}
    }
    gdk_region_destroy (visible_region);
}


static void
gdk_window_impl_directfb_process_updates (GdkPaintable *paintable,
                    gboolean      update_children)
{
  GdkWindow *window;
  GdkWindowObject *private;
  GdkWindowImplDirectFB *wimpl;
  GdkDrawableImplDirectFB *impl;

  wimpl = GDK_WINDOW_IMPL_DIRECTFB (paintable);
  impl = (GdkDrawableImplDirectFB *)wimpl;
  window = wimpl->gdkWindow;
  private = (GdkWindowObject *)window;
  gboolean save_region = FALSE;

  /* If an update got queued during update processing, we can get a
   * window in the update queue that has an empty update_area.
   * just ignore it.
   */
  if (private->update_area)
    {
      GdkRegion *update_area = private->update_area;
      private->update_area = NULL;
      
      if (_gdk_event_func && gdk_window_is_viewable (window))
	{
	  GdkRectangle window_rect;
	  GdkRegion *expose_region;
	  GdkRegion *window_region;
          gint width, height;
	  save_region = _gdk_windowing_window_queue_antiexpose (window, update_area);

	  if (save_region)
	    expose_region = gdk_region_copy (update_area);
	  else
	    expose_region = update_area;
	  
          gdk_drawable_get_size (GDK_DRAWABLE (private), &width, &height);

	  window_rect.x = 0;
	  window_rect.y = 0;
	  window_rect.width = width;
	  window_rect.height = height;

	  window_region = gdk_region_rectangle (&window_rect);
	  gdk_region_intersect (expose_region,
				window_region);
	  gdk_region_destroy (window_region);
	  
	  if (!gdk_region_empty (expose_region) &&
	      (private->event_mask & GDK_EXPOSURE_MASK))
	    {
	      GdkEvent event;
	      
	      event.expose.type = GDK_EXPOSE;
	      event.expose.window = g_object_ref (window);
	      event.expose.send_event = FALSE;
	      event.expose.count = 0;
	      event.expose.region = expose_region;
	      gdk_region_get_clipbox (expose_region, &event.expose.area);
	      (*_gdk_event_func) (&event, _gdk_event_data);
	      
	      g_object_unref (window);
	    }

	  if (expose_region != update_area)
	    gdk_region_destroy (expose_region);
	}
      if (!save_region)
	gdk_region_destroy (update_area);
    }
}


static void
gdk_window_impl_directfb_begin_paint_region (GdkPaintable *paintable,
					   GdkRegion    *region)
{
  GdkDrawableImplDirectFB *impl;
  GdkWindowImplDirectFB *wimpl;
  gint                     i;


  g_assert (region != NULL );
  wimpl = GDK_WINDOW_IMPL_DIRECTFB (paintable);
  impl = (GdkDrawableImplDirectFB *)wimpl;
  impl->buffered = TRUE;
  impl->paint_depth++;

  if (!region)
    return;

  if (impl->paint_region)
    gdk_region_union (impl->paint_region, region);
  else
    impl->paint_region = gdk_region_copy (region);

  for (i = 0; i < region->numRects; i++)
    {
      GdkRegionBox *box = &region->rects[i];

      _gdk_windowing_window_clear_area (GDK_WINDOW(wimpl->gdkWindow),
                                        box->x1,
                                        box->y1,
                                        box->x2 - box->x1,
                                        box->y2 - box->y1);
                                        
    }
}

static void
gdk_window_impl_directfb_end_paint (GdkPaintable *paintable)
{
  GdkDrawableImplDirectFB *impl;

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (paintable);

  g_return_if_fail (impl->paint_depth > 0);

  impl->paint_depth--;

  if (impl->paint_depth == 0)
    {
      impl->buffered = FALSE;

      if (impl->paint_region)
        {
          DFBRegion reg = { impl->paint_region->extents.x1,
                            impl->paint_region->extents.y1,
                            impl->paint_region->extents.x2 ,
                            impl->paint_region->extents.y2 };

          impl->surface->Flip(impl->surface, &reg,0);
          gdk_region_destroy (impl->paint_region);
          impl->paint_region = NULL;
        }
    }
}


static void
gdk_window_impl_directfb_paintable_init (GdkPaintableIface *iface)
{
  iface->begin_paint_region = gdk_window_impl_directfb_begin_paint_region;
  iface->end_paint = gdk_window_impl_directfb_end_paint;

  iface->invalidate_maybe_recurse = gdk_window_impl_directfb_invalidate_maybe_recurse;
  iface->process_updates = gdk_window_impl_directfb_process_updates;
}

#define __GDK_WINDOW_X11_C__
#include "gdkaliasdef.c"

