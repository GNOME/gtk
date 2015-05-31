/* gtkcloudprintaccount.c: Google Cloud Print account class
 * Copyright (C) 2014, Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <rest/oauth2-proxy.h>
#include <rest/rest-proxy.h>
#include <rest/rest-proxy-call.h>
#include <json-glib/json-glib.h>

#include <gtk/gtkunixprint.h>
#include "gtkcloudprintaccount.h"
#include "gtkprintercloudprint.h"

#define CLOUDPRINT_PROXY "GTK+"

#define ACCOUNT_IFACE        "org.gnome.OnlineAccounts.Account"
#define O_AUTH2_BASED_IFACE  "org.gnome.OnlineAccounts.OAuth2Based"

#define GTK_CLOUDPRINT_ACCOUNT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CLOUDPRINT_ACCOUNT, GtkCloudprintAccountClass))
#define GTK_IS_CLOUDPRINT_ACCOUNT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CLOUDPRINT_ACCOUNT))
#define GTK_CLOUDPRINT_ACCOUNT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CLOUDPRINT_ACCOUNT, GtkCloudprintAccountClass))

static GObjectClass *gtk_cloudprint_account_parent_class;
static GType gtk_cloudprint_account_type = 0;

typedef struct _GtkCloudprintAccountClass GtkCloudprintAccountClass;

struct _GtkCloudprintAccountClass
{
  GObjectClass parent_class;
};

struct _GtkCloudprintAccount
{
  GObject parent_instance;

  gchar *printer_id;
  gchar *goa_path;
  gchar *presentation_identity;
  RestProxy *rest_proxy;
  gchar *oauth2_access_token;
};

static void                 gtk_cloudprint_account_class_init      (GtkCloudprintAccountClass *class);
static void                 gtk_cloudprint_account_init            (GtkCloudprintAccount      *impl);
static void                 gtk_cloudprint_account_finalize	 (GObject *object);

void
gtk_cloudprint_account_register_type (GTypeModule *module)
{
  const GTypeInfo cloudprint_account_info =
  {
    sizeof (GtkCloudprintAccountClass),
    NULL,		/* base_init */
    NULL,		/* base_finalize */
    (GClassInitFunc) gtk_cloudprint_account_class_init,
    NULL,		/* class_finalize */
    NULL,		/* class_data */
    sizeof (GtkCloudprintAccount),
    0,		/* n_preallocs */
    (GInstanceInitFunc) gtk_cloudprint_account_init,
  };

  gtk_cloudprint_account_type = g_type_module_register_type (module,
							     G_TYPE_OBJECT,
							     "GtkCloudprintAccount",
							     &cloudprint_account_info, 0);
}

/*
 * GtkCloudprintAccount
 */
GType
gtk_cloudprint_account_get_type (void)
{
  return gtk_cloudprint_account_type;
}

/**
 * gtk_cloudprint_account_new:
 *
 * Creates a new #GtkCloudprintAccount object, representing a Google
 * Cloud Print account and its state data.
 *
 * Returns: the new #GtkCloudprintAccount object
 **/
GtkCloudprintAccount *
gtk_cloudprint_account_new (const gchar *id,
			    const gchar *path,
			    const gchar *presentation_identity)
{
  GtkCloudprintAccount *account = g_object_new (GTK_TYPE_CLOUDPRINT_ACCOUNT,
						NULL);
  account->printer_id = g_strdup (id);
  account->goa_path = g_strdup (path);
  account->presentation_identity = g_strdup (presentation_identity);
  return account;
}

static void
gtk_cloudprint_account_class_init (GtkCloudprintAccountClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gtk_cloudprint_account_parent_class = g_type_class_peek_parent (klass);
  gobject_class->finalize = gtk_cloudprint_account_finalize;
}

static void
gtk_cloudprint_account_init (GtkCloudprintAccount *account)
{
  account->printer_id = NULL;
  account->goa_path = NULL;
  account->presentation_identity = NULL;
  account->rest_proxy = NULL;
  account->oauth2_access_token = NULL;

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: +GtkCloudprintAccount(%p)\n",
		     account));
}

static void
gtk_cloudprint_account_finalize (GObject *object)
{
  GtkCloudprintAccount *account;

  account = GTK_CLOUDPRINT_ACCOUNT (object);

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: -GtkCloudprintAccount(%p)\n",
		     account));

  g_clear_object (&(account->rest_proxy));
  g_clear_pointer (&(account->printer_id), g_free);
  g_clear_pointer (&(account->goa_path), g_free);
  g_clear_pointer (&(account->presentation_identity), g_free);
  g_clear_pointer (&(account->oauth2_access_token), g_free);

  G_OBJECT_CLASS (gtk_cloudprint_account_parent_class)->finalize (object);
}

static JsonParser *
cloudprint_json_parse (RestProxyCall *call, JsonObject **result, GError **error)
{
  JsonParser *json_parser = json_parser_new ();
  JsonNode *root;
  JsonObject *json_object;
  gboolean success = FALSE;

  if (!json_parser_load_from_data (json_parser,
				   rest_proxy_call_get_payload (call),
				   rest_proxy_call_get_payload_length (call),
				   error))
    {
      g_object_unref (json_parser);
      return NULL;
    }

  root = json_parser_get_root (json_parser);
  if (JSON_NODE_TYPE (root) != JSON_NODE_OBJECT)
    {
      if (error != NULL)
	*error = g_error_new_literal (gtk_print_error_quark (),
				      GTK_PRINT_ERROR_INTERNAL_ERROR,
				      "Bad reply");

      g_object_unref (json_parser);
      return NULL;
    }

  json_object = json_node_get_object (root);
  if (json_object_has_member (json_object, "success"))
    success = json_object_get_boolean_member (json_object, "success");

  if (!success)
    {
      const gchar *message = "(no message)";

      if (json_object_has_member (json_object, "message"))
	message = json_object_get_string_member (json_object, "message");

      GTK_NOTE (PRINTING,
		g_print ("Cloud Print Backend: unsuccessful submit: %s\n",
			 message));

      if (error != NULL)
	*error = g_error_new_literal (gtk_print_error_quark (),
				      GTK_PRINT_ERROR_INTERNAL_ERROR,
				      message);

      g_object_unref (json_parser);
      return NULL;
    }

  if (result != NULL)
    *result = json_node_dup_object (root);

  return json_parser;
}

static void
gtk_cloudprint_account_search_rest_call_cb (RestProxyCall *call,
					    const GError *cb_error,
					    GObject *weak_object,
					    gpointer user_data)
{
  GTask *task = user_data;
  GtkCloudprintAccount *account = g_task_get_task_data (task);
  JsonParser *json_parser = NULL;
  JsonObject *result;
  JsonNode *printers = NULL;
  GError *error = NULL;

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: (%p) 'search' REST call "
		     "returned\n", account));

  if (cb_error != NULL)
    {
      error = g_error_copy (cb_error);
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  if (g_task_return_error_if_cancelled (task))
    {
      g_object_unref (task);
      return;
    }

  if ((json_parser = cloudprint_json_parse (call, &result, &error)) == NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_object_unref (json_parser);

  if (json_object_has_member (result, "printers"))
    printers = json_object_dup_member (result, "printers");

  json_object_unref (result);
  if (printers == NULL)
    {
      g_task_return_new_error (task,
			       gtk_print_error_quark (),
			       GTK_PRINT_ERROR_INTERNAL_ERROR,
			       "Bad reply to 'search' request");
      return;
    }

  g_task_return_pointer (task,
			 printers,
			 (GDestroyNotify) json_node_free);
  g_object_unref (task);
}

static void
gtk_cloudprint_account_got_oauth2_access_token_cb (GObject *source,
						   GAsyncResult *result,
						   gpointer user_data)
{
  GTask *task = user_data;
  GtkCloudprintAccount *account = g_task_get_task_data (task);
  RestProxyCall *call;
  RestProxy *rest;
  GVariant *output;
  gint   expires_in = 0;
  GError *error = NULL;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source),
					  result,
					  &error);
  g_object_unref (source);

  if (output == NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_variant_get (output, "(si)",
		 &account->oauth2_access_token,
		 &expires_in);
  g_variant_unref (output);

  rest = oauth2_proxy_new_with_token (account->printer_id,
				      account->oauth2_access_token,
				      "https://accounts.google.com/o/oauth2/token",
				      "https://www.google.com/cloudprint/",
				      FALSE);

  if (rest == NULL)
    {
      g_task_return_new_error (task,
			       gtk_print_error_quark (),
			       GTK_PRINT_ERROR_INTERNAL_ERROR,
			       "REST proxy creation failed");
      g_object_unref (task);
      return;
    }

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: (%p) 'search' REST call\n",
		     account));

  account->rest_proxy = g_object_ref (rest);

  call = rest_proxy_new_call (REST_PROXY (rest));
  g_object_unref (rest);
  rest_proxy_call_set_function (call, "search");
  rest_proxy_call_add_header (call, "X-CloudPrint-Proxy", CLOUDPRINT_PROXY);
  rest_proxy_call_add_param (call, "connection_status", "ALL");
  if (!rest_proxy_call_async (call,
			      gtk_cloudprint_account_search_rest_call_cb,
			      NULL,
			      task,
			      &error))
    {
      g_task_return_error (task, error);
      g_object_unref (task);
    }

  g_object_unref (call);
}

static void
gtk_cloudprint_account_ensure_credentials_cb (GObject *source,
					      GAsyncResult *result,
					      gpointer user_data)
{
  GTask *task = user_data;
  GtkCloudprintAccount *account = g_task_get_task_data (task);
  GVariant *output;
  gint expires_in = 0;
  GError *error = NULL;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source),
					  result,
					  &error);

  if (output == NULL)
    {
      g_object_unref (source);
      if (error->domain != G_DBUS_ERROR ||
	  (error->code != G_DBUS_ERROR_SERVICE_UNKNOWN &&
	   error->code != G_DBUS_ERROR_UNKNOWN_METHOD))
	g_task_return_error (task, error);
      else
	/* Return an empty list. */
	g_task_return_pointer (task,
			       json_node_new (JSON_NODE_ARRAY),
			       (GDestroyNotify) json_node_free);

      g_object_unref (task);
      return;
    }

  g_variant_get (output, "(i)",
		 &expires_in);
  g_variant_unref (output);

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: (%p) getting access token\n",
		     account));

  g_dbus_connection_call (G_DBUS_CONNECTION (source),
			  ONLINE_ACCOUNTS_BUS,
			  account->goa_path,
			  O_AUTH2_BASED_IFACE,
			  "GetAccessToken",
			  NULL,
			  G_VARIANT_TYPE ("(si)"),
			  G_DBUS_CALL_FLAGS_NONE,
			  -1,
			  g_task_get_cancellable (task),
			  gtk_cloudprint_account_got_oauth2_access_token_cb,
			  task);
}

void
gtk_cloudprint_account_search (GtkCloudprintAccount *account,
			       GDBusConnection *dbus_connection,
			       GCancellable *cancellable,
			       GAsyncReadyCallback callback,
			       gpointer user_data)
{
  GTask *task = g_task_new (G_OBJECT (account),
			    cancellable,
			    callback,
			    user_data);
  g_task_set_task_data (task,
			g_object_ref (account),
			(GDestroyNotify) g_object_unref);

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: (%p) ensuring credentials\n",
		     account));

  g_dbus_connection_call (g_object_ref (dbus_connection),
			  ONLINE_ACCOUNTS_BUS,
			  account->goa_path,
			  ACCOUNT_IFACE,
			  "EnsureCredentials",
			  NULL,
			  G_VARIANT_TYPE ("(i)"),
			  G_DBUS_CALL_FLAGS_NONE,
			  -1,
			  cancellable,
			  gtk_cloudprint_account_ensure_credentials_cb,
			  task);
}

JsonNode *
gtk_cloudprint_account_search_finish (GtkCloudprintAccount *account,
				      GAsyncResult *result,
				      GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, account), NULL);
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
gtk_cloudprint_account_printer_rest_call_cb (RestProxyCall *call,
					     const GError *cb_error,
					     GObject *weak_object,
					     gpointer user_data)
{
  GTask *task = user_data;
  GtkCloudprintAccount *account = g_task_get_task_data (task);
  JsonParser *json_parser = NULL;
  JsonObject *result;
  GError *error = NULL;

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: (%p) 'printer' REST call "
		     "returned\n", account));

  if (cb_error != NULL)
    {
      error = g_error_copy (cb_error);
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  if (g_task_return_error_if_cancelled (task))
    {
      g_object_unref (task);
      return;
    }

  if ((json_parser = cloudprint_json_parse (call, &result, &error)) == NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_object_unref (json_parser);
  g_task_return_pointer (task,
			 result,
			 (GDestroyNotify) json_object_unref);
  g_object_unref (task);
}

void
gtk_cloudprint_account_printer (GtkCloudprintAccount *account,
				const gchar *printerid,
				GCancellable *cancellable,
				GAsyncReadyCallback callback,
				gpointer user_data)
{
  RestProxyCall *call;
  GTask *task;
  GError *error = NULL;

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: (%p) 'printer' REST call for "
		     "printer id %s", account, printerid));

  task = g_task_new (G_OBJECT (account), cancellable, callback, user_data);

  g_task_set_task_data (task,
			g_object_ref (account),
			(GDestroyNotify) g_object_unref);

  call = rest_proxy_new_call (REST_PROXY (account->rest_proxy));
  rest_proxy_call_set_function (call, "printer");
  rest_proxy_call_add_header (call, "X-CloudPrint-Proxy", CLOUDPRINT_PROXY);
  rest_proxy_call_add_param (call, "printerid", printerid);
  if (!rest_proxy_call_async (call,
			      gtk_cloudprint_account_printer_rest_call_cb,
			      NULL,
			      task,
			      &error))
    {
      g_task_return_error (task, error);
      g_object_unref (task);
    }

  g_object_unref (call);
}

JsonObject *
gtk_cloudprint_account_printer_finish (GtkCloudprintAccount *account,
				       GAsyncResult *result,
				       GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, account), NULL);
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
gtk_cloudprint_account_submit_rest_call_cb (RestProxyCall *call,
					    const GError *cb_error,
					    GObject *weak_object,
					    gpointer user_data)
{
  GTask *task = user_data;
  GtkCloudprintAccount *account = g_task_get_task_data (task);
  JsonParser *json_parser = NULL;
  JsonObject *result;
  GError *error = NULL;

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: (%p) 'submit' REST call "
		     "returned\n", account));

  if (cb_error != NULL)
    {
      error = g_error_copy (cb_error);
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  if (g_task_return_error_if_cancelled (task))
    {
      g_object_unref (task);
      return;
    }

  if ((json_parser = cloudprint_json_parse (call, &result, &error)) == NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_object_unref (json_parser);
  g_task_return_pointer (task,
			 result,
			 (GDestroyNotify) json_object_unref);
  g_object_unref (task);
}

void
gtk_cloudprint_account_submit (GtkCloudprintAccount *account,
			       GtkPrinterCloudprint *printer,
			       GMappedFile *file,
			       const gchar *title,
			       GCancellable *cancellable,
			       GAsyncReadyCallback callback,
			       gpointer user_data)
{
  GTask *task;
  RestProxyCall *call;
  gchar *printerid = NULL;
  RestParam *param;
  GError *error = NULL;
  gchar *auth;

  g_object_get (printer,
		"printer-id", &printerid,
		NULL);

  g_warn_if_fail (printerid != NULL);

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: (%p) 'submit' REST call for "
		     "printer id %s\n", account, printerid));

  task = g_task_new (G_OBJECT (account),
		     cancellable,
		     callback,
		     user_data);

  g_task_set_task_data (task,
			g_object_ref (account),
			(GDestroyNotify) g_object_unref);

  call = rest_proxy_new_call (REST_PROXY (account->rest_proxy));
  rest_proxy_call_set_method (call, "POST");
  rest_proxy_call_set_function (call, "submit");

  auth = g_strdup_printf ("Bearer %s", account->oauth2_access_token);
  rest_proxy_call_add_header (call, "Authorization", auth);
  g_free (auth);
  rest_proxy_call_add_header (call, "X-CloudPrint-Proxy", CLOUDPRINT_PROXY);

  rest_proxy_call_add_param (call, "printerid", printerid);
  g_free (printerid);

  rest_proxy_call_add_param (call, "contentType", "dataUrl");
  rest_proxy_call_add_param (call, "title", title);
  param = rest_param_new_with_owner ("content",
				     g_mapped_file_get_contents (file),
				     g_mapped_file_get_length (file),
				     "dataUrl",
				     NULL,
				     file,
				     (GDestroyNotify) g_mapped_file_unref);
  rest_proxy_call_add_param_full (call, param);

  if (!rest_proxy_call_async (call,
			      gtk_cloudprint_account_submit_rest_call_cb,
			      NULL,
			      task,
			      &error))
    {
      g_task_return_error (task, error);
      g_object_unref (call);
      g_object_unref (task);
      return;
    }

  g_object_unref (call);
}

JsonObject *
gtk_cloudprint_account_submit_finish (GtkCloudprintAccount *account,
				      GAsyncResult *result,
				      GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, account), NULL);
  return g_task_propagate_pointer (G_TASK (result), error);
}

const gchar *
gtk_cloudprint_account_get_presentation_identity (GtkCloudprintAccount *account)
{
  return account->presentation_identity;
}
