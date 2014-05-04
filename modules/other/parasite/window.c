/*
 * Copyright (c) 2008-2009  Christian Hammond
 * Copyright (c) 2008-2009  David Trowbridge
 * Copyright (c) 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include "parasite.h"
#include "prop-list.h"
#include "classes-list.h"
#include "css-editor.h"
#include "object-hierarchy.h"
#include "widget-tree.h"
#include "python-hooks.h"
#include "python-shell.h"
#include "button-path.h"
#include "themes.h"

static void
on_widget_tree_selection_changed (ParasiteWidgetTree *widget_tree,
                                  ParasiteWindow     *parasite)
{
  GObject *selected = parasite_widget_tree_get_selected_object (widget_tree);

  if (selected != NULL)
    {
      if (!parasite_proplist_set_object (PARASITE_PROPLIST (parasite->prop_list), selected))
        return;

      parasite_proplist_set_object (PARASITE_PROPLIST (parasite->child_prop_list), selected);
      parasite_object_hierarchy_set_object (PARASITE_OBJECT_HIERARCHY (parasite->oh), selected);

      if (GTK_IS_WIDGET (selected))
        {
          GtkWidget *widget = GTK_WIDGET (selected);

          gtkparasite_flash_widget(parasite, widget);
          parasite_button_path_set_widget (PARASITE_BUTTON_PATH (parasite->button_path), widget);
          parasite_classeslist_set_widget (PARASITE_CLASSESLIST (parasite->classes_list), widget);
          parasite_css_editor_set_widget (PARASITE_CSS_EDITOR (parasite->widget_css_editor), widget);
        }
      else
        {
          gtk_widget_set_sensitive (parasite->classes_list, FALSE);
          gtk_widget_set_sensitive (parasite->widget_css_editor, FALSE);
        }
    }
}


static gboolean
on_widget_tree_button_press(ParasiteWidgetTree *widget_tree,
                            GdkEventButton *event,
                            ParasiteWindow *parasite)
{
    if (event->button == 3)
    {
        gtk_menu_popup(GTK_MENU(parasite->widget_popup), NULL, NULL,
                       NULL, NULL, event->button, event->time);
    }

    return FALSE;
}


static void
on_send_widget_to_shell_activate(GtkWidget *menuitem,
                                 ParasiteWindow *parasite)
{
  char *str;
  GObject *object;

  object = parasite_widget_tree_get_selected_object (PARASITE_WIDGET_TREE (parasite->widget_tree));

  if (!object)
    return;

  str = g_strdup_printf ("parasite.gobj(%p)", object);
  parasite_python_shell_append_text (PARASITE_PYTHON_SHELL (parasite->python_shell),
                                     str,
                                     NULL);

  g_free(str);
  parasite_python_shell_focus (PARASITE_PYTHON_SHELL (parasite->python_shell));
}


static GtkWidget *
create_widget_list_pane(ParasiteWindow *parasite)
{
    GtkWidget *swin;

    swin = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                        "hscrollbar-policy", GTK_POLICY_AUTOMATIC,
                        "vscrollbar-policy", GTK_POLICY_ALWAYS,
                        "shadow-type", GTK_SHADOW_IN,
                        "width-request", 250,
                        "expand", TRUE,
                        NULL);

    parasite->widget_tree = parasite_widget_tree_new();
    gtk_container_add(GTK_CONTAINER(swin), parasite->widget_tree);

    g_signal_connect(G_OBJECT(parasite->widget_tree),
                     "widget-changed",
                     G_CALLBACK(on_widget_tree_selection_changed),
                     parasite);

    if (parasite_python_is_enabled())
    {
        g_signal_connect(G_OBJECT(parasite->widget_tree),
                         "button-press-event",
                         G_CALLBACK(on_widget_tree_button_press),
                         parasite);
    }

    return swin;
}

static GtkWidget *
create_prop_list_pane (ParasiteWindow *parasite,
                       gboolean        child_properties)
{
    GtkWidget *swin;
    GtkWidget *pl;

    swin = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                        "hscrollbar-policy", GTK_POLICY_AUTOMATIC,
                        "vscrollbar-policy", GTK_POLICY_ALWAYS,
                        "shadow-type", GTK_SHADOW_IN,
                        "width-request", 250,
                        NULL);

    pl = parasite_proplist_new (parasite->widget_tree, child_properties);
    gtk_container_add (GTK_CONTAINER (swin), pl);

    if (child_properties)
      parasite->child_prop_list = pl;
    else
      parasite->prop_list = pl;

    return swin;
}

static void
on_show_graphic_updates_toggled(GtkWidget *toggle_button,
                                ParasiteWindow *parasite)
{
    gdk_window_set_debug_updates(
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button)));
}

static GtkWidget *
create_toolbar (ParasiteWindow *window) {
  GtkWidget *button;
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkStyleContext *context = gtk_widget_get_style_context (box);
  GtkWidget *image;

  gtk_style_context_add_class (context, "linked");

  button = gtkparasite_inspect_button_new (window);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_toggle_button_new ();
  image = gtk_image_new_from_icon_name ("view-refresh", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);
  gtk_widget_set_tooltip_text (button, "Show Graphic Updates");
  gtk_container_add (GTK_CONTAINER (box), button);
  g_signal_connect (button,
                    "toggled",
                    G_CALLBACK (on_show_graphic_updates_toggled),
                    window);

  gtk_widget_show_all (box);
  return box;
}

static void
delete_window (GtkWidget *widget) {
  GApplication *app = g_application_get_default ();

  gtk_widget_hide (widget);

  if (app)
    g_application_quit (app);
  else
    exit (0);
}

void
gtkparasite_window_create()
{
    ParasiteWindow *window;
    GtkWidget *vpaned, *hpaned;
    GtkWidget *header;
    GtkWidget *box;
    GtkWidget *nb;
    char *title;
    GtkWindowGroup *group;

    window = g_new0(ParasiteWindow, 1);

    /*
     * Create the top-level window.
     */
    window->window = g_object_new (GTK_TYPE_WINDOW,
                                   "default-height", 500,
                                   "default-width", 1000,
                                   NULL);
    g_signal_connect (window->window,
                      "delete-event",
                      G_CALLBACK (delete_window),
                      NULL);

    group = gtk_window_group_new ();
    gtk_window_group_add_window (group, GTK_WINDOW (window->window));

    title = g_strdup_printf("Parasite - %s", g_get_application_name());
    gtk_window_set_title (GTK_WINDOW (window->window), title);

    header = gtk_header_bar_new ();
    gtk_header_bar_set_title (GTK_HEADER_BAR (header), title);
    gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (header), TRUE);
    gtk_window_set_titlebar (GTK_WINDOW (window->window), header);
    gtk_header_bar_pack_start (GTK_HEADER_BAR (header), create_toolbar (window));

    g_free(title);

    nb = g_object_new (GTK_TYPE_NOTEBOOK,
                       "show-border", FALSE,
                       "margin-left", 6,
                       "margin-right", 6,
                       "margin-bottom", 6,
                       NULL);
    gtk_container_add (GTK_CONTAINER (window->window), nb);

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_notebook_append_page (GTK_NOTEBOOK (nb),
                              box,
                              gtk_label_new ("Widget Tree"));

    gtk_notebook_append_page (GTK_NOTEBOOK (nb),
                              parasite_themes_new (),
                              gtk_label_new ("Themes"));

    gtk_notebook_append_page (GTK_NOTEBOOK (nb),
                              parasite_css_editor_new (TRUE),
                              gtk_label_new ("Custom CSS"));

    window->button_path = parasite_button_path_new ();
    gtk_container_add (GTK_CONTAINER (box), window->button_path);

    hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_container_add (GTK_CONTAINER (box), hpaned);

    vpaned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
    gtk_paned_pack1 (GTK_PANED (hpaned), vpaned, TRUE, TRUE);
    gtk_paned_pack1 (GTK_PANED (vpaned), create_widget_list_pane (window), TRUE, FALSE);

    nb = g_object_new (GTK_TYPE_NOTEBOOK,
                       "enable-popup", TRUE,
                       "show-border", FALSE,
                       NULL);
    gtk_notebook_append_page (GTK_NOTEBOOK (nb),
                              create_prop_list_pane (window, FALSE),
                              gtk_label_new ("Properties"));

    gtk_notebook_append_page (GTK_NOTEBOOK (nb),
                              create_prop_list_pane (window, TRUE),
                              gtk_label_new ("Child Properties"));

    window->oh = parasite_object_hierarchy_new ();
    gtk_notebook_append_page (GTK_NOTEBOOK (nb),
                              window->oh,
                              gtk_label_new ("Hierarchy"));

    window->classes_list = parasite_classeslist_new ();
    gtk_notebook_append_page (GTK_NOTEBOOK (nb),
                              window->classes_list,
                              gtk_label_new ("CSS Classes"));

    window->widget_css_editor = parasite_css_editor_new (FALSE);
    gtk_notebook_append_page (GTK_NOTEBOOK (nb),
                              window->widget_css_editor,
                              gtk_label_new ("Custom CSS"));

    gtk_paned_pack2 (GTK_PANED (hpaned), nb, TRUE, TRUE);

    if (parasite_python_is_enabled())
    {
        GtkWidget *menuitem;

        window->python_shell = parasite_python_shell_new();
        gtk_paned_pack2(GTK_PANED(vpaned), window->python_shell, FALSE, FALSE);

        /*
         * XXX Eventually we'll want to put more in here besides the menu
         *     item we define below. At that point, we'll need to make this
         *     more generic.
         */
        window->widget_popup = gtk_menu_new();
        gtk_widget_show(window->widget_popup);

        menuitem = gtk_menu_item_new_with_label("Send Widget to Shell");
        gtk_widget_show(menuitem);
        gtk_menu_shell_append(GTK_MENU_SHELL(window->widget_popup), menuitem);

        g_signal_connect(G_OBJECT(menuitem), "activate",
                         G_CALLBACK(on_send_widget_to_shell_activate), window);
    }

    gtk_widget_show_all (window->window);
}

// vim: set et sw=4 ts=4:
