#include "theme2.h"

#define CHILD_SPACING     1
#define DEFAULT_LEFT_POS  4
#define DEFAULT_TOP_POS   4
#define DEFAULT_SPACING   7

struct _butinfo
{
   int state;
   int foc;
   int def;
   int w,h;
};

/* Theme functions to export */
void button_border             (GtkWidget        *widget);
void button_init               (GtkWidget        *widget);
void button_draw               (GtkWidget        *widget,
				GdkRectangle     *area);
void button_exit               (GtkWidget        *widget);

/* internals */

void 
button_border (GtkWidget        *widget)
{
   ThemeConfig *cf;
   int state,def,foc;
   
   cf=(ThemeConfig *)th_dat.data;
   def=0;
   if (GTK_WIDGET_CAN_DEFAULT(widget))                       def=1;
   if (GTK_WIDGET_HAS_DEFAULT(widget))                       def=2;
   foc=0;
   if (GTK_WIDGET_HAS_FOCUS(widget))                         foc=1;
   state=0;
   if      (GTK_WIDGET_STATE(widget)==GTK_STATE_ACTIVE)      state=1;
   else if (GTK_WIDGET_STATE(widget)==GTK_STATE_PRELIGHT)    state=2;
   else if (GTK_WIDGET_STATE(widget)==GTK_STATE_SELECTED)    state=3;
   else if (GTK_WIDGET_STATE(widget)==GTK_STATE_INSENSITIVE) state=4;
   GTK_CONTAINER(widget)->internal_border_left=
     cf->buttonconfig[def][state][foc].button_padding.left;
   GTK_CONTAINER(widget)->internal_border_right=
     cf->buttonconfig[def][state][foc].button_padding.right;
   GTK_CONTAINER(widget)->internal_border_top=
     cf->buttonconfig[def][state][foc].button_padding.top;
   GTK_CONTAINER(widget)->internal_border_bottom=
     cf->buttonconfig[def][state][foc].button_padding.bottom;
}

void 
button_init               (GtkWidget        *widget)
{
   struct _butinfo *bi;

   bi=malloc(sizeof(struct _butinfo));
   GTK_CONTAINER(widget)->border_width=0;
   gtk_object_set_data(GTK_OBJECT(widget),"gtk-widget-theme-data",bi);
   bi->w=-1;bi->h=-1;bi->state=-1;bi->foc=-1;bi->def=-1;
}

void 
button_draw               (GtkWidget        *widget,
			   GdkRectangle     *area)
{
   struct _butinfo *bi;
   int state,def,foc;
   ThemeConfig *cf;
   int i,x,y,w,h;
   
   cf=(ThemeConfig *)th_dat.data;
   bi=gtk_object_get_data(GTK_OBJECT(widget),"gtk-widget-theme-data");
   def=0;
   if (GTK_WIDGET_CAN_DEFAULT(widget))                       def=1;
   if (GTK_WIDGET_HAS_DEFAULT(widget))                       def=2;
   foc=0;
   if (GTK_WIDGET_HAS_FOCUS(widget))                         foc=1;
   state=0;
   if      (GTK_WIDGET_STATE(widget)==GTK_STATE_ACTIVE)      state=1;
   else if (GTK_WIDGET_STATE(widget)==GTK_STATE_PRELIGHT)    state=2;
   else if (GTK_WIDGET_STATE(widget)==GTK_STATE_SELECTED)    state=3;
   else if (GTK_WIDGET_STATE(widget)==GTK_STATE_INSENSITIVE) state=4;
   if ((bi->state!=state)||(bi->foc!=foc)||(bi->def!=def)||
       (bi->w!=widget->allocation.width)||(bi->h!=widget->allocation.height))
     {
	if (cf->buttonconfig[def][state][foc].background.image)
	  {
	     if (cf->buttonconfig[def][state][foc].background.scale_to_fit)
	       gdk_imlib_apply_image(cf->buttonconfig[def][state][foc].background.image,
				     widget->window);
	     else
	       {
		  GdkPixmap *pmap;
		  
		  gdk_imlib_render(cf->buttonconfig[def][state][foc].background.image,
				   cf->buttonconfig[def][state][foc].background.image->rgb_width,
				   cf->buttonconfig[def][state][foc].background.image->rgb_height);
		  pmap=gdk_imlib_move_image(cf->buttonconfig[def][state][foc].background.image);
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
	       
	     cl.pixel=cf->buttonconfig[def][state][foc].background.color.pixel;
	     gdk_window_set_background(widget->window,&cl);
	     gdk_window_clear(widget->window);
	  }
	bi->w=widget->allocation.width;bi->h=widget->allocation.height;
	bi->state=state;bi->foc=foc;bi->def=def;
     }
   if (cf->buttonconfig[def][state][foc].border.image)
     {
	gdk_imlib_paste_image_border(cf->buttonconfig[def][state][foc].border.image,
				     widget->window,
				     0,0,widget->allocation.width,
				     widget->allocation.height);
     }
   for(i=0;i<cf->buttonconfig[def][state][foc].number_of_decorations;i++)
     {
	if (cf->buttonconfig[def][state][foc].decoration[i].image)
	  {
	     x=cf->buttonconfig[def][state][foc].decoration[i].xabs+
	       ((cf->buttonconfig[def][state][foc].decoration[i].xrel*
		 widget->allocation.width)>>10);
	     y=cf->buttonconfig[def][state][foc].decoration[i].yabs+
	       ((cf->buttonconfig[def][state][foc].decoration[i].yrel*
		 widget->allocation.height)>>10);
	     w=cf->buttonconfig[def][state][foc].decoration[i].x2abs+
	       ((cf->buttonconfig[def][state][foc].decoration[i].x2rel*
		 widget->allocation.width)>>10)-x;
	     h=cf->buttonconfig[def][state][foc].decoration[i].y2abs+
	       ((cf->buttonconfig[def][state][foc].decoration[i].y2rel*
		 widget->allocation.height)>>10)-y;
	     gdk_imlib_paste_image(cf->buttonconfig[def][state][foc].decoration[i].image,
				   widget->window,x,y,w,h);
	  }
     }
}

void 
button_exit               (GtkWidget        *widget)
{
  struct _butinfo *bi;

   bi=gtk_object_get_data(GTK_OBJECT(widget),"gtk-widget-theme-data");
   free(bi);
   gtk_object_remove_data(GTK_OBJECT(widget),"gtk-widget-theme-data");
}

