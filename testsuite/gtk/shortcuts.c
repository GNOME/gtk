#include <gtk/gtk.h>

typedef struct _GdkEventAny GdkEventAny;
typedef struct _GdkEventKey GdkEventKey;

struct _GdkEventAny
{
  int ref_count;
  GdkEventType type;
  GdkSurface *surface;
  guint32 time;
  guint16 flags;
  guint pointer_emulated  : 1;
  guint touch_emulating   : 1;
  guint scroll_is_stop    : 1;
  guint key_is_modifier   : 1;
  guint focus_in          : 1;
  GdkDevice *device;
  GdkDevice *source_device;
};

struct _GdkEventKey
{
  GdkEventAny any;
  GdkModifierType state;
  guint keyval;
  guint16 hardware_keycode;
  guint16 key_scancode;
  guint8 group;
};

static GdkEvent * gdk_event_key_new     (GdkEventType     type,
                                         GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDevice       *source_device,
                                         guint32          time,
                                         GdkModifierType  state,
                                         guint            keyval,
                                         guint16          keycode,
                                         guint16          scancode,
                                         guint8           group,
                                         gboolean         is_modifier);

static GdkEvent *
gdk_event_key_new (GdkEventType     type,
                   GdkSurface      *surface,
                   GdkDevice       *device,
                   GdkDevice       *source_device,
                   guint32          time,
                   GdkModifierType  state,
                   guint            keyval,
                   guint16          keycode,
                   guint16          scancode,
                   guint8           group,
                   gboolean         is_modifier)
{
  GdkEventKey *event;

  g_return_val_if_fail (type == GDK_KEY_PRESS ||
                        type == GDK_KEY_RELEASE, NULL);

  event = g_new0 (GdkEventKey, 1);

  event->any.ref_count = 1;
  event->any.type = type;
  event->any.time = time;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->state = state;
  event->keyval = keyval;
  event->hardware_keycode = keycode;
  event->key_scancode = scancode;
  event->group = group;
  event->any.key_is_modifier = is_modifier;

  return (GdkEvent *)event;
}

static void
test_trigger_basic (void)
{
  GtkShortcutTrigger *trigger;

  trigger = gtk_never_trigger_get ();

  trigger = gtk_keyval_trigger_new (GDK_KEY_a, GDK_CONTROL_MASK);
  g_assert_cmpint (gtk_keyval_trigger_get_keyval (GTK_KEYVAL_TRIGGER (trigger)), ==, GDK_KEY_a);
  g_assert_cmpint (gtk_keyval_trigger_get_modifiers (GTK_KEYVAL_TRIGGER (trigger)), ==, GDK_CONTROL_MASK);
  g_object_unref (trigger);

  trigger = gtk_mnemonic_trigger_new (GDK_KEY_u);
  g_assert_cmpint (gtk_mnemonic_trigger_get_keyval (GTK_MNEMONIC_TRIGGER (trigger)), ==, GDK_KEY_u);
  g_object_unref (trigger);
}

static void
test_trigger_equal (void)
{
  GtkShortcutTrigger *trigger1, *trigger2, *trigger3, *trigger4;
  GtkShortcutTrigger *trigger5, *trigger6, *trigger1a, *trigger2a;

  trigger1 = gtk_keyval_trigger_new ('u', GDK_CONTROL_MASK);
  trigger2 = g_object_ref (gtk_never_trigger_get ());
  trigger3 = gtk_alternative_trigger_new (g_object_ref (trigger1), g_object_ref (trigger2));
  trigger4 = gtk_alternative_trigger_new (g_object_ref (trigger2), g_object_ref (trigger1));
  trigger5 = gtk_keyval_trigger_new ('u', GDK_SHIFT_MASK);
  trigger6 = gtk_mnemonic_trigger_new ('u');

  trigger1a = gtk_keyval_trigger_new ('u', GDK_CONTROL_MASK);
  trigger2a = g_object_ref (gtk_never_trigger_get ());

  g_assert_true (gtk_shortcut_trigger_equal (trigger1, trigger1));
  g_assert_true (gtk_shortcut_trigger_equal (trigger2, trigger2));
  g_assert_true (gtk_shortcut_trigger_equal (trigger3, trigger3));
  g_assert_true (gtk_shortcut_trigger_equal (trigger4, trigger4));
  g_assert_true (gtk_shortcut_trigger_equal (trigger5, trigger5));
  g_assert_true (gtk_shortcut_trigger_equal (trigger6, trigger6));

  g_assert_false (gtk_shortcut_trigger_equal (trigger1, trigger2));
  g_assert_false (gtk_shortcut_trigger_equal (trigger1, trigger3));
  g_assert_false (gtk_shortcut_trigger_equal (trigger1, trigger4));
  g_assert_false (gtk_shortcut_trigger_equal (trigger1, trigger5));
  g_assert_false (gtk_shortcut_trigger_equal (trigger1, trigger6));

  g_assert_false (gtk_shortcut_trigger_equal (trigger2, trigger3));
  g_assert_false (gtk_shortcut_trigger_equal (trigger2, trigger4));
  g_assert_false (gtk_shortcut_trigger_equal (trigger2, trigger5));
  g_assert_false (gtk_shortcut_trigger_equal (trigger2, trigger6));

  g_assert_false (gtk_shortcut_trigger_equal (trigger3, trigger4));
  g_assert_false (gtk_shortcut_trigger_equal (trigger3, trigger5));
  g_assert_false (gtk_shortcut_trigger_equal (trigger3, trigger6));

  g_assert_false (gtk_shortcut_trigger_equal (trigger4, trigger5));
  g_assert_false (gtk_shortcut_trigger_equal (trigger4, trigger6));

  g_assert_false (gtk_shortcut_trigger_equal (trigger5, trigger6));

  g_assert_true (gtk_shortcut_trigger_equal (trigger1, trigger1a));
  g_assert_true (gtk_shortcut_trigger_equal (trigger2, trigger2a));

  g_object_unref (trigger1);
  g_object_unref (trigger2);
  g_object_unref (trigger3);
  g_object_unref (trigger4);
  g_object_unref (trigger5);
  g_object_unref (trigger6);
  g_object_unref (trigger1a);
  g_object_unref (trigger2a);
}

static void
test_trigger_parse (void)
{
  struct {
    const char *str;
    GdkModifierType modifiers;
    guint keyval;
  } tests[] = {
    { "<Primary><Alt>z", GDK_CONTROL_MASK|GDK_MOD1_MASK, 'z' },
    { "<Control>U", GDK_CONTROL_MASK, 'u' },
    { "<Hyper>x", GDK_HYPER_MASK, 'x' },
    { "<Meta>y", GDK_META_MASK, 'y' },
    { "KP_7", 0, GDK_KEY_KP_7 },
    { "<Shift>exclam", GDK_SHIFT_MASK, '!' },
  };
  GtkShortcutTrigger *trigger;
  int i;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      trigger = gtk_shortcut_trigger_parse_string (tests[i].str);

      g_assert_true (GTK_IS_KEYVAL_TRIGGER (trigger));
      g_assert_cmpint (gtk_keyval_trigger_get_modifiers (GTK_KEYVAL_TRIGGER (trigger)), ==, tests[i].modifiers);
      g_assert_cmpuint (gtk_keyval_trigger_get_keyval (GTK_KEYVAL_TRIGGER (trigger)), ==, tests[i].keyval);

      g_object_unref (trigger);
    }
}

static void
test_trigger_trigger (void)
{
  GtkShortcutTrigger *trigger1, *trigger2, *trigger3, *trigger4;
  GdkDisplay *display;
  GdkSurface *surface;
  GdkDevice *device;
  GdkEvent *event;
  struct {
    guint keyval;
    GdkModifierType state;
    gboolean mnemonic;
    GtkShortcutTriggerMatch result[4];
  } tests[] = {
    { GDK_KEY_a, GDK_CONTROL_MASK, FALSE, { GTK_SHORTCUT_TRIGGER_MATCH_NONE, GTK_SHORTCUT_TRIGGER_MATCH_EXACT, GTK_SHORTCUT_TRIGGER_MATCH_NONE, GTK_SHORTCUT_TRIGGER_MATCH_EXACT } }, 
    { GDK_KEY_a, GDK_CONTROL_MASK, TRUE,  { GTK_SHORTCUT_TRIGGER_MATCH_NONE, GTK_SHORTCUT_TRIGGER_MATCH_EXACT, GTK_SHORTCUT_TRIGGER_MATCH_NONE, GTK_SHORTCUT_TRIGGER_MATCH_EXACT } }, 
    { GDK_KEY_a, GDK_SHIFT_MASK,   FALSE, { GTK_SHORTCUT_TRIGGER_MATCH_NONE, GTK_SHORTCUT_TRIGGER_MATCH_NONE, GTK_SHORTCUT_TRIGGER_MATCH_NONE, GTK_SHORTCUT_TRIGGER_MATCH_NONE } }, 
    { GDK_KEY_a, GDK_SHIFT_MASK,   TRUE,  { GTK_SHORTCUT_TRIGGER_MATCH_NONE, GTK_SHORTCUT_TRIGGER_MATCH_NONE, GTK_SHORTCUT_TRIGGER_MATCH_NONE, GTK_SHORTCUT_TRIGGER_MATCH_NONE } }, 
    { GDK_KEY_u, GDK_SHIFT_MASK,   FALSE, { GTK_SHORTCUT_TRIGGER_MATCH_NONE, GTK_SHORTCUT_TRIGGER_MATCH_NONE, GTK_SHORTCUT_TRIGGER_MATCH_NONE, GTK_SHORTCUT_TRIGGER_MATCH_NONE } }, 
    { GDK_KEY_u, GDK_SHIFT_MASK,   TRUE,  { GTK_SHORTCUT_TRIGGER_MATCH_NONE, GTK_SHORTCUT_TRIGGER_MATCH_NONE, GTK_SHORTCUT_TRIGGER_MATCH_EXACT, GTK_SHORTCUT_TRIGGER_MATCH_EXACT } }, 
  };
  int i;

  trigger1 = g_object_ref (gtk_never_trigger_get ());
  trigger2 = gtk_keyval_trigger_new (GDK_KEY_a, GDK_CONTROL_MASK);
  trigger3 = gtk_mnemonic_trigger_new (GDK_KEY_u);
  trigger4 = gtk_alternative_trigger_new (g_object_ref (trigger2),
                                          g_object_ref (trigger3));

  display = gdk_display_get_default ();
  device = gdk_seat_get_keyboard (gdk_display_get_default_seat (display));
  surface = gdk_surface_new_toplevel (display, 100, 100);

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      event = gdk_event_key_new (GDK_KEY_PRESS,
                                 surface,
                                 device,
                                 device,
                                 GDK_CURRENT_TIME,
                                 tests[i].state,
                                 tests[i].keyval,
                                 0,
                                 0,
                                 0,
                                 FALSE);
      g_assert_cmpint (gtk_shortcut_trigger_trigger (trigger1, event, tests[i].mnemonic), ==, tests[i].result[0]);
      g_assert_cmpint (gtk_shortcut_trigger_trigger (trigger2, event, tests[i].mnemonic), ==, tests[i].result[1]);
      g_assert_cmpint (gtk_shortcut_trigger_trigger (trigger3, event, tests[i].mnemonic), ==, tests[i].result[2]);
      g_assert_cmpint (gtk_shortcut_trigger_trigger (trigger4, event, tests[i].mnemonic), ==, tests[i].result[3]);

      gdk_event_unref (event);
    }

  g_object_unref (surface);

  g_object_unref (trigger1);
  g_object_unref (trigger2);
  g_object_unref (trigger3);
  g_object_unref (trigger4);
}

static gboolean
callback (GtkWidget *widget,
          GVariant  *args,
          gpointer   user_data)
{
  int *callback_count = user_data;
  *callback_count += 1;
  return TRUE;
}

static void
test_action_basic (void)
{
  GtkShortcutAction *action;

  action = gtk_signal_action_new ("activate");
  g_assert_cmpstr (gtk_signal_action_get_signal_name (GTK_SIGNAL_ACTION (action)), ==, "activate");
  g_object_unref (action);

  action = gtk_named_action_new ("text.undo");
  g_assert_cmpstr (gtk_named_action_get_action_name (GTK_NAMED_ACTION (action)), ==, "text.undo");
  g_object_unref (action);
}

static void
test_action_activate (void)
{
  GtkShortcutAction *action;
  GtkWidget *widget;
  int callback_count;

  widget = gtk_label_new ("");
  g_object_ref_sink (widget);

  action = gtk_nothing_action_get ();
  g_assert_false (gtk_shortcut_action_activate (action, 0, widget, NULL));

  callback_count = 0;
  action = gtk_callback_action_new (callback, &callback_count, NULL);
  g_assert_true (gtk_shortcut_action_activate (action, 0, widget, NULL));
  g_assert_cmpint (callback_count, ==, 1);
  g_object_unref (action);

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/shortcuts/trigger/basic", test_trigger_basic);
  g_test_add_func ("/shortcuts/trigger/equal", test_trigger_equal);
  g_test_add_func ("/shortcuts/trigger/parse", test_trigger_parse);
  g_test_add_func ("/shortcuts/trigger/trigger", test_trigger_trigger);
  g_test_add_func ("/shortcuts/action/basic", test_action_basic);
  g_test_add_func ("/shortcuts/action/activate", test_action_activate);

  return g_test_run ();
}
