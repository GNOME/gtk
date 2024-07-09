#include <gtk/gtk.h>
#include "gdk/gdksurfaceprivate.h"
#include "gdk/gdksubsurfaceprivate.h"
#include "gdk/gdkdebugprivate.h"

#ifdef GDK_WINDOWING_WAYLAND
#include "gdk/wayland/gdkwayland.h"
#endif

#define TEXTURE_RECT(t) \
  GRAPHENE_RECT_INIT (0, 0, \
                      gdk_texture_get_width (t), \
                      gdk_texture_get_height (t))

static void
test_subsurface_basics (void)
{
  GdkSurface *surface;
  GdkSubsurface *sub;
  GdkTexture *texture;
  graphene_rect_t rect;

#ifdef GDK_WINDOWING_WAYLAND
  if (!GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
#endif
    {
      g_test_skip ("No subsurface support");
      return;
    }

  surface = gdk_surface_new_toplevel (gdk_display_get_default ());

  g_assert_true (surface->subsurfaces_below == NULL);
  g_assert_true (surface->subsurfaces_above == NULL);

  sub = gdk_surface_create_subsurface (surface);

  g_assert_true (gdk_subsurface_get_parent (sub) == surface);

  g_assert_null (gdk_subsurface_get_texture (sub));
  g_assert_false (gdk_subsurface_is_above_parent (sub));
  g_assert_true (gdk_subsurface_get_transform (sub) == GDK_DIHEDRAL_NORMAL);

  texture = gdk_texture_new_from_resource ("/org/gtk/libgtk/icons/16x16/actions/media-eject.png");
  gdk_subsurface_attach (sub, texture, &TEXTURE_RECT (texture), &GRAPHENE_RECT_INIT (0, 0, 10, 10), GDK_DIHEDRAL_90, &GRAPHENE_RECT_INIT (0, 0, 20, 20), TRUE, NULL);

  g_assert_true (gdk_subsurface_get_texture (sub) == texture);
  g_assert_true (gdk_subsurface_is_above_parent (sub));
  g_assert_true (gdk_subsurface_get_transform (sub) == GDK_DIHEDRAL_90);
  gdk_subsurface_get_source_rect (sub, &rect);
  g_assert_true (graphene_rect_equal (&rect, &TEXTURE_RECT (texture)));
  gdk_subsurface_get_texture_rect (sub, &rect);
  g_assert_true (graphene_rect_equal (&rect, &GRAPHENE_RECT_INIT (0, 0, 10, 10)));
  gdk_subsurface_get_background_rect (sub, &rect);
  g_assert_true (graphene_rect_equal (&rect, &GRAPHENE_RECT_INIT (0, 0, 20, 20)));

  g_object_unref (sub);
  g_object_unref (texture);
  gdk_surface_destroy (surface);
}

static void
test_subsurface_stacking (void)
{
  GdkSurface *surface;
  GdkSubsurface *sub0, *sub1, *sub2;
  GdkTexture *texture;

#ifdef GDK_WINDOWING_WAYLAND
  if (!GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
#endif
    {
      g_test_skip ("No subsurface support");
      return;
    }

  surface = gdk_surface_new_toplevel (gdk_display_get_default ());

  g_assert_true (surface->subsurfaces_below == NULL);
  g_assert_true (surface->subsurfaces_above == NULL);

  sub0 = gdk_surface_create_subsurface (surface);
  sub1 = gdk_surface_create_subsurface (surface);
  sub2 = gdk_surface_create_subsurface (surface);

  g_assert_true (gdk_surface_get_n_subsurfaces (surface) == 3);
  g_assert_true (gdk_surface_get_subsurface (surface, 0) == sub0);
  g_assert_true (gdk_surface_get_subsurface (surface, 1) == sub1);
  g_assert_true (gdk_surface_get_subsurface (surface, 2) == sub2);

  texture = gdk_texture_new_from_resource ("/org/gtk/libgtk/icons/16x16/actions/media-eject.png");

  gdk_subsurface_attach (sub0, texture, &TEXTURE_RECT (texture), &GRAPHENE_RECT_INIT (0, 0, 10, 10), GDK_DIHEDRAL_NORMAL, NULL, TRUE, NULL);
  gdk_subsurface_attach (sub1, texture, &TEXTURE_RECT (texture), &GRAPHENE_RECT_INIT (0, 0, 10, 10), GDK_DIHEDRAL_NORMAL, NULL, TRUE, NULL);
  gdk_subsurface_attach (sub2, texture, &TEXTURE_RECT (texture), &GRAPHENE_RECT_INIT (0, 0, 10, 10), GDK_DIHEDRAL_NORMAL, NULL, TRUE, NULL);

  g_assert_true (surface->subsurfaces_above == sub2);
  g_assert_true (sub2->sibling_below == NULL);
  g_assert_true (sub2->sibling_above == sub1);
  g_assert_true (sub2->above_parent);
  g_assert_true (sub1->sibling_below == sub2);
  g_assert_true (sub1->sibling_above == sub0);
  g_assert_true (sub1->above_parent);
  g_assert_true (sub0->sibling_below == sub1);
  g_assert_true (sub0->sibling_above == NULL);
  g_assert_true (sub0->above_parent);

  gdk_subsurface_detach (sub1);

  g_assert_true (surface->subsurfaces_above == sub2);
  g_assert_true (sub2->sibling_below == NULL);
  g_assert_true (sub2->sibling_above == sub0);
  g_assert_true (sub2->above_parent);
  g_assert_true (sub0->sibling_below == sub2);
  g_assert_true (sub0->sibling_above == NULL);
  g_assert_true (sub0->above_parent);

  gdk_subsurface_attach (sub2, texture, &TEXTURE_RECT (texture), &GRAPHENE_RECT_INIT (0, 0, 10, 10), GDK_DIHEDRAL_NORMAL, NULL, FALSE, NULL);

  g_assert_true (surface->subsurfaces_above == sub0);
  g_assert_true (sub0->sibling_below == NULL);
  g_assert_true (sub0->sibling_above == NULL);
  g_assert_true (sub0->above_parent);

  g_assert_true (surface->subsurfaces_below == sub2);
  g_assert_true (sub2->sibling_below == NULL);
  g_assert_true (sub2->sibling_above == NULL);
  g_assert_false (sub2->above_parent);

  gdk_subsurface_attach (sub1, texture, &TEXTURE_RECT (texture), &GRAPHENE_RECT_INIT (0, 0, 10, 10), GDK_DIHEDRAL_NORMAL, NULL, TRUE, sub2);

  g_assert_true (surface->subsurfaces_below == sub1);
  g_assert_true (sub1->sibling_above == NULL);
  g_assert_true (sub1->sibling_below == sub2);
  g_assert_false (sub1->above_parent);
  g_assert_true (sub2->sibling_above == sub1);
  g_assert_true (sub2->sibling_below == NULL);
  g_assert_false (sub2->above_parent);

  g_object_unref (sub0);
  g_object_unref (sub1);
  g_object_unref (sub2);

  g_object_unref (texture);

  gdk_surface_destroy (surface);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  gdk_display_set_debug_flags (gdk_display_get_default (), GDK_DEBUG_FORCE_OFFLOAD);

  g_test_add_func ("/subsurface/basics", test_subsurface_basics);
  g_test_add_func ("/subsurface/stacking", test_subsurface_stacking);

  return g_test_run ();
}
