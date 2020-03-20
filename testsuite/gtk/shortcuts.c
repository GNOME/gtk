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

  g_assert_cmpint (gtk_shortcut_trigger_get_trigger_type (trigger), ==, GTK_SHORTCUT_TRIGGER_NEVER);

  trigger = gtk_keyval_trigger_new (GDK_KEY_a, GDK_CONTROL_MASK);
  g_assert_cmpint (gtk_shortcut_trigger_get_trigger_type (trigger), ==, GTK_SHORTCUT_TRIGGER_KEYVAL);
  g_assert_cmpint (gtk_keyval_trigger_get_keyval (trigger), ==, GDK_KEY_a);
  g_assert_cmpint (gtk_keyval_trigger_get_modifiers (trigger), ==, GDK_CONTROL_MASK);
  gtk_shortcut_trigger_unref (trigger);

  trigger = gtk_mnemonic_trigger_new (GDK_KEY_u);
  g_assert_cmpint (gtk_shortcut_trigger_get_trigger_type (trigger), ==, GTK_SHORTCUT_TRIGGER_MNEMONIC);
  g_assert_cmpint (gtk_mnemonic_trigger_get_keyval (trigger), ==, GDK_KEY_u);
  gtk_shortcut_trigger_unref (trigger);
}

static void
test_trigger_equal (void)
{
  GtkShortcutTrigger *trigger1, *trigger2, *trigger3, *trigger4;
  GtkShortcutTrigger *trigger5, *trigger6, *trigger1a, *trigger2a;

  trigger1 = gtk_keyval_trigger_new ('u', GDK_CONTROL_MASK);
  trigger2 = gtk_shortcut_trigger_ref (gtk_never_trigger_get ());
  trigger3 = gtk_alternative_trigger_new (gtk_shortcut_trigger_ref (trigger1), gtk_shortcut_trigger_ref (trigger2));
  trigger4 = gtk_alternative_trigger_new (gtk_shortcut_trigger_ref (trigger2), gtk_shortcut_trigger_ref (trigger1));
  trigger5 = gtk_keyval_trigger_new ('u', GDK_SHIFT_MASK);
  trigger6 = gtk_mnemonic_trigger_new ('u');

  trigger1a = gtk_keyval_trigger_new ('u', GDK_CONTROL_MASK);
  trigger2a = gtk_shortcut_trigger_ref (gtk_never_trigger_get ());

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

  gtk_shortcut_trigger_unref (trigger1);
  gtk_shortcut_trigger_unref (trigger2);
  gtk_shortcut_trigger_unref (trigger3);
  gtk_shortcut_trigger_unref (trigger4);
  gtk_shortcut_trigger_unref (trigger5);
  gtk_shortcut_trigger_unref (trigger6);
  gtk_shortcut_trigger_unref (trigger1a);
  gtk_shortcut_trigger_unref (trigger2a);
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
      g_assert_cmpint (gtk_shortcut_trigger_get_trigger_type (trigger), ==, GTK_SHORTCUT_TRIGGER_KEYVAL);
      g_assert_cmpint (gtk_keyval_trigger_get_modifiers (trigger), ==, tests[i].modifiers);
      g_assert_cmpuint (gtk_keyval_trigger_get_keyval (trigger), ==, tests[i].keyval);

      gtk_shortcut_trigger_unref (trigger);
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
    gboolean result[4];
  } tests[] = {
    { GDK_KEY_a, GDK_CONTROL_MASK, FALSE, { 0, 1, 0, 1 } }, 
    { GDK_KEY_a, GDK_CONTROL_MASK, TRUE,  { 0, 1, 0, 1 } }, 
    { GDK_KEY_a, GDK_SHIFT_MASK,   FALSE, { 0, 0, 0, 0 } }, 
    { GDK_KEY_a, GDK_SHIFT_MASK,   TRUE,  { 0, 0, 0, 0 } }, 
    { GDK_KEY_u, GDK_SHIFT_MASK,   FALSE, { 0, 0, 0, 0 } }, 
    { GDK_KEY_u, GDK_SHIFT_MASK,   TRUE,  { 0, 0, 1, 1 } }, 
  };
  int i;

  trigger1 = gtk_shortcut_trigger_ref (gtk_never_trigger_get ());
  trigger2 = gtk_keyval_trigger_new (GDK_KEY_a, GDK_CONTROL_MASK);
  trigger3 = gtk_mnemonic_trigger_new (GDK_KEY_u);
  trigger4 = gtk_alternative_trigger_new (gtk_shortcut_trigger_ref (trigger2),
                                          gtk_shortcut_trigger_ref (trigger3));

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

  gtk_shortcut_trigger_unref (trigger1);
  gtk_shortcut_trigger_unref (trigger2);
  gtk_shortcut_trigger_unref (trigger3);
  gtk_shortcut_trigger_unref (trigger4);
}

static int callback_count;

static gboolean
callback (GtkWidget *widget,
          GVariant  *args,
          gpointer   user_data)
{
  callback_count++;
  return TRUE;
}

static void
test_action_basic (void)
{
  GtkShortcutAction *action;

  action = gtk_nothing_action_new ();
  g_assert_cmpint (gtk_shortcut_action_get_action_type (action), ==, GTK_SHORTCUT_ACTION_NOTHING);
  gtk_shortcut_action_unref (action);

  action = gtk_callback_action_new (callback, NULL, NULL);
  g_assert_cmpint (gtk_shortcut_action_get_action_type (action), ==, GTK_SHORTCUT_ACTION_CALLBACK);
  gtk_shortcut_action_unref (action);

  action = gtk_mnemonic_action_new ();
  g_assert_cmpint (gtk_shortcut_action_get_action_type (action), ==, GTK_SHORTCUT_ACTION_MNEMONIC);
  gtk_shortcut_action_unref (action);

  action = gtk_activate_action_new ();
  g_assert_cmpint (gtk_shortcut_action_get_action_type (action), ==, GTK_SHORTCUT_ACTION_ACTIVATE);
  gtk_shortcut_action_unref (action);

  action = gtk_signal_action_new ("activate");
  g_assert_cmpint (gtk_shortcut_action_get_action_type (action), ==, GTK_SHORTCUT_ACTION_SIGNAL);
  g_assert_cmpstr (gtk_signal_action_get_signal_name (action), ==, "activate");
  gtk_shortcut_action_unref (action);

  action = gtk_action_action_new ("text.undo");
  g_assert_cmpint (gtk_shortcut_action_get_action_type (action), ==, GTK_SHORTCUT_ACTION_ACTION);
  g_assert_cmpstr (gtk_action_action_get_name (action), ==, "text.undo");
  gtk_shortcut_action_unref (action);
}

static void
test_action_activate (void)
{
  GtkShortcutAction *action;
  GtkWidget *widget;

  widget = gtk_label_new ("");
  g_object_ref_sink (widget);

  action = gtk_nothing_action_new ();
  g_assert_cmpint (gtk_shortcut_action_activate (action, 0, widget, NULL), ==, FALSE);
  gtk_shortcut_action_unref (action);

  callback_count = 0;
  action = gtk_callback_action_new (callback, NULL, NULL);
  g_assert_cmpint (gtk_shortcut_action_activate (action, 0, widget , NULL), ==, TRUE);
  g_assert_cmpint (callback_count, ==, 1);
  gtk_shortcut_action_unref (action);

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
