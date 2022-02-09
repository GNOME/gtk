#include <gtk/gtk.h>

static gboolean should_fail = FALSE;


#define BACON_TYPE_VIDEO_WIDGET              (bacon_video_widget_get_type ())
G_DECLARE_FINAL_TYPE(BaconVideoWidget, bacon_video_widget, BACON, VIDEO_WIDGET, GtkOverlay)

static void bacon_video_widget_initable_iface_init (GInitableIface *iface);

struct _BaconVideoWidget
{
  GtkOverlay parent;
  gboolean object_init;
  gboolean initable_init;
};

G_DEFINE_TYPE_WITH_CODE (BaconVideoWidget, bacon_video_widget, GTK_TYPE_OVERLAY,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                bacon_video_widget_initable_iface_init))

static gboolean
bacon_video_widget_initable_init (GInitable     *initable,
                                  GCancellable  *cancellable,
                                  GError       **error)
{
  BaconVideoWidget *bvw;

  bvw = BACON_VIDEO_WIDGET (initable);

  if (should_fail) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "Initable widget was setup to fail.");
    g_object_ref_sink (G_OBJECT (initable));
    return FALSE;
  }

  bvw->initable_init = TRUE;

  return TRUE;
}

static void
bacon_video_widget_initable_iface_init (GInitableIface *iface)
{
  iface->init = bacon_video_widget_initable_init;
}

static void
bacon_video_widget_class_init (BaconVideoWidgetClass *klass)
{
  /* empty */
}

static void
bacon_video_widget_init (BaconVideoWidget *bvw)
{
  bvw->object_init = TRUE;
}

int main (int argc, char **argv)
{
  GtkBuilder *builder;
  guint num_objects;
  GError *error = NULL;
  GObject *bvw;
  GSList *list = NULL;

  gtk_init (&argc, &argv);

  builder = gtk_builder_new ();
  num_objects = gtk_builder_add_from_file (builder, "testbuilder.ui", &error);
  g_assert_cmpint (num_objects, >, 0);
  g_assert_no_error (error);
  bvw = gtk_builder_get_object (builder, "bvw");
  g_assert_nonnull (bvw);
  g_assert_true(BACON_VIDEO_WIDGET(bvw)->initable_init);
  list = gtk_builder_get_objects (builder);
  g_assert_cmpint (g_slist_length (list), ==, 3);
  g_clear_pointer (&list, g_slist_free);
  gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "totem_main_window")));
  g_clear_object (&builder);

  builder = gtk_builder_new_from_file ("testbuilder.ui");
  bvw = gtk_builder_get_object (builder, "bvw");
  g_assert_nonnull (bvw);
  g_assert_true(BACON_VIDEO_WIDGET(bvw)->initable_init);
  list = gtk_builder_get_objects (builder);
  g_assert_cmpint (g_slist_length (list), ==, 3);
  g_clear_pointer (&list, g_slist_free);
  gtk_widget_destroy (GTK_WIDGET (gtk_builder_get_object (builder, "totem_main_window")));
  g_clear_object (&builder);

  should_fail = TRUE;

  builder = gtk_builder_new ();
  num_objects = gtk_builder_add_from_file (builder, "testbuilder.ui", &error);
  g_assert_error(error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_clear_error (&error);
  g_assert_cmpint (num_objects, ==, 0);
  list = gtk_builder_get_objects (builder);
  g_assert_cmpint (g_slist_length (list), ==, 2);
  g_clear_pointer (&list, g_slist_free);
  g_clear_object (&builder);

  /* This calls g_error() so disabled
  builder = gtk_builder_new_from_file ("testbuilder.ui");
  bvw = gtk_builder_get_object (builder, "bvw");
  g_assert_null (bvw);
  g_clear_object (&builder);
  */

  return 0;
}
