#include "theme2.h"

#define CHILD_SPACING     1
#define DEFAULT_LEFT_POS  4
#define DEFAULT_TOP_POS   4
#define DEFAULT_SPACING   7

#define CHECK_BUTTON_CLASS(w)  GTK_CHECK_BUTTON_CLASS (GTK_OBJECT (w)->klass)

struct _butinfo
{
   int state;
   int foc;
   int def;
   int w,h;
};

/* Theme functions to export */
void check_button_border             (GtkWidget        *widget);
void check_button_init               (GtkWidget        *widget);
void check_button_draw               (GtkWidget        *widget,
				GdkRectangle     *area);
void check_button_exit               (GtkWidget        *widget);

/* internals */

void 
check_button_border (GtkWidget        *widget)
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
   GTK_CONTAINER(widget)->minimum_width=
     cf->buttonconfig[def][state][foc].min_w;
   GTK_CONTAINER(widget)->minimum_height=
     cf->buttonconfig[def][state][foc].min_h;
}

void 
check_button_init               (GtkWidget        *widget)
{
   struct _butinfo *bi;

   bi=malloc(sizeof(struct _butinfo));
   GTK_CONTAINER(widget)->border_width=0;
   gtk_object_set_data(GTK_OBJECT(widget),"gtk-widget-theme-data",bi);
   bi->w=-1;bi->h=-1;bi->state=-1;bi->foc=-1;bi->def=-1;
}

void 
check_button_draw               (GtkWidget        *widget,
			   GdkRectangle     *area)
{
   struct _butinfo *bi;
   int state,def,foc;
   ThemeConfig *cf;
   int i,x,y,w,h;
   GdkPixmap *pmap,*p;
   GdkPixmap *mask,*m;
   GdkGC *mgc,*gc;
   GdkColor cl;
   char refresh=0;

   if (!widget->window) return;
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
   pmap=NULL;
   mask=NULL;
   if ((bi->state!=state)||(bi->foc!=foc)||(bi->def!=def)||
       (bi->w!=widget->allocation.width)||(bi->h!=widget->allocation.height))
     refresh=1;
   refresh=1;
   if (refresh)
     {
	gdk_window_clear(widget->window);
	gdk_flush();
	if (cf->buttonconfig[def][state][foc].background.image)
	  {
	     x = CHECK_BUTTON_CLASS (widget)->indicator_spacing 
	       + GTK_CONTAINER (widget)->border_width;
	     y = (widget->allocation.height - 
		  CHECK_BUTTON_CLASS (widget)->indicator_size) / 2;
	     w = CHECK_BUTTON_CLASS (widget)->indicator_size;
	     h = CHECK_BUTTON_CLASS (widget)->indicator_size;
	     
	     if (cf->buttonconfig[def][state][foc].background.scale_to_fit)
	       {
		  gdk_imlib_paste_image(cf->buttonconfig[def][state][foc].background.image,
					widget->window,x,y,w,h);
	       }
	  }
	bi->w=widget->allocation.width;bi->h=widget->allocation.height;
	bi->state=state;bi->foc=foc;bi->def=def;
     }
/*   if (cf->buttonconfig[def][state][foc].border.image)
     gdk_imlib_paste_image_border(cf->buttonconfig[def][state][foc].border.image,
				  widget->window,
				  0,0,widget->allocation.width,
				  widget->allocation.height);
 */
}

void 
check_button_exit               (GtkWidget        *widget)
{
  struct _butinfo *bi;

   bi=gtk_object_get_data(GTK_OBJECT(widget),"gtk-widget-theme-data");
   free(bi);
   gtk_object_remove_data(GTK_OBJECT(widget),"gtk-widget-theme-data");
}

