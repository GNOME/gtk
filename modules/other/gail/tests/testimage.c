#include <gtk/gtk.h>
#include "testlib.h"
#include <stdlib.h>

/*
 * This test modules tests the AtkImage interface. When the module is loaded
 * with testgtk , it also creates a dialog that contains GtkArrows and a 
 * GtkImage. 
 *
 */

typedef struct
{
  GtkWidget *dialog;
  GtkWidget *arrow1;
  GtkWidget *arrow2;
  GtkWidget *arrow3;
  GtkWidget *arrow4;
  GtkWidget *close_button;
  GtkImage  *image;
}MainDialog;

static void destroy (GtkWidget *widget, gpointer data)
{
  gtk_widget_destroy(GTK_WIDGET(data));
}

static void _check_arrows (AtkObject *obj)
{
  AtkRole role;
  MainDialog *md;
  static gint visibleDialog = 0;


  role = atk_object_get_role(obj);
  if(role == ATK_ROLE_FRAME) {

	md = (MainDialog *) malloc (sizeof(MainDialog));
	if (visibleDialog == 0)
    {
		md->arrow1 = gtk_arrow_new(GTK_ARROW_UP,GTK_SHADOW_IN);
		md->arrow2 = gtk_arrow_new(GTK_ARROW_DOWN,GTK_SHADOW_IN);
		md->arrow3 = gtk_arrow_new(GTK_ARROW_LEFT,GTK_SHADOW_OUT);
		md->arrow4 = gtk_arrow_new(GTK_ARROW_RIGHT,GTK_SHADOW_OUT);
		md->dialog = gtk_dialog_new();
		gtk_window_set_modal(GTK_WINDOW(md->dialog), TRUE);
                gtk_box_pack_start(GTK_BOX (GTK_DIALOG (md->dialog)->vbox),
                                   md->arrow1, TRUE,TRUE, 0);
		gtk_box_pack_start(GTK_BOX (GTK_DIALOG (md->dialog)->vbox),
                                   md->arrow2, TRUE,TRUE, 0);
		gtk_box_pack_start(GTK_BOX (GTK_DIALOG (md->dialog)->vbox),
                                   md->arrow3, TRUE,TRUE, 0);
		gtk_box_pack_start(GTK_BOX (GTK_DIALOG (md->dialog)->vbox),
                                   md->arrow4, TRUE,TRUE, 0);
		g_signal_connect(GTK_OBJECT(md->dialog), "destroy",
                                 G_CALLBACK (destroy), md->dialog);

	        md->image = GTK_IMAGE(gtk_image_new_from_file("circles.xbm"));
		gtk_box_pack_start(GTK_BOX (GTK_DIALOG (md->dialog)->vbox),
                                   GTK_WIDGET(md->image), TRUE,TRUE, 0);
		md->close_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
		g_signal_connect(GTK_OBJECT(md->close_button), "clicked",
                                 G_CALLBACK (destroy), md->dialog);

		gtk_box_pack_start(GTK_BOX (GTK_DIALOG (md->dialog)->action_area),
                                   md->close_button, TRUE,TRUE, 0);

		gtk_widget_show_all(md->dialog);
		visibleDialog = 1;
    }
 }
}


static void 
_print_image_info(AtkObject *obj) {

  gint height, width;
  const gchar *desc;
  const gchar *name = atk_object_get_name (obj);
  const gchar *type_name = g_type_name(G_TYPE_FROM_INSTANCE (obj));

  height = width = 0;


  if(!ATK_IS_IMAGE(obj)) 
	return;

  g_print("atk_object_get_name : %s\n", name ? name : "<NULL>");
  g_print("atk_object_get_type_name : %s\n",type_name ?type_name :"<NULL>");
  g_print("*** Start Image Info ***\n");
  desc = atk_image_get_image_description(ATK_IMAGE(obj));
  g_print ("atk_image_get_image_desc returns : %s\n",desc ? desc:"<NULL>");
  atk_image_get_image_size(ATK_IMAGE(obj), &height ,&width);
  g_print("atk_image_get_image_size returns: height %d width %d\n",
											height,width);
  if(atk_image_set_image_description(ATK_IMAGE(obj),"New image Description")){
	desc = atk_image_get_image_description(ATK_IMAGE(obj));
	g_print ("atk_image_get_image_desc now returns : %s\n",desc?desc:"<NULL>");
  }
  g_print("*** End Image Info ***\n");


}
static void _traverse_children (AtkObject *obj)
{
  gint n_children, i;

  n_children = atk_object_get_n_accessible_children (obj);
  for (i = 0; i < n_children; i++)
  {
    AtkObject *child;

    child = atk_object_ref_accessible_child (obj, i);
	_print_image_info(child);
    _traverse_children (child);
    g_object_unref (G_OBJECT (child));
  }
}


static void _check_objects (AtkObject *obj)
{
  AtkRole role;

  g_print ("Start of _check_values\n");

  _check_arrows(obj);
  role = atk_object_get_role (obj);

  if (role == ATK_ROLE_FRAME || role == ATK_ROLE_DIALOG)
  {
    /*
     * Add handlers to all children.
     */
    _traverse_children (obj);
  }
  g_print ("End of _check_values\n");
}


static void
_create_event_watcher (void)
{
  atk_add_focus_tracker (_check_objects);
}

int
gtk_module_init(gint argc, char* argv[])
{
  g_print("testimages Module loaded\n");

  _create_event_watcher();

  return 0;
}
