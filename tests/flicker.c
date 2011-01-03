#include <gtk/gtk.h>

GtkWidget*
create_flicker (void)
{
  GtkWidget *window1;
  GtkWidget *hpaned1;
  GtkWidget *vpaned2;
  GtkWidget *hbox2;
  GtkObject *spinbutton7_adj;
  GtkWidget *spinbutton7;
  GtkObject *spinbutton8_adj;
  GtkWidget *spinbutton8;
  GtkWidget *vbox1;
  GtkObject *spinbutton9_adj;
  GtkWidget *spinbutton9;
  GtkObject *spinbutton10_adj;
  GtkWidget *spinbutton10;
  GtkObject *spinbutton11_adj;
  GtkWidget *spinbutton11;
  GtkObject *spinbutton12_adj;
  GtkWidget *spinbutton12;
  GtkObject *spinbutton13_adj;
  GtkWidget *spinbutton13;
  GtkObject *spinbutton14_adj;
  GtkWidget *spinbutton14;
  GtkObject *spinbutton15_adj;
  GtkWidget *spinbutton15;
  GtkObject *spinbutton16_adj;
  GtkWidget *spinbutton16;
  GtkWidget *vpaned1;
  GtkWidget *hbox1;
  GtkObject *spinbutton17_adj;
  GtkWidget *spinbutton17;
  GtkObject *spinbutton18_adj;
  GtkWidget *spinbutton18;
  GtkObject *spinbutton19_adj;
  GtkWidget *spinbutton19;
  GtkWidget *vbox2;
  GtkObject *spinbutton20_adj;
  GtkWidget *spinbutton20;
  GtkObject *spinbutton21_adj;
  GtkWidget *spinbutton21;
  GtkObject *spinbutton22_adj;
  GtkWidget *spinbutton22;
  GtkObject *spinbutton23_adj;
  GtkWidget *spinbutton23;
  GtkObject *spinbutton24_adj;
  GtkWidget *spinbutton24;
  GtkObject *spinbutton25_adj;
  GtkWidget *spinbutton25;
  GtkObject *spinbutton26_adj;
  GtkWidget *spinbutton26;
  GtkObject *spinbutton27_adj;
  GtkWidget *spinbutton27;

  window1 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window1), 500, 400);
  gtk_window_set_title (GTK_WINDOW (window1), "window1");

  hpaned1 = gtk_hpaned_new ();
  gtk_widget_show (hpaned1);
  gtk_container_add (GTK_CONTAINER (window1), hpaned1);
  gtk_paned_set_position (GTK_PANED (hpaned1), 100);

  vpaned2 = gtk_vpaned_new ();
  gtk_widget_show (vpaned2);
  gtk_paned_pack1 (GTK_PANED (hpaned1), vpaned2, FALSE, TRUE);
  gtk_paned_set_position (GTK_PANED (vpaned2), 100);

  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox2);
  gtk_paned_pack1 (GTK_PANED (vpaned2), hbox2, FALSE, TRUE);

  spinbutton7_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton7 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton7_adj), 1, 0);
  gtk_widget_show (spinbutton7);
  gtk_box_pack_start (GTK_BOX (hbox2), spinbutton7, TRUE, TRUE, 0);

  spinbutton8_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton8 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton8_adj), 1, 0);
  gtk_widget_show (spinbutton8);
  gtk_box_pack_start (GTK_BOX (hbox2), spinbutton8, TRUE, TRUE, 0);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox1);
  gtk_paned_pack2 (GTK_PANED (vpaned2), vbox1, TRUE, TRUE);

  spinbutton9_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton9 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton9_adj), 1, 0);
  gtk_widget_show (spinbutton9);
  gtk_box_pack_start (GTK_BOX (vbox1), spinbutton9, FALSE, FALSE, 0);

  spinbutton10_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton10 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton10_adj), 1, 0);
  gtk_widget_show (spinbutton10);
  gtk_box_pack_start (GTK_BOX (vbox1), spinbutton10, FALSE, FALSE, 0);

  spinbutton11_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton11 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton11_adj), 1, 0);
  gtk_widget_show (spinbutton11);
  gtk_box_pack_start (GTK_BOX (vbox1), spinbutton11, FALSE, FALSE, 0);

  spinbutton12_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton12 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton12_adj), 1, 0);
  gtk_widget_show (spinbutton12);
  gtk_box_pack_start (GTK_BOX (vbox1), spinbutton12, FALSE, FALSE, 0);

  spinbutton13_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton13 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton13_adj), 1, 0);
  gtk_widget_show (spinbutton13);
  gtk_box_pack_start (GTK_BOX (vbox1), spinbutton13, FALSE, FALSE, 0);

  spinbutton14_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton14 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton14_adj), 1, 0);
  gtk_widget_show (spinbutton14);
  gtk_box_pack_start (GTK_BOX (vbox1), spinbutton14, FALSE, FALSE, 0);

  spinbutton15_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton15 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton15_adj), 1, 0);
  gtk_widget_show (spinbutton15);
  gtk_box_pack_start (GTK_BOX (vbox1), spinbutton15, FALSE, FALSE, 0);

  spinbutton16_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton16 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton16_adj), 1, 0);
  gtk_widget_show (spinbutton16);
  gtk_box_pack_start (GTK_BOX (vbox1), spinbutton16, FALSE, FALSE, 0);

  vpaned1 = gtk_vpaned_new ();
  gtk_widget_show (vpaned1);
  gtk_paned_pack2 (GTK_PANED (hpaned1), vpaned1, TRUE, TRUE);
  gtk_paned_set_position (GTK_PANED (vpaned1), 0);

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox1);
  gtk_paned_pack1 (GTK_PANED (vpaned1), hbox1, FALSE, TRUE);

  spinbutton17_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton17 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton17_adj), 1, 0);
  gtk_widget_show (spinbutton17);
  gtk_box_pack_start (GTK_BOX (hbox1), spinbutton17, TRUE, TRUE, 0);

  spinbutton18_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton18 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton18_adj), 1, 0);
  gtk_widget_show (spinbutton18);
  gtk_box_pack_start (GTK_BOX (hbox1), spinbutton18, TRUE, TRUE, 0);

  spinbutton19_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton19 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton19_adj), 1, 0);
  gtk_widget_show (spinbutton19);
  gtk_box_pack_start (GTK_BOX (hbox1), spinbutton19, TRUE, TRUE, 0);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox2);
  gtk_paned_pack2 (GTK_PANED (vpaned1), vbox2, FALSE, FALSE);

  spinbutton20_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton20 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton20_adj), 1, 0);
  gtk_widget_show (spinbutton20);
  gtk_box_pack_start (GTK_BOX (vbox2), spinbutton20, FALSE, FALSE, 0);

  spinbutton21_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton21 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton21_adj), 1, 0);
  gtk_widget_show (spinbutton21);
  gtk_box_pack_start (GTK_BOX (vbox2), spinbutton21, FALSE, FALSE, 0);

  spinbutton22_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton22 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton22_adj), 1, 0);
  gtk_widget_show (spinbutton22);
  gtk_box_pack_start (GTK_BOX (vbox2), spinbutton22, FALSE, FALSE, 0);

  spinbutton23_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton23 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton23_adj), 1, 0);
  gtk_widget_show (spinbutton23);
  gtk_box_pack_start (GTK_BOX (vbox2), spinbutton23, FALSE, FALSE, 0);

  spinbutton24_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton24 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton24_adj), 1, 0);
  gtk_widget_show (spinbutton24);
  gtk_box_pack_start (GTK_BOX (vbox2), spinbutton24, FALSE, FALSE, 0);

  spinbutton25_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton25 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton25_adj), 1, 0);
  gtk_widget_show (spinbutton25);
  gtk_box_pack_start (GTK_BOX (vbox2), spinbutton25, FALSE, FALSE, 0);

  spinbutton26_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton26 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton26_adj), 1, 0);
  gtk_widget_show (spinbutton26);
  gtk_box_pack_start (GTK_BOX (vbox2), spinbutton26, TRUE, FALSE, 0);

  spinbutton27_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton27 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton27_adj), 1, 0);
  gtk_widget_show (spinbutton27);
  gtk_box_pack_end (GTK_BOX (vbox2), spinbutton27, FALSE, FALSE, 0);


  return window1;
}


int
main (int argc, char *argv[])
{
  GtkWidget *window1;

  gtk_init (&argc, &argv);

  window1 = create_flicker ();
  gtk_widget_show (window1);

  gtk_main ();
  return 0;
}

