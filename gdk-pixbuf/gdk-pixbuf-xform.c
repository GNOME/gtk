GdkPixbuf *
gdk_pixbuf_scale (const GdkPixbuf *pixbuf, gint w, gint h)
{
	art_u8 *pixels;
	gint rowstride;
	double affine[6];
	ArtAlphaGamma *alphagamma;
	ArtPixBuf *art_pixbuf = NULL;
	GdkPixbuf *copy = NULL;

	alphagamma = NULL;

	affine[1] = affine[2] = affine[4] = affine[5] = 0;

	affine[0] = w / (double)(pixbuf->art_pixbuf->width);
	affine[3] = h / (double)(pixbuf->art_pixbuf->height);

	/* rowstride = w * pixbuf->art_pixbuf->n_channels; */
	rowstride = w * 3;

	pixels = art_alloc (h * rowstride);
	art_rgb_pixbuf_affine (pixels, 0, 0, w, h, rowstride,
			       pixbuf->art_pixbuf,
			       affine, ART_FILTER_NEAREST, alphagamma);

	if (pixbuf->art_pixbuf->has_alpha)
		/* should be rgba */
		art_pixbuf = art_pixbuf_new_rgb(pixels, w, h, rowstride);
	else
		art_pixbuf = art_pixbuf_new_rgb(pixels, w, h, rowstride);

	copy = gdk_pixbuf_new (art_pixbuf, NULL);

	if (!copy)
		art_free (pixels);

	return copy;
}

GdkPixbuf *
gdk_pixbuf_rotate (GdkPixbuf *pixbuf, gdouble angle)
{
	art_u8 *pixels;
	gint rowstride, w, h;
	gdouble rad;
	double rot[6], trans[6], affine[6];
	ArtAlphaGamma *alphagamma = NULL;
	ArtPixBuf *art_pixbuf = NULL;

	w = pixbuf->art_pixbuf->width;
	h = pixbuf->art_pixbuf->height;

	rad = (M_PI * angle / 180.0);

	rot[0] = cos(rad);
	rot[1] = sin(rad);
	rot[2] = -sin(rad);
	rot[3] = cos(rad);
	rot[4] = rot[5] = 0;

	trans[0] = trans[3] = 1;
	trans[1] = trans[2] = 0;
	trans[4] = -(double)w / 2.0;
	trans[5] = -(double)h / 2.0;

	art_affine_multiply(rot, trans, rot);

	trans[0] = trans[3] = 1;
	trans[1] = trans[2] = 0;
	trans[4] = (double)w / 2.0;
	trans[5] = (double)h / 2.0;

	art_affine_multiply(affine, rot, trans);

	g_print("Affine: %e %e %e %e %e %e\n", affine[0], affine[1], affine[2],
		affine[3], affine[4], affine[5]);

	/* rowstride = w * pixbuf->art_pixbuf->n_channels; */
	rowstride = w * 3;

	pixels = art_alloc (h * rowstride);
	art_rgb_pixbuf_affine (pixels, 0, 0, w, h, rowstride,
			       pixbuf->art_pixbuf,
			       affine, ART_FILTER_NEAREST, alphagamma);
	if (pixbuf->art_pixbuf->has_alpha)
		/* should be rgba */
		art_pixbuf = art_pixbuf_new_rgb(pixels, w, h, rowstride);
	else
		art_pixbuf = art_pixbuf_new_rgb(pixels, w, h, rowstride);

	art_pixbuf_free (pixbuf->art_pixbuf);
	pixbuf->art_pixbuf = art_pixbuf;

	return pixbuf;
}
