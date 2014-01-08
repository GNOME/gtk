#include <gtk/gtk.h>

static void
removed (GActionGroup *group,
         const gchar  *name,
         gpointer      user_data)
{
  gboolean *was_removed = user_data;

  *was_removed = TRUE;
}

static void
test_as_actiongroup (void)
{
  GSimpleAction *action;
  gboolean was_removed;
  gpointer window;
  gchar **list;

  /* do a dummy round... */
  window = g_object_ref_sink (g_object_new (GTK_TYPE_APPLICATION_WINDOW, NULL));
  gtk_widget_destroy (window);
  g_object_unref (window);

  /* create a window, add an action */
  window = g_object_ref_sink (g_object_new (GTK_TYPE_APPLICATION_WINDOW, NULL));
  action = g_simple_action_new ("foo", NULL);
  g_action_map_add_action (window, G_ACTION (action));
  g_object_unref (action);

  /* check which actions we have in the group */
  list = g_action_group_list_actions (window);
  g_assert_cmpstr (list[0], ==, "foo");
  g_assert_cmpstr (list[1], ==, NULL);
  g_strfreev (list);

  /* make sure that destroying the window keeps our view of the actions
   * consistent.
   */
  g_signal_connect (window, "action-removed", G_CALLBACK (removed), &was_removed);
  gtk_widget_destroy (window);

  /* One of two things will have happened, depending on the
   * implementation.  Both are valid:
   *
   *  - we received a signal that the action was removed when we
   *    destroyed the window; or
   *
   *  - the action is still available.
   *
   * Make sure we're in one of those states.
   *
   * This additionally ensures that calling into methods on the window
   * will continue to work after it has been destroy (and not segfault).
   */
  list = g_action_group_list_actions (window);

  if (was_removed == FALSE)
    {
      /* should still be here */
      g_assert_cmpstr (list[0], ==, "foo");
      g_assert_cmpstr (list[1], ==, NULL);
    }
  else
    /* should be gone */
    g_assert_cmpstr (list[0], ==, NULL);

  g_object_unref (window);
  g_strfreev (list);
}

int
main (int argc, char **argv)
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gtkapplicationwindow/as-actiongroup", test_as_actiongroup);

  return g_test_run ();
}
