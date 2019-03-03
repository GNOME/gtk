#include "config.h"
#include "focusoverlay.h"
#include "gtkwidgetprivate.h"

struct _GtkFocusOverlay
{
  GtkInspectorOverlay parent_instance;
};

struct _GtkFocusOverlayClass
{
  GtkInspectorOverlayClass parent_class;
};

G_DEFINE_TYPE (GtkFocusOverlay, gtk_focus_overlay, GTK_TYPE_INSPECTOR_OVERLAY)

static void
draw_focus_location (GtkWidget   *widget,
                     GtkWidget   *child,
                     GtkSnapshot *snapshot,
                     const char  *color,
                     const char  *text)
{
  GtkAllocation allocation;
  graphene_rect_t bounds;
  PangoLayout *layout;
  PangoRectangle rect;
  GskRoundedRect rrect;
  int x, y;
  GdkRGBA rgba;

  if (child == NULL)
    return;

  gdk_rgba_parse (&rgba, color);

  gtk_widget_get_allocation (child, &allocation);

  gtk_snapshot_save (snapshot);

  gtk_widget_translate_coordinates (child, widget, 0, 0, &x, &y);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));

  gtk_snapshot_push_debug (snapshot, "Widget focus debugging");

  layout = gtk_widget_create_pango_layout (widget, NULL);
  pango_layout_set_markup (layout, text, -1);
  pango_layout_get_extents (layout, NULL, &rect);
  graphene_rect_init (&bounds,
                      0, 0,
                      rect.width / PANGO_SCALE + 20,
                      rect.height / PANGO_SCALE + 20);
  gsk_rounded_rect_init_from_rect (&rrect, &bounds, 10.0f); 

  gtk_snapshot_push_rounded_clip (snapshot, &rrect);

  gtk_snapshot_append_color (snapshot, &rgba, &bounds);

  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (10, 10));
  gtk_snapshot_append_layout (snapshot, layout, &(GdkRGBA) { 1, 1, 1, 1 });
  g_object_unref (layout);

  gtk_snapshot_pop (snapshot);
  gtk_snapshot_pop (snapshot);

  gtk_snapshot_restore (snapshot);
}

static void
draw_focus_locations (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GtkWidget *focus;
  GtkWidget *next;
  int i;
  char text[20];

  if (!gtk_widget_get_mapped (widget))
    return;

  focus = gtk_root_get_focus (GTK_ROOT (widget));
  if (focus == NULL)
    return;

  draw_focus_location (widget, focus, snapshot, "rgba(0,0,255,0.4)", "Focus");

  next = focus;
  for (i = 1; i < 4; i++)
    {
      g_snprintf (text, 20, "<big>⇥</big> %d", i);
      next = gtk_widget_get_next_focus (next, GTK_DIR_TAB_FORWARD);
      if (!next)
        break;
      draw_focus_location (widget, next, snapshot, "rgba(255,0,255,0.4)", text);
    }

  next = focus;
  for (i = 1; i < 4; i++)
    {
      g_snprintf (text, 20, "<big>⇤</big> %d", i);
      next = gtk_widget_get_next_focus (next, GTK_DIR_TAB_BACKWARD);
      if (!next)
        break;
      draw_focus_location (widget, next, snapshot, "rgba(255,0,255,0.4)", text);
    }
}

static void
gtk_focus_overlay_snapshot (GtkInspectorOverlay *overlay,
                            GtkSnapshot         *snapshot,
                            GskRenderNode       *node,
                            GtkWidget           *widget)
{
  draw_focus_locations (widget, snapshot);
}

static void
gtk_focus_overlay_init (GtkFocusOverlay *self)
{

}

static void
gtk_focus_overlay_class_init (GtkFocusOverlayClass *klass)
{
  GtkInspectorOverlayClass *overlay_class = (GtkInspectorOverlayClass *)klass;

  overlay_class->snapshot = gtk_focus_overlay_snapshot;
}

GtkInspectorOverlay *
gtk_focus_overlay_new (void)
{
  return g_object_new (GTK_TYPE_FOCUS_OVERLAY, NULL);
}
