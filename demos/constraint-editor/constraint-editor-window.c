/*
 * Copyright © 2019 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen
 */

#include "config.h"

#include "constraint-editor-window.h"
#include "constraint-view.h"
#include "constraint-editor.h"
#include "guide-editor.h"

struct _ConstraintEditorWindow
{
  GtkApplicationWindow parent_instance;

  GtkWidget *paned;
  GtkWidget *view;
  GtkWidget *list;
};

G_DEFINE_TYPE(ConstraintEditorWindow, constraint_editor_window, GTK_TYPE_APPLICATION_WINDOW);

gboolean
constraint_editor_window_load (ConstraintEditorWindow *self,
                               GFile            *file)
{
  GBytes *bytes;

  bytes = g_file_load_bytes (file, NULL, NULL, NULL);
  if (bytes == NULL)
    return FALSE;

  if (!g_utf8_validate (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), NULL))
    {
      g_bytes_unref (bytes);
      return FALSE;
    }

#if 0

  gtk_text_buffer_get_end_iter (self->text_buffer, &end);
  gtk_text_buffer_insert (self->text_buffer,
                          &end,
                          g_bytes_get_data (bytes, NULL),
                          g_bytes_get_size (bytes));
#endif

  g_bytes_unref (bytes);

  return TRUE;
}

static void
open_response_cb (GtkNativeDialog        *dialog,
                  gint                    response,
                  ConstraintEditorWindow *self)
{
  gtk_native_dialog_hide (dialog);

  if (response == GTK_RESPONSE_ACCEPT)
    {
      GFile *file;

      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
      constraint_editor_window_load (self, file);
      g_object_unref (file);
    }

  gtk_native_dialog_destroy (dialog);
}

static void
open_cb (GtkWidget              *button,
         ConstraintEditorWindow *self)
{
  GtkFileChooserNative *dialog;

  dialog = gtk_file_chooser_native_new ("Open file",
                                        GTK_WINDOW (self),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        "_Load",
                                        "_Cancel");

  gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (dialog), TRUE);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), ".");
  g_signal_connect (dialog, "response", G_CALLBACK (open_response_cb), self);
  gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));
}

static void
serialize_child (GString   *str,
                 int        indent,
                 GtkWidget *child)
{
  const char *name;

  name = gtk_widget_get_name (child);
  g_string_append_printf (str, "%*s<child>\n", indent, "");
  g_string_append_printf (str, "%*s  <object class=\"GtkLabel\" id=\"%s\">\n", indent, "", name);
  g_string_append_printf (str, "%*s    <property name=\"label\">%s</property>\n", indent, "", name);
  g_string_append_printf (str, "%*s  </object>\n", indent, "");
  g_string_append_printf (str, "%*s</child>\n", indent, "");
}

static char *
get_current_text (ConstraintEditorWindow *self)
{
  GString *str = g_string_new ("");
  GtkLayoutManager *manager;
  GListModel *list;
  int i;

  list = constraint_view_get_model (CONSTRAINT_VIEW (self->view));

  g_string_append (str, "<interface>\n");
  g_string_append (str, "  <object class=\"GtkBox\" id=\"view\">\n");
  g_string_append (str, "    <property name=\"layout-manager\">\n");
  g_string_append (str, "      <object class=\"GtkConstraintLayout\">\n");
  g_string_append (str, "        <constraints>\n");
  for (i = 0; i < g_list_model_get_n_items (list); i++)
    {
      gpointer item = g_list_model_get_item (list, i);
      g_object_unref (item);
      if (GTK_IS_CONSTRAINT (item))
        constraint_editor_serialize_constraint (str, 10, GTK_CONSTRAINT (item));
      else if (GTK_IS_CONSTRAINT_GUIDE (item))
        guide_editor_serialize_guide (str, 10, GTK_CONSTRAINT_GUIDE (item));
    }
  g_string_append (str, "        </constraints>\n");
  g_string_append (str, "      </object>\n");
  g_string_append (str, "    </property>\n");
  for (i = 0; i < g_list_model_get_n_items (list); i++)
    {
      gpointer item = g_list_model_get_item (list, i);
      g_object_unref (item);
      if (GTK_IS_WIDGET (item))
        serialize_child (str, 4, GTK_WIDGET (item));
    }
  g_string_append (str, "  </object>\n");
  g_string_append (str, "</interface>\n");

  return g_string_free (str, FALSE);
}


static void
save_response_cb (GtkNativeDialog        *dialog,
                  gint                    response,
                  ConstraintEditorWindow *self)
{
  gtk_native_dialog_hide (dialog);

  if (response == GTK_RESPONSE_ACCEPT)
    {
      char *text, *filename;
      GError *error = NULL;

      text = get_current_text (self);

      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      if (!g_file_set_contents (filename, text, -1, &error))
        {
          GtkWidget *dialog;

          dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
                                           GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_INFO,
                                           GTK_BUTTONS_OK,
                                           "Saving failed");
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                    "%s", error->message);
          g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
          gtk_widget_show (dialog);
          g_error_free (error);
        }
      g_free (filename);
    }

  gtk_native_dialog_destroy (dialog);
}

static void
save_cb (GtkWidget              *button,
         ConstraintEditorWindow *self)
{
  GtkFileChooserNative *dialog;

  dialog = gtk_file_chooser_native_new ("Save constraints",
                                        GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (button))),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        "_Save",
                                        "_Cancel");

  gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (dialog), TRUE);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), ".");
  g_signal_connect (dialog, "response", G_CALLBACK (save_response_cb), self);
  gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));
}

static void
constraint_editor_window_finalize (GObject *object)
{
  //ConstraintEditorWindow *self = (ConstraintEditorWindow *)object;

  G_OBJECT_CLASS (constraint_editor_window_parent_class)->finalize (object);
}

static int child_counter;
static int guide_counter;

static void
add_child (ConstraintEditorWindow *win)
{
  char *name;

  child_counter++;
  name = g_strdup_printf ("Child %d", child_counter);
  constraint_view_add_child (CONSTRAINT_VIEW (win->view), name);
  g_free (name);
}

static void
add_guide (ConstraintEditorWindow *win)
{
  char *name;
  GtkConstraintGuide *guide;

  guide_counter++;
  name = g_strdup_printf ("Guide %d", guide_counter);
  guide = gtk_constraint_guide_new ();
  gtk_constraint_guide_set_name (guide, name);
  g_free (name);

  constraint_view_add_guide (CONSTRAINT_VIEW (win->view), guide);
}

static void
constraint_editor_done (ConstraintEditor *editor,
                        GtkConstraint    *constraint,
                        ConstraintEditorWindow *win)
{
  GtkConstraint *old_constraint;

  g_object_get (editor, "constraint", &old_constraint, NULL);

  if (old_constraint)
    constraint_view_remove_constraint (CONSTRAINT_VIEW (win->view), old_constraint);

  constraint_view_add_constraint (CONSTRAINT_VIEW (win->view), constraint);

  g_clear_object (&old_constraint);

  gtk_widget_destroy (gtk_widget_get_ancestor (GTK_WIDGET (editor), GTK_TYPE_WINDOW));
}

static void
edit_constraint (ConstraintEditorWindow *win,
                 GtkConstraint          *constraint)
{
  GtkWidget *window;
  ConstraintEditor *editor;
  GListModel *model;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (win));
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  if (constraint)
    gtk_window_set_title (GTK_WINDOW (window), "Edit Constraint");
  else
    gtk_window_set_title (GTK_WINDOW (window), "Create Constraint");

  model = constraint_view_get_model (CONSTRAINT_VIEW (win->view));

  editor = constraint_editor_new (model, constraint);

  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (editor));

  g_signal_connect (editor, "done", G_CALLBACK (constraint_editor_done), win);

  gtk_widget_show (window);
}

static void
add_constraint (ConstraintEditorWindow *win)
{
  edit_constraint (win, NULL);
}

static void
guide_editor_done (GuideEditor            *editor,
                   GtkConstraintGuide     *guide,
                   ConstraintEditorWindow *win)
{
  gtk_widget_destroy (gtk_widget_get_ancestor (GTK_WIDGET (editor), GTK_TYPE_WINDOW));
}

static void
edit_guide (ConstraintEditorWindow *win,
            GtkConstraintGuide     *guide)
{
  GtkWidget *window;
  GuideEditor *editor;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (win));
  gtk_window_set_title (GTK_WINDOW (window), "Edit Guide");

  editor = guide_editor_new (guide);
  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (editor));

  g_signal_connect (editor, "done", G_CALLBACK (guide_editor_done), win);
  gtk_widget_show (window);
}

static void
row_activated (GtkListBox *list,
               GtkListBoxRow *row,
               ConstraintEditorWindow *win)
{
  GObject *item;

  item = G_OBJECT (g_object_get_data (G_OBJECT (row), "item"));

  if (GTK_IS_CONSTRAINT (item))
    edit_constraint (win, GTK_CONSTRAINT (item));
  else if (GTK_IS_CONSTRAINT_GUIDE (item))
    edit_guide (win, GTK_CONSTRAINT_GUIDE (item));
}

static void
constraint_editor_window_class_init (ConstraintEditorWindowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  g_type_ensure (CONSTRAINT_VIEW_TYPE);

  object_class->finalize = constraint_editor_window_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/gtk4/constraint-editor/constraint-editor-window.ui");

  gtk_widget_class_bind_template_child (widget_class, ConstraintEditorWindow, paned);
  gtk_widget_class_bind_template_child (widget_class, ConstraintEditorWindow, view);
  gtk_widget_class_bind_template_child (widget_class, ConstraintEditorWindow, list);

  gtk_widget_class_bind_template_callback (widget_class, open_cb);
  gtk_widget_class_bind_template_callback (widget_class, save_cb);
  gtk_widget_class_bind_template_callback (widget_class, add_child);
  gtk_widget_class_bind_template_callback (widget_class, add_guide);
  gtk_widget_class_bind_template_callback (widget_class, add_constraint);
  gtk_widget_class_bind_template_callback (widget_class, row_activated);
}

static void
row_edit (GtkButton *button,
          ConstraintEditorWindow *win)
{
  GtkWidget *row;
  GObject *item;

  row = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_LIST_BOX_ROW);
  item = (GObject *)g_object_get_data (G_OBJECT (row), "item");
  if (GTK_IS_CONSTRAINT (item))
    edit_constraint (win, GTK_CONSTRAINT (item));
  else if (GTK_IS_CONSTRAINT_GUIDE (item))
    edit_guide (win, GTK_CONSTRAINT_GUIDE (item));
}

static void
mark_constraints_invalid (ConstraintEditorWindow *win,
                          gpointer                removed)
{
  GtkWidget *child;
  GObject *item;

  for (child = gtk_widget_get_first_child (win->list);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      item = (GObject *)g_object_get_data (G_OBJECT (child), "item");
      if (GTK_IS_CONSTRAINT (item))
        {
          GtkConstraint *constraint = GTK_CONSTRAINT (item);

          if (gtk_constraint_get_target (constraint) == (GtkConstraintTarget *)removed ||
              gtk_constraint_get_source (constraint) == (GtkConstraintTarget *)removed)
            {
              GtkWidget *button;
              button = (GtkWidget *)g_object_get_data (G_OBJECT (child), "edit");
              gtk_button_set_icon_name (GTK_BUTTON (button), "dialog-warning-symbolic");
              gtk_widget_set_tooltip_text (button, "Constraint is invalid");
            }
        }
    }
}

static void
row_delete (GtkButton *button,
            ConstraintEditorWindow *win)
{
  GtkWidget *row;
  GObject *item;

  row = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_LIST_BOX_ROW);
  item = (GObject *)g_object_get_data (G_OBJECT (row), "item");
  if (GTK_IS_CONSTRAINT (item))
    constraint_view_remove_constraint (CONSTRAINT_VIEW (win->view),
                                       GTK_CONSTRAINT (item));
  else if (GTK_IS_CONSTRAINT_GUIDE (item))
    {
      mark_constraints_invalid (win, item);
      constraint_view_remove_guide (CONSTRAINT_VIEW (win->view),
                                    GTK_CONSTRAINT_GUIDE (item));
    }
  else if (GTK_IS_WIDGET (item))
    {
      mark_constraints_invalid (win, item);
      constraint_view_remove_child (CONSTRAINT_VIEW (win->view),
                                    GTK_WIDGET (item));
    }
}

static GtkWidget *
create_widget_func (gpointer item,
                    gpointer user_data)
{
  ConstraintEditorWindow *win = user_data;
  const char *name;
  GtkWidget *row, *box, *label, *button;

  if (GTK_IS_WIDGET (item))
    name = gtk_widget_get_name (GTK_WIDGET (item));
  else if (GTK_IS_CONSTRAINT_GUIDE (item))
    name = gtk_constraint_guide_get_name (GTK_CONSTRAINT_GUIDE (item));
  else
    name = (const char *)g_object_get_data (G_OBJECT (item), "name");

  row = gtk_list_box_row_new ();
  g_object_set_data_full (G_OBJECT (row), "item", g_object_ref (item), g_object_unref);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  label = gtk_label_new (name);
  if (GTK_IS_WIDGET (item) || GTK_IS_CONSTRAINT_GUIDE (item))
    g_object_bind_property (item, "name",
                            label, "label",
                            G_BINDING_DEFAULT);
  g_object_set (label, "margin", 10, NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_container_add (GTK_CONTAINER (box), label);

  if (GTK_IS_CONSTRAINT (item) || GTK_IS_CONSTRAINT_GUIDE (item))
    {
      button = gtk_button_new_from_icon_name ("document-edit-symbolic");
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
      g_signal_connect (button, "clicked", G_CALLBACK (row_edit), win);
      g_object_set_data (G_OBJECT (row), "edit", button);
      gtk_container_add (GTK_CONTAINER (box), button);
      button = gtk_button_new_from_icon_name ("edit-delete-symbolic");
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
      g_signal_connect (button, "clicked", G_CALLBACK (row_delete), win);
      gtk_container_add (GTK_CONTAINER (box), button);
    }
  else if (GTK_IS_WIDGET (item))
    {
      button = gtk_button_new_from_icon_name ("edit-delete-symbolic");
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
      g_signal_connect (button, "clicked", G_CALLBACK (row_delete), win);
      gtk_container_add (GTK_CONTAINER (box), button);
    }

  return row;
}

static void
constraint_editor_window_init (ConstraintEditorWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_bind_model (GTK_LIST_BOX (self->list),
                           constraint_view_get_model (CONSTRAINT_VIEW (self->view)),
                           create_widget_func,
                           self,
                           NULL);
}

ConstraintEditorWindow *
constraint_editor_window_new (ConstraintEditorApplication *application)
{
  return g_object_new (CONSTRAINT_EDITOR_WINDOW_TYPE,
                       "application", application,
                       NULL);
}
