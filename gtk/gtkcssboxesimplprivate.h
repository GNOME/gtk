/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Benjamin Otte <otte@gnome.org>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_CSS_BOXES_IMPL_PRIVATE_H__
#define __GTK_CSS_BOXES_IMPL_PRIVATE_H__

#include "gtkcssboxesprivate.h"

#include "gtkcsscornervalueprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssdimensionvalueprivate.h"
#include "gtkwidgetprivate.h"

/* This file is included from gtkcssboxesprivate.h */

static inline void
gtk_css_boxes_init (GtkCssBoxes *boxes,
                    GtkWidget   *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  gtk_css_boxes_init_content_box (boxes,
                                  gtk_css_node_get_style (priv->cssnode),
                                  0, 0,
                                  priv->width,
                                  priv->height);
}

static inline void
gtk_css_boxes_init_content_box (GtkCssBoxes *boxes,
                                GtkCssStyle *style,
                                double       x,
                                double       y,
                                double       width,
                                double       height)
{
  memset (boxes, 0, sizeof (GtkCssBoxes));

  boxes->style = style;
  boxes->box[GTK_CSS_AREA_CONTENT_BOX].bounds = GRAPHENE_RECT_INIT (x, y, width, height);
  boxes->has_rect[GTK_CSS_AREA_CONTENT_BOX] = TRUE;
}

static inline void
gtk_css_boxes_init_border_box (GtkCssBoxes *boxes,
                               GtkCssStyle *style,
                               double       x,
                               double       y,
                               double       width,
                               double       height)
{
  memset (boxes, 0, sizeof (GtkCssBoxes));

  boxes->style = style;
  boxes->box[GTK_CSS_AREA_BORDER_BOX].bounds = GRAPHENE_RECT_INIT (x, y, width, height);
  boxes->has_rect[GTK_CSS_AREA_BORDER_BOX] = TRUE;
}

static inline void
gtk_css_boxes_rect_grow (GskRoundedRect *dest,
                         GskRoundedRect *src,
                         double          top,
                         double          right,
                         double          bottom,
                         double          left)
{
  if (left == 0)
    {
      dest->bounds.origin.x = src->bounds.origin.x;
      if (right == 0)
        dest->bounds.size.width = src->bounds.size.width;
      else
        dest->bounds.size.width = src->bounds.size.width + right;
    }
  else
    {
      dest->bounds.origin.x = src->bounds.origin.x - left;
      if (right == 0)
        dest->bounds.size.width = src->bounds.size.width + left;
      else
        dest->bounds.size.width = src->bounds.size.width + left + right;
    }


  if (top == 0)
    {
      dest->bounds.origin.y = src->bounds.origin.y;
      if (bottom == 0)
        dest->bounds.size.height = src->bounds.size.height;
      else
        dest->bounds.size.height = src->bounds.size.height + bottom;
    }
  else
    {
      dest->bounds.origin.y = src->bounds.origin.y - top;
      if (bottom == 0)
        dest->bounds.size.height = src->bounds.size.height + top;
      else
        dest->bounds.size.height = src->bounds.size.height + top + bottom;
    }
}

static inline void
gtk_css_boxes_rect_shrink (GskRoundedRect *dest,
                           GskRoundedRect *src,
                           double          top,
                           double          right,
                           double          bottom,
                           double          left)
{
  /* FIXME: Do we need underflow checks here? */
  dest->bounds.origin.x = src->bounds.origin.x + left;
  dest->bounds.origin.y = src->bounds.origin.y + top;
  dest->bounds.size.width = src->bounds.size.width - left - right;
  dest->bounds.size.height = src->bounds.size.height - top - bottom;
}

static inline void gtk_css_boxes_compute_padding_rect (GtkCssBoxes *boxes);

static inline const graphene_rect_t *
gtk_css_boxes_get_rect (GtkCssBoxes *boxes,
                        GtkCssArea   area)
{
  switch (area)
    {
      case GTK_CSS_AREA_BORDER_BOX:
        return gtk_css_boxes_get_border_rect (boxes);
      case GTK_CSS_AREA_PADDING_BOX:
        return gtk_css_boxes_get_padding_rect (boxes);
      case GTK_CSS_AREA_CONTENT_BOX:
        return gtk_css_boxes_get_content_rect (boxes);
      default:
        g_assert_not_reached ();
        return NULL;
    }
}

static inline void
gtk_css_boxes_compute_border_rect (GtkCssBoxes *boxes)
{
  if (boxes->has_rect[GTK_CSS_AREA_BORDER_BOX])
    return;

  gtk_css_boxes_compute_padding_rect (boxes);

  gtk_css_boxes_rect_grow (&boxes->box[GTK_CSS_AREA_BORDER_BOX],
                           &boxes->box[GTK_CSS_AREA_PADDING_BOX],
                           boxes->style->border->_border_top_width,
                           boxes->style->border->_border_right_width,
                           boxes->style->border->_border_bottom_width,
                           boxes->style->border->_border_left_width);

  boxes->has_rect[GTK_CSS_AREA_BORDER_BOX] = TRUE;
}

static inline void
gtk_css_boxes_compute_padding_rect (GtkCssBoxes *boxes)
{
  if (boxes->has_rect[GTK_CSS_AREA_PADDING_BOX])
    return;

  if (boxes->has_rect[GTK_CSS_AREA_BORDER_BOX])
    {
      gtk_css_boxes_rect_shrink (&boxes->box[GTK_CSS_AREA_PADDING_BOX],
                                 &boxes->box[GTK_CSS_AREA_BORDER_BOX],
                                 boxes->style->border->_border_top_width,
                                 boxes->style->border->_border_right_width,
                                 boxes->style->border->_border_bottom_width,
                                 boxes->style->border->_border_left_width);
    }
  else
    {
      gtk_css_boxes_rect_grow (&boxes->box[GTK_CSS_AREA_PADDING_BOX],
                               &boxes->box[GTK_CSS_AREA_CONTENT_BOX],
                               boxes->style->size->_padding_top,
                               boxes->style->size->_padding_right,
                               boxes->style->size->_padding_bottom,
                               boxes->style->size->_padding_left);
    }

  boxes->has_rect[GTK_CSS_AREA_PADDING_BOX] = TRUE;
}

static inline void
gtk_css_boxes_compute_content_rect (GtkCssBoxes *boxes)
{
  if (boxes->has_rect[GTK_CSS_AREA_CONTENT_BOX])
    return;

  gtk_css_boxes_compute_padding_rect (boxes);

  gtk_css_boxes_rect_shrink (&boxes->box[GTK_CSS_AREA_CONTENT_BOX],
                             &boxes->box[GTK_CSS_AREA_PADDING_BOX],
                             boxes->style->size->_padding_top,
                             boxes->style->size->_padding_right,
                             boxes->style->size->_padding_bottom,
                             boxes->style->size->_padding_left);

  boxes->has_rect[GTK_CSS_AREA_CONTENT_BOX] = TRUE;
}

static inline void
gtk_css_boxes_compute_margin_rect (GtkCssBoxes *boxes)
{
  if (boxes->has_rect[GTK_CSS_AREA_MARGIN_BOX])
    return;

  gtk_css_boxes_compute_border_rect (boxes);

  gtk_css_boxes_rect_grow (&boxes->box[GTK_CSS_AREA_MARGIN_BOX],
                           &boxes->box[GTK_CSS_AREA_BORDER_BOX],
                           boxes->style->size->_margin_top,
                           boxes->style->size->_margin_right,
                           boxes->style->size->_margin_bottom,
                           boxes->style->size->_margin_left);

  boxes->has_rect[GTK_CSS_AREA_MARGIN_BOX] = TRUE;
}

static inline void
gtk_css_boxes_compute_outline_rect (GtkCssBoxes *boxes)
{
  graphene_rect_t *dest, *src;
  double d;

  if (boxes->has_rect[GTK_CSS_AREA_OUTLINE_BOX])
    return;

  gtk_css_boxes_compute_border_rect (boxes);

  dest = &boxes->box[GTK_CSS_AREA_OUTLINE_BOX].bounds;
  src = &boxes->box[GTK_CSS_AREA_BORDER_BOX].bounds;

  d = boxes->style->outline->_outline_offset +
      boxes->style->outline->_outline_width;

  dest->origin.x = src->origin.x - d;
  dest->origin.y = src->origin.y - d;
  dest->size.width = src->size.width + d + d;
  dest->size.height = src->size.height + d + d;

  boxes->has_rect[GTK_CSS_AREA_OUTLINE_BOX] = TRUE;
}

static inline const graphene_rect_t *
gtk_css_boxes_get_margin_rect (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_margin_rect (boxes);

  return &boxes->box[GTK_CSS_AREA_MARGIN_BOX].bounds;
}

static inline const graphene_rect_t *
gtk_css_boxes_get_border_rect (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_border_rect (boxes);

  return &boxes->box[GTK_CSS_AREA_BORDER_BOX].bounds;
}

static inline const graphene_rect_t *
gtk_css_boxes_get_padding_rect (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_padding_rect (boxes);

  return &boxes->box[GTK_CSS_AREA_PADDING_BOX].bounds;
}

static inline const graphene_rect_t *
gtk_css_boxes_get_content_rect (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_content_rect (boxes);

  return &boxes->box[GTK_CSS_AREA_CONTENT_BOX].bounds;
}

static inline const graphene_rect_t *
gtk_css_boxes_get_outline_rect (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_outline_rect (boxes);

  return &boxes->box[GTK_CSS_AREA_OUTLINE_BOX].bounds;
}

/* clamp border radius, following CSS specs */
static inline void
gtk_css_boxes_clamp_border_radius (GskRoundedRect *box)
{
  gdouble factor = 1.0;
  gdouble corners;

  corners = box->corner[GSK_CORNER_TOP_LEFT].width + box->corner[GSK_CORNER_TOP_RIGHT].width;
  if (corners != 0)
    factor = MIN (factor, box->bounds.size.width / corners);

  corners = box->corner[GSK_CORNER_TOP_RIGHT].height + box->corner[GSK_CORNER_BOTTOM_RIGHT].height;
  if (corners != 0)
    factor = MIN (factor, box->bounds.size.height / corners);

  corners = box->corner[GSK_CORNER_BOTTOM_RIGHT].width + box->corner[GSK_CORNER_BOTTOM_LEFT].width;
  if (corners != 0)
    factor = MIN (factor, box->bounds.size.width / corners);

  corners = box->corner[GSK_CORNER_TOP_LEFT].height + box->corner[GSK_CORNER_BOTTOM_LEFT].height;
  if (corners != 0)
    factor = MIN (factor, box->bounds.size.height / corners);

  box->corner[GSK_CORNER_TOP_LEFT].width *= factor;
  box->corner[GSK_CORNER_TOP_LEFT].height *= factor;
  box->corner[GSK_CORNER_TOP_RIGHT].width *= factor;
  box->corner[GSK_CORNER_TOP_RIGHT].height *= factor;
  box->corner[GSK_CORNER_BOTTOM_RIGHT].width *= factor;
  box->corner[GSK_CORNER_BOTTOM_RIGHT].height *= factor;
  box->corner[GSK_CORNER_BOTTOM_LEFT].width *= factor;
  box->corner[GSK_CORNER_BOTTOM_LEFT].height *= factor;
}

static inline void
gtk_css_boxes_apply_border_radius (GskRoundedRect    *box,
                                   double             top_left_x,
                                   double             top_left_y,
                                   double             top_right_x,
                                   double             top_right_y,
                                   double             bottom_right_x,
                                   double             bottom_right_y,
                                   double             bottom_left_x,
                                   double             bottom_left_y)
{
  box->corner[GSK_CORNER_TOP_LEFT].width = top_left_x;
  box->corner[GSK_CORNER_TOP_LEFT].height = top_left_y;
  box->corner[GSK_CORNER_TOP_RIGHT].width = top_right_x;
  box->corner[GSK_CORNER_TOP_RIGHT].height = top_right_y;
  box->corner[GSK_CORNER_BOTTOM_RIGHT].width = bottom_right_x;
  box->corner[GSK_CORNER_BOTTOM_RIGHT].height = bottom_right_y;
  box->corner[GSK_CORNER_BOTTOM_LEFT].width = bottom_left_x;
  box->corner[GSK_CORNER_BOTTOM_LEFT].height = bottom_left_y;

  if (top_left_x != 0 || top_left_y != 0 ||
      top_right_x != 0 || top_right_y != 0 ||
      bottom_right_x != 0 || bottom_right_y != 0 ||
      bottom_left_x != 0 || bottom_left_y != 0)
    gtk_css_boxes_clamp_border_radius (box);
}

/* NB: width and height must be >= 0 */
static inline void
gtk_css_boxes_shrink_border_radius (graphene_size_t       *dest,
                                    const graphene_size_t *src,
                                    double                 width,
                                    double                 height)
{
  dest->width = src->width - width;
  dest->height = src->height - height;

  if (dest->width <= 0 || dest->height <= 0)
    {
      dest->width = 0;
      dest->height = 0;
    }
}

static inline void
gtk_css_boxes_shrink_corners (GskRoundedRect       *dest,
                              const GskRoundedRect *src)
{
  double top = dest->bounds.origin.y - src->bounds.origin.y;
  double right = src->bounds.origin.x + src->bounds.size.width - dest->bounds.origin.x - dest->bounds.size.width;
  double bottom = src->bounds.origin.y + src->bounds.size.height - dest->bounds.origin.y - dest->bounds.size.height;
  double left = dest->bounds.origin.x - src->bounds.origin.x;

  gtk_css_boxes_shrink_border_radius (&dest->corner[GSK_CORNER_TOP_LEFT],
                                      &src->corner[GSK_CORNER_TOP_LEFT],
                                      top, left);
  gtk_css_boxes_shrink_border_radius (&dest->corner[GSK_CORNER_TOP_RIGHT],
                                      &src->corner[GSK_CORNER_TOP_RIGHT],
                                      top, right);
  gtk_css_boxes_shrink_border_radius (&dest->corner[GSK_CORNER_BOTTOM_RIGHT],
                                      &src->corner[GSK_CORNER_BOTTOM_RIGHT],
                                      bottom, right);
  gtk_css_boxes_shrink_border_radius (&dest->corner[GSK_CORNER_BOTTOM_LEFT],
                                      &src->corner[GSK_CORNER_BOTTOM_LEFT],
                                      bottom, left);
}

static inline void
gtk_css_boxes_compute_border_box (GtkCssBoxes *boxes)
{
  if (boxes->has_box[GTK_CSS_AREA_BORDER_BOX])
    return;

  gtk_css_boxes_compute_border_rect (boxes);

  gtk_css_boxes_apply_border_radius (&boxes->box[GTK_CSS_AREA_BORDER_BOX],
                                     boxes->style->border->_border_top_left_radius_x,
                                     boxes->style->border->_border_top_left_radius_y,
                                     boxes->style->border->_border_top_right_radius_x,
                                     boxes->style->border->_border_top_right_radius_y,
                                     boxes->style->border->_border_bottom_right_radius_x,
                                     boxes->style->border->_border_bottom_right_radius_y,
                                     boxes->style->border->_border_bottom_left_radius_x,
                                     boxes->style->border->_border_bottom_left_radius_y);

  boxes->has_box[GTK_CSS_AREA_BORDER_BOX] = TRUE;
}

static inline void
gtk_css_boxes_compute_padding_box (GtkCssBoxes *boxes)
{
  if (boxes->has_box[GTK_CSS_AREA_PADDING_BOX])
    return;

  gtk_css_boxes_compute_border_box (boxes);
  gtk_css_boxes_compute_padding_rect (boxes);

  gtk_css_boxes_shrink_corners (&boxes->box[GTK_CSS_AREA_PADDING_BOX],
                                &boxes->box[GTK_CSS_AREA_BORDER_BOX]);

  boxes->has_box[GTK_CSS_AREA_PADDING_BOX] = TRUE;
}

static inline void
gtk_css_boxes_compute_content_box (GtkCssBoxes *boxes)
{
  if (boxes->has_box[GTK_CSS_AREA_CONTENT_BOX])
    return;

  gtk_css_boxes_compute_padding_box (boxes);
  gtk_css_boxes_compute_content_rect (boxes);

  gtk_css_boxes_shrink_corners (&boxes->box[GTK_CSS_AREA_CONTENT_BOX],
                                &boxes->box[GTK_CSS_AREA_PADDING_BOX]);

  boxes->has_box[GTK_CSS_AREA_CONTENT_BOX] = TRUE;
}

static inline void
gtk_css_boxes_compute_outline_box (GtkCssBoxes *boxes)
{
  if (boxes->has_box[GTK_CSS_AREA_OUTLINE_BOX])
    return;

  gtk_css_boxes_compute_outline_rect (boxes);

  gtk_css_boxes_apply_border_radius (&boxes->box[GTK_CSS_AREA_OUTLINE_BOX],
                                     boxes->style->outline->_outline_top_left_radius_x,
                                     boxes->style->outline->_outline_top_left_radius_y,
                                     boxes->style->outline->_outline_top_right_radius_x,
                                     boxes->style->outline->_outline_top_right_radius_y,
                                     boxes->style->outline->_outline_bottom_right_radius_x,
                                     boxes->style->outline->_outline_bottom_right_radius_y,
                                     boxes->style->outline->_outline_bottom_left_radius_x,
                                     boxes->style->outline->_outline_bottom_left_radius_y);

  boxes->has_box[GTK_CSS_AREA_OUTLINE_BOX] = TRUE;
}

static inline const GskRoundedRect *
gtk_css_boxes_get_box (GtkCssBoxes *boxes,
                       GtkCssArea  area)
{
  switch (area)
    {
      case GTK_CSS_AREA_BORDER_BOX:
        return gtk_css_boxes_get_border_box (boxes);
      case GTK_CSS_AREA_PADDING_BOX:
        return gtk_css_boxes_get_padding_box (boxes);
      case GTK_CSS_AREA_CONTENT_BOX:
        return gtk_css_boxes_get_content_box (boxes);
      default:
        g_assert_not_reached ();
        return NULL;
    }
}

static inline const GskRoundedRect *
gtk_css_boxes_get_border_box (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_border_box (boxes);

  return &boxes->box[GTK_CSS_AREA_BORDER_BOX];
}

static inline const GskRoundedRect *
gtk_css_boxes_get_padding_box (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_padding_box (boxes);

  return &boxes->box[GTK_CSS_AREA_PADDING_BOX];
}

static inline const GskRoundedRect *
gtk_css_boxes_get_content_box (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_content_box (boxes);

  return &boxes->box[GTK_CSS_AREA_CONTENT_BOX];
}

static inline const GskRoundedRect *
gtk_css_boxes_get_outline_box (GtkCssBoxes *boxes)
{
  gtk_css_boxes_compute_outline_box (boxes);

  return &boxes->box[GTK_CSS_AREA_OUTLINE_BOX];
}

#endif /* __GTK_CSS_BOXES_IMPL_PRIVATE_H__ */
