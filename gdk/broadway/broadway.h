typedef struct BroadwayOutput BroadwayOutput;

typedef struct  {
    int x, y;
    int width, height;
} BroadwayRect;

BroadwayOutput *broadway_output_new             (int             fd);
void            broadway_output_free            (BroadwayOutput *output);
int             broadway_output_flush           (BroadwayOutput *output);
int             broadway_output_has_error       (BroadwayOutput *output);
void            broadway_output_new_surface     (BroadwayOutput *output,
						 int             id,
						 int             x,
						 int             y,
						 int             w,
						 int             h);
void            broadway_output_show_surface    (BroadwayOutput *output,
						 int             id);
void            broadway_output_hide_surface    (BroadwayOutput *output,
						 int             id);
void            broadway_output_destroy_surface (BroadwayOutput *output,
						 int             id);
void            broadway_output_move_surface    (BroadwayOutput *output,
						 int             id,
						 int             x,
						 int             y);
void            broadway_output_resize_surface  (BroadwayOutput *output,
						 int             id,
						 int             w,
						 int             h);
void            broadway_output_put_rgb         (BroadwayOutput *output,
						 int             id,
						 int             x,
						 int             y,
						 int             w,
						 int             h,
						 int             byte_stride,
						 void           *data);
void            broadway_output_put_rgba        (BroadwayOutput *output,
						 int             id,
						 int             x,
						 int             y,
						 int             w,
						 int             h,
						 int             byte_stride,
						 void           *data);
void            broadway_output_copy_rectangles (BroadwayOutput *output,
						 int             id,
						 BroadwayRect   *rects,
						 int             n_rects,
						 int             dx,
						 int             dy);
