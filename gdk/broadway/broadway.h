typedef struct BroadwayClient BroadwayClient;

typedef struct  {
    int x, y;
    int width, height;
} BroadwayRect;

BroadwayClient *broadway_client_new             (int             fd);
void            broadway_client_flush           (BroadwayClient *client);
void            broadway_client_new_surface     (BroadwayClient *client,
						 int             id,
						 int             x,
						 int             y,
						 int             w,
						 int             h);
void            broadway_client_show_surface    (BroadwayClient *client,
						 int             id);
void            broadway_client_hide_surface    (BroadwayClient *client,
						 int             id);
void            broadway_client_destroy_surface (BroadwayClient *client,
						 int             id);
void            broadway_client_move_surface    (BroadwayClient *client,
						 int             id,
						 int             x,
						 int             y);
void            broadway_client_put_rgb         (BroadwayClient *client,
						 int             id,
						 int             x,
						 int             y,
						 int             w,
						 int             h,
						 int             byte_stride,
						 void           *data);
void            broadway_client_put_rgba        (BroadwayClient *client,
						 int             id,
						 int             x,
						 int             y,
						 int             w,
						 int             h,
						 int             byte_stride,
						 void           *data);
void            broadway_client_copy_rectangles (BroadwayClient *client,
						 int             id,
						 BroadwayRect   *rects,
						 int             n_rects,
						 int             dx,
						 int             dy);
