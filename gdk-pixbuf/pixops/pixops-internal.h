#ifdef USE_MMX
art_u8 *pixops_scale_line_22_33_mmx (art_u32 weights[16][8], art_u8 *p, art_u8 *q1, art_u8 *q2, int x_step, art_u8 *p_stop, int x_init);
art_u8 *pixops_composite_line_22_4a4_mmx (art_u32 weights[16][8], art_u8 *p, art_u8 *q1, art_u8 *q2, int x_step, art_u8 *p_stop, int x_init);
art_u8 *pixops_composite_line_color_22_4a4_mmx (art_u32 weights[16][8], art_u8 *p, art_u8 *q1, art_u8 *q2, int x_step, art_u8 *p_stop, int x_init, int dest_x, int check_shift, int *colors);
int pixops_have_mmx (void);
#endif

