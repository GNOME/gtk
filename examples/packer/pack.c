/* 
 *  This is a demo of gtkpacker. 
 */

#include <gtk/gtk.h>

void add_widget (GtkWidget *widget, gpointer data);
void toggle_side (GtkWidget *widget, gpointer data);
void toggle_anchor (GtkWidget *widget, gpointer data);
void toggle_options (GtkWidget *widget, gpointer data);

typedef struct {
    GList     *widgets;
    GtkWidget *packer;
    GtkWidget *current;
    GtkPackerChild *pchild;

    GtkWidget *button_top;
    GtkWidget *button_bottom;
    GtkWidget *button_left;
    GtkWidget *button_right;

    GtkWidget *button_n;
    GtkWidget *button_ne;
    GtkWidget *button_nw;
    GtkWidget *button_e;
    GtkWidget *button_w;
    GtkWidget *button_s;
    GtkWidget *button_se;
    GtkWidget *button_sw;
    GtkWidget *button_center;

    GtkWidget *button_fillx;
    GtkWidget *button_filly;
    GtkWidget *button_expand;
} Info;

void destroy (GtkWidget *widget, gpointer data)
{
    gtk_main_quit ();
}

int
main (int argv, char **argc)
{
    GtkWidget *window;
    GtkWidget *window_pack;
    GtkWidget *top_pack;
    GtkWidget *frame;
    GtkWidget *packer;
    GtkWidget *button_pack;
    GtkWidget *button_add;
    GtkWidget *button_quit;
    GtkWidget *bottom_pack;
    GtkWidget *side_frame;
    GtkWidget *side_pack;
    GtkWidget *button_top;
    GtkWidget *button_bottom;
    GtkWidget *button_left;
    GtkWidget *button_right;
    GtkWidget *anchor_table;
    GtkWidget *anchor_frame;
    GtkWidget *button_n;
    GtkWidget *button_ne;
    GtkWidget *button_nw;
    GtkWidget *button_e;
    GtkWidget *button_w;
    GtkWidget *button_center;
    GtkWidget *button_s;
    GtkWidget *button_se;
    GtkWidget *button_sw;
    GtkWidget *button_expand;
    GtkWidget *button_fillx;
    GtkWidget *button_filly;
    GtkWidget *options_pack;
    GtkWidget *options_frame;
    GtkWidget *anchor_pack;
    Info *info;

    gtk_init(&argv, &argc);

    info = g_malloc(sizeof(Info));

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_signal_connect (GTK_OBJECT (window), "destroy",
                        GTK_SIGNAL_FUNC (destroy), NULL);

    window_pack = gtk_packer_new();
    gtk_container_add(GTK_CONTAINER(window), window_pack);
    gtk_container_border_width(GTK_CONTAINER(window), 4);

    top_pack = gtk_packer_new();
    gtk_packer_add_defaults(GTK_PACKER(window_pack),
                            top_pack,
                            GTK_SIDE_TOP,
                            GTK_ANCHOR_CENTER,
                            GTK_FILL_X | GTK_FILL_Y | GTK_EXPAND);

    frame = gtk_frame_new("Packing Area");
    gtk_widget_set_usize(frame, 400, 400);
    gtk_packer_add(GTK_PACKER(top_pack), 
                             frame,
                             GTK_SIDE_LEFT,
                             GTK_ANCHOR_CENTER,
                             GTK_FILL_X | GTK_FILL_Y | GTK_EXPAND,     
                             0, 8, 8, 0, 0);
    packer = gtk_packer_new();
    gtk_container_add(GTK_CONTAINER(frame), packer);

    button_pack = gtk_packer_new();
    gtk_packer_add(GTK_PACKER(top_pack),
                   button_pack, 
                   GTK_SIDE_LEFT,
                   GTK_ANCHOR_N,
                   0, 
                   0, 0, 0, 0, 0);

    button_add = gtk_button_new_with_label("Add Button");
    gtk_packer_add(GTK_PACKER(top_pack),
                   button_add, 
                   GTK_SIDE_TOP,
                   GTK_ANCHOR_CENTER,
                   GTK_FILL_X, 
                   0, 8, 8, 8, 0);
    gtk_signal_connect (GTK_OBJECT (button_add), "clicked",
                        GTK_SIGNAL_FUNC (add_widget), (gpointer) info);

    button_quit = gtk_button_new_with_label("Quit");
    gtk_packer_add(GTK_PACKER(top_pack),
                   button_quit, 
                   GTK_SIDE_TOP,
                   GTK_ANCHOR_CENTER,
                   GTK_FILL_X, 
                   0, 8, 8, 0, 0);
    gtk_signal_connect_object (GTK_OBJECT (button_quit), "clicked",
                               GTK_SIGNAL_FUNC (gtk_widget_destroy),
                               GTK_OBJECT (window));

    bottom_pack = gtk_packer_new();
    gtk_packer_add_defaults(GTK_PACKER(window_pack),
                            bottom_pack,
                            GTK_SIDE_TOP,
                            GTK_ANCHOR_CENTER,
                            GTK_FILL_X);
  
    side_frame = gtk_frame_new("Side");
    gtk_packer_add(GTK_PACKER(window_pack),
                   side_frame,
                   GTK_SIDE_LEFT,
                   GTK_ANCHOR_W,
                   GTK_FILL_Y,
                   0, 10, 10, 0, 0);

    side_pack = gtk_packer_new();
    gtk_container_add(GTK_CONTAINER(side_frame), side_pack);
    
    button_top = gtk_toggle_button_new_with_label("Top");
    button_bottom = gtk_toggle_button_new_with_label("Bottom");
    button_left = gtk_toggle_button_new_with_label("Left");
    button_right = gtk_toggle_button_new_with_label("Right");

    gtk_object_set_data(GTK_OBJECT(button_top), "side", GINT_TO_POINTER (GTK_SIDE_TOP));
    gtk_object_set_data(GTK_OBJECT(button_bottom), "side", GINT_TO_POINTER (GTK_SIDE_BOTTOM));
    gtk_object_set_data(GTK_OBJECT(button_left), "side", GINT_TO_POINTER (GTK_SIDE_LEFT));
    gtk_object_set_data(GTK_OBJECT(button_right), "side", GINT_TO_POINTER (GTK_SIDE_RIGHT));

    gtk_widget_set_usize(button_top, 50, -1);
    gtk_widget_set_usize(button_bottom, 50, -1);
    gtk_widget_set_usize(button_left, 50, -1);
    gtk_widget_set_usize(button_right, 50, -1);
    gtk_packer_add(GTK_PACKER(side_pack),
                   button_top,
                   GTK_SIDE_TOP,
                   GTK_ANCHOR_CENTER,
                   0,
                   0, 5, 5, 0, 0);
    gtk_packer_add(GTK_PACKER(side_pack),
                   button_bottom,
                   GTK_SIDE_BOTTOM,
                   GTK_ANCHOR_CENTER,
                   0,
                   0, 5, 5, 0, 0);
    gtk_packer_add(GTK_PACKER(side_pack),
                   button_left,
                   GTK_SIDE_LEFT,
                   GTK_ANCHOR_CENTER,
                   0,
                   0, 10, 5, 0, 0);
    gtk_packer_add(GTK_PACKER(side_pack),
                   button_right,
                   GTK_SIDE_RIGHT,
                   GTK_ANCHOR_CENTER,
                   0,
                   0, 10, 5, 0, 0);
    gtk_signal_connect (GTK_OBJECT (button_top), "toggled",
                        GTK_SIGNAL_FUNC (toggle_side), (gpointer) info);
    gtk_signal_connect (GTK_OBJECT (button_bottom), "toggled",
                        GTK_SIGNAL_FUNC (toggle_side), (gpointer) info);
    gtk_signal_connect (GTK_OBJECT (button_left), "toggled",
                        GTK_SIGNAL_FUNC (toggle_side), (gpointer) info);
    gtk_signal_connect (GTK_OBJECT (button_right), "toggled",
                        GTK_SIGNAL_FUNC (toggle_side), (gpointer) info);

    anchor_frame = gtk_frame_new("Anchor");
    gtk_packer_add(GTK_PACKER(window_pack),
                   anchor_frame,
                   GTK_SIDE_LEFT,
                   GTK_ANCHOR_W,
                   GTK_FILL_Y,
                   0, 10, 10, 0, 0);

    anchor_pack = gtk_packer_new();
    gtk_container_add(GTK_CONTAINER(anchor_frame), anchor_pack);

    anchor_table = gtk_table_new(3,3,TRUE);
    gtk_packer_add(GTK_PACKER(anchor_pack),
                   anchor_table,
                   GTK_SIDE_TOP,
                   GTK_ANCHOR_CENTER,
                   GTK_FILL_Y | GTK_FILL_X | GTK_PACK_EXPAND,
                   0, 10, 5, 0, 0);

    button_n = gtk_toggle_button_new_with_label("N");
    button_s = gtk_toggle_button_new_with_label("S");
    button_w = gtk_toggle_button_new_with_label("W");
    button_e = gtk_toggle_button_new_with_label("E");
    button_ne = gtk_toggle_button_new_with_label("NE");
    button_nw = gtk_toggle_button_new_with_label("NW");
    button_se = gtk_toggle_button_new_with_label("SE");
    button_sw = gtk_toggle_button_new_with_label("SW");
    button_center = gtk_toggle_button_new_with_label("");

    gtk_object_set_data(GTK_OBJECT(button_n), "anchor", GINT_TO_POINTER (GTK_ANCHOR_N));
    gtk_object_set_data(GTK_OBJECT(button_nw), "anchor", GINT_TO_POINTER (GTK_ANCHOR_NW));
    gtk_object_set_data(GTK_OBJECT(button_ne), "anchor", GINT_TO_POINTER (GTK_ANCHOR_NE));
    gtk_object_set_data(GTK_OBJECT(button_s), "anchor", GINT_TO_POINTER (GTK_ANCHOR_S));
    gtk_object_set_data(GTK_OBJECT(button_sw), "anchor", GINT_TO_POINTER (GTK_ANCHOR_SW));
    gtk_object_set_data(GTK_OBJECT(button_se), "anchor", GINT_TO_POINTER (GTK_ANCHOR_SE));
    gtk_object_set_data(GTK_OBJECT(button_w), "anchor", GINT_TO_POINTER (GTK_ANCHOR_W));
    gtk_object_set_data(GTK_OBJECT(button_e), "anchor", GINT_TO_POINTER (GTK_ANCHOR_E));
    gtk_object_set_data(GTK_OBJECT(button_center), "anchor", GINT_TO_POINTER (GTK_ANCHOR_CENTER));

    gtk_signal_connect (GTK_OBJECT (button_n), "toggled",
                        GTK_SIGNAL_FUNC (toggle_anchor), (gpointer) info);
    gtk_signal_connect (GTK_OBJECT (button_nw), "toggled",
                        GTK_SIGNAL_FUNC (toggle_anchor), (gpointer) info);
    gtk_signal_connect (GTK_OBJECT (button_ne), "toggled",
                        GTK_SIGNAL_FUNC (toggle_anchor), (gpointer) info);
    gtk_signal_connect (GTK_OBJECT (button_s), "toggled",
                        GTK_SIGNAL_FUNC (toggle_anchor), (gpointer) info);
    gtk_signal_connect (GTK_OBJECT (button_sw), "toggled",
                        GTK_SIGNAL_FUNC (toggle_anchor), (gpointer) info);
    gtk_signal_connect (GTK_OBJECT (button_se), "toggled",
                        GTK_SIGNAL_FUNC (toggle_anchor), (gpointer) info);
    gtk_signal_connect (GTK_OBJECT (button_w), "toggled",
                        GTK_SIGNAL_FUNC (toggle_anchor), (gpointer) info);
    gtk_signal_connect (GTK_OBJECT (button_e), "toggled",
                        GTK_SIGNAL_FUNC (toggle_anchor), (gpointer) info);
    gtk_signal_connect (GTK_OBJECT (button_center), "toggled",
                        GTK_SIGNAL_FUNC (toggle_anchor), (gpointer) info);

    gtk_table_attach_defaults(GTK_TABLE(anchor_table),
                              button_nw,
                              0, 1, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(anchor_table),
                              button_n,
                              1, 2, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(anchor_table),
                              button_ne,
                              2, 3, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(anchor_table),
                              button_w,
                              0, 1, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(anchor_table),
                              button_center,
                              1, 2, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(anchor_table),
                              button_e,
                              2, 3, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(anchor_table),
                              button_sw,
                              0, 1, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(anchor_table),
                              button_s,
                              1, 2, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(anchor_table),
                              button_se,
                              2, 3, 2, 3);

    options_frame = gtk_frame_new("Options");
    gtk_packer_add(GTK_PACKER(window_pack),
                   options_frame,
                   GTK_SIDE_LEFT,
                   GTK_ANCHOR_W,
                   GTK_FILL_Y,
                   0, 10, 10, 0, 0);

    options_pack = gtk_packer_new();
    gtk_container_add(GTK_CONTAINER(options_frame), options_pack);

    button_fillx = gtk_toggle_button_new_with_label("Fill X");
    button_filly = gtk_toggle_button_new_with_label("Fill Y");
    button_expand = gtk_toggle_button_new_with_label("Expand");

    gtk_packer_add(GTK_PACKER(options_pack),
                   button_fillx,
                   GTK_SIDE_TOP,
                   GTK_ANCHOR_N,
                   GTK_FILL_X | GTK_PACK_EXPAND,
                   0, 10, 5, 0, 0);
    gtk_packer_add(GTK_PACKER(options_pack),
                   button_filly,
                   GTK_SIDE_TOP,
                   GTK_ANCHOR_CENTER,
                   GTK_FILL_X | GTK_PACK_EXPAND,
                   0, 10, 5, 0, 0);
    gtk_packer_add(GTK_PACKER(options_pack),
                   button_expand,
                   GTK_SIDE_TOP,
                   GTK_ANCHOR_S,
                   GTK_FILL_X | GTK_PACK_EXPAND,
                   0, 10, 5, 0, 0);

    gtk_object_set_data(GTK_OBJECT(button_fillx), "option", GINT_TO_POINTER (GTK_FILL_X));
    gtk_object_set_data(GTK_OBJECT(button_filly), "option", GINT_TO_POINTER (GTK_FILL_Y));
    gtk_object_set_data(GTK_OBJECT(button_expand), "option", GINT_TO_POINTER (GTK_PACK_EXPAND));

    gtk_signal_connect (GTK_OBJECT (button_fillx), "toggled",
                        GTK_SIGNAL_FUNC (toggle_options), (gpointer) info);
    gtk_signal_connect (GTK_OBJECT (button_filly), "toggled",
                        GTK_SIGNAL_FUNC (toggle_options), (gpointer) info);
    gtk_signal_connect (GTK_OBJECT (button_expand), "toggled",
                        GTK_SIGNAL_FUNC (toggle_options), (gpointer) info);



    info->widgets = NULL;
    info->packer = packer;
    info->button_top = button_top;
    info->button_bottom = button_bottom;
    info->button_left = button_left;
    info->button_right = button_right;
    info->button_n = button_n;
    info->button_nw = button_nw;
    info->button_ne = button_ne;
    info->button_e = button_e;
    info->button_w = button_w;
    info->button_center = button_center;
    info->button_s = button_s;
    info->button_sw = button_sw;
    info->button_se = button_se;
    info->button_fillx = button_fillx;
    info->button_filly = button_filly;
    info->button_expand = button_expand;

    add_widget(NULL, (gpointer) info);

    gtk_widget_show_all(window);

    gtk_main();

    return 0;
}

void 
toggle_options (GtkWidget *widget, gpointer data)
{
   Info *info;
   gint option;
   GList *list;
   GtkPackerChild *pchild;
   gint fillx, filly, expand;

   info = (Info*) data;
 
   option = GPOINTER_TO_INT (gtk_object_get_data(GTK_OBJECT(widget), "option"));

   pchild = info->pchild;
   if (pchild == NULL) {
       abort();
   };

   fillx = filly = expand = 0;

   if (GTK_TOGGLE_BUTTON(info->button_fillx)->active)
       fillx = GTK_FILL_X;
   if (GTK_TOGGLE_BUTTON(info->button_filly)->active)
       filly = GTK_FILL_Y;
   if (GTK_TOGGLE_BUTTON(info->button_expand)->active)
       expand = GTK_PACK_EXPAND;

   gtk_packer_configure(GTK_PACKER(info->packer),
                        info->current,
                        pchild->side,
                        pchild->anchor,
                        fillx | filly | expand, 
                        pchild->border_width, 
                        pchild->pad_x, 
                        pchild->pad_y, 
                        pchild->i_pad_x, 
                        pchild->i_pad_y);
}

void 
toggle_anchor (GtkWidget *widget, gpointer data)
{
   Info *info;
   gint anchor;
   GList *list;
   GtkPackerChild *pchild;

   info = (Info*) data;
 
   if (GTK_TOGGLE_BUTTON(widget)->active) {
       anchor = GPOINTER_TO_INT (gtk_object_get_data(GTK_OBJECT(widget), "anchor"));

       pchild = info->pchild;
       if (pchild == NULL) {
           abort();
       };

       gtk_packer_configure(GTK_PACKER(info->packer),
                           info->current,
                           pchild->side,
                           anchor,
                           pchild->options, 
                           pchild->border_width, 
                           pchild->pad_x, 
                           pchild->pad_y, 
                           pchild->i_pad_x, 
                           pchild->i_pad_y);

       if (info->button_n != widget) {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_n),0);
           gtk_widget_set_sensitive(info->button_n, 1);
       }
       if (info->button_nw != widget) {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_nw),0);
           gtk_widget_set_sensitive(info->button_nw, 1);
       }
       if (info->button_ne != widget) {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_ne),0);
           gtk_widget_set_sensitive(info->button_ne, 1);
       }
       if (info->button_s != widget) {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_s),0);
           gtk_widget_set_sensitive(info->button_s, 1);
       }
       if (info->button_sw != widget) {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_sw),0);
           gtk_widget_set_sensitive(info->button_sw, 1);
       }
       if (info->button_se != widget) {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_se),0);
           gtk_widget_set_sensitive(info->button_se, 1);
       }
       if (info->button_e != widget) {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_e),0);
           gtk_widget_set_sensitive(info->button_e, 1);
       }
       if (info->button_w != widget) {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_w),0);
           gtk_widget_set_sensitive(info->button_w, 1);
       }
       if (info->button_center != widget) {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_center),0);
           gtk_widget_set_sensitive(info->button_center, 1);
       }

       gtk_widget_set_sensitive(widget, 0);
    }
}

void 
toggle_side (GtkWidget *widget, gpointer data)
{
   Info *info;
   gint side;
   GList *list;
   GtkPackerChild *pchild;

   info = (Info*) data;
 
   if (GTK_TOGGLE_BUTTON(widget)->active) {

       side = GPOINTER_TO_INT (gtk_object_get_data(GTK_OBJECT(widget), "side"));

       pchild = info->pchild;
       if (pchild == NULL) {
           abort();
       };
       gtk_packer_configure(GTK_PACKER(info->packer),
                           info->current,
                           side,
                           pchild->anchor,
                           pchild->options, 
                           pchild->border_width, 
                           pchild->pad_x, 
                           pchild->pad_y, 
                           pchild->i_pad_x, 
                           pchild->i_pad_y);

       if (info->button_top != widget) {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_top),0);
           gtk_widget_set_sensitive(info->button_top, 1);
       }

       if (info->button_bottom != widget) {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_bottom),0);
           gtk_widget_set_sensitive(info->button_bottom, 1);
       }

       if (info->button_left != widget) {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_left),0);
           gtk_widget_set_sensitive(info->button_left, 1);
       }

       if (info->button_right != widget) {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_right),0);
           gtk_widget_set_sensitive(info->button_right, 1);
       }

       gtk_widget_set_sensitive(widget, 0);
    }
}

void
set_widget (GtkWidget *w, gpointer data) {
   Info *info;
   GList *list;
   GtkWidget *widget;
   GtkPackerChild *pchild;
   gint options;

   if (GTK_TOGGLE_BUTTON(w)->active) {

       info = (Info*) data;
       info->current = w;

       pchild = NULL;
       list = g_list_first(GTK_PACKER(info->packer)->children);
       while (list) {
           if (((GtkPackerChild*)(list->data))->widget == info->current) {
               pchild = (GtkPackerChild*)(list->data);
               break;
           }
           list = g_list_next(list);
       }
       if (pchild == NULL) {
           abort();
       };

       info->pchild = pchild;
 
       switch (pchild->side) {
         case GTK_SIDE_TOP:
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_top),1);
           break;
         case GTK_SIDE_BOTTOM:
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_bottom),1);
           break;
         case GTK_SIDE_LEFT:
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_left),1);
           break;
         case GTK_SIDE_RIGHT:
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_right),1);
           break;
         default:
           printf("foo... side == %d\n", pchild->side);
       };

       switch (pchild->anchor) {
         case GTK_ANCHOR_N:
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_n),1);
           break;
         case GTK_ANCHOR_NW:
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_nw),1);
           break;
         case GTK_ANCHOR_NE:
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_ne),1);
           break;
         case GTK_ANCHOR_S:
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_s),1);
           break;
         case GTK_ANCHOR_SW:
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_sw),1);
           break;
         case GTK_ANCHOR_SE:
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_se),1);
           break;
         case GTK_ANCHOR_W:
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_w),1);
           break;
         case GTK_ANCHOR_E:
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_e),1);
           break;
         case GTK_ANCHOR_CENTER:
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_center),1);
           break;
         default:
       };

       options = pchild->options;
       if (options & GTK_PACK_EXPAND) {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_expand),1);
       } else {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_expand),0);
       }
       if (options & GTK_FILL_X) {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_fillx),1);
       } else {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_fillx),0);
       }
       if (options & GTK_FILL_Y) {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_filly),1);
       } else {
           gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(info->button_filly),0);
       }
       

       gtk_widget_set_sensitive(w, 0);

       list = g_list_first(info->widgets);
       while (list) {
           widget = (GtkWidget*)(list->data);
           if (widget != info->current) {
               gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(widget),0);
               gtk_widget_set_sensitive(widget, 1);
           }

           list = g_list_next(list);
       } 
   }

}

void 
add_widget (GtkWidget *w, gpointer data)
{  
   static gint n = 0;
   GtkPacker *packer;
   GtkWidget *widget;
   gchar str[255];
   Info *info;

   info = (Info*) data;
   packer = GTK_PACKER(info->packer);
   
   sprintf(str, "%d", n);
   widget = gtk_toggle_button_new_with_label(str);
   gtk_widget_set_usize(widget, 50, 50);
   gtk_container_add(GTK_CONTAINER(packer), widget);
   gtk_widget_show(widget);

   gtk_signal_connect (GTK_OBJECT (widget), "toggled",
                       GTK_SIGNAL_FUNC (set_widget), (gpointer) info);

   info->widgets = g_list_append(info->widgets, (gpointer) widget);
   gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(widget),1);
   set_widget(widget, info);

   n++;
}

