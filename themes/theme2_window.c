#include "theme2.h"

#define CHILD_SPACING     1
#define DEFAULT_LEFT_POS  4
#define DEFAULT_TOP_POS   4
#define DEFAULT_SPACING   7

/* Theme functions to export */
void window_border             (GtkWidget        *widget);
void window_init               (GtkWidget        *widget);
void window_draw               (GtkWidget        *widget,
				GdkRectangle     *area);
void window_exit               (GtkWidget        *widget);

/* internals */
struct _wininfo
{
   int w,h;
};

void 
window_border (GtkWidget        *widget)
{
   ThemeConfig *cf;
   
   cf=(ThemeConfig *)th_dat.data;
   GTK_CONTAINER(widget)->internal_border_left=
     cf->windowconfig.window_padding.left;
   GTK_CONTAINER(widget)->internal_border_right=
     cf->windowconfig.window_padding.right;
   GTK_CONTAINER(widget)->internal_border_top=
     cf->windowconfig.window_padding.top;
   GTK_CONTAINER(widget)->internal_border_bottom=
     cf->windowconfig.window_padding.bottom;
   GTK_CONTAINER(widget)->minimum_width=
     cf->windowconfig.min_w;
   GTK_CONTAINER(widget)->minimum_height=
     cf->windowconfig.min_h;
}

void 
window_init               (GtkWidget        *widget)
{
   struct _wininfo *wi;

   wi=malloc(sizeof(struct _wininfo));
   GTK_CONTAINER(widget)->border_width=0;
   gtk_object_set_data(GTK_OBJECT(widget),"gtk-widget-theme-data",wi);
   wi->w=-1;wi->h=-1;
}

void 
window_draw               (GtkWidget        *widget,
			   GdkRectangle     *area)
{
   struct _wininfo *wi;
   ThemeConfig *cf;
   int i,x,y,w,h;
   GdkPixmap *pmap,*p;
   GdkPixmap *mask,*m;
   GdkGC *mgc,*gc;
   GdkColor cl;
   char refresh=0;
   
   if (!widget->window) return;
   wi=gtk_object_get_data(GTK_OBJECT(widget),"gtk-widget-theme-data");
   cf=(ThemeConfig *)th_dat.data;
   pmap=NULL;
   mask=NULL;
   if ((wi->w!=widget->allocation.width)||(wi->h!=widget->allocation.height))
     refresh=1;
   if (refresh)
     {
	if (cf->windowconfig.background.image)
	  {
	     if (cf->windowconfig.background.scale_to_fit)
	       {
		  
		  gdk_imlib_render(cf->windowconfig.background.image,
				   widget->allocation.width,
				   widget->allocation.height);
		  pmap=gdk_imlib_move_image(cf->windowconfig.background.image);
		  mask=gdk_imlib_copy_mask(cf->windowconfig.background.image);
	       }
	     else
	       {
		  GdkPixmap *pmap;
		  
		  gdk_imlib_render(cf->windowconfig.background.image,
				   cf->windowconfig.background.image->rgb_width,
				   cf->windowconfig.background.image->rgb_height);
		  pmap=gdk_imlib_move_image(cf->windowconfig.background.image);
		  if (pmap)
		    {
		       gdk_window_set_back_pixmap(widget->window,pmap,0);
		       gdk_window_clear(widget->window);
		       gdk_imlib_free_pixmap(pmap);
		    }
	       }
	  }
	else
	  {
	     GdkColor cl;
	       
	     cl.pixel=cf->windowconfig.background.color.pixel;
	     gdk_window_set_background(widget->window,&cl);
	     gdk_window_clear(widget->window);
	  }
	wi->w=widget->allocation.width;wi->h=widget->allocation.height;
     }
   if (pmap)
     {
	gdk_window_set_back_pixmap(widget->window,pmap,0);
	gdk_window_clear(widget->window);
	gdk_imlib_free_pixmap(pmap);
     }
   if (cf->windowconfig.border.image)
     gdk_imlib_paste_image_border(cf->windowconfig.border.image,
				  widget->window,
				  0,0,widget->allocation.width,
				  widget->allocation.height);
   if (cf->windowconfig.number_of_decorations>0)
     {
	mgc=NULL;
	gc = gdk_gc_new(widget->window);
	if (mask)
	  {
	     mgc = gdk_gc_new(mask);
	     gdk_gc_set_function(mgc, GDK_OR);
	     cl.pixel=1;
	     gdk_gc_set_foreground(mgc, &cl);
	  }
	for(i=0;i<cf->windowconfig.number_of_decorations;i++)
	  {
	     if (cf->windowconfig.decoration[i].image)
	       {
		  x=cf->windowconfig.decoration[i].xabs+
		    ((cf->windowconfig.decoration[i].xrel*
		      widget->allocation.width)>>10);
		  y=cf->windowconfig.decoration[i].yabs+
		    ((cf->windowconfig.decoration[i].yrel*
		      widget->allocation.height)>>10);
		  w=cf->windowconfig.decoration[i].x2abs+
		    ((cf->windowconfig.decoration[i].x2rel*
		      widget->allocation.width)>>10)-x+1;
		  h=cf->windowconfig.decoration[i].y2abs+
		    ((cf->windowconfig.decoration[i].y2rel*
		      widget->allocation.height)>>10)-y+1;
		  p=NULL;m=NULL;
		  gdk_imlib_render(cf->windowconfig.decoration[i].image,w,h);
		  p=gdk_imlib_move_image(cf->windowconfig.decoration[i].image);
		  m=gdk_imlib_move_mask(cf->windowconfig.decoration[i].image);
		  if (p)
		    {
		       if (m)
			 {
			    gdk_gc_set_clip_mask(gc, m);
			    gdk_gc_set_clip_origin(gc, x, y);
			 }
		       else
			 gdk_gc_set_clip_mask(gc, NULL);
		       gdk_draw_pixmap(widget->window, gc, p, 0, 0, x, y, w, h);
		       if (mask)
			 {
			    gdk_gc_set_clip_mask(mgc, m);
			    gdk_gc_set_clip_origin(mgc, x, y);
			    if (m)
			      gdk_draw_pixmap(mask, mgc, m, 0, 0, x, y, w, h);
			    else
			      gdk_draw_rectangle(mask, mgc, TRUE, x, y, w, h);
			 }
		       gdk_imlib_free_pixmap(p);
		    }
	       }
	  }
	if (mask)
	  gdk_gc_destroy(mgc);
	gdk_gc_destroy(gc);
     }
   if (mask)
     {
	gdk_window_shape_combine_mask(widget->window,mask,0,0);
	gdk_imlib_free_pixmap(mask);
     }
}

void 
window_exit               (GtkWidget        *widget)
{ 
   struct _wininfo *wi;

   wi=gtk_object_get_data(GTK_OBJECT(widget),"gtk-widget-theme-data");
   free(wi);
   gtk_object_remove_data(GTK_OBJECT(widget),"gtk-widget-theme-data");
}

