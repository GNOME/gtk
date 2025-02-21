/* testlockbutton.c
 * Copyright (C) 2011 Red Hat, Inc.
 * Authors: Matthias Clasen
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>
#include <gio/gio.h>

/* This is used to take screenshots of GtkLockButton for the docs.
 *
 * Run it like: testlockbutton lockbutton.ui style.css
 *
 * with the ui and css from the images directory for the docs.
 */

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/* a fake permission implementation */

#define G_TYPE_TEST_PERMISSION      (g_test_permission_get_type ())
#define G_TEST_PERMISSION(inst)     (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
                                     G_TYPE_TEST_PERMISSION,             \
                                     GTestPermission))
#define G_IS_TEST_PERMISSION(inst)  (G_TYPE_CHECK_INSTANCE_TYPE ((inst), \
                                     G_TYPE_TEST_PERMISSION))

typedef struct _GTestPermission GTestPermission;
typedef struct _GTestPermissionClass GTestPermissionClass;

struct _GTestPermission
{
  GPermission parent;

  gboolean success;
};

struct _GTestPermissionClass
{
  GPermissionClass parent_class;
};

static GType g_test_permission_get_type (void);
G_DEFINE_TYPE (GTestPermission, g_test_permission, G_TYPE_PERMISSION)

static void
g_test_permission_init (GTestPermission *test)
{
}

static gboolean
update_allowed (GTestPermission  *test,
                gboolean          allowed,
                GError          **error)
{
  gboolean can_acquire, can_release;

  g_object_get (test,
                "can-acquire", &can_acquire,
                "can-release", &can_release,
                NULL);

  if (test->success)
    {
      g_permission_impl_update (G_PERMISSION (test),
                                allowed, can_acquire, can_release);
      return TRUE;
    }
  else
    {
      g_set_error_literal (error,
                           G_IO_ERROR, G_IO_ERROR_FAILED, "Sorry, no luck");
      return FALSE;
    }
}

static gboolean
acquire (GPermission   *permission,
         GCancellable  *cancellable,
         GError       **error)
{
  GTestPermission *test = G_TEST_PERMISSION (permission);
  return update_allowed (test, TRUE, error);
}

static void
acquire_async (GPermission         *permission,
               GCancellable        *cancellable,
               GAsyncReadyCallback  callback,
               gpointer             user_data)
{
  GTask *result;
  g_print ("GTestPermission::acquire_async\n");
  result = g_task_new ((GObject*)permission,
                       cancellable,
                       callback,
                       user_data);
  g_task_return_boolean (result, TRUE);
  g_object_unref (result);
}

static gboolean
acquire_finish (GPermission   *permission,
                GAsyncResult  *result,
                GError       **error)
{
  GTestPermission *test = G_TEST_PERMISSION (permission);
  g_print ("GTestPermission::acquire_finish\n");
  return update_allowed (test, TRUE, error);
}

static gboolean
release (GPermission   *permission,
         GCancellable  *cancellable,
         GError       **error)
{
  GTestPermission *test = G_TEST_PERMISSION (permission);
  return update_allowed (test, FALSE, error);
}

static void
release_async (GPermission         *permission,
               GCancellable        *cancellable,
               GAsyncReadyCallback  callback,
               gpointer             user_data)
{
  GTask *result;
  result = g_task_new ((GObject*)permission,
                       cancellable,
                       callback,
                       user_data);
  g_task_return_boolean (result, TRUE);
  g_object_unref (result);
}

static gboolean
release_finish (GPermission   *permission,
                GAsyncResult  *result,
                GError       **error)
{
  GTestPermission *test = G_TEST_PERMISSION (permission);
  return update_allowed (test, FALSE, error);
}

static void
g_test_permission_class_init (GTestPermissionClass *class)
{
  GPermissionClass *permission_class = G_PERMISSION_CLASS (class);

  permission_class->acquire = acquire;
  permission_class->acquire_async = acquire_async;
  permission_class->acquire_finish = acquire_finish;

  permission_class->release = release;
  permission_class->release_async = release_async;
  permission_class->release_finish = release_finish;
}

static void
g_test_permission_set_success (GTestPermission *permission,
                               gboolean         success)
{
  permission->success = success;
}

static GtkWidget *allowed_button;
static GtkWidget *can_acquire_button;
static GtkWidget *can_release_button;
static GtkWidget *success_button;

static void
update_clicked (GtkButton *button, GtkLockButton *lockbutton)
{
  GPermission *permission;
  gboolean allowed, can_acquire, can_release;
  gboolean success;

  permission = gtk_lock_button_get_permission (lockbutton);

  allowed = gtk_check_button_get_active (GTK_CHECK_BUTTON (allowed_button));
  can_acquire = gtk_check_button_get_active (GTK_CHECK_BUTTON (can_acquire_button));
  can_release = gtk_check_button_get_active (GTK_CHECK_BUTTON (can_release_button));
  success = gtk_check_button_get_active (GTK_CHECK_BUTTON (success_button));
  g_permission_impl_update (permission, allowed, can_acquire, can_release);
  g_test_permission_set_success (G_TEST_PERMISSION (permission), success);
}

static void
permission_changed (GPermission *permission,
                    GParamSpec  *pspec)
{
  gboolean allowed, can_acquire, can_release;

  g_object_get (permission,
                "allowed", &allowed,
                "can-acquire", &can_acquire,
                "can-release", &can_release,
                NULL);

  gtk_check_button_set_active (GTK_CHECK_BUTTON (allowed_button), allowed);
  gtk_check_button_set_active (GTK_CHECK_BUTTON (can_acquire_button), can_acquire);
  gtk_check_button_set_active (GTK_CHECK_BUTTON (can_release_button), can_release);
}

static void
draw_paintable (GdkPaintable *paintable)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  GdkTexture *texture;
  GskRenderer *renderer;
  graphene_rect_t bounds;
  static int count = 0;
  char *path;

  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (paintable,
                          snapshot,
                          gdk_paintable_get_intrinsic_width (paintable),
                          gdk_paintable_get_intrinsic_height (paintable));
  node = gtk_snapshot_free_to_node (snapshot);

  /* If the window literally draws nothing, we assume it hasn't been mapped yet and as such
   * the invalidations were only side effects of resizes.
   */
  if (node == NULL)
    return;

  if (gsk_render_node_get_node_type (node) == GSK_CLIP_NODE)
    {
      GskRenderNode *child;

      child = gsk_render_node_ref (gsk_clip_node_get_child (node));
      gsk_render_node_unref (node);
      node = child;
    }

  renderer = gtk_native_get_renderer (
                 gtk_widget_get_native (
                     gtk_widget_paintable_get_widget (GTK_WIDGET_PAINTABLE (paintable))));

  gsk_render_node_get_bounds (node, &bounds);
  graphene_rect_union (&bounds,
                       &GRAPHENE_RECT_INIT (
                         0, 0,
                         gdk_paintable_get_intrinsic_width (paintable),
                         gdk_paintable_get_intrinsic_height (paintable)
                       ),
                       &bounds);

  texture = gsk_renderer_render_texture (renderer, node, &bounds);
  g_object_set_data_full (G_OBJECT (texture),
                          "source-render-node",
                          node,
                          (GDestroyNotify) gsk_render_node_unref);

  path = g_strdup_printf ("screenshot%d.png", count++);
  gdk_texture_save_to_png (texture, path);
  g_free (path);

  g_object_unref (texture);

  g_signal_handlers_disconnect_by_func (paintable, draw_paintable, NULL);
  g_object_unref (paintable);
}

static gboolean
do_snapshot (gpointer data)
{
  GtkWidget *widget = data;
  GdkPaintable *paintable;

  paintable = gtk_widget_paintable_new (widget);
  g_signal_connect (paintable, "invalidate-contents", G_CALLBACK (draw_paintable), NULL);

  gtk_widget_queue_draw (widget);

  return G_SOURCE_REMOVE;
}

static void
screenshot_clicked (GtkButton *button,
                    GtkWidget *widget)
{
  g_assert_true (gtk_widget_get_realized (widget));

  gtk_widget_grab_focus (GTK_WIDGET (gtk_widget_get_root (widget)));

  g_idle_add (do_snapshot, gtk_widget_get_root (widget));
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *dialog;
  GtkWidget *button;
  GtkWidget *box;
  GtkWidget *update;
  GtkWidget *screenshot;
  GPermission *permission;
  GtkBuilder *builder;
  GtkCssProvider *provider;

  gtk_init ();

  permission = g_object_new (G_TYPE_TEST_PERMISSION, NULL);

  window = gtk_window_new ();
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_window_set_child (GTK_WINDOW (window), box);

  allowed_button = gtk_check_button_new_with_label ("Allowed");
  gtk_box_append (GTK_BOX (box), allowed_button);
  can_acquire_button = gtk_check_button_new_with_label ("Can acquire");
  gtk_box_append (GTK_BOX (box), can_acquire_button);
  can_release_button = gtk_check_button_new_with_label ("Can release");
  gtk_box_append (GTK_BOX (box), can_release_button);
  success_button = gtk_check_button_new_with_label ("Will succeed");
  gtk_box_append (GTK_BOX (box), success_button);
  update = gtk_button_new_with_label ("Update");
  gtk_box_append (GTK_BOX (box), update);
  screenshot = gtk_button_new_with_label ("Screenshot");
  gtk_box_append (GTK_BOX (box), screenshot);
  g_signal_connect (permission, "notify",
                    G_CALLBACK (permission_changed), NULL);

  builder = gtk_builder_new_from_file (argv[1]);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_path (provider, argv[2]);

  gtk_style_context_add_provider_for_display (gdk_display_get_default (), GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

  button = GTK_WIDGET (gtk_builder_get_object (builder, "lockbutton"));
  gtk_lock_button_set_permission (GTK_LOCK_BUTTON (button), permission);

  dialog = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
  gtk_widget_add_css_class (dialog, "nobackground");

  g_signal_connect (update, "clicked",
                    G_CALLBACK (update_clicked), button);
  g_signal_connect (screenshot, "clicked",
                    G_CALLBACK (screenshot_clicked), button);

  gtk_window_present (GTK_WINDOW (window));
  gtk_window_present (GTK_WINDOW (dialog));

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
