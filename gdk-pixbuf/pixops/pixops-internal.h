#ifdef USE_MMX
guchar *pixops_scale_line_22_33_mmx (guint32 weights[16][8], guchar *p, guchar *q1, guchar *q2, int x_step, guchar *p_stop, int x_init);
guchar *pixops_composite_line_22_4a4_mmx (guint32 weights[16][8], guchar *p, guchar *q1, guchar *q2, int x_step, guchar *p_stop, int x_init);
guchar *pixops_composite_line_color_22_4a4_mmx (guint32 weights[16][8], guchar *p, guchar *q1, guchar *q2, int x_step, guchar *p_stop, int x_init, int dest_x, int check_shift, int *colors);
int pixops_have_mmx (void);
#endif

