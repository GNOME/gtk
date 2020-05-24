
#pragma once

#define SANITY_CHECKS 0



#define rounded_rect_top_left(r) (GRAPHENE_RECT_INIT(r->bounds.origin.x, \
                                                     r->bounds.origin.y, \
                                                     r->corner[0].width, r->corner[0].height))

#define rounded_rect_top_right(r) (GRAPHENE_RECT_INIT(r->bounds.origin.x + r->bounds.size.width - r->corner[1].width, \
                                                      r->bounds.origin.y, \
                                                      r->corner[1].width, r->corner[1].height))

#define rounded_rect_bottom_right(r) (GRAPHENE_RECT_INIT(r->bounds.origin.x + r->bounds.size.width - r->corner[2].width, \
                                                         r->bounds.origin.y + r->bounds.size.height - r->corner[2].height, \
                                                         r->corner[2].width, r->corner[2].height))

#define rounded_rect_bottom_left(r) (GRAPHENE_RECT_INIT(r->bounds.origin.x, \
                                                         r->bounds.origin.y + r->bounds.size.height - r->corner[2].height, \
                                                         r->corner[3].width, r->corner[3].height))


#define rounded_rect_corner0(r) rounded_rect_top_left(r)
#define rounded_rect_corner1(r) rounded_rect_top_right(r)
#define rounded_rect_corner2(r) rounded_rect_bottom_right(r)
#define rounded_rect_corner3(r) rounded_rect_bottom_left(r)

#define rounded_rect_corner(r, i) (rounded_rect_corner ##i(r))
#define graphene_size_non_zero(s) (s->width > 0 && s->height > 0)
#define rounded_rect_has_corner(r, i) (r->corner[i].width > 0 && r->corner[i].height > 0)

#define rect_contains_point(r, _x, _y) (_x >= (r)->origin.x && _x <= (r)->origin.x + (r)->size.width && \
                                        _y >= (r)->origin.y && _y <= (r)->origin.y + (r)->size.height)

enum {
  NINE_SLICE_TOP_LEFT      = 0,
  NINE_SLICE_TOP_CENTER    = 1,
  NINE_SLICE_TOP_RIGHT     = 2,
  NINE_SLICE_LEFT_CENTER   = 3,
  NINE_SLICE_CENTER        = 4,
  NINE_SLICE_RIGHT_CENTER  = 5,
  NINE_SLICE_BOTTOM_LEFT   = 6,
  NINE_SLICE_BOTTOM_CENTER = 7,
  NINE_SLICE_BOTTOM_RIGHT  = 8,
};
#define NINE_SLICE_SIZE 9 /* Hah. */

typedef struct
{
  int texture_id;
  float x;
  float y;
  float x2;
  float y2;
} TextureRegion;

static inline bool G_GNUC_PURE
slice_is_visible (const cairo_rectangle_int_t *r)
{
  return (r->width > 0 && r->height > 0);
}

static inline void
nine_slice_rounded_rect (const GskRoundedRect  *rect,
                         cairo_rectangle_int_t *out_rects)
{
  const graphene_point_t *origin = &rect->bounds.origin;
  const graphene_size_t *size = &rect->bounds.size;
  const int top_height = ceilf (MAX (rect->corner[GSK_CORNER_TOP_LEFT].height,
                                     rect->corner[GSK_CORNER_TOP_RIGHT].height));
  const int bottom_height = ceilf (MAX (rect->corner[GSK_CORNER_BOTTOM_LEFT].height,
                                        rect->corner[GSK_CORNER_BOTTOM_RIGHT].height));
  const int right_width = ceilf (MAX (rect->corner[GSK_CORNER_TOP_RIGHT].width,
                                      rect->corner[GSK_CORNER_BOTTOM_RIGHT].width));
  const int left_width = ceilf (MAX (rect->corner[GSK_CORNER_TOP_LEFT].width,
                                     rect->corner[GSK_CORNER_BOTTOM_LEFT].width));

  /* Top left */
  out_rects[0] = (cairo_rectangle_int_t) {
                   origin->x, origin->y,
                   left_width, top_height,
                 };

  /* Top center */
  out_rects[1] = (cairo_rectangle_int_t) {
                   origin->x + size->width / 2.0 - 0.5, origin->y,
                   1, top_height,
                 };

  /* Top right */
  out_rects[2] = (cairo_rectangle_int_t) {
                   origin->x + size->width - right_width, origin->y,
                   right_width, top_height
                 };

  /* Left center */
  out_rects[3] = (cairo_rectangle_int_t) {
                   origin->x, origin->y + size->height / 2,
                   left_width, 1,
                 };

  /* center */
  out_rects[4] = (cairo_rectangle_int_t) {
                   origin->x + size->width / 2.0 - 0.5,
                   origin->y + size->height / 2.0 - 0.5,
                   1, 1
                 };

  /* Right center */
  out_rects[5] = (cairo_rectangle_int_t) {
                   origin->x + size->width - right_width,
                   origin->y + (size->height / 2.0) - 0.5,
                   right_width,
                   1,
                 };

  /* Bottom Left */
  out_rects[6] = (cairo_rectangle_int_t) {
                   origin->x, origin->y + size->height - bottom_height,
                   left_width, bottom_height,
                 };

  /* Bottom center */
  out_rects[7] = (cairo_rectangle_int_t) {
                   origin->x + (size->width / 2.0) - 0.5,
                     origin->y + size->height - bottom_height,
                   1, bottom_height,
                 };

  /* Bottom right */
  out_rects[8] = (cairo_rectangle_int_t) {
                   origin->x + size->width - right_width,
                   origin->y + size->height - bottom_height,
                   right_width, bottom_height,
                 };

#if SANITY_CHECKS
  g_assert_cmpfloat (size->width, >=, left_width + right_width);
  g_assert_cmpfloat (size->height, >=, top_height + bottom_height);
#endif
}

static inline void
nine_slice_grow (cairo_rectangle_int_t *slices,
                 const int              amount)
{
  /* top left */
  slices[0].x -= amount;
  slices[0].y -= amount;
  if (amount > slices[0].width)
    slices[0].width += amount * 2;
  else
    slices[0].width += amount;

  if (amount > slices[0].height)
    slices[0].height += amount * 2;
  else
    slices[0].height += amount;


  /* Top center */
  slices[1].y -= amount;
  if (amount > slices[1].height)
    slices[1].height += amount * 2;
  else
    slices[1].height += amount;

  /* top right */
  slices[2].y -= amount;
  if (amount > slices[2].width)
    {
      slices[2].x -= amount;
      slices[2].width += amount * 2;
    }
  else
    {
     slices[2].width += amount;
    }

  if (amount > slices[2].height)
    slices[2].height += amount * 2;
  else
    slices[2].height += amount;



  slices[3].x -= amount;
  if (amount > slices[3].width)
    slices[3].width += amount * 2;
  else
    slices[3].width += amount;

  /* Leave Britney^Wcenter alone */

  if (amount > slices[5].width)
    {
      slices[5].x -= amount;
      slices[5].width += amount * 2;
    }
  else
    {
      slices[5].width += amount;
    }


  /* Bottom left */
  slices[6].x -= amount;
  if (amount > slices[6].width)
    {
      slices[6].width += amount * 2;
    }
  else
    {
      slices[6].width += amount;
    }

  if (amount > slices[6].height)
    {
      slices[6].y -= amount;
      slices[6].height += amount * 2;
    }
  else
    {
      slices[6].height += amount;
    }


  /* Bottom center */
  if (amount > slices[7].height)
    {
      slices[7].y -= amount;
      slices[7].height += amount * 2;
    }
  else
    {
      slices[7].height += amount;
    }

  if (amount > slices[8].width)
    {
      slices[8].x -= amount;
      slices[8].width += amount * 2;
    }
  else
    {
      slices[8].width += amount;
    }

  if (amount > slices[8].height)
    {
      slices[8].y -= amount;
      slices[8].height += amount * 2;
    }
  else
    {
      slices[8].height += amount;
    }

#if SANITY_CHECKS
  {
    for (int i = 0; i < 9; i ++)
      {
        g_assert_cmpint (slices[i].x, >=, 0);
        g_assert_cmpint (slices[i].y, >=, 0);
        g_assert_cmpint (slices[i].width, >=, 0);
        g_assert_cmpint (slices[i].height, >=, 0);
      }

    /* Rows don't overlap */
    for (int i = 0; i < 3; i++)
      {
        g_assert_cmpint (slices[i * 3 + 0].x + slices[i * 3 + 0].width, <, slices[i * 3 + 1].x);
      }
  }
#endif

}

static inline void
nine_slice_to_texture_coords (const cairo_rectangle_int_t *slices,
                              const int                    texture_width,
                              const int                    texture_height,
                              TextureRegion               *out_regions)
{
  const float fw = (float)texture_width;
  const float fh = (float)texture_height;
  int i;

  for (i = 0; i < 9; i++)
    {
      out_regions[i] = (TextureRegion) {
        0, /* Texture id */
        slices[i].x / fw,
        1.0 - ((slices[i].y + slices[i].height) / fh),
        (slices[i].x + slices[i].width)  / fw,
        1.0 - (slices[i].y / fh),
      };
    }

#if SANITY_CHECKS
  {
    for (i = 0; i < 9; i++)
      {
        const TextureRegion *r = &out_regions[i];
        g_assert_cmpfloat (r->x, >=, 0);
        g_assert_cmpfloat (r->x, <=, 1);
        g_assert_cmpfloat (r->y, >=, 0);
        g_assert_cmpfloat (r->y, <=, 1);

        g_assert_cmpfloat (r->x, <, r->x2);
        g_assert_cmpfloat (r->y, <, r->y2);
      }
  }
#endif
}
