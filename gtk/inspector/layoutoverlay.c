
#include "config.h"
#include "layoutoverlay.h"
#include "gtkwidgetprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"

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
  gboolean needs_clip;
  int width = gtk_widget_get_width (widget);
  int height = gtk_widget_get_height (widget);
  GtkCssStyle *style;
  GtkWidget *child;
  GtkBorder boxes[4];
  const GdkRGBA colors[4] = {
    {0.7, 0.0, 0.7, 0.6}, /* Padding */
    {0.0, 0.0, 0.0, 0.0}, /* Border */
    {0.7, 0.7, 0.0, 0.6}, /* CSS Margin */
    {0.7, 0.0, 0.0, 0.6}, /* Widget Margin */
  };
  int i;

  if (!gtk_widget_get_mapped (widget))
    return;

  G_STATIC_ASSERT (G_N_ELEMENTS (boxes) == G_N_ELEMENTS (colors));

  style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
  get_box_padding (style, &boxes[0]);
  get_box_border (style, &boxes[1]);
  get_box_margin (style, &boxes[2]);

  /* TODO: Eh, left = start? RTL? */
  boxes[3].left = gtk_widget_get_margin_start (widget);
  boxes[3].top = gtk_widget_get_margin_top (widget);
  boxes[3].right = gtk_widget_get_margin_end (widget);
  boxes[3].bottom = gtk_widget_get_margin_bottom (widget);

  /* width/height are the content size and we're going to grow that
   * as we're drawing the boxes, as well as offset the origin.
   * Right now we're at the widget's own origin.
   */
  gtk_snapshot_save (snapshot);
  gtk_snapshot_push_debug (snapshot, "Widget layout debugging");

  for (i = 0; i < G_N_ELEMENTS (boxes); i ++)
    {
      const GdkRGBA *color = &colors[i];
      const GtkBorder *box = &boxes[i];

      gtk_snapshot_append_color (snapshot, color,
                                 &GRAPHENE_RECT_INIT ( 0, - box->top, width, box->top));
      gtk_snapshot_append_color (snapshot, color,
                                 &GRAPHENE_RECT_INIT (width, 0, box->right, height));
      gtk_snapshot_append_color (snapshot, color,
                                 &GRAPHENE_RECT_INIT (0, height, width, box->bottom));
      gtk_snapshot_append_color (snapshot, color,
                                 &GRAPHENE_RECT_INIT (- box->left, 0, box->left, height));

      /* Grow box + offset */
      width += box->left + box->right;
      height += box->top + box->bottom;
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (- box->left, - box->top));
    }

  gtk_snapshot_pop (snapshot);


  needs_clip = gtk_widget_get_overflow (widget) == GTK_OVERFLOW_HIDDEN &&
               gtk_widget_get_first_child (widget) != NULL;

  if (needs_clip)
    gtk_snapshot_push_clip (snapshot,
                            &GRAPHENE_RECT_INIT (0, 0, gtk_widget_get_width (widget), gtk_widget_get_height (widget)));

  /* Recurse into child widgets */
  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_transform (snapshot, child->priv->transform);

      recurse_child_widgets (child, snapshot);

      gtk_snapshot_restore (snapshot);
    }

  if (needs_clip)
    gtk_snapshot_pop (snapshot);

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
