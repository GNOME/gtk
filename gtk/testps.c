#include <gtk/gtk.h>

#include <stdio.h>
#include <unistd.h>
#include <time.h>

gint a4w = 595;
gint a4h = 842;

GdkColor red;
GdkColor blue;
GdkColor green;
GdkColor white;
GdkColor black;
GdkGC *gc;
GdkColormap* cmap;
GdkFont *font;
int fsize;
GdkPoint poly[] = {{10, 20}, {37,50}, {15,90}, {60,20}, {70,30}};
GtkWidget *win;
GtkWidget *vb;
GtkWidget *sw;
GtkWidget *pix;
GdkPixmap* bp;
GdkPixmap* xpm;
GdkBitmap* bitmap;
gint xpm_w, xpm_h;
gint depth;
gchar dashes[] = {1,2,3,4,5};
gint ndashes=5;

void page1(GdkDrawable* d, int print) {
	if (print)
		gdk_ps_drawable_page_start(d, 0, 1, 0, 72, a4w, a4h);
	else {
		d = gdk_pixmap_new(win->window, 300, 300, depth);
		gdk_gc_set_foreground(gc, &white);
		gdk_draw_rectangle(d, gc, 1, 0, 0, -1, -1);
	}
	gdk_gc_set_foreground(gc, &black);
	gdk_gc_set_line_attributes(gc, 1, -1, GDK_CAP_BUTT, -1);
	gdk_draw_polygon(d, gc, 0, poly, 5);
	gdk_draw_line(d, gc, 20, 20, 100, 20);
	gdk_draw_text(d, font, gc, 120, 20, "default", 7);
	gdk_gc_set_foreground(gc, &red);
	gdk_gc_set_line_attributes(gc, 2, -1, GDK_CAP_BUTT, -1);
	gdk_draw_line(d, gc, 20, 40, 100, 40);
	gdk_draw_text(d, font, gc, 120, 40, "butt", 4);
	gdk_gc_set_line_attributes(gc, 4, -1, GDK_CAP_ROUND, -1);
	gdk_draw_line(d, gc, 20, 60, 100, 60);
	gdk_draw_text(d, font, gc, 120, 60, "round", 5);
	gdk_gc_set_foreground(gc, &blue);
	gdk_gc_set_line_attributes(gc, 8, -1, GDK_CAP_PROJECTING, -1);
	gdk_draw_line(d, gc, 20, 80, 100, 80);
	gdk_draw_text(d, font, gc, 120, 80, "projecting", 10);
	gdk_gc_set_line_attributes(gc, 16, -1, GDK_CAP_NOT_LAST, -1);
	gdk_draw_line(d, gc, 20, 100, 100, 100);
	gdk_draw_text(d, font, gc, 120, 100, "(not last)", 10);
	/*gdk_draw_pixmap(d, gc, xpm, 0, 0, 160, 100, xpm_w, xpm_h);
	gdk_draw_pixmap(d, gc, bitmap, 0, 0, 160, 140, xpm_w, xpm_h);
	*/
	if (print)
		gdk_ps_drawable_page_end(d);
	else {
		pix = gtk_pixmap_new(d, NULL);
		gtk_widget_show(pix);
		gtk_container_add(GTK_CONTAINER(vb), pix);
	}

}

void page2(GdkDrawable* d, int print) {
	GdkRectangle clip = {30, 30, 320, 350};
	if (print)
		gdk_ps_drawable_page_start(d, 0, 1, 0, 72, a4w, a4h);
	else {
		d = gdk_pixmap_new(win->window, 350, 350, depth);
		gdk_gc_set_foreground(gc, &white);
		gdk_draw_rectangle(d, gc, 1, 0, 0, -1, -1);
	}
	gdk_gc_set_foreground(gc, &blue);
	gdk_draw_points(d, gc, poly, 5);
	gdk_gc_set_clip_rectangle(gc, &clip);
	gdk_draw_rectangle(d, gc, 0, 200, 200, 100, 300);
	gdk_draw_rectangle(d, gc, 1, 0, 0, 300, 100);
	gdk_draw_arc(d, gc, 0, 20, 20, 100, 100, 0, 60*64);
	gdk_gc_set_foreground(gc, &red);
	gdk_draw_arc(d, gc, 1, 200, 200, 100, 300, 0, 60*64);
	gdk_gc_set_clip_rectangle(gc, NULL);
	if (print)
		gdk_ps_drawable_page_end(d);
	else {
		pix = gtk_pixmap_new(d, NULL);
		gtk_widget_show(pix);
		gtk_container_add(GTK_CONTAINER(vb), pix);
	}
}

void page3(GdkDrawable* d, int print) {
	GdkRectangle clip1 = {30, 30, 200, 200};
	GdkRectangle clip2 = {150, 150, 200, 100};
	GdkRegion *region1;
	GdkRegion *region2;
	gint len;

	region1 = gdk_region_new();
	region2 = gdk_region_union_with_rect(region1, &clip1);
	gdk_region_destroy(region1);
	region1 = gdk_region_union_with_rect(region2, &clip2);
	gdk_region_destroy(region2);
	gdk_gc_set_clip_rectangle(gc, NULL);
	if (print)
		gdk_ps_drawable_page_start(d, 0, 1, 0, 72, a4w, a4h);
	else {
		d = gdk_pixmap_new(win->window, 300, 300, depth);
		gdk_gc_set_foreground(gc, &white);
		gdk_draw_rectangle(d, gc, 1, 0, 0, -1, -1);
	}
	gdk_gc_set_clip_region(gc, region1);
	gdk_gc_set_line_attributes(gc, 2, -1, GDK_CAP_BUTT, -1);
	gdk_gc_set_foreground(gc, &green);
	gdk_draw_rectangle(d, gc, 1, 10, 10, 580, 1180);
	gdk_gc_set_foreground(gc, &red);
	gdk_draw_string(d, font, gc, 50, fsize, "First line");
	gdk_draw_string(d, font, gc, 50, fsize*2, "Second line");
	gdk_draw_string(d, font, gc, 50, fsize*3, "Third line");
	gdk_draw_string(d, font, gc, 50, fsize*4, "Fourth line");
	gdk_draw_string(d, font, gc, 50, fsize*5, "Fifth line");
	len = gdk_string_width(font, "Fifth line");
	gdk_draw_string(d, font, gc, 50+len, fsize*5, "This continues right after line");
	gdk_gc_set_foreground(gc, &black);
	gdk_draw_rectangle(d, gc, 0, 50, fsize*5-font->ascent, len, font->ascent+font->descent);
	gdk_gc_set_line_attributes(gc, 1, GDK_LINE_DOUBLE_DASH, GDK_CAP_BUTT, -1);
	gdk_draw_rectangle(d, gc, 0, 50, 10+fsize*6, len, font->ascent+font->descent);
	gdk_gc_set_dashes(gc, 0, dashes, ndashes);
	gdk_draw_rectangle(d, gc, 0, 50+10+len, 10+fsize*6, len, font->ascent+font->descent);
	if (print)
		gdk_ps_drawable_page_end(d);
	else {
		pix = gtk_pixmap_new(d, NULL);
		gtk_widget_show(pix);
		gtk_container_add(GTK_CONTAINER(vb), pix);
	}
	gdk_region_destroy(region1);
}

int main(int argc, char* argv[]) {
	GdkDrawable *d;

	gtk_init(&argc, &argv);
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(win);
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(win), sw);
	vb = gtk_vbox_new(5, 5);
	gtk_container_add(GTK_CONTAINER(sw), vb);
	gc = gdk_gc_new(win->window);
	font = gdk_font_load(argc>1?argv[1]:"-adobe-helvetica-medium-r-*");
	gdk_window_get_geometry(win->window, NULL, NULL, NULL, NULL, &depth);
	gtk_signal_connect(GTK_OBJECT(win), "delete_event", 
		(GtkSignalFunc)gtk_main_quit, NULL);
	gtk_widget_set_usize(win, 350, 450);
	if (!font)
		g_error("Cannot load font: %s\n", argv[1]);
	cmap = gdk_colormap_get_system();
	gdk_color_parse("red", &red);
	gdk_color_parse("blue", &blue);
	gdk_color_parse("steelblue", &green);
	gdk_color_parse("white", &white);
	gdk_color_parse("black", &black);
	gdk_color_alloc(cmap, &red);
	gdk_color_alloc(cmap, &blue);
	gdk_color_alloc(cmap, &green);
	gdk_color_alloc(cmap, &white);
	gdk_color_alloc(cmap, &black);

	fsize = font->ascent+font->descent;

	xpm = gdk_pixmap_create_from_xpm(win->window, &bitmap, &white, "test.xpm");
	gdk_window_get_size(xpm, &xpm_w, &xpm_h);
	
	d = gdk_ps_drawable_new(1, "Test for GdkPs", "lupus");
	page1(d, 1);
	page1(NULL, 0);
	page2(d, 1);
	page2(NULL, 0);
	page3(d, 1);
	page3(NULL, 0);
	gdk_ps_drawable_end(d);
	gtk_widget_show_all(win);
	gtk_main();
	gdk_window_destroy(d);
	return 0;
}

