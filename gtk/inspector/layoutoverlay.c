
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
  gboolean needs_clip;
  int width = gtk_widget_get_width (widget);
  int height = gtk_widget_get_height (widget);
  GtkBorder margin, border, padding;
  GtkBorder widget_margin;
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

  /* width/height are the content size and we're going to grow that
   * as we're drawing the boxes, as well as offset the origin.
   * Right now we're at the widget's own origin.
   */
  gtk_snapshot_save (snapshot);
  gtk_snapshot_push_debug (snapshot, "Widget layout debugging");


  /* CSS Padding */
  gtk_snapshot_append_color (snapshot, &PADDING_COLOR,
                             &GRAPHENE_RECT_INIT ( 0, - padding.top, width, padding.top));
  gtk_snapshot_append_color (snapshot, &PADDING_COLOR,
                             &GRAPHENE_RECT_INIT (width, 0, padding.right, height));
  gtk_snapshot_append_color (snapshot, &PADDING_COLOR,
                             &GRAPHENE_RECT_INIT (0, height, width, padding.bottom));
  gtk_snapshot_append_color (snapshot, &PADDING_COLOR,
                             &GRAPHENE_RECT_INIT (- padding.left, 0, padding.left, height));

  /* Grow box + offset */
  width += padding.left + padding.right;
  height += padding.top + padding.bottom;
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (- padding.left, - padding.top));

  /* CSS Margins */
  gtk_snapshot_append_color (snapshot, &MARGIN_COLOR,
                             &GRAPHENE_RECT_INIT (0, - margin.top, width, margin.top));
  gtk_snapshot_append_color (snapshot, &MARGIN_COLOR,
                             &GRAPHENE_RECT_INIT (width, 0, margin.right, height));
  gtk_snapshot_append_color (snapshot, &MARGIN_COLOR,
                             &GRAPHENE_RECT_INIT (0, height, width, margin.bottom));
  gtk_snapshot_append_color (snapshot, &MARGIN_COLOR,
                             &GRAPHENE_RECT_INIT (- margin.left, 0, margin.left, height));

  width += margin.left + margin.right;
  height += margin.top + margin.bottom;
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (- margin.left, - margin.top));

  /* Widget Margins */
  gtk_snapshot_append_color (snapshot, &WIDGET_MARGIN_COLOR,
                             &GRAPHENE_RECT_INIT (0, - widget_margin.top, width, widget_margin.top));
  gtk_snapshot_append_color (snapshot, &WIDGET_MARGIN_COLOR,
                             &GRAPHENE_RECT_INIT (width, 0, widget_margin.right, height));
  gtk_snapshot_append_color (snapshot, &WIDGET_MARGIN_COLOR,
                             &GRAPHENE_RECT_INIT (0, height, width, widget_margin.bottom));
  gtk_snapshot_append_color (snapshot, &WIDGET_MARGIN_COLOR,
                             &GRAPHENE_RECT_INIT (- widget_margin.left, 0, widget_margin.left, height));

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
