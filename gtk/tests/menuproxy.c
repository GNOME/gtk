/*
 * Copyright (C) 2009 Canonical, Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Cody Russell <bratsche@gnome.org>
 */

#undef GTK_DISABLE_DEPRECATED
#include "../gtk/gtk.h"

typedef struct _TestProxy      TestProxy;
typedef struct _TestProxyClass TestProxyClass;

//static GType           test_proxy_type_id      = 0;
//static TestProxyClass *test_proxy_parent_class = NULL;

#define TEST_TYPE_PROXY     (test_proxy_type_id)
#define TEST_PROXY(o)       (G_TYPE_CHECK_INSTANCE_CAST ((o), TEST_TYPE_PROXY, TestProxy))
#define TEST_PROXY_CLASS(k) (G_TYPE_CHECK_CLASS_CAST((k), TEST_TYPE_PROXY, TestProxyClass))
#define TEST_IS_PROXY(o)    (G_TYPE_CHECK_INSTANCE_TYPE ((o), TEST_TYPE_PROXY))

struct _TestProxy
{
  GtkMenuProxy parent_object;
};

struct _TestProxyClass
{
  GtkMenuProxyClass parent_class;
};

static void test_proxy_insert         (GtkMenuProxy *proxy,
                                       GtkWidget    *child,
                                       guint         position);

G_DEFINE_DYNAMIC_TYPE(TestProxy, test_proxy, GTK_TYPE_MENU_PROXY)

static void
test_proxy_init (TestProxy *proxy)
{
}

static void
test_proxy_class_init (TestProxyClass *class)
{
  GtkMenuProxyClass *proxy_class = GTK_MENU_PROXY_CLASS (class);

  test_proxy_parent_class = g_type_class_peek_parent (class);

  proxy_class->insert = test_proxy_insert;
}

static void
test_proxy_class_finalize (TestProxyClass *class)
{
}

static void
test_proxy_insert (GtkMenuProxy *proxy,
                   GtkWidget    *child,
                   guint         position)
{
}

/* ---------------------------------------------------- */

#define TEST_TYPE_MODULE         (test_module_get_type ())
#define TEST_MODULE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TEST_TYPE_MODULE, TestModule))
#define TEST_MODULE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TEST_TYPE_MODULE, TestModuleClass))
#define TEST_IS_MODULE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TEST_TYPE_MODULE))
#define TEST_IS_MODULE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TEST_TYPE_MODULE))
#define TEST_MODULE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TEST_TYPE_MODULE, TestModuleClass))


typedef struct _TestModule      TestModule;
typedef struct _TestModuleClass TestModuleClass;

struct _TestModule
{
  GTypeModule  parent_instance;
};

struct _TestModuleClass
{
  GTypeModuleClass  parent_class;
};

static gboolean
test_module_load (GTypeModule *module)
{
  g_print ("registering type...\n");
  g_print ("     type_id = %d\n", test_proxy_type_id);

  test_proxy_register_type (G_TYPE_MODULE (module));

  //test_proxy_get_type (G_TYPE_MODULE (module));

  g_print ("     type_id = %d\n", test_proxy_type_id);

  return TRUE;
}

static void
test_module_unload (GTypeModule *module)
{
}

static void
test_module_class_init (TestModuleClass *class)
{
  GTypeModuleClass *type_module_class = G_TYPE_MODULE_CLASS (class);

  type_module_class->load = test_module_load;
  type_module_class->unload = test_module_unload;
}

static void
test_module_init (TestModule *module)
{
}

G_DEFINE_TYPE (TestModule, test_module, G_TYPE_TYPE_MODULE);

TestModule *
test_module_new (void)
{
  TestModule *module = g_object_new (TEST_TYPE_MODULE,
                                     NULL);

  g_print ("test_module_new(): %p\n", module);

  return module;
}


/* ---------------------------------------------------- */

static void
non_null_proxy_test (void)
{
  GtkMenuProxyModule *module;

  /* prevent the module loader from finding a proxy module */
  g_unsetenv ("GTK_MENUPROXY");

  module = gtk_menu_proxy_module_get ();
  test_proxy_register_type (G_TYPE_MODULE (module));
  //test_proxy_get_type (G_TYPE_MODULE (module));

  GtkWidget *widget = g_object_new (GTK_TYPE_MENU_BAR, NULL);
  g_object_ref_sink (widget);

  g_assert (GTK_IS_MENU_BAR (widget));
  //g_assert (GTK_MENU_SHELL (widget)->proxy != NULL);

  g_object_unref (widget);
}

static void
null_proxy_test (void)
{
  GtkWidget *widget = g_object_new (GTK_TYPE_MENU_BAR, NULL);
  g_object_ref_sink (widget);

  g_assert (GTK_IS_MENU_BAR (widget));

  //g_assert (GTK_MENU_SHELL (widget)->proxy == NULL);

  g_object_unref (widget);
}

static gboolean inserted_called = FALSE;

static void
inserted_cb (GtkMenuProxy *proxy,
             GtkWidget    *child,
             guint         position,
             gpointer      data)
{
  g_return_if_fail (GTK_IS_MENU_PROXY (proxy));
  g_return_if_fail (GTK_IS_WIDGET (child));
  inserted_called = TRUE;
}

static void
menubar_signals_proxy_test (void)
{
  GtkWidget *widget   = NULL;
  GtkWidget *menuitem = NULL;
  GtkMenuProxy *proxy;

  //gtk_menu_proxy_register_type (test_proxy_get_type ());

  widget = g_object_new (GTK_TYPE_MENU_BAR, NULL);
  g_object_ref_sink (widget);

  g_assert (GTK_IS_MENU_BAR (widget));
  //g_assert (GTK_MENU_SHELL (widget)->proxy != NULL);

  /*
  proxy = GTK_MENU_SHELL (widget)->proxy;

  g_signal_connect (proxy,
                    "inserted", G_CALLBACK (inserted_cb),
                    NULL);
  */

  // insert menuitem
  menuitem = gtk_menu_item_new_with_label ("Test Item");
  gtk_menu_shell_append (GTK_MENU_SHELL (widget),
                         menuitem);

  g_assert (inserted_called == TRUE);

  g_object_unref (widget);
}

static void
proxy_type_exists_test (void)
{
#if 0
  GtkMenuProxyModule *module;

  g_unsetenv ("GTK_MENUPROXY");

  module = gtk_menu_proxy_module_get ();
  test_proxy_get_type (G_TYPE_MODULE (module));
#endif

  g_assert (gtk_menu_proxy_get_type () != 0);
}

static void
can_instantiate_test (void)
{
  TestModule *module = test_module_new ();

  g_type_module_use (G_TYPE_MODULE (module));

  GtkMenuProxy *proxy = gtk_menu_proxy_get ();

  g_assert (proxy != NULL);

  g_object_ref_sink (proxy);

  g_assert (TEST_IS_PROXY (proxy));
  g_assert (GTK_IS_MENU_PROXY (proxy));

  g_object_unref (proxy);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/proxy/null-proxy", null_proxy_test);
  g_test_add_func ("/proxy/type-exists", proxy_type_exists_test);
  g_test_add_func ("/proxy/can-instantiate", can_instantiate_test);
  g_test_add_func ("/proxy/non-null-proxy", non_null_proxy_test);
  g_test_add_func ("/proxy/menubar-signals-proxy", menubar_signals_proxy_test);

  return g_test_run();
}
