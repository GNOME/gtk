/* buildertest.c
 * Copyright (C) 2006-2007 Async Open Source
 * Authors: Johan Dahlin
 *          Henrique Romano
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <libintl.h>
#include <locale.h>
#include <math.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkprintjob.h>

static GtkBuilder *
builder_new_from_string (const gchar *buffer,
                         gsize length,
                         gchar *domain)
{
  GtkBuilder *builder;
  builder = gtk_builder_new ();
  if (domain)
    gtk_builder_set_translation_domain (builder, domain);
  gtk_builder_add_from_string (builder, buffer, length, NULL);
  return builder;
}

gboolean test_parser (void)
{
  GtkBuilder *builder;
  GError *error;
  
  builder = gtk_builder_new ();

  error = NULL;
  gtk_builder_add_from_string (builder, "<xxx/>", -1, &error);
  g_assert (error != NULL);
  g_return_val_if_fail (error->domain == GTK_BUILDER_ERROR, FALSE);
  g_return_val_if_fail (error->code == GTK_BUILDER_ERROR_UNHANDLED_TAG, FALSE);
    g_error_free (error);
  
  error = NULL;
  gtk_builder_add_from_string (builder, "<interface invalid=\"X\"/>", -1, &error);
  g_assert (error != NULL);
  g_return_val_if_fail (error->domain == GTK_BUILDER_ERROR, FALSE);
  g_return_val_if_fail (error->code == GTK_BUILDER_ERROR_INVALID_ATTRIBUTE, FALSE);
  g_error_free (error);

  error = NULL;
  gtk_builder_add_from_string (builder, "<interface><child/></interface>", -1, &error);
  g_assert (error != NULL);
  g_return_val_if_fail (error->domain == GTK_BUILDER_ERROR, FALSE);
  g_return_val_if_fail (error->code == GTK_BUILDER_ERROR_INVALID_TAG, FALSE);
  g_error_free (error);

  error = NULL;
  gtk_builder_add_from_string (builder, "<interface><object class=\"GtkVBox\" id=\"a\"><object class=\"GtkHBox\" id=\"b\"/></object></interface>", -1, &error);
  g_assert (error != NULL);
  g_return_val_if_fail (error->domain == GTK_BUILDER_ERROR, FALSE);
  g_return_val_if_fail (error->code == GTK_BUILDER_ERROR_INVALID_TAG, FALSE);
  g_error_free (error);

  g_object_unref (builder);
  
  return TRUE;
}

int normal;
int after;
int object;
int object_after;

void
signal_normal (GtkWindow *window, GParamSpec spec)
{
  g_assert (GTK_IS_WINDOW (window));
  g_assert (normal == 0);
  g_assert (after == 0);

  normal++;
}
                    
void
signal_after (GtkWindow *window, GParamSpec spec)
{
  g_assert (GTK_IS_WINDOW (window));
  g_assert (normal == 1);
  g_assert (after == 0);
  
  after++;
}

void
signal_object (GtkButton *button, GParamSpec spec)
{
  g_assert (GTK_IS_BUTTON (button));
  g_assert (object == 0);
  g_assert (object_after == 0);

  object++;
}

void
signal_object_after (GtkButton *button, GParamSpec spec)
{
  g_assert (GTK_IS_BUTTON (button));
  g_assert (object == 1);
  g_assert (object_after == 0);

  object_after++;
}

void
signal_first (GtkButton *button, GParamSpec spec)
{
  g_assert (normal == 0);
  normal = 10;
}

void
signal_second (GtkButton *button, GParamSpec spec)
{
  g_assert (normal == 10);
  normal = 20;
}

void
signal_extra (GtkButton *button, GParamSpec spec)
{
  g_assert (normal == 20);
  normal = 30;
}

void
signal_extra2 (GtkButton *button, GParamSpec spec)
{
  g_assert (normal == 30);
  normal = 40;
}

gboolean test_connect_signals (void)
{
  GtkBuilder *builder;
  GObject *window;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkButton\" id=\"button\"/>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <signal name=\"notify::title\" handler=\"signal_normal\"/>"
    "    <signal name=\"notify::title\" handler=\"signal_after\" after=\"yes\"/>"
    "    <signal name=\"notify::title\" handler=\"signal_object\""
    "            object=\"button\"/>"
    "    <signal name=\"notify::title\" handler=\"signal_object_after\""
    "            object=\"button\" after=\"yes\"/>"
    "  </object>"
    "</interface>";
  const gchar buffer_order[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <signal name=\"notify::title\" handler=\"signal_first\"/>"
    "    <signal name=\"notify::title\" handler=\"signal_second\"/>"
    "  </object>"
    "</interface>";
  const gchar buffer_extra[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window2\">"
    "    <signal name=\"notify::title\" handler=\"signal_extra\"/>"
    "  </object>"
    "</interface>";
  const gchar buffer_extra2[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window3\">"
    "    <signal name=\"notify::title\" handler=\"signal_extra2\"/>"
    "  </object>"
    "</interface>";
  const gchar buffer_after_child[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkButton\" id=\"button1\"/>"
    "    </child>"
    "    <signal name=\"notify::title\" handler=\"signal_normal\"/>"
    "  </object>"
    "</interface>";

  builder = builder_new_from_string (buffer, -1, NULL);
  gtk_builder_connect_signals (builder, NULL);

  window = gtk_builder_get_object (builder, "window1");
  gtk_window_set_title (GTK_WINDOW (window), "test");

  g_return_val_if_fail (normal == 1, FALSE);
  g_return_val_if_fail (after == 1, FALSE);
  g_return_val_if_fail (object == 1, FALSE);
  g_return_val_if_fail (object_after == 1, FALSE);
  gtk_widget_destroy (GTK_WIDGET (window));
  g_object_unref (builder);
  
  builder = builder_new_from_string (buffer_order, -1, NULL);
  gtk_builder_connect_signals (builder, NULL);
  window = gtk_builder_get_object (builder, "window1");
  normal = 0;
  gtk_window_set_title (GTK_WINDOW (window), "test");
  g_assert (normal == 20);

  gtk_widget_destroy (GTK_WIDGET (window));

  gtk_builder_add_from_string (builder, buffer_extra,
			       strlen (buffer_extra), NULL);
  gtk_builder_add_from_string (builder, buffer_extra2,
			       strlen (buffer_extra2), NULL);
  gtk_builder_connect_signals (builder, NULL);
  window = gtk_builder_get_object (builder, "window2");
  gtk_window_set_title (GTK_WINDOW (window), "test");
  g_assert (normal == 30);

  gtk_widget_destroy (GTK_WIDGET (window));
  window = gtk_builder_get_object (builder, "window3");
  gtk_window_set_title (GTK_WINDOW (window), "test");
  g_assert (normal == 40);
  gtk_widget_destroy (GTK_WIDGET (window));
  
  g_object_unref (builder);

  /* new test, reset globals */
  after = 0;
  normal = 0;
  
  builder = builder_new_from_string (buffer_after_child, -1, NULL);
  window = gtk_builder_get_object (builder, "window1");
  gtk_builder_connect_signals (builder, NULL);
  gtk_window_set_title (GTK_WINDOW (window), "test");

  g_return_val_if_fail (normal == 1, FALSE);
  gtk_widget_destroy (GTK_WIDGET (window));
  g_object_unref (builder);
  
  return TRUE;
}

gboolean test_uimanager_simple (void)
{
  GtkBuilder *builder;
  GObject *window, *uimgr, *menubar;
  GObject *menu, *label;
  GList *children;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkUIManager\" id=\"uimgr1\"/>"
    "</interface>";
    
  const gchar buffer2[] =
    "<interface>"
    "  <object class=\"GtkUIManager\" id=\"uimgr1\">"
    "    <child>"
    "      <object class=\"GtkActionGroup\" id=\"ag1\">"
    "        <child>"
    "          <object class=\"GtkAction\" id=\"file\">"
    "            <property name=\"label\">_File</property>"
    "          </object>"
    "          <accelerator key=\"n\" modifiers=\"GDK_CONTROL_MASK\"/>"
    "        </child>"
    "      </object>"
    "    </child>"
    "    <ui>"
    "      <menubar name=\"menubar1\">"
    "        <menu action=\"file\">"
    "        </menu>"
    "      </menubar>"
    "    </ui>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkMenuBar\" id=\"menubar1\" constructor=\"uimgr1\"/>"
    "    </child>"
    "  </object>"
    "</interface>";

  builder = builder_new_from_string (buffer, -1, NULL);

  uimgr = gtk_builder_get_object (builder, "uimgr1");
  g_return_val_if_fail (uimgr != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_UI_MANAGER (uimgr), FALSE);
  g_object_unref (builder);
  
  builder = builder_new_from_string (buffer2, -1, NULL);

  menubar = gtk_builder_get_object (builder, "menubar1");
  g_return_val_if_fail (menubar != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_BAR (menubar), FALSE);

  children = gtk_container_get_children (GTK_CONTAINER (menubar));
  menu = children->data;
  g_return_val_if_fail (menu != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_ITEM (menu), FALSE);
  g_return_val_if_fail (strcmp (GTK_WIDGET (menu)->name, "file") == 0, FALSE);
  g_list_free (children);
  
  label = G_OBJECT (GTK_BIN (menu)->child);
  g_return_val_if_fail (label != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);
  g_return_val_if_fail
    (strcmp (gtk_label_get_text (GTK_LABEL (label)), "File") == 0, FALSE);

  window = gtk_builder_get_object (builder, "window1");
  gtk_widget_destroy (GTK_WIDGET (window));
  g_object_unref (builder);
  
  return TRUE;
}

gboolean test_domain (void)
{
  GtkBuilder *builder;
  const gchar buffer1[] = "<interface/>";
  const gchar buffer2[] = "<interface domain=\"domain\"/>";
  const gchar *domain;
  
  builder = builder_new_from_string (buffer1, -1, NULL);
  domain = gtk_builder_get_translation_domain (builder);
  g_assert (domain == NULL);
  g_object_unref (builder);
  
  builder = builder_new_from_string (buffer1, -1, "domain-1");
  domain = gtk_builder_get_translation_domain (builder);
  g_assert (domain);
  g_assert (strcmp (domain, "domain-1") == 0);
  g_object_unref (builder);
  
  builder = builder_new_from_string (buffer2, -1, NULL);
  domain = gtk_builder_get_translation_domain (builder);
  g_assert (domain);
  g_assert (strcmp (domain, "domain") == 0);
  g_object_unref (builder);
  
  builder = builder_new_from_string (buffer2, -1, "domain-1");
  domain = gtk_builder_get_translation_domain (builder);
  g_assert (domain);
  g_assert (strcmp (domain, "domain-1") == 0);
  g_object_unref (builder);

  return TRUE;
}

#if 0
gboolean test_translation (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label\">"
    "        <property name=\"label\" translatable=\"yes\">File</property>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GtkLabel *window, *label;

  setlocale (LC_ALL, "sv_SE");
  textdomain ("builder");
  bindtextdomain ("builder", "tests");

  builder = builder_new_from_string (buffer, -1, NULL);
  label = GTK_LABEL (gtk_builder_get_object (builder, "label"));
  g_assert (strcmp (gtk_label_get_text (label), "Arkiv") == 0);

  window = gtk_builder_get_object (builder, "window1");
  gtk_widget_destroy (GTK_WIDGET (window));
  g_object_unref (builder);
  
  return TRUE;
}
#endif

gboolean test_sizegroup (void)
{
  GtkBuilder * builder;
  const gchar buffer1[] =
    "<interface domain=\"test\">"
    "  <object class=\"GtkSizeGroup\" id=\"sizegroup1\">"
    "    <property name=\"mode\">GTK_SIZE_GROUP_HORIZONTAL</property>"
    "    <widgets>"
    "      <widget name=\"radio1\"/>"
    "      <widget name=\"radio2\"/>"
    "    </widgets>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkVBox\" id=\"vbox1\">"
    "        <child>"
    "          <object class=\"GtkRadioButton\" id=\"radio1\"/>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkRadioButton\" id=\"radio2\"/>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  const gchar buffer2[] =
    "<interface domain=\"test\">"
    "  <object class=\"GtkSizeGroup\" id=\"sizegroup1\">"
    "    <property name=\"mode\">GTK_SIZE_GROUP_HORIZONTAL</property>"
    "    <widgets>"
    "    </widgets>"
    "   </object>"
    "</interface>";
  const gchar buffer3[] =
    "<interface domain=\"test\">"
    "  <object class=\"GtkSizeGroup\" id=\"sizegroup1\">"
    "    <property name=\"mode\">GTK_SIZE_GROUP_HORIZONTAL</property>"
    "    <widgets>"
    "      <widget name=\"radio1\"/>"
    "      <widget name=\"radio2\"/>"
    "    </widgets>"
    "  </object>"
    "  <object class=\"GtkSizeGroup\" id=\"sizegroup2\">"
    "    <property name=\"mode\">GTK_SIZE_GROUP_HORIZONTAL</property>"
    "    <widgets>"
    "      <widget name=\"radio1\"/>"
    "      <widget name=\"radio2\"/>"
    "    </widgets>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkVBox\" id=\"vbox1\">"
    "        <child>"
    "          <object class=\"GtkRadioButton\" id=\"radio1\"/>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkRadioButton\" id=\"radio2\"/>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *sizegroup;
  GSList *widgets;

  builder = builder_new_from_string (buffer1, -1, NULL);
  sizegroup = gtk_builder_get_object (builder, "sizegroup1");
  widgets = gtk_size_group_get_widgets (GTK_SIZE_GROUP (sizegroup));
  g_return_val_if_fail (g_slist_length (widgets) == 2, FALSE);
  g_slist_free (widgets);
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  sizegroup = gtk_builder_get_object (builder, "sizegroup1");
  widgets = gtk_size_group_get_widgets (GTK_SIZE_GROUP (sizegroup));
  g_return_val_if_fail (g_slist_length (widgets) == 0, FALSE);
  g_slist_free (widgets);
  g_object_unref (builder);

  builder = builder_new_from_string (buffer3, -1, NULL);
  sizegroup = gtk_builder_get_object (builder, "sizegroup1");
  widgets = gtk_size_group_get_widgets (GTK_SIZE_GROUP (sizegroup));
  g_return_val_if_fail (g_slist_length (widgets) == 2, FALSE);
  g_slist_free (widgets);
  sizegroup = gtk_builder_get_object (builder, "sizegroup2");
  widgets = gtk_size_group_get_widgets (GTK_SIZE_GROUP (sizegroup));
  g_return_val_if_fail (g_slist_length (widgets) == 2, FALSE);
  g_slist_free (widgets);

#if 0
  {
    GObject *window;
    window = gtk_builder_get_object (builder, "window1");
    gtk_widget_destroy (GTK_WIDGET (window));
  }
#endif  
  g_object_unref (builder);

  return TRUE;
}

gboolean test_list_store (void)
{
  const gchar buffer1[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"guint\"/>"
    "    </columns>"
    "  </object>"
    "</interface>";
  const char buffer2[] = 
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"gint\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\">John</col>"
    "        <col id=\"1\">Doe</col>"
    "        <col id=\"2\">25</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"0\">Johan</col>"
    "        <col id=\"1\">Dole</col>"
    "        <col id=\"2\">50</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "</interface>";
  GtkBuilder *builder;
  GObject *store;
  GtkTreeIter iter;
  gchar *surname, *lastname;
  int age;
  
  builder = builder_new_from_string (buffer1, -1, NULL);
  store = gtk_builder_get_object (builder, "liststore1");
  g_return_val_if_fail (gtk_tree_model_get_n_columns (GTK_TREE_MODEL (store)) == 2, FALSE);
  g_return_val_if_fail (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 0) == G_TYPE_STRING, FALSE);
  g_return_val_if_fail (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 1) == G_TYPE_UINT, FALSE);
  g_object_unref (builder);
  
  builder = builder_new_from_string (buffer2, -1, NULL);
  store = gtk_builder_get_object (builder, "liststore1");
  g_return_val_if_fail (gtk_tree_model_get_n_columns (GTK_TREE_MODEL (store)) == 3, FALSE);
  g_return_val_if_fail (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 0) == G_TYPE_STRING, FALSE);
  g_return_val_if_fail (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 1) == G_TYPE_STRING, FALSE);
  g_return_val_if_fail (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 2) == G_TYPE_INT, FALSE);
  
  g_return_val_if_fail (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter) == TRUE, FALSE);
  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert (surname != NULL);
  g_return_val_if_fail (strcmp (surname, "John") == 0, FALSE);
  g_free (surname);
  g_assert (lastname != NULL);
  g_return_val_if_fail (strcmp (lastname, "Doe") == 0, FALSE);
  g_free (lastname);
  g_return_val_if_fail (age == 25, FALSE);
  g_return_val_if_fail (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter) == TRUE, FALSE);
  
  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert (surname != NULL);
  g_return_val_if_fail (strcmp (surname, "Johan") == 0, FALSE);
  g_free (surname);
  g_assert (lastname != NULL);
  g_return_val_if_fail (strcmp (lastname, "Dole") == 0, FALSE);
  g_free (lastname);
  g_return_val_if_fail (age == 50, FALSE);
  g_return_val_if_fail (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter) == FALSE, FALSE);
  
  g_object_unref (builder);

  return TRUE;
}

gboolean test_tree_store (void)
{
  const gchar buffer[] =
    "<interface domain=\"test\">"
    "  <object class=\"GtkTreeStore\" id=\"treestore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"guint\"/>"
    "    </columns>"
    "  </object>"
    "</interface>";
  GtkBuilder *builder;
  GObject *store;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  store = gtk_builder_get_object (builder, "treestore1");
  g_return_val_if_fail (gtk_tree_model_get_n_columns (GTK_TREE_MODEL (store)) == 2, FALSE);
  g_return_val_if_fail (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 0) == G_TYPE_STRING, FALSE);
  g_return_val_if_fail (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 1) == G_TYPE_UINT, FALSE);
  
  g_object_unref (builder);

  return TRUE;
}

gboolean test_types (void)
{
  const gchar buffer[] = 
    "<interface>"
    "  <object class=\"GtkAction\" id=\"action\"/>"
    "  <object class=\"GtkActionGroup\" id=\"actiongroup\"/>"
    "  <object class=\"GtkAlignment\" id=\"alignment\"/>"
    "  <object class=\"GtkArrow\" id=\"arrow\"/>"
    "  <object class=\"GtkButton\" id=\"button\"/>"
    "  <object class=\"GtkCheckButton\" id=\"checkbutton\"/>"
    "  <object class=\"GtkDialog\" id=\"dialog\"/>"
    "  <object class=\"GtkDrawingArea\" id=\"drawingarea\"/>"
    "  <object class=\"GtkEventBox\" id=\"eventbox\"/>"
    "  <object class=\"GtkEntry\" id=\"entry\"/>"
    "  <object class=\"GtkFontButton\" id=\"fontbutton\"/>"
    "  <object class=\"GtkHButtonBox\" id=\"hbuttonbox\"/>"
    "  <object class=\"GtkHBox\" id=\"hbox\"/>"
    "  <object class=\"GtkHPaned\" id=\"hpaned\"/>"
    "  <object class=\"GtkHRuler\" id=\"hruler\"/>"
    "  <object class=\"GtkHScale\" id=\"hscale\"/>"
    "  <object class=\"GtkHScrollbar\" id=\"hscrollbar\"/>"
    "  <object class=\"GtkHSeparator\" id=\"hseparator\"/>"
    "  <object class=\"GtkImage\" id=\"image\"/>"
    "  <object class=\"GtkLabel\" id=\"label\"/>"
    "  <object class=\"GtkListStore\" id=\"liststore\"/>"
    "  <object class=\"GtkMenuBar\" id=\"menubar\"/>"
    "  <object class=\"GtkNotebook\" id=\"notebook\"/>"
    "  <object class=\"GtkProgressBar\" id=\"progressbar\"/>"
    "  <object class=\"GtkRadioButton\" id=\"radiobutton\"/>"
    "  <object class=\"GtkSizeGroup\" id=\"sizegroup\"/>"
    "  <object class=\"GtkScrolledWindow\" id=\"scrolledwindow\"/>"
    "  <object class=\"GtkSpinButton\" id=\"spinbutton\"/>"
    "  <object class=\"GtkStatusbar\" id=\"statusbar\"/>"
    "  <object class=\"GtkTextView\" id=\"textview\"/>"
    "  <object class=\"GtkToggleAction\" id=\"toggleaction\"/>"
    "  <object class=\"GtkToggleButton\" id=\"togglebutton\"/>"
    "  <object class=\"GtkToolbar\" id=\"toolbar\"/>"
    "  <object class=\"GtkTreeStore\" id=\"treestore\"/>"
    "  <object class=\"GtkTreeView\" id=\"treeview\"/>"
    "  <object class=\"GtkTable\" id=\"table\"/>"
    "  <object class=\"GtkVBox\" id=\"vbox\"/>"
    "  <object class=\"GtkVButtonBox\" id=\"vbuttonbox\"/>"
    "  <object class=\"GtkVScrollbar\" id=\"vscrollbar\"/>"
    "  <object class=\"GtkVSeparator\" id=\"vseparator\"/>"
    "  <object class=\"GtkViewport\" id=\"viewport\"/>"
    "  <object class=\"GtkVRuler\" id=\"vruler\"/>"
    "  <object class=\"GtkVPaned\" id=\"vpaned\"/>"
    "  <object class=\"GtkVScale\" id=\"vscale\"/>"
    "  <object class=\"GtkWindow\" id=\"window\"/>"
    "  <object class=\"GtkUIManager\" id=\"uimanager\"/>"
    "</interface>";
  const gchar buffer2[] = 
    "<interface>"
    "  <object type-func=\"gtk_window_get_type\" id=\"window\"/>"
    "</interface>";
  const gchar buffer3[] = 
    "<interface>"
    "  <object type-func=\"xxx_invalid_get_type_function\" id=\"window\"/>"
    "</interface>";
  GtkBuilder *builder;
  GObject *window;
  GError *error;

  builder = builder_new_from_string (buffer, -1, NULL);
  gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "dialog")));
  gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "window")));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  window = gtk_builder_get_object (builder, "window");
  g_assert (window != NULL);
  g_assert (GTK_IS_WINDOW (window));
  gtk_widget_destroy (GTK_WIDGET (window));
  g_object_unref (builder);
  
  error = NULL;
  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, buffer3, -1, &error);
  g_assert (error != NULL);
  g_return_val_if_fail (error->domain == GTK_BUILDER_ERROR, FALSE);
  g_return_val_if_fail (error->code == GTK_BUILDER_ERROR_INVALID_TYPE_FUNCTION, FALSE);
  g_error_free (error);
  g_object_unref (builder);

  return TRUE;
}

gboolean test_spin_button (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "<object class=\"GtkAdjustment\" id=\"adjustment1\">"
    "<property name=\"lower\">0</property>"
    "<property name=\"upper\">10</property>"
    "<property name=\"step-increment\">2</property>"
    "<property name=\"page-increment\">3</property>"
    "<property name=\"page-size\">5</property>"
    "<property name=\"value\">1</property>"
    "</object>"
    "<object class=\"GtkSpinButton\" id=\"spinbutton1\">"
    "<property name=\"visible\">True</property>"
    "<property name=\"adjustment\">adjustment1</property>"
    "</object>"
    "</interface>";
  GObject *object;
  GtkAdjustment *adjustment;
  gdouble value;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  object = gtk_builder_get_object (builder, "spinbutton1");
  g_assert (GTK_IS_SPIN_BUTTON (object));
  adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (object));
  g_assert (GTK_IS_ADJUSTMENT (adjustment));
  g_object_get (adjustment, "value", &value, NULL);
  g_return_val_if_fail (value == 1, FALSE);
  g_object_get (adjustment, "lower", &value, NULL);
  g_return_val_if_fail (value == 0, FALSE);
  g_object_get (adjustment, "upper", &value, NULL);
  g_return_val_if_fail (value == 10, FALSE);
  g_object_get (adjustment, "step-increment", &value, NULL);
  g_return_val_if_fail (value == 2, FALSE);
  g_object_get (adjustment, "page-increment", &value, NULL);
  g_return_val_if_fail (value == 3, FALSE);
  g_object_get (adjustment, "page-size", &value, NULL);
  g_return_val_if_fail (value == 5, FALSE);
  
  g_object_unref (builder);
  return TRUE;
}

gboolean test_notebook (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkNotebook\" id=\"notebook1\">"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label1\">"
    "        <property name=\"label\">label1</property>"
    "      </object>"
    "    </child>"
    "    <child type=\"tab\">"
    "      <object class=\"GtkLabel\" id=\"tablabel1\">"
    "        <property name=\"label\">tab_label1</property>"
    "      </object>"
    "    </child>"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label2\">"
    "        <property name=\"label\">label2</property>"
    "      </object>"
    "    </child>"
    "    <child type=\"tab\">"
    "      <object class=\"GtkLabel\" id=\"tablabel2\">"
    "        <property name=\"label\">tab_label2</property>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *notebook;
  GtkWidget *label;

  builder = builder_new_from_string (buffer, -1, NULL);
  notebook = gtk_builder_get_object (builder, "notebook1");
  g_assert (notebook != NULL);
  g_return_val_if_fail (gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) == 2,
                        FALSE);

  label = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0);
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);
  g_return_val_if_fail (strcmp (gtk_label_get_label (GTK_LABEL (label)),
                                "label1") == 0, FALSE);
  label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (notebook), label);
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);
  g_return_val_if_fail (strcmp (gtk_label_get_label (GTK_LABEL (label)),
				"tab_label1") == 0, FALSE);

  label = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 1);
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);
  g_return_val_if_fail (strcmp (gtk_label_get_label (GTK_LABEL (label)),
                                "label2") == 0, FALSE);
  label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (notebook), label);
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);
  g_return_val_if_fail (strcmp (gtk_label_get_label (GTK_LABEL (label)),
				"tab_label2") == 0, FALSE);

  g_object_unref (builder);
  return TRUE;
}

gboolean test_construct_only_property (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <property name=\"type\">GTK_WINDOW_POPUP</property>"
    "  </object>"
    "</interface>";
  const gchar buffer2[] =
    "<interface>"
    "  <object class=\"GtkTextTagTable\" id=\"tagtable1\"/>"
    "  <object class=\"GtkTextBuffer\" id=\"textbuffer1\">"
    "    <property name=\"tag-table\">tagtable1</property>"
    "  </object>"
    "</interface>";
  GObject *widget, *tagtable, *textbuffer;
  GtkWindowType type;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  widget = gtk_builder_get_object (builder, "window1");
  g_object_get (widget, "type", &type, NULL);
  g_return_val_if_fail (type == GTK_WINDOW_POPUP, FALSE);

  gtk_widget_destroy (GTK_WIDGET (widget));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  textbuffer = gtk_builder_get_object (builder, "textbuffer1");
  g_return_val_if_fail (textbuffer != NULL, FALSE);
  g_object_get (textbuffer, "tag-table", &tagtable, NULL);
  g_return_val_if_fail (tagtable == gtk_builder_get_object (builder, "tagtable1"), FALSE);
  g_object_unref (tagtable);
  g_object_unref (builder);

  return TRUE;
}

gboolean test_object_properties (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkVBox\" id=\"vbox\">"
    "        <property name=\"border-width\">10</property>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label1\">"
    "            <property name=\"mnemonic-widget\">spinbutton1</property>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkSpinButton\" id=\"spinbutton1\"/>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *label, *spinbutton;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  label = gtk_builder_get_object (builder, "label1");
  g_return_val_if_fail (label != NULL, FALSE);
  spinbutton = gtk_builder_get_object (builder, "spinbutton1");
  g_return_val_if_fail (spinbutton != NULL, FALSE);
  g_return_val_if_fail (spinbutton == (GObject*)gtk_label_get_mnemonic_widget (GTK_LABEL (label)), FALSE);
  
  g_object_unref (builder);

  return TRUE;
}

gboolean test_children (void)
{
  GtkBuilder * builder;
  GList *children;
  const gchar buffer1[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkButton\" id=\"button1\">"
    "        <property name=\"label\">Hello</property>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  const gchar buffer2[] =
    "<interface>"
    "  <object class=\"GtkDialog\" id=\"dialog1\">"
    "    <child internal-child=\"vbox\">"
    "      <object class=\"GtkVBox\" id=\"dialog1-vbox\">"
    "        <property name=\"border-width\">10</property>"
    "          <child internal-child=\"action_area\">"
    "            <object class=\"GtkHButtonBox\" id=\"dialog1-action_area\">"
    "              <property name=\"border-width\">20</property>"
    "            </object>"
    "          </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";

  GObject *window, *button;
  GObject *dialog, *vbox, *action_area;
  
  builder = builder_new_from_string (buffer1, -1, NULL);
  window = gtk_builder_get_object (builder, "window1");
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  button = gtk_builder_get_object (builder, "button1");
  g_return_val_if_fail (button != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);
  g_return_val_if_fail (strcmp (GTK_WIDGET (GTK_WIDGET (button)->parent)->name, "window1") == 0, FALSE);

  gtk_widget_destroy (GTK_WIDGET (window));
  g_object_unref (builder);
  
  builder = builder_new_from_string (buffer2, -1, NULL);
  dialog = gtk_builder_get_object (builder, "dialog1");
  g_return_val_if_fail (dialog != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_DIALOG (dialog), FALSE);
  children = gtk_container_get_children (GTK_CONTAINER (dialog));
  g_return_val_if_fail (g_list_length (children) == 1, FALSE);
  g_list_free (children);
  
  vbox = gtk_builder_get_object (builder, "dialog1-vbox");
  g_return_val_if_fail (vbox != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_VBOX (vbox), FALSE);
  g_return_val_if_fail (GTK_WIDGET (vbox)->parent != NULL, FALSE);
  g_return_val_if_fail (strcmp (GTK_WIDGET (GTK_WIDGET (vbox)->parent)->name, "dialog1") == 0, FALSE);
  g_return_val_if_fail (GTK_CONTAINER (vbox)->border_width == 10, FALSE);
  g_return_val_if_fail (strcmp (GTK_WIDGET (GTK_DIALOG (dialog)->vbox)->name,
				"dialog1-vbox") == 0, FALSE);

  action_area = gtk_builder_get_object (builder, "dialog1-action_area");
  g_return_val_if_fail (action_area != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HBUTTON_BOX (action_area), FALSE);
  g_return_val_if_fail (GTK_WIDGET (action_area)->parent != NULL, FALSE);
  g_return_val_if_fail (GTK_CONTAINER (action_area)->border_width == 20, FALSE);
  g_return_val_if_fail (GTK_DIALOG (dialog)->action_area != NULL, FALSE);
  g_return_val_if_fail (GTK_WIDGET (GTK_DIALOG (dialog)->action_area)->name != NULL, FALSE);
  g_return_val_if_fail (strcmp (GTK_WIDGET (GTK_DIALOG (dialog)->action_area)->name,
				"dialog1-action_area") == 0, FALSE);
  gtk_widget_destroy (GTK_WIDGET (dialog));
  g_object_unref (builder);
  
  return TRUE;
}

gboolean test_child_properties (void)
{
  GtkBuilder * builder;
  const gchar buffer1[] =
    "<interface>"
    "  <object class=\"GtkVBox\" id=\"vbox1\">"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label1\"/>"
    "      <packing>"
    "        <property name=\"pack-type\">start</property>"
    "      </packing>"
    "    </child>"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label2\"/>"
    "      <packing>"
    "        <property name=\"pack-type\">end</property>"
    "      </packing>"
    "    </child>"
    "  </object>"
    "</interface>";

  GObject *label, *vbox;
  GtkPackType pack_type;
  
  builder = builder_new_from_string (buffer1, -1, NULL);
  vbox = gtk_builder_get_object (builder, "vbox1");
  g_return_val_if_fail (GTK_IS_VBOX (vbox), FALSE);

  label = gtk_builder_get_object (builder, "label1");
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);
  gtk_container_child_get (GTK_CONTAINER (vbox),
                           GTK_WIDGET (label),
                           "pack-type",
                           &pack_type,
                           NULL);
  g_return_val_if_fail (pack_type == GTK_PACK_START, FALSE);
  
  label = gtk_builder_get_object (builder, "label2");
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);
  gtk_container_child_get (GTK_CONTAINER (vbox),
                           GTK_WIDGET (label),
                           "pack-type",
                           &pack_type,
                           NULL);
  g_return_val_if_fail (pack_type == GTK_PACK_END, FALSE);

  g_object_unref (builder);
  
  return TRUE;
}

gboolean test_treeview_column (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "<object class=\"GtkListStore\" id=\"liststore1\">"
    "  <columns>"
    "    <column type=\"gchararray\"/>"
    "    <column type=\"guint\"/>"
    "  </columns>"
    "  <data>"
    "    <row>"
    "      <col id=\"0\">John</col>"
    "      <col id=\"1\">25</col>"
    "    </row>"
    "  </data>"
    "</object>"
    "<object class=\"GtkWindow\" id=\"window1\">"
    "  <child>"
    "    <object class=\"GtkTreeView\" id=\"treeview1\">"
    "      <property name=\"visible\">True</property>"
    "      <property name=\"model\">liststore1</property>"
    "      <child>"
    "        <object class=\"GtkTreeViewColumn\" id=\"column1\">"
    "          <property name=\"title\">Test</property>"
    "          <child>"
    "            <object class=\"GtkCellRendererText\" id=\"renderer1\"/>"
    "            <attributes>"
    "              <attribute name=\"text\">1</attribute>"
    "            </attributes>"
    "          </child>"
    "        </object>"
    "      </child>"
    "      <child>"
    "        <object class=\"GtkTreeViewColumn\" id=\"column2\">"
    "          <property name=\"title\">Number</property>"
    "          <child>"
    "            <object class=\"GtkCellRendererText\" id=\"renderer2\"/>"
    "            <attributes>"
    "              <attribute name=\"text\">0</attribute>"
    "            </attributes>"
    "          </child>"
    "        </object>"
    "      </child>"
    "    </object>"
    "  </child>"
    "</object>"
    "</interface>";
  GObject *window, *treeview;
  GtkTreeViewColumn *column;
  GList *renderers;
  GObject *renderer;
  gchar *text;

  builder = builder_new_from_string (buffer, -1, NULL);
  treeview = gtk_builder_get_object (builder, "treeview1");
  g_return_val_if_fail (treeview, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (treeview), FALSE);
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (treeview), 0);
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (column), FALSE);
  g_return_val_if_fail (strcmp (gtk_tree_view_column_get_title (column),
				"Test") == 0, FALSE);

  renderers = gtk_tree_view_column_get_cell_renderers (column);
  g_return_val_if_fail (g_list_length (renderers) == 1, FALSE);
  renderer = g_list_nth_data (renderers, 0);
  g_return_val_if_fail (renderer, FALSE);
  g_return_val_if_fail (GTK_IS_CELL_RENDERER_TEXT (renderer), FALSE);
  g_list_free (renderers);

  gtk_widget_realize (GTK_WIDGET (treeview));

  renderer = gtk_builder_get_object (builder, "renderer1");
  g_object_get (renderer, "text", &text, NULL);
  g_assert (text);
  g_return_val_if_fail (strcmp (text, "25") == 0, FALSE);
  g_free (text);
  
  renderer = gtk_builder_get_object (builder, "renderer2");
  g_object_get (renderer, "text", &text, NULL);
  g_assert (text);
  g_return_val_if_fail (strcmp (text, "John") == 0, FALSE);
  g_free (text);

  gtk_widget_unrealize (GTK_WIDGET (treeview));

  window = gtk_builder_get_object (builder, "window1");
  gtk_widget_destroy (GTK_WIDGET (window));
  
  g_object_unref (builder);
  return TRUE;
}

gboolean test_icon_view (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"GdkPixbuf\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\">test</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkIconView\" id=\"iconview1\">"
    "        <property name=\"model\">liststore1</property>"
    "        <property name=\"text-column\">0</property>"
    "        <property name=\"pixbuf-column\">1</property>"
    "        <property name=\"visible\">True</property>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer1\"/>"
    "          <attributes>"
    "            <attribute name=\"text\">0</attribute>"
    "          </attributes>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *window, *iconview, *renderer;
  gchar *text;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  iconview = gtk_builder_get_object (builder, "iconview1");
  g_return_val_if_fail (iconview, FALSE);
  g_return_val_if_fail (GTK_IS_ICON_VIEW (iconview), FALSE);

  gtk_widget_realize (GTK_WIDGET (iconview));

  renderer = gtk_builder_get_object (builder, "renderer1");
  g_object_get (renderer, "text", &text, NULL);
  g_assert (text);
  g_return_val_if_fail (strcmp (text, "test") == 0, FALSE);
  g_free (text);
    
  window = gtk_builder_get_object (builder, "window1");
  gtk_widget_destroy (GTK_WIDGET (window));
  g_object_unref (builder);
  return TRUE;
}

gboolean test_combo_box (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"guint\"/>"
    "      <column type=\"gchararray\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\">1</col>"
    "        <col id=\"1\">Foo</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"0\">2</col>"
    "        <col id=\"1\">Bar</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkComboBox\" id=\"combobox1\">"
    "        <property name=\"model\">liststore1</property>"
    "        <property name=\"visible\">True</property>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer1\"/>"
    "          <attributes>"
    "            <attribute name=\"text\">0</attribute>"
    "          </attributes>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer2\"/>"
    "          <attributes>"
    "            <attribute name=\"text\">1</attribute>"
    "          </attributes>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *window, *combobox, *renderer;
  gchar *text;

  builder = builder_new_from_string (buffer, -1, NULL);
  combobox = gtk_builder_get_object (builder, "combobox1");
  g_return_val_if_fail (combobox, FALSE);
  gtk_widget_realize (GTK_WIDGET (combobox));

  renderer = gtk_builder_get_object (builder, "renderer2");
  g_assert (renderer);
  g_object_get (renderer, "text", &text, NULL);
  g_assert (text);
  g_return_val_if_fail (strcmp (text, "Bar") == 0, FALSE);
  g_free (text);

  renderer = gtk_builder_get_object (builder, "renderer1");
  g_assert (renderer);
  g_object_get (renderer, "text", &text, NULL);
  g_assert (text);
  g_return_val_if_fail (strcmp (text, "2") == 0, FALSE);
  g_free (text);

  window = gtk_builder_get_object (builder, "window1");
  gtk_widget_destroy (GTK_WIDGET (window));

  g_object_unref (builder);
  return TRUE;
}

gboolean test_combo_box_entry (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"guint\"/>"
    "      <column type=\"gchararray\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\">1</col>"
    "        <col id=\"1\">Foo</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"0\">2</col>"
    "        <col id=\"1\">Bar</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkComboBoxEntry\" id=\"comboboxentry1\">"
    "        <property name=\"model\">liststore1</property>"
    "        <property name=\"visible\">True</property>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer1\"/>"
    "            <attributes>"
    "              <attribute name=\"text\">0</attribute>"
    "            </attributes>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer2\"/>"
    "            <attributes>"
    "              <attribute name=\"text\">1</attribute>"
    "            </attributes>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *window, *combobox, *renderer;
  gchar *text;

  builder = builder_new_from_string (buffer, -1, NULL);
  combobox = gtk_builder_get_object (builder, "comboboxentry1");
  g_return_val_if_fail (combobox, FALSE);

  renderer = gtk_builder_get_object (builder, "renderer2");
  g_assert (renderer);
  g_object_get (renderer, "text", &text, NULL);
  g_assert (text);
  g_return_val_if_fail (strcmp (text, "Bar") == 0, FALSE);
  g_free (text);

  renderer = gtk_builder_get_object (builder, "renderer1");
  g_assert (renderer);
  g_object_get (renderer, "text", &text, NULL);
  g_assert (text);
  g_return_val_if_fail (strcmp (text, "2") == 0, FALSE);
  g_free (text);

  window = gtk_builder_get_object (builder, "window1");
  gtk_widget_destroy (GTK_WIDGET (window));

  g_object_unref (builder);
  return TRUE;
}

gboolean test_cell_view (void)
{
  GtkBuilder *builder;
  gchar *buffer =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\">test</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkCellView\" id=\"cellview1\">"
    "        <property name=\"visible\">True</property>"
    "        <property name=\"model\">liststore1</property>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer1\"/>"
    "          <attributes>"
    "            <attribute name=\"text\">0</attribute>"
    "          </attributes>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *cellview;
  GObject *model, *window;
  GtkTreePath *path;
  GList *renderers;
  GObject *renderer;
  gchar *text;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  cellview = gtk_builder_get_object (builder, "cellview1");
  g_assert (builder);
  g_assert (cellview);
  g_return_val_if_fail (GTK_IS_CELL_VIEW (cellview), FALSE);
  g_object_get (cellview, "model", &model, NULL);
  g_assert (model);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), FALSE);
  g_object_unref (model);
  path = gtk_tree_path_new_first ();
  gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (cellview), path);
  
  renderers = gtk_cell_view_get_cell_renderers (GTK_CELL_VIEW (cellview));
  g_assert (renderers);
  g_return_val_if_fail (g_list_length (renderers) == 1, FALSE);
  
  gtk_widget_realize (GTK_WIDGET (cellview));

  renderer = g_list_nth_data (renderers, 0);
  g_list_free (renderers);
  g_assert (renderer);
  g_object_get (renderer, "text", &text, NULL);
  g_return_val_if_fail (strcmp (text, "test") == 0, FALSE);
  g_free (text);
  gtk_tree_path_free (path);

  window = gtk_builder_get_object (builder, "window1");
  gtk_widget_destroy (GTK_WIDGET (window));
  
  g_object_unref (builder);
  return TRUE;
}

gboolean test_dialog (void)
{
  GtkBuilder * builder;
  const gchar buffer1[] =
    "<interface>"
    "  <object class=\"GtkDialog\" id=\"dialog1\">"
    "    <child internal-child=\"vbox\">"
    "      <object class=\"GtkVBox\" id=\"dialog1-vbox\">"
    "          <child internal-child=\"action_area\">"
    "            <object class=\"GtkHButtonBox\" id=\"dialog1-action_area\">"
    "              <child>"
    "                <object class=\"GtkButton\" id=\"button_cancel\"/>"
    "              </child>"
    "              <child>"
    "                <object class=\"GtkButton\" id=\"button_ok\"/>"
    "              </child>"
    "            </object>"
    "          </child>"
    "      </object>"
    "    </child>"
    "    <action-widgets>"
    "      <action-widget response=\"3\">button_ok</action-widget>"
    "      <action-widget response=\"-5\">button_cancel</action-widget>"
    "    </action-widgets>"
    "  </object>"
    "</interface>";

  GObject *dialog1;
  GObject *button_ok;
  GObject *button_cancel;
  
  builder = builder_new_from_string (buffer1, -1, NULL);
  dialog1 = gtk_builder_get_object (builder, "dialog1");
  button_ok = gtk_builder_get_object (builder, "button_ok");
  g_return_val_if_fail (gtk_dialog_get_response_for_widget
                        (GTK_DIALOG (dialog1),
                         GTK_WIDGET (button_ok)) == 3, FALSE);
  button_cancel = gtk_builder_get_object (builder, "button_cancel");
  g_return_val_if_fail (gtk_dialog_get_response_for_widget
                        (GTK_DIALOG (dialog1),
                         GTK_WIDGET (button_cancel)) == -5, FALSE);
  
  gtk_widget_destroy (GTK_WIDGET (dialog1));
  g_object_unref (builder);
  
  return TRUE;
}

gboolean test_accelerators (void)
{
  GtkBuilder *builder;
  gchar *buffer =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkButton\" id=\"button1\">"
    "        <accelerator key=\"q\" modifiers=\"GDK_CONTROL_MASK\" signal=\"clicked\"/>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  gchar *buffer2 =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkTreeView\" id=\"treeview1\">"
    "        <signal name=\"cursor-changed\" handler=\"gtk_main_quit\"/>"
    "        <accelerator key=\"f\" modifiers=\"GDK_CONTROL_MASK\" signal=\"grab_focus\"/>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *window1;
  GSList *accel_groups;
  GObject *accel_group;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  window1 = gtk_builder_get_object (builder, "window1");
  g_assert (window1);
  g_assert (GTK_IS_WINDOW (window1));

  accel_groups = gtk_accel_groups_from_object (window1);
  g_return_val_if_fail (g_slist_length (accel_groups) == 1, FALSE);
  accel_group = g_slist_nth_data (accel_groups, 0);
  g_assert (accel_group);

  gtk_widget_destroy (GTK_WIDGET (window1));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  window1 = gtk_builder_get_object (builder, "window1");
  g_assert (window1);
  g_assert (GTK_IS_WINDOW (window1));

  accel_groups = gtk_accel_groups_from_object (window1);
  g_return_val_if_fail (g_slist_length (accel_groups) == 1, FALSE);
  accel_group = g_slist_nth_data (accel_groups, 0);
  g_assert (accel_group);

  gtk_widget_destroy (GTK_WIDGET (window1));
  g_object_unref (builder);
  return TRUE;
}

gboolean test_widget (void)
{
  gchar *buffer =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkButton\" id=\"button1\">"
    "         <property name=\"can-focus\">True</property>"
    "         <property name=\"has-focus\">True</property>"
    "      </object>"
    "    </child>"
    "  </object>"
   "</interface>";
  gchar *buffer2 =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkButton\" id=\"button1\">"
    "         <property name=\"can-default\">True</property>"
    "         <property name=\"has-default\">True</property>"
    "      </object>"
    "    </child>"
    "  </object>"
   "</interface>";
  gchar *buffer3 =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "     <accessibility>"
    "       <atkproperty name=\"AtkObject::accessible_name\" translatable=\"yes\">Contacts</atkproperty>"
    "       <atkrelation target=\"button1\" type=\"labelled-by\"/>"
    "     </accessibility>"
    "  </object>"
   "</interface>";
  GtkBuilder *builder;
  GObject *window1, *button1;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  button1 = gtk_builder_get_object (builder, "button1");

#if 0
  g_return_val_if_fail (GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (button1)), FALSE);
#endif
  window1 = gtk_builder_get_object (builder, "window1");
  gtk_widget_destroy (GTK_WIDGET (window1));
  
  g_object_unref (builder);
  
  builder = builder_new_from_string (buffer2, -1, NULL);
  button1 = gtk_builder_get_object (builder, "button1");

  g_return_val_if_fail (GTK_WIDGET_RECEIVES_DEFAULT (GTK_WIDGET (button1)), FALSE);
  
  window1 = gtk_builder_get_object (builder, "window1");
  gtk_widget_destroy (GTK_WIDGET (window1));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer3, -1, NULL);
  window1 = gtk_builder_get_object (builder, "window1");
  gtk_widget_destroy (GTK_WIDGET (window1));
  g_object_unref (builder);

  return TRUE;
}

gboolean test_window (void)
{
  gchar *buffer1 =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "     <property name=\"title\"></property>"
    "  </object>"
   "</interface>";
  gchar *buffer2 =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "  </object>"
   "</interface>";
  GtkBuilder *builder;
  GObject *window1;
  gchar *title;
  
  builder = builder_new_from_string (buffer1, -1, NULL);
  window1 = gtk_builder_get_object (builder, "window1");
  g_object_get (window1, "title", &title, NULL);
  g_return_val_if_fail (strcmp (title, "") == 0, FALSE);
  g_free (title);
  gtk_widget_destroy (GTK_WIDGET (window1));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  window1 = gtk_builder_get_object (builder, "window1");
  gtk_widget_destroy (GTK_WIDGET (window1));
  g_object_unref (builder);

  return TRUE;
}

static gboolean
test_value_from_string (void)
{
  GValue value = { 0 };
  GError *error = NULL;
  GtkBuilder *builder;

  builder = gtk_builder_new ();
  
  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_STRING, "test", &value, &error), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_STRING (&value), FALSE);
  g_return_val_if_fail (strcmp (g_value_get_string (&value), "test") == 0, FALSE);
  g_value_unset (&value);

  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "true", &value, &error), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_BOOLEAN (&value), FALSE);
  g_return_val_if_fail (g_value_get_boolean (&value) == TRUE, FALSE);
  g_value_unset (&value);

  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "false", &value, &error), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_BOOLEAN (&value), FALSE);
  g_return_val_if_fail (g_value_get_boolean (&value) == FALSE, FALSE);
  g_value_unset (&value);

  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "yes", &value, &error), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_BOOLEAN (&value), FALSE);
  g_return_val_if_fail (g_value_get_boolean (&value) == TRUE, FALSE);
  g_value_unset (&value);

  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "no", &value, &error), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_BOOLEAN (&value), FALSE);
  g_return_val_if_fail (g_value_get_boolean (&value) == FALSE, FALSE);
  g_value_unset (&value);

  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "0", &value, &error), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_BOOLEAN (&value), FALSE);
  g_return_val_if_fail (g_value_get_boolean (&value) == FALSE, FALSE);
  g_value_unset (&value);

  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "1", &value, &error), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_BOOLEAN (&value), FALSE);
  g_return_val_if_fail (g_value_get_boolean (&value) == TRUE, FALSE);
  g_value_unset (&value);

  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "tRuE", &value, &error), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_BOOLEAN (&value), FALSE);
  g_return_val_if_fail (g_value_get_boolean (&value) == TRUE, FALSE);
  g_value_unset (&value);
  
  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "blaurgh", &value, &error) == FALSE, FALSE);
  g_return_val_if_fail (error != NULL, FALSE);
  g_value_unset (&value);
  g_return_val_if_fail (error->domain == GTK_BUILDER_ERROR, FALSE);
  g_return_val_if_fail (error->code == GTK_BUILDER_ERROR_INVALID_VALUE, FALSE);
  g_error_free (error);
  error = NULL;

  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "yess", &value, &error) == FALSE, FALSE);
  g_value_unset (&value);
  g_return_val_if_fail (error->domain == GTK_BUILDER_ERROR, FALSE);
  g_return_val_if_fail (error->code == GTK_BUILDER_ERROR_INVALID_VALUE, FALSE);
  g_error_free (error);
  error = NULL;
  
  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "trueee", &value, &error) == FALSE, FALSE);
  g_value_unset (&value);
  g_return_val_if_fail (error->domain == GTK_BUILDER_ERROR, FALSE);
  g_return_val_if_fail (error->code == GTK_BUILDER_ERROR_INVALID_VALUE, FALSE);
  g_error_free (error);
  error = NULL;
  
  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "", &value, &error) == FALSE, FALSE);
  g_value_unset (&value);
  g_return_val_if_fail (error->domain == GTK_BUILDER_ERROR, FALSE);
  g_return_val_if_fail (error->code == GTK_BUILDER_ERROR_INVALID_VALUE, FALSE);
  g_error_free (error);
  error = NULL;
  
  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_INT, "12345", &value, &error), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_INT (&value), FALSE);
  g_return_val_if_fail (g_value_get_int (&value) == 12345, FALSE);
  g_value_unset (&value);

  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_LONG, "9912345", &value, &error), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_LONG (&value), FALSE);
  g_return_val_if_fail (g_value_get_long (&value) == 9912345, FALSE);
  g_value_unset (&value);

  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_UINT, "2345", &value, &error), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_UINT (&value), FALSE);
  g_return_val_if_fail (g_value_get_uint (&value) == 2345, FALSE);
  g_value_unset (&value);

  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_FLOAT, "1.454", &value, &error), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_FLOAT (&value), FALSE);
  g_return_val_if_fail (fabs (g_value_get_float (&value) - 1.454) < 0.00001, FALSE);
  g_value_unset (&value);

  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_FLOAT, "abc", &value, &error) == FALSE, FALSE);
  g_value_unset (&value);
  g_return_val_if_fail (error->domain == GTK_BUILDER_ERROR, FALSE);
  g_return_val_if_fail (error->code == GTK_BUILDER_ERROR_INVALID_VALUE, FALSE);
  g_error_free (error);
  error = NULL;

  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, G_TYPE_INT, "/-+,abc", &value, &error) == FALSE, FALSE);
  g_value_unset (&value);
  g_return_val_if_fail (error->domain == GTK_BUILDER_ERROR, FALSE);
  g_return_val_if_fail (error->code == GTK_BUILDER_ERROR_INVALID_VALUE, FALSE);
  g_error_free (error);
  error = NULL;

  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, GTK_TYPE_WINDOW_TYPE, "toplevel", &value, &error) == TRUE, FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_ENUM (&value), FALSE);
  g_return_val_if_fail (g_value_get_enum (&value) == GTK_WINDOW_TOPLEVEL, FALSE);
  g_value_unset (&value);

  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, GTK_TYPE_WINDOW_TYPE, "sliff", &value, &error) == FALSE, FALSE);
  g_value_unset (&value);
  g_return_val_if_fail (error->domain == GTK_BUILDER_ERROR, FALSE);
  g_return_val_if_fail (error->code == GTK_BUILDER_ERROR_INVALID_VALUE, FALSE);
  g_error_free (error);
  error = NULL;
  
  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, GTK_TYPE_WIDGET_FLAGS, "mapped", &value, &error) == TRUE, FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_FLAGS (&value), FALSE);
  g_return_val_if_fail (g_value_get_flags (&value) == GTK_MAPPED, FALSE);
  g_value_unset (&value);

  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, GTK_TYPE_WIDGET_FLAGS, "GTK_VISIBLE | GTK_REALIZED", &value, &error) == TRUE, FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_FLAGS (&value), FALSE);
  g_return_val_if_fail (g_value_get_flags (&value) == (GTK_VISIBLE | GTK_REALIZED), FALSE);
  g_value_unset (&value);
  
  g_return_val_if_fail (gtk_builder_value_from_string_type (builder, GTK_TYPE_WINDOW_TYPE, "foobar", &value, &error) == FALSE, FALSE);
  g_value_unset (&value);
  g_return_val_if_fail (error->domain == GTK_BUILDER_ERROR, FALSE);
  g_return_val_if_fail (error->code == GTK_BUILDER_ERROR_INVALID_VALUE, FALSE);
  g_error_free (error);
  error = NULL;
  
  g_object_unref (builder);
  
  return TRUE;
}

gboolean model_freed = FALSE;

static void model_weakref (gpointer data, GObject *model)
{
  model_freed = TRUE;
}

static gboolean
test_reference_counting (void)
{
  GtkBuilder *builder;
  const gchar buffer1[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\"/>"
    "  <object class=\"GtkListStore\" id=\"liststore2\"/>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkTreeView\" id=\"treeview1\">"
    "        <property name=\"model\">liststore1</property>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  const gchar buffer2[] =
    "<interface>"
    "  <object class=\"GtkVBox\" id=\"vbox1\">"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label1\"/>"
    "      <packing>"
    "        <property name=\"pack-type\">start</property>"
    "      </packing>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *window, *treeview, *model;
  
  builder = builder_new_from_string (buffer1, -1, NULL);
  window = gtk_builder_get_object (builder, "window1");
  treeview = gtk_builder_get_object (builder, "treeview1");
  model = gtk_builder_get_object (builder, "liststore1");
  g_object_unref (builder);

  g_object_weak_ref (model, (GWeakNotify)model_weakref, NULL);

  g_assert (model_freed == FALSE);
  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), NULL);
  g_assert (model_freed == TRUE);
  
  gtk_widget_destroy (GTK_WIDGET (window));

  builder = builder_new_from_string (buffer2, -1, NULL);
  g_object_unref (builder);
  
  return TRUE;
}

static void 
test_file (const gchar *filename)
{
  GtkBuilder *builder;
  GError *error = NULL;

  builder = gtk_builder_new ();

  if (!gtk_builder_add_from_file (builder, filename, &error))
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
    }

  g_object_unref (builder);
  builder = NULL;

}

int
main (int argc, char **argv)
{
  gtk_init (&argc, &argv);
  
  if (argc > 1) 
    {
      test_file (argv[1]);

      return 0;
    }

  g_print ("Testing parser\n");
  if (!test_parser ())
    g_error ("test_parser failed");

  g_print ("Testing types\n");
  if (!test_types ())
    g_error ("test_types failed");

  g_print ("Testing construct-only property\n");
  if (!test_construct_only_property ())
    g_error ("test_construct_only_property failed");
  
  g_print ("Testing children\n");
  if (!test_children ())
    g_error ("test_children failed");

  g_print ("Testing child properties\n");
  if (!test_child_properties ())
    g_error ("test_child_properties failed");

  g_print ("Testing object properties\n");
  if (!test_object_properties ())
    g_error ("test_object_properties failed");

  g_print ("Testing notebook\n");
  if (!test_notebook ())
    g_error ("test_notebook failed");

  g_print ("Testing domain\n");
  if (!test_domain ())
    g_error ("test_domain failed");

  g_print ("Testing signal autoconnect\n");
  if (!test_connect_signals ())
    g_error ("test_connect_signals failed");

  g_print ("Testing uimanager simple\n");
  if (!test_uimanager_simple ())
    g_error ("test_uimanager_simple failed");

  g_print ("Testing spin button\n");
  if (!test_spin_button ())
    g_error ("test_spin_button failed");

  g_print ("Testing sizegroup\n");
  if (!test_sizegroup ())
    g_error ("test_sizegroup failed");
  
  g_print ("Testing list store\n");
  if (!test_list_store ())
    g_error ("test_list_store failed");
  
  g_print ("Testing tree store\n");
  if (!test_tree_store ())
    g_error ("test_tree_store failed");

  g_print ("Testing treeview column\n");
  if (!test_treeview_column ())
    g_error ("test_treeview_column failed");

  g_print ("Testing iconview\n");
  if (!test_icon_view ())
    g_error ("test_icon_view failed");

  g_print ("Testing combobox\n");
  if (!test_combo_box ())
    g_error ("test_combo_box failed");

  g_print ("Testing combobox entry\n");
  if (!test_combo_box_entry ())
    g_error ("test_combo_box_entry failed");

  g_print ("Testing cell view\n");
  if (!test_cell_view ())
    g_error ("test_cell_view failed");
  
  g_print ("Testing dialog\n");
  if (!test_dialog ())
    g_error ("test_dialog failed");

  g_print ("Testing accelerators\n");
  if (!test_accelerators ())
    g_error ("test_accelerators failed");

  g_print ("Testing widget\n");
  if (!test_widget ())
    g_error ("test_widget failed");

  g_print ("Testing value from string\n");
  if (!test_value_from_string ())
    g_error ("test_value_from_string failed");

  g_print ("Testing reference counting\n");
  if (!test_reference_counting ())
    g_error ("test_reference_counting failed");

  g_print ("Testing window\n");
  if (!test_window ())
    g_error ("test_window failed");

  return 0;
}
