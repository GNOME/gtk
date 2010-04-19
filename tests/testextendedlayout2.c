#include <gtk/gtk.h>


typedef struct {
  const gchar *name;
  const gchar *tooltip;
  const gchar *interface;
  GtkWidget   *window;
} TestInterface;


/* These strings were generated with:
 *
 *     IFS=""; while read line; do echo -n \"; echo -n $line | sed -e 's|\"|\\"|g'; echo \"; done < file.glade
 */
TestInterface interfaces[] = {
  {
    "Ellipsizing Labels",
    "Demonstrates how labels will request a natural size in a horizontal space",
    "<interface>"
    "  <requires lib=\"gtk+\" version=\"2.20\"/>"
    "  <!-- interface-naming-policy project-wide -->"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "    <property name=\"default_width\">450</property>"
    "    <property name=\"default_height\">50</property>"
    "    <child>"
    "      <object class=\"GtkHBox\" id=\"hbox5\">"
    "        <property name=\"visible\">True</property>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label9\">"
    "            <property name=\"visible\">True</property>"
    "            <property name=\"label\" translatable=\"yes\">Some labels do ellipsize</property>"
    "            <property name=\"ellipsize\">end</property>"
    "            <attributes>"
    "              <attribute name=\"weight\" value=\"bold\"/>"
    "              <attribute name=\"foreground\" value=\"#09610feefe03\"/>"
    "            </attributes>"
    "          </object>"
    "          <packing>"
    "            <property name=\"position\">0</property>"
    "          </packing>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label10\">"
    "            <property name=\"visible\">True</property>"
    "            <property name=\"label\" translatable=\"yes\">but some</property>"
    "            <property name=\"ellipsize\">end</property>"
    "            <attributes>"
    "              <attribute name=\"weight\" value=\"bold\"/>"
    "              <attribute name=\"foreground\" value=\"#0000af6b0993\"/>"
    "            </attributes>"
    "          </object>"
    "          <packing>"
    "            <property name=\"position\">1</property>"
    "          </packing>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label11\">"
    "            <property name=\"visible\">True</property>"
    "            <property name=\"label\" translatable=\"yes\">do not at all</property>"
    "            <attributes>"
    "              <attribute name=\"style\" value=\"normal\"/>"
    "              <attribute name=\"weight\" value=\"bold\"/>"
    "              <attribute name=\"foreground\" value=\"#ffff00000000\"/>"
    "            </attributes>"
    "          </object>"
    "          <packing>"
    "            <property name=\"position\">2</property>"
    "          </packing>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>",
    NULL
  },

  {
    "Wrapping Label",
    "Demonstrates how a wrapping label can require a height contextual to its allocated width",
    "<interface>"
    "  <requires lib=\"gtk+\" version=\"2.18\"/>"
    "  <!-- interface-naming-policy project-wide -->"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "    <property name=\"border_width\">12</property>"
    "    <property name=\"default_width\">300</property>"
    "    <child>"
    "      <object class=\"GtkHPaned\" id=\"hpaned1\">"
    "        <property name=\"visible\">True</property>"
    "        <property name=\"can_focus\">True</property>"
    "        <child>"
    "          <object class=\"GtkVBox\" id=\"vbox2\">"
    "            <property name=\"visible\">True</property>"
    "            <child>"
    "              <object class=\"GtkLabel\" id=\"label3\">"
    "                <property name=\"visible\">True</property>"
    "                <property name=\"label\" translatable=\"yes\">A short static label.</property>"
    "                <attributes>"
    "                  <attribute name=\"weight\" value=\"bold\"/>"
    "                </attributes>"
    "              </object>"
    "              <packing>"
    "                <property name=\"position\">0</property>"
    "              </packing>"
    "            </child>"
    "            <child>"
    "              <object class=\"GtkLabel\" id=\"label1\">"
    "                <property name=\"visible\">True</property>"
    "                <property name=\"label\" translatable=\"yes\">This is a really long label for the purpose of testing line wrapping is working correctly in conjunction with height-for-width support in GTK+</property>"
    "                <property name=\"wrap\">True</property>"
    "                <property name=\"max_width_chars\">30</property>"
    "                <attributes>"
    "                  <attribute name=\"foreground\" value=\"#18c52119f796\"/>"
    "                </attributes>"
    "              </object>"
    "              <packing>"
    "                <property name=\"expand\">False</property>"
    "                <property name=\"position\">1</property>"
    "              </packing>"
    "            </child>"
    "            <child>"
    "              <object class=\"GtkButton\" id=\"button2\">"
    "                <property name=\"visible\">True</property>"
    "                <property name=\"can_focus\">True</property>"
    "                <property name=\"receives_default\">True</property>"
    "                <child>"
    "                  <object class=\"GtkLabel\" id=\"label2\">"
    "                    <property name=\"visible\">True</property>"
    "                    <property name=\"label\" translatable=\"yes\">A really really long label inside a button to demonstrate height for width working inside buttons</property>"
    "                    <property name=\"wrap\">True</property>"
    "                    <property name=\"max_width_chars\">25</property>"
    "                    <attributes>"
    "                      <attribute name=\"foreground\" value=\"#1e3687ab0a52\"/>"
    "                    </attributes>"
    "                  </object>"
    "                </child>"
    "              </object>"
    "              <packing>"
    "                <property name=\"expand\">False</property>"
    "                <property name=\"position\">2</property>"
    "              </packing>"
    "            </child>"
    "          </object>"
    "          <packing>"
    "            <property name=\"resize\">False</property>"
    "            <property name=\"shrink\">False</property>"
    "          </packing>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label4\">"
    "            <property name=\"visible\">True</property>"
    "            <property name=\"label\" translatable=\"yes\">This label can shrink.</property>"
    "            <property name=\"justify\">center</property>"
    "            <attributes>"
    "              <attribute name=\"style\" value=\"normal\"/>"
    "              <attribute name=\"foreground\" value=\"#ffff00000000\"/>"
    "            </attributes>"
    "          </object>"
    "          <packing>"
    "            <property name=\"resize\">True</property>"
    "            <property name=\"shrink\">True</property>"
    "          </packing>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>",
    NULL
  },

  {
    "Horizontal Box",
    "Demonstrates how a horizontal box can calculate the collective height for an allocated width",
    "<interface>"
    "  <requires lib=\"gtk+\" version=\"2.20\"/>"
    "  <!-- interface-naming-policy project-wide -->"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "    <property name=\"default_height\">200</property>"
    "    <property name=\"default_width\">600</property>"
    "    <child>"
    "      <object class=\"GtkHPaned\" id=\"hpaned1\">"
    "        <property name=\"visible\">True</property>"
    "        <property name=\"can_focus\">True</property>"
    "        <child>"
    "          <object class=\"GtkVBox\" id=\"vbox1\">"
    "            <property name=\"visible\">True</property>"
    "            <child>"
    "              <object class=\"GtkHBox\" id=\"hbox1\">"
    "                <property name=\"visible\">True</property>"
    "                <child>"
    "                  <object class=\"GtkButton\" id=\"button1\">"
    "                    <property name=\"visible\">True</property>"
    "                    <property name=\"can_focus\">True</property>"
    "                    <property name=\"receives_default\">True</property>"
    "                    <property name=\"use_action_appearance\">False</property>"
    "                    <child>"
    "                      <object class=\"GtkLabel\" id=\"label2\">"
    "                        <property name=\"visible\">True</property>"
    "                        <property name=\"label\" translatable=\"yes\">A button that wraps.</property>"
    "                        <property name=\"wrap\">True</property>"
    "                        <property name=\"width_chars\">10</property>"
    "                        <attributes>"
    "                          <attribute name=\"foreground\" value=\"#0000041dffff\"/>"
    "                        </attributes>"
    "                      </object>"
    "                    </child>"
    "                  </object>"
    "                  <packing>"
    "                    <property name=\"expand\">False</property>"
    "                    <property name=\"position\">0</property>"
    "                  </packing>"
    "                </child>"
    "                <child>"
    "                  <object class=\"GtkLabel\" id=\"label1\">"
    "                    <property name=\"visible\">True</property>"
    "                    <property name=\"label\" translatable=\"yes\">Lets try to set some text start to wrap up in this hbox and see if the height-for-width is gonna work !</property>"
    "                    <property name=\"wrap\">True</property>"
    "                    <property name=\"width_chars\">30</property>"
    "                    <attributes>"
    "                      <attribute name=\"foreground\" value=\"#07d0a9b20972\"/>"
    "                    </attributes>"
    "                  </object>"
    "                  <packing>"
    "                    <property name=\"position\">1</property>"
    "                  </packing>"
    "                </child>"
    "              </object>"
    "              <packing>"
    "                <property name=\"expand\">False</property>"
    "                <property name=\"position\">0</property>"
    "              </packing>"
    "            </child>"
    "            <child>"
    "              <object class=\"GtkButton\" id=\"button2\">"
    "                <property name=\"label\" translatable=\"yes\">A button that expands in the container</property>"
    "                <property name=\"visible\">True</property>"
    "                <property name=\"can_focus\">True</property>"
    "                <property name=\"receives_default\">True</property>"
    "                <property name=\"use_action_appearance\">False</property>"
    "              </object>"
    "              <packing>"
    "                <property name=\"position\">1</property>"
    "              </packing>"
    "            </child>"
    "          </object>"
    "          <packing>"
    "            <property name=\"resize\">False</property>"
    "            <property name=\"shrink\">False</property>"
    "          </packing>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label4\">"
    "            <property name=\"visible\">True</property>"
    "            <property name=\"label\" translatable=\"yes\">This label can shrink.</property>"
    "            <property name=\"justify\">center</property>"
    "            <attributes>"
    "              <attribute name=\"style\" value=\"normal\"/>"
    "              <attribute name=\"foreground\" value=\"#ffff00000000\"/>"
    "            </attributes>"
    "          </object>"
    "          <packing>"
    "            <property name=\"resize\">True</property>"
    "            <property name=\"shrink\">True</property>"
    "          </packing>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>",
    NULL
  },

};


static void
test_clicked (GtkWidget     *button, 
	      TestInterface *interface)
{
  if (!interface->window)
    {
      GtkBuilder *builder = gtk_builder_new ();
      
      gtk_builder_add_from_string (builder, interface->interface, -1, NULL);
      interface->window = (GtkWidget *)gtk_builder_get_object (builder, "window");

      g_signal_connect (interface->window, "delete_event", 
			G_CALLBACK (gtk_widget_hide_on_delete), NULL);
    }

  gtk_widget_show (interface->window);
}


static GtkWidget *
create_window (void)
{
  GtkWidget *window, *vbox, *button;
  gint i;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  vbox   = gtk_vbox_new (FALSE, 6);

  gtk_container_set_border_width (GTK_CONTAINER (window), 8);

  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  for (i = 0; i < G_N_ELEMENTS (interfaces); i++)
    {
      button = gtk_button_new_with_label (interfaces[i].name);

      gtk_widget_set_tooltip_text (button, interfaces[i].tooltip);

      g_signal_connect (G_OBJECT (button), "clicked", 
			G_CALLBACK (test_clicked), &interfaces[i]);

      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      gtk_widget_show (button);
    }

  return window;
}



int
main (int argc, char *argv[])
{
  GtkWidget *window;

  gtk_init (&argc, &argv);

  window = create_window ();

  g_signal_connect (window, "delete-event",
                    G_CALLBACK (gtk_main_quit), window);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
