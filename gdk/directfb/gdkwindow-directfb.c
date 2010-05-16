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

#include "config.h"
#include "gdk.h"
#include "gdkwindowimpl.h"
#include "gdkwindow.h"

#include "gdkdirectfb.h"
#include "gdkprivate-directfb.h"
#include "gdkdisplay-directfb.h"

#include "gdkregion-generic.h"

#include "gdkinternals.h"
#include "gdkalias.h"
#include "cairo.h"
#include <assert.h>

#include <direct/debug.h>

#include <directfb_util.h>

D_DEBUG_DOMAIN (GDKDFB_Crossing,  "GDKDFB/Crossing",  "GDK DirectFB Crossing Events");
D_DEBUG_DOMAIN (GDKDFB_Updates,   "GDKDFB/Updates",   "GDK DirectFB Updates");
D_DEBUG_DOMAIN (GDKDFB_Paintable, "GDKDFB/Paintable", "GDK DirectFB Paintable");
D_DEBUG_DOMAIN (GDKDFB_Window,    "GDKDFB/Window",    "GDK DirectFB Window");


static GdkRegion * gdk_window_impl_directfb_get_visible_region (GdkDrawable *drawable);
static void        gdk_window_impl_directfb_set_colormap       (GdkDrawable *drawable,
                                                                GdkColormap *colormap);
static void gdk_window_impl_directfb_init       (GdkWindowImplDirectFB      *window);
static void gdk_window_impl_directfb_class_init (GdkWindowImplDirectFBClass *klass);
static void gdk_window_impl_directfb_finalize   (GObject                    *object);

static void gdk_window_impl_iface_init (GdkWindowImplIface *iface);
static void gdk_directfb_window_destroy (GdkWindow *window,
                                         gboolean   recursing,
                                         gboolean   foreign_destroy);

typedef struct
{
  GdkWindowChildChanged changed;
  GdkWindowChildGetPos  get_pos;
  gpointer              user_data;
} GdkWindowChildHandlerData;


static GdkWindow *gdk_directfb_window_containing_pointer = NULL;
static GdkWindow *gdk_directfb_focused_window            = NULL;
static gpointer   parent_class                           = NULL;
GdkWindow * _gdk_parent_root = NULL;

static void gdk_window_impl_directfb_paintable_init (GdkPaintableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkWindowImplDirectFB,
                         gdk_window_impl_directfb,
                         GDK_TYPE_DRAWABLE_IMPL_DIRECTFB,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_WINDOW_IMPL,
                                                gdk_window_impl_iface_init)
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gdk_window_impl_directfb_paintable_init));

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
  impl->cursor  = gdk_cursor_new_for_display (GDK_DISPLAY_OBJECT (_gdk_display),
                                              GDK_LEFT_PTR);
  impl->opacity = 255;
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

  D_DEBUG_AT (GDKDFB_Window, "%s( %p ) <- %dx%d\n", G_STRFUNC,
              impl, impl->drawable.width, impl->drawable.height);

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

static GdkRegion *
gdk_window_impl_directfb_get_visible_region (GdkDrawable *drawable)
{
  GdkDrawableImplDirectFB *priv  = GDK_DRAWABLE_IMPL_DIRECTFB (drawable);
  GdkRectangle             rect  = { 0, 0, 0, 0 };
  DFBRectangle             drect = { 0, 0, 0, 0 };

  D_DEBUG_AT (GDKDFB_Window, "%s( %p )\n", G_STRFUNC, drawable);

  if (priv->surface)
    priv->surface->GetVisibleRectangle (priv->surface, &drect);
  rect.x      = drect.x;
  rect.y      = drect.y;
  rect.width  = drect.w;
  rect.height = drect.h;

  D_DEBUG_AT (GDKDFB_Window, "  -> returning %4d,%4d-%4dx%4d\n",
              drect.x, drect.y, drect.w, drect.h);

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

  D_DEBUG_AT (GDKDFB_Window, "%s( %4dx%4d, caps 0x%08x )\n", G_STRFUNC,
              desc->width, desc->height, desc->caps);

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

#ifndef GDK_DIRECTFB_NO_EXPERIMENTS
  //direct_log_printf (NULL, "Initializing (window %p, wimpl %p)\n", win, impl);

  dfb_updates_init (&impl->flips, impl->flip_regions,
                    G_N_ELEMENTS (impl->flip_regions));
#endif

  return TRUE;
}

void
_gdk_windowing_window_init (GdkScreen *screen)
{
  GdkWindowObject         *private;
  GdkDrawableImplDirectFB *draw_impl;
  DFBDisplayLayerConfig    dlc;

  g_assert (_gdk_parent_root == NULL);

  _gdk_display->layer->GetConfiguration (_gdk_display->layer, &dlc);

  _gdk_parent_root = g_object_new (GDK_TYPE_WINDOW, NULL);

  private = GDK_WINDOW_OBJECT (_gdk_parent_root);
  private->impl = g_object_new (_gdk_window_impl_get_type (), NULL);
  private->impl_window = private;

  draw_impl = GDK_DRAWABLE_IMPL_DIRECTFB (private->impl);

  /* custom root window init */
  {
    DFBWindowDescription   desc;
    desc.flags = 0;
    /*XXX I must do this now its a bug  ALPHA ROOT*/

    desc.flags   = DWDESC_CAPS;
    desc.caps    = 0;
    desc.caps   |= DWCAPS_NODECORATION;
    desc.caps   |= DWCAPS_ALPHACHANNEL;
    desc.flags  |= (DWDESC_WIDTH | DWDESC_HEIGHT |
                    DWDESC_POSX  | DWDESC_POSY);
    desc.posx    = 0;
    desc.posy    = 0;
    desc.width   = dlc.width;
    desc.height  = dlc.height;

    create_directfb_window (GDK_WINDOW_IMPL_DIRECTFB (private->impl),
                            &desc, 0);

    g_assert (GDK_WINDOW_IMPL_DIRECTFB (private->impl)->window != NULL);
    g_assert (draw_impl->surface != NULL);
  }

  private->window_type = GDK_WINDOW_ROOT;
  private->viewable    = TRUE;
  private->x           = 0;
  private->y           = 0;
  private->abs_x       = 0;
  private->abs_y       = 0;
  private->width       = dlc.width;
  private->height      = dlc.height;

  //  impl->drawable.paint_region = NULL;
  /* impl->window                    = NULL; */
  draw_impl->abs_x    = 0;
  draw_impl->abs_y    = 0;
  draw_impl->width    = dlc.width;
  draw_impl->height   = dlc.height;
  draw_impl->wrapper  = GDK_DRAWABLE (private);
  draw_impl->colormap = gdk_screen_get_system_colormap (screen);
  g_object_ref (draw_impl->colormap);

  draw_impl->surface->GetPixelFormat (draw_impl->surface,
                                      &draw_impl->format);
  private->depth = DFB_BITS_PER_PIXEL (draw_impl->format);

  _gdk_window_update_size (_gdk_parent_root);
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

  D_DEBUG_AT( GDKDFB_Window, "%s( %p )\n", G_STRFUNC, parent );

  if (!parent || attributes->window_type != GDK_WINDOW_CHILD)
    parent = _gdk_parent_root;

  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = GDK_WINDOW_OBJECT (window);
  private->impl = g_object_new (_gdk_window_impl_get_type (), NULL);

  parent_private = GDK_WINDOW_OBJECT (parent);
  parent_impl = GDK_WINDOW_IMPL_DIRECTFB (parent_private->impl);
  private->parent = parent_private;

  x = (attributes_mask & GDK_WA_X) ? attributes->x : 0;
  y = (attributes_mask & GDK_WA_Y) ? attributes->y : 0;

  gdk_window_set_events (window, attributes->event_mask | GDK_STRUCTURE_MASK);

  impl = GDK_WINDOW_IMPL_DIRECTFB (private->impl);
  impl->drawable.wrapper = GDK_DRAWABLE (window);

  private->x = x;
  private->y = y;

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

      if (desc.caps != DWCAPS_INPUTONLY)
        {
          impl->window->SetOpacity(impl->window, 0x00 );
        }

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


void
_gdk_window_impl_new (GdkWindow     *window,
                      GdkWindow     *real_parent,
                      GdkScreen     *screen,
                      GdkVisual     *visual,
                      GdkEventMask   event_mask,
                      GdkWindowAttr *attributes,
                      gint           attributes_mask)
{
  GdkWindowObject       *private;
  GdkWindowObject       *parent_private;
  GdkWindowImplDirectFB *impl;
  GdkWindowImplDirectFB *parent_impl;
  DFBWindowDescription   desc;

  impl = g_object_new (_gdk_window_impl_get_type (), NULL);
  impl->drawable.wrapper = GDK_DRAWABLE (window);

  private = GDK_WINDOW_OBJECT (window);
  private->impl = (GdkDrawable *)impl;

  parent_private = GDK_WINDOW_OBJECT (real_parent);
  parent_impl = GDK_WINDOW_IMPL_DIRECTFB (parent_private->impl);

  impl->drawable.width  = MAX (1, attributes->width);
  impl->drawable.height = MAX (1, attributes->height);

  desc.flags = 0;

  switch (attributes->wclass)
    {
    case GDK_INPUT_OUTPUT:
      desc.flags       |= DWDESC_PIXELFORMAT;
      desc.pixelformat  = ((GdkVisualDirectFB *)visual)->format;

      if (DFB_PIXELFORMAT_HAS_ALPHA (desc.pixelformat))
        {
          desc.flags |= DWDESC_CAPS;
          desc.caps   = DWCAPS_ALPHACHANNEL;
        }

      break;

    case GDK_INPUT_ONLY:
      desc.flags          |= DWDESC_CAPS;
      desc.caps            = DWCAPS_INPUTONLY;
      break;

    default:
      g_warning ("_gdk_window_impl_new: unsupported window class\n");
      _gdk_window_destroy (window, FALSE);
      return;
    }

  switch (private->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
      desc.flags |= (DWDESC_WIDTH | DWDESC_HEIGHT |
                     DWDESC_POSX  | DWDESC_POSY);
      desc.posx   = private->x;
      desc.posy   = private->y;
      desc.width  = impl->drawable.width;
      desc.height = impl->drawable.height;

      if (!create_directfb_window (impl, &desc, DWOP_NONE))
        {
          g_assert (0);
          _gdk_window_destroy (window, FALSE);

          return;
        }

      if (desc.caps != DWCAPS_INPUTONLY)
        {
          impl->window->SetOpacity (impl->window, 0x00);
        }

      break;

    case GDK_WINDOW_CHILD:
      impl->window = NULL;

      if (!private->input_only && parent_impl->drawable.surface)
        {
          DFBRectangle rect = { private->x,
                                private->y,
                                impl->drawable.width,
                                impl->drawable.height };
          parent_impl->drawable.surface->GetSubSurface (parent_impl->drawable.surface,
                                                        &rect,
                                                        &impl->drawable.surface);
        }

      break;

    default:
      g_warning ("_gdk_window_impl_new: unsupported window type: %d",
                 private->window_type);
      _gdk_window_destroy (window, FALSE);

      return;
    }

  if (impl->drawable.surface)
    {
      GdkColormap *colormap;

      impl->drawable.surface->GetPixelFormat (impl->drawable.surface,
                                              &impl->drawable.format);

      if ((attributes_mask & GDK_WA_COLORMAP) && attributes->colormap)
        {
          colormap = attributes->colormap;
        }
      else
        {
          if (gdk_visual_get_system () == visual)
            colormap = gdk_colormap_get_system ();
          else
            colormap = gdk_colormap_new (visual, FALSE);
        }

      gdk_drawable_set_colormap (GDK_DRAWABLE (window), colormap);
    }
  else
    {
      impl->drawable.format = ((GdkVisualDirectFB *)visual)->format;
    }

  gdk_window_set_cursor (window,
                         ((attributes_mask & GDK_WA_CURSOR) ?
                          (attributes->cursor) : NULL));

  /* we hold a reference count on ourself */
  g_object_ref (window);

  if (impl->window)
    {
      impl->window->GetID (impl->window, &impl->dfb_id);
      gdk_directfb_window_id_table_insert (impl->dfb_id, window);
      gdk_directfb_event_windows_add (window);
    }

  if (attributes_mask & GDK_WA_TYPE_HINT)
    gdk_window_set_type_hint (window, attributes->type_hint);
}

void
_gdk_windowing_window_destroy_foreign (GdkWindow *window)
{
  /* It's somebody else's window, but in our hierarchy,
   * so reparent it to the root window, and then send
   * it a delete event, as if we were a WM
   */
  gdk_directfb_window_destroy (window, TRUE, TRUE);
}

static void
gdk_directfb_window_destroy (GdkWindow *window,
                             gboolean   recursing,
                             gboolean   foreign_destroy)
{
  GdkWindowObject       *private;
  GdkWindowImplDirectFB *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  D_DEBUG_AT (GDKDFB_Window, "%s( %p, %srecursing, %sforeign )\n", G_STRFUNC, window,
              recursing ? "" : "not ", foreign_destroy ? "" : "no ");

  private = GDK_WINDOW_OBJECT (window);
  impl    = GDK_WINDOW_IMPL_DIRECTFB (private->impl);

  _gdk_selection_window_destroyed (window);
  gdk_directfb_event_windows_remove (window);

  if (window == _gdk_directfb_pointer_grab_window)
    gdk_pointer_ungrab (GDK_CURRENT_TIME);
  if (window == _gdk_directfb_keyboard_grab_window)
    gdk_keyboard_ungrab (GDK_CURRENT_TIME);

  if (window == gdk_directfb_focused_window)
    gdk_directfb_change_focus (NULL);

  if (impl->drawable.surface) {
    GdkDrawableImplDirectFB *dimpl = GDK_DRAWABLE_IMPL_DIRECTFB (private->impl);
    if (dimpl->cairo_surface)
      cairo_surface_destroy (dimpl->cairo_surface);
    impl->drawable.surface->Release (impl->drawable.surface);
    impl->drawable.surface = NULL;
  }

  if (!recursing && !foreign_destroy && impl->window ) {
    impl->window->SetOpacity (impl->window, 0);
    impl->window->Close (impl->window);
    impl->window->Release (impl->window);
    impl->window = NULL;
  }
}

/* This function is called when the window is really gone.
 */
void
gdk_window_destroy_notify (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  D_DEBUG_AT (GDKDFB_Window, "%s( %p )\n", G_STRFUNC, window);

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

  D_DEBUG_AT (GDKDFB_Window, "%s( %p )\n", G_STRFUNC, new_focus_window);

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

  D_DEBUG_AT (GDKDFB_Window, "%s( %p )\n", G_STRFUNC, window);

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

  D_DEBUG_AT (GDKDFB_Window, "%s( %p )\n", G_STRFUNC, window);

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

  D_DEBUG_AT (GDKDFB_Window, "%s( %p )\n", G_STRFUNC, private);

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

  D_DEBUG_AT (GDKDFB_Crossing, "%s( %p -> %p, %d )\n", G_STRFUNC, src, dest, mode);

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

  if (dest == gdk_directfb_window_containing_pointer) {
    D_DEBUG_AT (GDKDFB_Crossing, "  -> already containing the pointer\n");
    return;
  }

  if (gdk_directfb_window_containing_pointer == NULL)
    gdk_directfb_window_containing_pointer = g_object_ref (_gdk_parent_root);

  if (src)
    a = src;
  else
    a = gdk_directfb_window_containing_pointer;

  b = dest;

  if (a == b) {
    D_DEBUG_AT (GDKDFB_Crossing, "  -> src == dest\n");
    return;
  }

  /* gdk_directfb_window_containing_pointer might have been destroyed.
   * The refcount we hold on it should keep it, but it's parents
   * might have died.
   */
  if (GDK_WINDOW_DESTROYED (a)) {
    D_DEBUG_AT (GDKDFB_Crossing, "  -> src is destroyed!\n");
    a = _gdk_parent_root;
  }

  gdk_directfb_mouse_get_info (&x, &y, &modifiers);

  c = gdk_directfb_find_common_ancestor (a, b);

  D_DEBUG_AT (GDKDFB_Crossing, "  -> common ancestor %p\n", c);

  non_linear = (c != a) && (c != b);

  D_DEBUG_AT (GDKDFB_Crossing, "  -> non_linear: %s\n", non_linear ? "YES" : "NO");

  event_win = gdk_directfb_pointer_event_window (a, GDK_LEAVE_NOTIFY);
  if (event_win)
    {
      D_DEBUG_AT (GDKDFB_Crossing, "  -> sending LEAVE to src\n");

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

      D_DEBUG_AT (GDKDFB_Crossing,
                  "  => LEAVE (%p/%p) at %4f,%4f (%4f,%4f) mode %d, detail %d\n",
                  event_win, a,
                  event->crossing.x, event->crossing.y,
                  event->crossing.x_root, event->crossing.y_root,
                  event->crossing.mode, event->crossing.detail);
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

              D_DEBUG_AT (GDKDFB_Crossing,
                          "  -> LEAVE (%p/%p) at %4f,%4f (%4f,%4f) mode %d, detail %d\n",
                          event_win, win,
                          event->crossing.x, event->crossing.y,
                          event->crossing.x_root, event->crossing.y_root,
                          event->crossing.mode, event->crossing.detail);
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

              D_DEBUG_AT (GDKDFB_Crossing,
                          "  -> ENTER (%p/%p) at %4f,%4f (%4f,%4f) mode %d, detail %d\n",
                          event_win, win,
                          event->crossing.x, event->crossing.y,
                          event->crossing.x_root, event->crossing.y_root,
                          event->crossing.mode, event->crossing.detail);
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

      D_DEBUG_AT (GDKDFB_Crossing,
                  "  => ENTER (%p/%p) at %4f,%4f (%4f,%4f) mode %d, detail %d\n",
                  event_win, b,
                  event->crossing.x, event->crossing.y,
                  event->crossing.x_root, event->crossing.y_root,
                  event->crossing.mode, event->crossing.detail);
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

  D_DEBUG_AT (GDKDFB_Window, "%s( %p, %sraise )\n", G_STRFUNC,
              window, raise ? "" : "no ");

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_DIRECTFB (private->impl);

  if (!private->destroyed && !GDK_WINDOW_IS_MAPPED (private))
    {
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

static void
gdk_directfb_window_show (GdkWindow *window,
                          gboolean   raise)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  D_DEBUG_AT (GDKDFB_Window, "%s( %p )\n", G_STRFUNC, window);

  show_window_internal (window, raise);
}

static void
gdk_directfb_window_hide (GdkWindow *window)
{
  GdkWindowObject       *private;
  GdkWindowImplDirectFB *impl;
  GdkWindow             *mousewin;
  GdkWindow             *event_win;

  g_return_if_fail (GDK_IS_WINDOW (window));

  D_DEBUG_AT (GDKDFB_Window, "%s( %p )\n", G_STRFUNC, window);

  private = GDK_WINDOW_OBJECT (window);
  impl    = GDK_WINDOW_IMPL_DIRECTFB (private->impl);

  if (impl->window)
    impl->window->SetOpacity (impl->window, 0);

  if (!private->destroyed && GDK_WINDOW_IS_MAPPED (private))
    {
      GdkEvent *event;

      if (!private->input_only && private->parent)
        {
          gdk_window_clear_area (GDK_WINDOW (private->parent),
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

static void
gdk_directfb_window_withdraw (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* for now this should be enough */
  gdk_window_hide (window);
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

  impl->drawable.width  = width;
  impl->drawable.height = height;

  if (!private->input_only)
    {
      if (impl->drawable.surface)
        {
          if (impl->drawable.cairo_surface)
            cairo_surface_destroy (impl->drawable.cairo_surface);

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
}

static  void
gdk_directfb_window_move (GdkWindow *window,
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
      impl->window->MoveTo (impl->window, x, y);
    }
  else
    {
      gint         width  = impl->drawable.width;
      gint         height = impl->drawable.height;
      GdkRectangle old    = { private->x, private->y,width,height };

      _gdk_directfb_move_resize_child (window, x, y, width, height);

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

static void
gdk_directfb_window_move_resize (GdkWindow *window,
                                 gboolean   with_move,
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

  if (with_move && (width < 0 && height < 0))
    {
      gdk_directfb_window_move (window, x, y);
      return;
    }

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
      if (with_move)
        gdk_directfb_window_move (window, x, y);
    }
  else if (impl->window)
    {
      if (with_move) {
        impl->window->MoveTo (impl->window, x, y);
      }
      impl->drawable.width = width;
      impl->drawable.height = height;

      impl->window->Resize (impl->window, width, height);
    }
  else
    {
      GdkRectangle old = { private->x, private->y,
                           impl->drawable.width, impl->drawable.height };
      GdkRectangle new = { x, y, width, height };

      if (! with_move)
        {
          new.x = private->x;
          new.y = private->y;
        }

      _gdk_directfb_move_resize_child (window,
                                       new.x, new.y, new.width, new.height);

      if (GDK_WINDOW_IS_MAPPED (private))
        {
          GdkWindow *mousewin;

          gdk_rectangle_union (&new, &old, &new);
          gdk_window_invalidate_rect (GDK_WINDOW (private->parent), &new,TRUE);

          /* The window the pointer is in might have changed */
          mousewin = gdk_window_at_pointer (NULL, NULL);
          gdk_directfb_window_send_crossing_events (NULL, mousewin,
                                                    GDK_CROSSING_NORMAL);
        }
    }
}

static gboolean
gdk_directfb_window_reparent (GdkWindow *window,
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

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;

  if (!new_parent)
    new_parent = _gdk_parent_root;

  window_private     = (GdkWindowObject *) window;
  old_parent_private = (GdkWindowObject *) window_private->parent;
  parent_private     = (GdkWindowObject *) new_parent;
  parent_impl        = GDK_WINDOW_IMPL_DIRECTFB (parent_private->impl);
  visual             = gdk_drawable_get_visual (window);

  /* already parented */
  if (window_private->parent == (GdkWindowObject *) new_parent)
    return FALSE;

  window_private->parent = (GdkWindowObject *) new_parent;

  impl = GDK_WINDOW_IMPL_DIRECTFB (window_private->impl);

  if (impl->drawable.surface) {
    impl->drawable.surface->Release (impl->drawable.surface);
    impl->drawable.surface = NULL;
  }

  if (impl->window != NULL) {
    gdk_directfb_window_id_table_remove (impl->dfb_id);
    impl->window->SetOpacity (impl->window, 0);
    impl->window->Close (impl->window);
    impl->window->Release (impl->window);
    impl->window = NULL;
  }

  //create window were a child of the root now
  if (window_private->parent == (GdkWindowObject *)_gdk_parent_root)  {
    DFBWindowDescription  desc;
    DFBWindowOptions  window_options = DWOP_NONE;
    desc.flags = DWDESC_CAPS;
    if (window_private->input_only) {
      desc.caps = DWCAPS_INPUTONLY;
    } else {
      desc.flags |= DWDESC_PIXELFORMAT;
      desc.pixelformat = ((GdkVisualDirectFB *) visual)->format;
      if (DFB_PIXELFORMAT_HAS_ALPHA (desc.pixelformat)) {
        desc.flags |= DWDESC_CAPS;
        desc.caps = DWCAPS_ALPHACHANNEL;
      }
    }
    if (window_private->window_type ==  GDK_WINDOW_CHILD)
      window_private->window_type = GDK_WINDOW_TOPLEVEL;
    desc.flags |= (DWDESC_WIDTH | DWDESC_HEIGHT |
                   DWDESC_POSX  | DWDESC_POSY);
    desc.posx   = x;
    desc.posy   = y;
    desc.width  = impl->drawable.width;
    desc.height = impl->drawable.height;
    if (!create_directfb_window (impl, &desc, window_options))
      {
        g_assert (0);
        _gdk_window_destroy (window, FALSE);
        return FALSE;
      }
    /* we hold a reference count on ourselves */
    g_object_ref (window);
    impl->window->GetID (impl->window, &impl->dfb_id);
    gdk_directfb_window_id_table_insert (impl->dfb_id, window);
    gdk_directfb_event_windows_add (window);
  } else {
    DFBRectangle rect = { x, y,
                          impl->drawable.width,
                          impl->drawable.height };
    impl->window = NULL;
    parent_impl->drawable.surface->GetSubSurface (parent_impl->drawable.surface,
                                                  &rect,
                                                  &impl->drawable.surface);
  }

  return TRUE;
}

static void
gdk_window_directfb_raise (GdkWindow *window)
{
  GdkWindowImplDirectFB *impl;

  D_DEBUG_AT (GDKDFB_Window, "%s( %p )\n", G_STRFUNC, window);

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

static void
gdk_window_directfb_lower (GdkWindow *window)
{
  GdkWindowImplDirectFB *impl;

  D_DEBUG_AT (GDKDFB_Window, "%s( %p )\n", G_STRFUNC, window);

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

  D_DEBUG_AT( GDKDFB_Window, "%s( %p, %3d,%3d, min %4dx%4d, max %4dx%4d, flags 0x%08x )\n", G_STRFUNC,
              window, x,y, min_width, min_height, max_width, max_height, flags );
  /* N/A */
}

void
gdk_window_set_geometry_hints (GdkWindow         *window,
                               const GdkGeometry *geometry,
                               GdkWindowHints     geom_mask)
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

  D_DEBUG_AT (GDKDFB_Window, "%s( %p, '%s' )\n", G_STRFUNC, window, title);
  /* N/A */
  D_DEBUG_AT (GDKDFB_Window, "%s( %p )\n", G_STRFUNC, window);
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

/**
 * gdk_window_set_startup_id:
 * @window: a toplevel #GdkWindow
 * @startup_id: a string with startup-notification identifier
 *
 * When using GTK+, typically you should use gtk_window_set_startup_id()
 * instead of this low-level function.
 *
 * Since: 2.12
 *
 **/
void
gdk_window_set_startup_id (GdkWindow   *window,
                           const gchar *startup_id)
{
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

static void
gdk_directfb_window_set_background (GdkWindow      *window,
                                    const GdkColor *color)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (color != NULL);

  D_DEBUG_AT (GDKDFB_Window, "%s( %p, %d,%d,%d )\n", G_STRFUNC,
              window, color->red, color->green, color->blue);
}

static void
gdk_directfb_window_set_back_pixmap (GdkWindow *window,
                                     GdkPixmap *pixmap)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  D_DEBUG_AT (GDKDFB_Window, "%s( %p, %p )\n", G_STRFUNC, window, pixmap);
}

static void
gdk_directfb_window_set_cursor (GdkWindow *window,
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

static void
gdk_directfb_window_get_geometry (GdkWindow *window,
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
	*depth = DFB_BITS_PER_PIXEL (impl->format);
    }
}

static gboolean
gdk_directfb_window_get_deskrelative_origin (GdkWindow *window,
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
gdk_directfb_window_get_pointer_helper (GdkWindow       *window,
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

static gboolean
gdk_directfb_window_get_pointer (GdkWindow       *window,
                                 gint            *x,
                                 gint            *y,
                                 GdkModifierType *mask)
{
  return gdk_directfb_window_get_pointer_helper (window, x, y, mask) != NULL;
}

GdkWindow *
_gdk_windowing_window_at_pointer (GdkDisplay      *display,
                                  gint            *win_x,
				  gint            *win_y,
                                  GdkModifierType *mask,
                                  gboolean         get_toplevel)
{
  GdkWindow *retval;
  gint       wx, wy;

  gdk_directfb_mouse_get_info (&wx, &wy, NULL);

  retval = gdk_directfb_child_at (_gdk_parent_root, &wx, &wy);

  if (win_x)
    *win_x = wx;

  if (win_y)
    *win_y = wy;

  if (get_toplevel)
    {
      GdkWindowObject *w = (GdkWindowObject *)retval;
      /* Requested toplevel, find it. */
      /* TODO: This can be implemented more efficient by never
	 recursing into children in the first place */
      if (w)
	{
	  /* Convert to toplevel */
	  while (w->parent != NULL &&
		 w->parent->window_type != GDK_WINDOW_ROOT)
	    {
	      *win_x += w->x;
	      *win_y += w->y;
	      w = w->parent;
	    }
	  retval = (GdkWindow *)w;
        }
    }

  return retval;
}

void
_gdk_windowing_get_pointer (GdkDisplay       *display,
                            GdkScreen       **screen,
                            gint             *x,
                            gint             *y,
                            GdkModifierType  *mask)
{
  if (screen) {
    *screen = gdk_display_get_default_screen (display);
  }

  gdk_directfb_window_get_pointer (_gdk_windowing_window_at_pointer (display,
                                                                     NULL,
                                                                     NULL,
                                                                     NULL,
                                                                     FALSE),
                                   x, y, mask);
}

static GdkEventMask
gdk_directfb_window_get_events (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (GDK_WINDOW_DESTROYED (window))
    return 0;
  else
    return GDK_WINDOW_OBJECT (window)->event_mask;
}

static void
gdk_directfb_window_set_events (GdkWindow    *window,
                                GdkEventMask  event_mask)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (event_mask & GDK_BUTTON_MOTION_MASK)
    event_mask |= (GDK_BUTTON1_MOTION_MASK |
                   GDK_BUTTON2_MOTION_MASK |
                   GDK_BUTTON3_MOTION_MASK);

  GDK_WINDOW_OBJECT (window)->event_mask = event_mask;
}

static void
gdk_directfb_window_shape_combine_region (GdkWindow       *window,
                                          const GdkRegion *shape_region,
                                          gint             offset_x,
                                          gint             offset_y)
{
}

void
gdk_directfb_window_input_shape_combine_region (GdkWindow       *window,
                                                const GdkRegion *shape_region,
                                                gint             offset_x,
                                                gint             offset_y)
{
}

static void
gdk_directfb_window_queue_translation (GdkWindow *window,
				       GdkGC     *gc,
                                       GdkRegion *region,
                                       gint       dx,
                                       gint       dy)
{
  GdkWindowObject         *private = GDK_WINDOW_OBJECT (window);
  GdkDrawableImplDirectFB *impl    = GDK_DRAWABLE_IMPL_DIRECTFB (private->impl);

  D_DEBUG_AT (GDKDFB_Window, "%s( %p, %p, %4d,%4d-%4d,%4d (%ld boxes), %d, %d )\n",
              G_STRFUNC, window, gc,
              GDKDFB_RECTANGLE_VALS_FROM_BOX (&region->extents),
              region->numRects, dx, dy);

  gdk_region_offset (region, dx, dy);
  gdk_region_offset (region, private->abs_x, private->abs_y);

  if (!impl->buffered)
    temp_region_init_copy (&impl->paint_region, region);
  else
    gdk_region_union (&impl->paint_region, region);
  impl->buffered = TRUE;

  gdk_region_offset (region, -dx, -dy);
  gdk_region_offset (region, -private->abs_x, -private->abs_y);
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
  g_warning (" DirectFb set_group groups not supported \n");

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* N/A */
}

GdkWindow *
gdk_window_get_group (GdkWindow *window)
{
  g_warning (" DirectFb get_group groups not supported \n");
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
  g_message ("unimplemented %s", G_STRFUNC);
}

static gboolean
gdk_directfb_window_set_static_gravities (GdkWindow *window,
                                          gboolean   use_static)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;

  /* N/A */
  g_message ("unimplemented %s", G_STRFUNC);

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

  g_message ("unimplemented %s", G_STRFUNC);
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

  g_message ("unimplemented %s", G_STRFUNC);
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
  rect->y      = impl->abs_y;
  rect->width  = impl->width;
  rect->height = impl->height;
}

/*
 * Given a directfb window and a subsurface of that window
 * create a gdkwindow child wrapper
 */
GdkWindow *
gdk_directfb_create_child_window (GdkWindow *parent,
                                  IDirectFBSurface *subsurface)
{
  GdkWindow             *window;
  GdkWindowObject       *private;
  GdkWindowObject       *parent_private;
  GdkWindowImplDirectFB *impl;
  GdkWindowImplDirectFB *parent_impl;
  gint                   x, y, w, h;

  g_return_val_if_fail (parent != NULL, NULL);

  window          = g_object_new (GDK_TYPE_WINDOW, NULL);
  private         = GDK_WINDOW_OBJECT (window);
  private->impl   = g_object_new (_gdk_window_impl_get_type (), NULL);
  parent_private  = GDK_WINDOW_OBJECT (parent);
  parent_impl     = GDK_WINDOW_IMPL_DIRECTFB (parent_private->impl);
  private->parent = parent_private;

  subsurface->GetPosition (subsurface, &x, &y);
  subsurface->GetSize (subsurface, &w, &h);

  impl = GDK_WINDOW_IMPL_DIRECTFB (private->impl);
  impl->drawable.wrapper = GDK_DRAWABLE (window);

  private->x = x;
  private->y = y;

  private->abs_x = 0;
  private->abs_y = 0;

  impl->drawable.width     = w;
  impl->drawable.height    = h;
  private->window_type     = GDK_WINDOW_CHILD;
  impl->drawable.surface   = subsurface;
  impl->drawable.format    = parent_impl->drawable.format;
  private->depth           = parent_private->depth;
  gdk_drawable_set_colormap (GDK_DRAWABLE (window),
                             gdk_drawable_get_colormap (parent));
  gdk_window_set_cursor (window, NULL);
  parent_private->children = g_list_prepend (parent_private->children,window);
  /*we hold a reference count on ourselves */
  g_object_ref (window);

  return window;

}

/*
 * The wrapping is not perfect since directfb does not give full access
 * to the current state of a window event mask etc need to fix dfb
 */
GdkWindow *
gdk_window_foreign_new_for_display (GdkDisplay* display, GdkNativeWindow anid)
{
  GdkWindow             *window         = NULL;
  GdkWindow             *parent         = NULL;
  GdkWindowObject       *private        = NULL;
  GdkWindowObject       *parent_private = NULL;
  GdkWindowImplDirectFB *parent_impl    = NULL;
  GdkWindowImplDirectFB *impl           = NULL;
  DFBWindowOptions       options;
  DFBResult              ret;
  GdkDisplayDFB *        gdkdisplay     = _gdk_display;
  IDirectFBWindow       *dfbwindow;

  window = gdk_window_lookup (anid);

  if (window) {
    g_object_ref (window);
    return window;
  }
  if (display != NULL)
    gdkdisplay = GDK_DISPLAY_DFB (display);

  ret = gdkdisplay->layer->GetWindow (gdkdisplay->layer,
                                      (DFBWindowID)anid,
                                      &dfbwindow);

  if (ret != DFB_OK) {
    DirectFBError ("gdk_window_new: Layer->GetWindow failed", ret);
    return NULL;
  }

  parent = _gdk_parent_root;

  if (parent) {
    parent_private = GDK_WINDOW_OBJECT (parent);
    parent_impl = GDK_WINDOW_IMPL_DIRECTFB (parent_private->impl);
  }

  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  /* we hold a reference count on ourselves */
  g_object_ref (window);
  private              = GDK_WINDOW_OBJECT (window);
  private->impl        = g_object_new (_gdk_window_impl_get_type (), NULL);
  private->parent      = parent_private;
  private->window_type = GDK_WINDOW_TOPLEVEL;
  private->viewable    = TRUE;
  impl                 = GDK_WINDOW_IMPL_DIRECTFB (private->impl);

  impl->drawable.wrapper = GDK_DRAWABLE (window);
  impl->window           = dfbwindow;
  dfbwindow->GetOptions (dfbwindow, &options);
  dfbwindow->GetPosition (dfbwindow, &private->x, &private->y);
  dfbwindow->GetSize (dfbwindow, &impl->drawable.width, &impl->drawable.height);


  private->input_only = FALSE;

  if (dfbwindow->GetSurface (dfbwindow,
                             &impl->drawable.surface) == DFB_UNSUPPORTED) {
    private->input_only    = TRUE;
    impl->drawable.surface = NULL;
  }

  /* We default to all events least surprise to the user
   * minus the poll for motion events
   */
  gdk_window_set_events (window, (GDK_ALL_EVENTS_MASK ^ GDK_POINTER_MOTION_HINT_MASK));

  if (impl->drawable.surface)
    {
      impl->drawable.surface->GetPixelFormat (impl->drawable.surface,
					      &impl->drawable.format);

      private->depth = DFB_BITS_PER_PIXEL(impl->drawable.format);
      if (parent)
        gdk_drawable_set_colormap (GDK_DRAWABLE (window), gdk_drawable_get_colormap (parent));
      else
        gdk_drawable_set_colormap (GDK_DRAWABLE (window), gdk_colormap_get_system ());
    }

  //can  be null for the soft cursor window itself when
  //running a gtk directfb wm
  if (gdk_display_get_default () != NULL) {
    gdk_window_set_cursor (window, NULL);
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

IDirectFBWindow *
gdk_directfb_window_lookup (GdkWindow *window)
{
  GdkWindowObject       *private;
  GdkWindowImplDirectFB *impl;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  private = GDK_WINDOW_OBJECT (window);
  impl    = GDK_WINDOW_IMPL_DIRECTFB (private->impl);

  return impl->window;
}

IDirectFBSurface *
gdk_directfb_surface_lookup (GdkWindow *window)
{
  GdkWindowObject       *private;
  GdkWindowImplDirectFB *impl;

  g_return_val_if_fail (GDK_IS_WINDOW (window),NULL);

  private = GDK_WINDOW_OBJECT (window);
  impl    = GDK_WINDOW_IMPL_DIRECTFB (private->impl);

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
gdk_window_impl_directfb_begin_paint_region (GdkPaintable    *paintable,
                                             GdkWindow       *window,
                                             const GdkRegion *region)
{
  GdkWindowObject         *private = GDK_WINDOW_OBJECT (window);
  /* GdkWindowImplDirectFB   *wimpl   = GDK_WINDOW_IMPL_DIRECTFB (paintable); */
  GdkDrawableImplDirectFB *impl    = GDK_DRAWABLE_IMPL_DIRECTFB (paintable);
  GdkRegion               *native_region;
  gint                     i;

  g_assert (region != NULL);

  D_DEBUG_AT (GDKDFB_Window, "%s( %p, %p, %4d,%4d-%4d,%4d (%ld boxes) )\n",
              G_STRFUNC, paintable, window,
              GDKDFB_RECTANGLE_VALS_FROM_BOX (&region->extents),
              region->numRects);
  D_DEBUG_AT (GDKDFB_Window, "  -> window @ pos=%ix%i abs_pos=%ix%i\n",
              private->x, private->y, private->abs_x, private->abs_y);

  native_region = gdk_region_copy (region);
  gdk_region_offset (native_region, private->abs_x, private->abs_y);

  /* /\* When it's buffered... *\/ */
  if (impl->buffered)
    {
      /* ...we're already painting on it! */
      D_DEBUG_AT (GDKDFB_Window, "  -> painted  %4d,%4d-%4dx%4d (%ld boxes)\n",
                  DFB_RECTANGLE_VALS_FROM_REGION (&impl->paint_region.extents),
                  impl->paint_region.numRects);

      if (impl->paint_depth < 1)
        gdk_directfb_clip_region (GDK_DRAWABLE (paintable),
                                  NULL, NULL, &impl->clip_region);

      gdk_region_union (&impl->paint_region, native_region);
    }
  else
    {
      /* ...otherwise it's the first time! */
      g_assert (impl->paint_depth == 0);

      /* Generate the clip region for painting around child windows. */
      gdk_directfb_clip_region (GDK_DRAWABLE (paintable),
                                NULL, NULL, &impl->clip_region);

      /* Initialize the paint region with the new one... */
      temp_region_init_copy (&impl->paint_region, native_region);

      impl->buffered = TRUE;
    }

  D_DEBUG_AT (GDKDFB_Window, "  -> painting %4d,%4d-%4dx%4d (%ld boxes)\n",
              DFB_RECTANGLE_VALS_FROM_REGION (&impl->paint_region.extents),
              impl->paint_region.numRects);

  /* ...but clip the initial/compound result against the clip region. */
  /* gdk_region_intersect (&impl->paint_region, &impl->clip_region); */

  D_DEBUG_AT (GDKDFB_Window, "  -> clipped  %4d,%4d-%4dx%4d (%ld boxes)\n",
              DFB_RECTANGLE_VALS_FROM_REGION (&impl->paint_region.extents),
              impl->paint_region.numRects);

  impl->paint_depth++;

  D_DEBUG_AT (GDKDFB_Window, "  -> depth is now %d\n", impl->paint_depth);

  /*
   * Redraw background on area which are going to be repainted.
   *
   * TODO: handle pixmap background
   */
  impl->surface->SetClip (impl->surface, NULL);
  for (i = 0 ; i < native_region->numRects ; i++)
    {
      GdkRegionBox *box = &native_region->rects[i];

      D_DEBUG_AT (GDKDFB_Window, "  -> clearing [%2d] %4d,%4d-%4dx%4d\n",
                  i, GDKDFB_RECTANGLE_VALS_FROM_BOX (box));

      /* gdk_window_clear_area (window, */
      /*                        box->x1, */
      /*                        box->y1, */
      /*                        box->x2 - box->x1, */
      /*                        box->y2 - box->y1); */

      impl->surface->SetColor (impl->surface,
                               private->bg_color.red,
                               private->bg_color.green,
                               private->bg_color.blue,
                               0xff);
      impl->surface->FillRectangle (impl->surface,
                                    box->x1,
                                    box->y1,
                                    box->x2 - box->x1,
                                    box->y2 - box->y1);
    }

  gdk_region_destroy (native_region);
}

static void
gdk_window_impl_directfb_end_paint (GdkPaintable *paintable)
{
  GdkDrawableImplDirectFB *impl;

  impl = GDK_DRAWABLE_IMPL_DIRECTFB (paintable);

  D_DEBUG_AT (GDKDFB_Window, "%s( %p )\n", G_STRFUNC, paintable);

  g_return_if_fail (impl->paint_depth > 0);

  g_assert (impl->buffered);

  impl->paint_depth--;

#ifdef GDK_DIRECTFB_NO_EXPERIMENTS
  if (impl->paint_depth == 0)
    {
      impl->buffered = FALSE;

      if (impl->paint_region.numRects)
        {
          DFBRegion reg = { impl->paint_region.extents.x1,
                            impl->paint_region.extents.y1,
                            impl->paint_region.extents.x2 - 1,
                            impl->paint_region.extents.y2 - 1 };

          D_DEBUG_AT (GDKDFB_Window, "  -> flip %4d,%4d-%4dx%4d (%ld boxes)\n",
                      DFB_RECTANGLE_VALS_FROM_REGION (&reg),
                      impl->paint_region.numRects);

          impl->surface->Flip (impl->surface, &reg, 0);

          temp_region_reset (&impl->paint_region);
        }
    }
#else
  if (impl->paint_depth == 0)
    {
      impl->buffered = FALSE;

      temp_region_deinit (&impl->clip_region);

      if (impl->paint_region.numRects)
        {
          GdkWindow *window = GDK_WINDOW (impl->wrapper);

          if (GDK_IS_WINDOW (window))
            {
              GdkWindowObject *top = GDK_WINDOW_OBJECT (gdk_window_get_toplevel (window));

              if (top)
                {
                  DFBRegion              reg;
                  GdkWindowImplDirectFB *wimpl = GDK_WINDOW_IMPL_DIRECTFB (top->impl);

                  reg.x1 = impl->abs_x - top->x + impl->paint_region.extents.x1;
                  reg.y1 = impl->abs_y - top->y + impl->paint_region.extents.y1;
                  reg.x2 = impl->abs_x - top->x + impl->paint_region.extents.x2 - 1;
                  reg.y2 = impl->abs_y - top->y + impl->paint_region.extents.y2 - 1;

                  D_DEBUG_AT (GDKDFB_Window, "  -> queue flip %4d,%4d-%4dx%4d (%ld boxes)\n",
                              DFB_RECTANGLE_VALS_FROM_REGION (&reg),
                              impl->paint_region.numRects);

                  dfb_updates_add (&wimpl->flips, &reg);
                }
            }

          temp_region_reset (&impl->paint_region);
        }
    }
#endif
  else
    D_DEBUG_AT (GDKDFB_Window, "  -> depth is still %d\n", impl->paint_depth);
}

GdkRegion *
_gdk_windowing_get_shape_for_mask (GdkBitmap *mask)
{
  return NULL;
}

GdkRegion *
_gdk_windowing_window_get_shape (GdkWindow *window)
{
  return NULL;
}

gulong
_gdk_windowing_window_get_next_serial (GdkDisplay *display)
{
  return 0;
}

GdkRegion *
_gdk_windowing_window_get_input_shape (GdkWindow *window)
{
  return NULL;
}

void
_gdk_windowing_before_process_all_updates (void)
{
}

void
_gdk_windowing_after_process_all_updates (void)
{
}

void
_gdk_windowing_window_process_updates_recurse (GdkWindow *window,
                                               GdkRegion *region)
{
  _gdk_window_process_updates_recurse (window, region);
}

static void
gdk_window_impl_directfb_paintable_init (GdkPaintableIface *iface)
{
  iface->begin_paint_region = gdk_window_impl_directfb_begin_paint_region;
  iface->end_paint          = gdk_window_impl_directfb_end_paint;
}

void
_gdk_windowing_window_beep (GdkWindow *window)
{
  gdk_display_beep (gdk_display_get_default());
}

void
gdk_window_set_opacity (GdkWindow *window,
			gdouble    opacity)
{
  GdkDisplay *display;
  guint8 cardinal;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  display = gdk_drawable_get_display (window);

  if (opacity < 0)
    opacity = 0;
  else if (opacity > 1)
    opacity = 1;
  cardinal = opacity * 0xff;
  gdk_directfb_window_set_opacity (window, cardinal);
}

void
_gdk_windowing_window_set_composited (GdkWindow *window,
                                      gboolean   composited)
{
}

static gint
gdk_directfb_window_get_root_coords (GdkWindow *window,
                                     gint       x,
                                     gint       y,
                                     gint      *root_x,
                                     gint      *root_y)
{
  /* TODO */
  return 1;
}

static gboolean
gdk_directfb_window_queue_antiexpose (GdkWindow *window,
                                      GdkRegion *area)
{
  return FALSE;
}

static void
gdk_window_impl_iface_init (GdkWindowImplIface *iface)
{
  iface->show                       = gdk_directfb_window_show;
  iface->hide                       = gdk_directfb_window_hide;
  iface->withdraw                   = gdk_directfb_window_withdraw;
  iface->set_events                 = gdk_directfb_window_set_events;
  iface->get_events                 = gdk_directfb_window_get_events;
  iface->raise                      = gdk_window_directfb_raise;
  iface->lower                      = gdk_window_directfb_lower;
  iface->move_resize                = gdk_directfb_window_move_resize;
  iface->set_background             = gdk_directfb_window_set_background;
  iface->set_back_pixmap            = gdk_directfb_window_set_back_pixmap;
  iface->reparent                   = gdk_directfb_window_reparent;
  iface->set_cursor                 = gdk_directfb_window_set_cursor;
  iface->get_geometry               = gdk_directfb_window_get_geometry;
  iface->get_root_coords            = gdk_directfb_window_get_root_coords;
  iface->get_pointer                = gdk_directfb_window_get_pointer;
  iface->get_deskrelative_origin    = gdk_directfb_window_get_deskrelative_origin;
  iface->shape_combine_region       = gdk_directfb_window_shape_combine_region;
  iface->input_shape_combine_region = gdk_directfb_window_input_shape_combine_region;
  iface->set_static_gravities       = gdk_directfb_window_set_static_gravities;
  iface->queue_antiexpose           = gdk_directfb_window_queue_antiexpose;
  iface->queue_translation          = gdk_directfb_window_queue_translation;
  iface->destroy                    = gdk_directfb_window_destroy;
}

#define __GDK_WINDOW_X11_C__
#include "gdkaliasdef.c"

