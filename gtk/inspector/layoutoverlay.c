
#include "config.h"
#include "layoutoverlay.h"
#include "gtkwidgetprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"

static const GdkRGBA WIDGET_MARGIN_COLOR = {0.7, 0,   0, 0.6};
static const GdkRGBA MARGIN_COLOR        = {0.7, 0.7, 0, 0.6};
static const GdkRGBA PADDING_COLOR       = {0.7, 0, 0.7, 0.6};

struct _GtkLayoutOverlay
{
  GtkInspectorOverlay parent_instance;
};

struct _GtkLayoutOverlayClass
{
  GtkInspectorOverlayClass parent_class;
};

G_DEFINE_TYPE (GtkLayoutOverlay, gtk_layout_overlay, GTK_TYPE_INSPECTOR_OVERLAY)


static gint
get_number (GtkCssStyle *style,
            guint        property)
{
  double d = _gtk_css_number_value_get (gtk_css_style_get_value (style, property), 100);

  if (d < 1)
    return ceil (d);
  else
    return floor (d);
}

static void
get_box_margin (GtkCssStyle *style,
                GtkBorder   *margin)
{
  margin->top = get_number (style, GTK_CSS_PROPERTY_MARGIN_TOP);
  margin->left = get_number (style, GTK_CSS_PROPERTY_MARGIN_LEFT);
  margin->bottom = get_number (style, GTK_CSS_PROPERTY_MARGIN_BOTTOM);
  margin->right = get_number (style, GTK_CSS_PROPERTY_MARGIN_RIGHT);
}

static void
get_box_border (GtkCssStyle *style,
                GtkBorder   *border)
{
  border->top = get_number (style, GTK_CSS_PROPERTY_BORDER_TOP_WIDTH);
  border->left = get_number (style, GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH);
  border->bottom = get_number (style, GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH);
  border->right = get_number (style, GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH);
}

static void
get_box_padding (GtkCssStyle *style,
                 GtkBorder   *border)
{
  border->top = get_number (style, GTK_CSS_PROPERTY_PADDING_TOP);
  border->left = get_number (style, GTK_CSS_PROPERTY_PADDING_LEFT);
  border->bottom = get_number (style, GTK_CSS_PROPERTY_PADDING_BOTTOM);
  border->right = get_number (style, GTK_CSS_PROPERTY_PADDING_RIGHT);
}

static void
recurse_child_widgets (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  GtkBorder margin, border, padding;
  GtkBorder widget_margin;
  GtkAllocation allocation;
  graphene_rect_t bounds;
  GtkCssStyle *style;
  GtkWidget *child;

  if (!gtk_widget_get_mapped (widget))
    return;

  style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
  get_box_margin (style, &margin);
  get_box_border (style, &border);
  get_box_padding (style, &padding);

  /* TODO: Eh, left = start? RTL? */
  widget_margin.left = gtk_widget_get_margin_start (widget);
  widget_margin.top = gtk_widget_get_margin_top (widget);
  widget_margin.right = gtk_widget_get_margin_end (widget);
  widget_margin.bottom = gtk_widget_get_margin_bottom (widget);

  gtk_widget_get_allocation (widget, &allocation);

  gtk_snapshot_save (snapshot);

  /* Offset for all of the drawing done here. We assume cooridinates relative to
   * the widget allocation, not the content allocation. */
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (allocation.x, allocation.y));

  /* Now do all the stuff */
  gtk_snapshot_push_debug (snapshot, "Widget layout debugging");

  /* Widget margins */
  graphene_rect_init (&bounds,
                      0, -widget_margin.top,
                      allocation.width, widget_margin.top);
  gtk_snapshot_append_color (snapshot, &WIDGET_MARGIN_COLOR, &bounds);

  graphene_rect_init (&bounds,
                      0, allocation.height,
                      allocation.width, widget_margin.bottom);
  gtk_snapshot_append_color (snapshot, &WIDGET_MARGIN_COLOR, &bounds);

  graphene_rect_init (&bounds,
                      -widget_margin.left, 0,
                      widget_margin.left, allocation.height);
  gtk_snapshot_append_color (snapshot, &WIDGET_MARGIN_COLOR, &bounds);

  graphene_rect_init (&bounds,
                      allocation.width, 0,
                      widget_margin.right, allocation.height);
  gtk_snapshot_append_color (snapshot, &WIDGET_MARGIN_COLOR, &bounds);


  /* CSS Margins */
  graphene_rect_init (&bounds,
                      0, 0,
                      allocation.width, margin.top);
  gtk_snapshot_append_color (snapshot, &MARGIN_COLOR, &bounds);

  graphene_rect_init (&bounds,
                      0, allocation.height - margin.bottom,
                      allocation.width, margin.bottom);
  gtk_snapshot_append_color (snapshot, &MARGIN_COLOR, &bounds);

  graphene_rect_init (&bounds,
                      0, margin.top,
                      margin.left, allocation.height - margin.top - margin.bottom);
  gtk_snapshot_append_color (snapshot, &MARGIN_COLOR, &bounds);

  graphene_rect_init (&bounds,
                      allocation.width - margin.right, margin.top,
                      margin.right, allocation.height - margin.top - margin.bottom);
  gtk_snapshot_append_color (snapshot, &MARGIN_COLOR, &bounds);


  /* Padding */
  graphene_rect_init (&bounds,
                      margin.left + border.left,
                      margin.top + border.top,
                      allocation.width - margin.left - margin.right - border.left - border.right,
                      padding.top);
  gtk_snapshot_append_color (snapshot, &PADDING_COLOR, &bounds);

  graphene_rect_init (&bounds,
                      margin.left + border.left,
                      allocation.height - margin.bottom - border.bottom - padding.bottom,
                      allocation.width - margin.left - margin.right - border.left - border.right,
                      padding.bottom);
  gtk_snapshot_append_color (snapshot, &PADDING_COLOR, &bounds);

  graphene_rect_init (&bounds,
                      margin.left + border.left,
                      margin.top + border.top + padding.top,
                      padding.left,
                      allocation.height - margin.top - margin.bottom - border.top - border.bottom - padding.top - padding.bottom);
  gtk_snapshot_append_color (snapshot, &PADDING_COLOR, &bounds);

  graphene_rect_init (&bounds,
                      allocation.width - margin.right - border.right - padding.right,
                      margin.top + border.top + padding.top,
                      padding.right,
                      allocation.height - margin.top - margin.bottom - border.top - border.bottom - padding.top - padding.bottom);
  gtk_snapshot_append_color (snapshot, &PADDING_COLOR, &bounds);

  gtk_snapshot_pop (snapshot);


  /* Recurse into child widgets */
  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      const int offset_x = margin.left + border.left + padding.left;
      const int offset_y = margin.top + border.top + padding.top;

      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (offset_x, offset_y));
      recurse_child_widgets (child, snapshot);
    }

  gtk_snapshot_restore (snapshot);
}

static void
gtk_layout_overlay_snapshot (GtkInspectorOverlay *overlay,
                             GtkSnapshot         *snapshot,
                             GskRenderNode       *node,
                             GtkWidget           *widget)
{
  recurse_child_widgets (widget, snapshot);
}

static void
gtk_layout_overlay_init (GtkLayoutOverlay *self)
{

}

static void
gtk_layout_overlay_class_init (GtkLayoutOverlayClass *klass)
{
  GtkInspectorOverlayClass *overlay_class = (GtkInspectorOverlayClass *)klass;

  overlay_class->snapshot = gtk_layout_overlay_snapshot;
}

GtkInspectorOverlay *
gtk_layout_overlay_new (void)
{
  return g_object_new (GTK_TYPE_LAYOUT_OVERLAY, NULL);
}
