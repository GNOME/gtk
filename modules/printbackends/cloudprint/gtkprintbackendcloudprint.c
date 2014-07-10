/* gtkprintbackendcloudprint.c: Google Cloud Print implementation of
 * GtkPrintBackend
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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <cairo.h>
#include <cairo-pdf.h>
#include <cairo-ps.h>

#include <glib/gi18n-lib.h>

#include <gtk/gtkprintbackend.h>
#include <gtk/gtkunixprint.h>
#include <gtk/gtkprinter-private.h>

#include "gtkprintbackendcloudprint.h"
#include "gtkcloudprintaccount.h"
#include "gtkprintercloudprint.h"

typedef struct _GtkPrintBackendCloudprintClass GtkPrintBackendCloudprintClass;

#define GTK_PRINT_BACKEND_CLOUDPRINT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINT_BACKEND_CLOUDPRINT, GtkPrintBackendCloudprintClass))
#define GTK_IS_PRINT_BACKEND_CLOUDPRINT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINT_BACKEND_CLOUDPRINT))
#define GTK_PRINT_BACKEND_CLOUDPRINT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINT_BACKEND_CLOUDPRINT, GtkPrintBackendCloudprintClass))

#define _STREAM_MAX_CHUNK_SIZE 8192

#define ONLINE_ACCOUNTS_PATH "/org/gnome/OnlineAccounts"
#define OBJECT_MANAGER_IFACE "org.freedesktop.DBus.ObjectManager"

static GType print_backend_cloudprint_type = 0;

struct _GtkPrintBackendCloudprintClass
{
  GtkPrintBackendClass parent_class;
};

struct _GtkPrintBackendCloudprint
{
  GtkPrintBackend parent_instance;
  GCancellable *cancellable;
  guint accounts_searching;
};

struct
{
  gchar *id;
  gchar *path;
  gchar *presentation_identity;
} typedef TGOAAccount;

static GObjectClass *backend_parent_class;
static void                 gtk_print_backend_cloudprint_class_init      (GtkPrintBackendCloudprintClass *class);
static void                 gtk_print_backend_cloudprint_init            (GtkPrintBackendCloudprint      *impl);
static void                 gtk_print_backend_cloudprint_finalize	 (GObject *object);
static void                 cloudprint_printer_get_settings_from_options (GtkPrinter           *printer,
									  GtkPrinterOptionSet  *options,
									  GtkPrintSettings     *settings);
static GtkPrinterOptionSet *cloudprint_printer_get_options               (GtkPrinter           *printer,
									  GtkPrintSettings     *settings,
									  GtkPageSetup         *page_setup,
									  GtkPrintCapabilities capabilities);
static void                 cloudprint_printer_prepare_for_print         (GtkPrinter       *printer,
									  GtkPrintJob      *print_job,
									  GtkPrintSettings *settings,
									  GtkPageSetup     *page_setup);
static void		    cloudprint_request_printer_list		 (GtkPrintBackend *print_backend);
static void                 gtk_print_backend_cloudprint_print_stream    (GtkPrintBackend         *print_backend,
									  GtkPrintJob             *job,
									  GIOChannel              *data_io,
									  GtkPrintJobCompleteFunc callback,
									  gpointer                user_data,
									  GDestroyNotify          dnotify);
static cairo_surface_t *    cloudprint_printer_create_cairo_surface      (GtkPrinter       *printer,
									  GtkPrintSettings *settings,
									  gdouble          width,
									  gdouble          height,
									  GIOChannel       *cache_io);
static void                 cloudprint_printer_request_details           (GtkPrinter *printer);
TGOAAccount *        t_goa_account_copy                 (TGOAAccount *account);
void                 t_goa_account_free                 (gpointer data);



static void
gtk_print_backend_cloudprint_register_type (GTypeModule *module)
{
  const GTypeInfo print_backend_cloudprint_info =
  {
    sizeof (GtkPrintBackendCloudprintClass),
    NULL,		/* base_init */
    NULL,		/* base_finalize */
    (GClassInitFunc) gtk_print_backend_cloudprint_class_init,
    NULL,		/* class_finalize */
    NULL,		/* class_data */
    sizeof (GtkPrintBackendCloudprint),
    0,		/* n_preallocs */
    (GInstanceInitFunc) gtk_print_backend_cloudprint_init,
  };

  print_backend_cloudprint_type = g_type_module_register_type (module,
							 GTK_TYPE_PRINT_BACKEND,
							 "GtkPrintBackendCloudprint",
							 &print_backend_cloudprint_info, 0);
}

G_MODULE_EXPORT void
pb_module_init (GTypeModule *module)
{
  gtk_print_backend_cloudprint_register_type (module);
  gtk_cloudprint_account_register_type (module);
  gtk_printer_cloudprint_register_type (module);
}

G_MODULE_EXPORT void
pb_module_exit (void)
{

}

G_MODULE_EXPORT GtkPrintBackend *
pb_module_create (void)
{
  return gtk_print_backend_cloudprint_new ();
}

/*
 * GtkPrintBackendCloudprint
 */
GType
gtk_print_backend_cloudprint_get_type (void)
{
  return print_backend_cloudprint_type;
}

/**
 * gtk_print_backend_cloudprint_new:
 *
 * Creates a new #GtkPrintBackendCloudprint
 * object. #GtkPrintBackendCloudprint implements the #GtkPrintBackend
 * interface using REST API calls to the Google Cloud Print service.
 *
 * Returns: the new #GtkPrintBackendCloudprint object
 **/
GtkPrintBackend *
gtk_print_backend_cloudprint_new (void)
{
  return g_object_new (GTK_TYPE_PRINT_BACKEND_CLOUDPRINT, NULL);
}

static void
gtk_print_backend_cloudprint_class_init (GtkPrintBackendCloudprintClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_CLASS (klass);

  backend_parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gtk_print_backend_cloudprint_finalize;

  backend_class->request_printer_list = cloudprint_request_printer_list;
  backend_class->print_stream = gtk_print_backend_cloudprint_print_stream;
  backend_class->printer_create_cairo_surface = cloudprint_printer_create_cairo_surface;
  backend_class->printer_get_options = cloudprint_printer_get_options;
  backend_class->printer_get_settings_from_options = cloudprint_printer_get_settings_from_options;
  backend_class->printer_prepare_for_print = cloudprint_printer_prepare_for_print;
  backend_class->printer_request_details = cloudprint_printer_request_details;
}

static void
gtk_print_backend_cloudprint_init (GtkPrintBackendCloudprint *backend)
{
  backend->cancellable = g_cancellable_new ();

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: +GtkPrintBackendCloudprint(%p)\n",
		     backend));
}

static void
gtk_print_backend_cloudprint_finalize (GObject *object)
{
  GtkPrintBackendCloudprint *backend;

  backend = GTK_PRINT_BACKEND_CLOUDPRINT (object);

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: -GtkPrintBackendCloudprint(%p)\n",
		     backend));

  g_cancellable_cancel (backend->cancellable);
  g_clear_object (&(backend->cancellable));

  backend_parent_class->finalize (object);
}

static cairo_status_t
_cairo_write (void                *closure,
	      const unsigned char *data,
	      unsigned int         length)
{
  GIOChannel *io = (GIOChannel *)closure;
  gsize written;
  GError *error;

  error = NULL;

  while (length > 0)
    {
      g_io_channel_write_chars (io, (const gchar *) data, length, &written, &error);

      if (error != NULL)
	{
	  GTK_NOTE (PRINTING,
		     g_print ("Cloud Print Backend: Error writing to temp file, %s\n", error->message));

	  g_error_free (error);
	  return CAIRO_STATUS_WRITE_ERROR;
	}

      data += written;
      length -= written;
    }

  return CAIRO_STATUS_SUCCESS;
}


static cairo_surface_t *
cloudprint_printer_create_cairo_surface (GtkPrinter       *printer,
				   GtkPrintSettings *settings,
				   gdouble           width,
				   gdouble           height,
				   GIOChannel       *cache_io)
{
  cairo_surface_t *surface;

  surface = cairo_pdf_surface_create_for_stream (_cairo_write, cache_io, width, height);

  cairo_surface_set_fallback_resolution (surface,
					 2.0 * gtk_print_settings_get_printer_lpi (settings),
					 2.0 * gtk_print_settings_get_printer_lpi (settings));

  return surface;
}

typedef struct {
  GtkPrintBackend *backend;
  GtkPrintJobCompleteFunc callback;
  GtkPrintJob *job;
  GIOChannel *target_io;
  gpointer user_data;
  GDestroyNotify dnotify;
  gchar *path;

  /* Base64 encoding state */
  gint b64state;
  gint b64save;
} _PrintStreamData;

static void
cloudprint_submit_cb (GObject *source,
		      GAsyncResult *res,
		      gpointer user_data)
{
  GtkCloudprintAccount *account = GTK_CLOUDPRINT_ACCOUNT (source);
  _PrintStreamData *ps = (_PrintStreamData *) user_data;
  JsonObject *result;
  GError *error = NULL;
  gboolean success = FALSE;

  result = gtk_cloudprint_account_submit_finish (account, res, &error);
  g_object_unref (account);
  if (result == NULL)
    {
      GTK_NOTE (PRINTING,
		g_print ("Cloud Print Backend: submit REST reply: %s\n",
			 error->message));
      goto done;
    }

  json_object_unref (result);
  success = TRUE;

 done:
  if (ps->callback != NULL)
    ps->callback (ps->job, ps->user_data, error);

  if (ps->dnotify != NULL)
    ps->dnotify (ps->user_data);

  gtk_print_job_set_status (ps->job,
			    (success ?
			     GTK_PRINT_STATUS_FINISHED :
			     GTK_PRINT_STATUS_FINISHED_ABORTED));

  g_clear_object (&(ps->job));
  g_clear_object (&(ps->backend));
  g_clear_pointer (&error, g_error_free);

  g_free (ps->path);
  g_free (ps);
}

static void
cloudprint_print_cb (GtkPrintBackendCloudprint *print_backend,
		     GError              *cb_error,
		     gpointer            user_data)
{
  _PrintStreamData *ps = (_PrintStreamData *) user_data;
  gsize encodedlen;
  gchar encoded[4]; /* Up to 4 bytes are needed to finish encoding */
  GError *error = NULL;

  encodedlen = g_base64_encode_close (FALSE,
				      encoded,
				      &ps->b64state,
				      &ps->b64save);

  if (encodedlen > 0)
    g_io_channel_write_chars (ps->target_io,
			      encoded,
			      encodedlen,
			      NULL,
			      &error);

  if (ps->target_io != NULL)
    g_io_channel_unref (ps->target_io);

  if (cb_error == NULL)
    {
      GMappedFile *map = g_mapped_file_new (ps->path, FALSE, &error);
      GtkPrinter *printer = gtk_print_job_get_printer (ps->job);
      GtkCloudprintAccount *account = NULL;

      if (map == NULL)
	{
	  GTK_NOTE (PRINTING,
		    g_printerr ("Cloud Print Backend: failed to map file: %s\n",
				error->message));
	  g_error_free (error);
	  goto out;
	}

      g_object_get (printer,
		    "cloudprint-account", &account,
		    NULL);

      g_warn_if_fail (account != NULL);

      GTK_NOTE (PRINTING,
		g_print ("Cloud Print Backend: submitting job\n"));
      gtk_cloudprint_account_submit (account,
				     GTK_PRINTER_CLOUDPRINT (printer),
				     map,
				     gtk_print_job_get_title (ps->job),
				     print_backend->cancellable,
				     cloudprint_submit_cb,
				     ps);
    }

 out:
  if (ps->path != NULL)
    unlink (ps->path);

  if (cb_error != NULL || error != NULL)
    {
      if (ps->callback != NULL)
	ps->callback (ps->job, ps->user_data, error);

      if (ps->dnotify != NULL)
	ps->dnotify (ps->user_data);

      gtk_print_job_set_status (ps->job,
				GTK_PRINT_STATUS_FINISHED_ABORTED);

      g_clear_object (&(ps->job));
      g_free (ps->path);
      g_free (ps);
    }
}

static gboolean
cloudprint_write (GIOChannel   *source,
		  GIOCondition  con,
		  gpointer      user_data)
{
  gchar buf[_STREAM_MAX_CHUNK_SIZE];
  /* Base64 converts 24 bits into 32 bits, so divide the number of
   * bytes by 3 (rounding up) and multiply by 4. Also, if the previous
   * call left a non-zero state we may need an extra 4 bytes. */
  gchar encoded[(_STREAM_MAX_CHUNK_SIZE / 3 + 1) * 4 + 4];
  gsize bytes_read;
  GError *error = NULL;
  GIOStatus read_status;
  _PrintStreamData *ps = (_PrintStreamData *) user_data;

  read_status =
    g_io_channel_read_chars (source,
			     buf,
			     _STREAM_MAX_CHUNK_SIZE,
			     &bytes_read,
			     &error);

  if (read_status != G_IO_STATUS_ERROR)
    {
      gsize encodedlen = g_base64_encode_step ((guchar *) buf,
					       bytes_read,
					       FALSE,
					       encoded,
					       &ps->b64state,
					       &ps->b64save);

      g_io_channel_write_chars (ps->target_io,
				encoded,
				encodedlen,
				NULL,
				&error);
    }

  if (error != NULL || read_status == G_IO_STATUS_EOF)
    {
      cloudprint_print_cb (GTK_PRINT_BACKEND_CLOUDPRINT (ps->backend),
			   error, user_data);

      if (error != NULL)
	{
	  GTK_NOTE (PRINTING,
		    g_print ("Cloud Print Backend: %s\n", error->message));

	  g_error_free (error);
	}

      return FALSE;
    }

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: Writing %i byte chunk to tempfile\n", (int)bytes_read));

  return TRUE;
}

static void
gtk_print_backend_cloudprint_print_stream (GtkPrintBackend        *print_backend,
					   GtkPrintJob            *job,
					   GIOChannel             *data_io,
					   GtkPrintJobCompleteFunc callback,
					   gpointer                user_data,
					   GDestroyNotify          dnotify)
{
  const gchar *prefix = "data:application/pdf;base64,";
  GError *internal_error = NULL;
  _PrintStreamData *ps;
  int tmpfd;

  ps = g_new0 (_PrintStreamData, 1);
  ps->callback = callback;
  ps->user_data = user_data;
  ps->dnotify = dnotify;
  ps->job = g_object_ref (job);
  ps->backend = g_object_ref (print_backend);
  ps->path = g_strdup_printf ("%s/cloudprintXXXXXX.pdf.b64",
			      g_get_tmp_dir ());
  ps->b64state = 0;
  ps->b64save = 0;

  internal_error = NULL;

  if (ps->path == NULL)
    goto error;

  tmpfd = g_mkstemp (ps->path);
  if (tmpfd == -1)
    {
      int err = errno;
      internal_error = g_error_new (gtk_print_error_quark (),
				    GTK_PRINT_ERROR_INTERNAL_ERROR,
				    "Error creating temporary file: %s",
				    g_strerror (err));
      goto error;
    }

  ps->target_io = g_io_channel_unix_new (tmpfd);

  if (ps->target_io != NULL)
    {
      g_io_channel_set_close_on_unref (ps->target_io, TRUE);
      g_io_channel_set_encoding (ps->target_io, NULL, &internal_error);
    }

  g_io_channel_write_chars (ps->target_io,
			    prefix,
			    strlen (prefix),
			    NULL,
			    &internal_error);

error:
  if (internal_error != NULL)
    {
      cloudprint_print_cb (GTK_PRINT_BACKEND_CLOUDPRINT (print_backend),
			   internal_error, ps);

      g_error_free (internal_error);
      return;
    }

  g_io_add_watch (data_io,
		  G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
		  (GIOFunc) cloudprint_write,
		  ps);
}

TGOAAccount *
t_goa_account_copy (TGOAAccount *account)
{
  TGOAAccount *result = NULL;

  if (account != NULL)
    {
      result = g_new0 (TGOAAccount, 1);
      result->id = g_strdup (account->id);
      result->path = g_strdup (account->path);
      result->presentation_identity = g_strdup (account->presentation_identity);
    }

  return result;
}

void
t_goa_account_free (gpointer data)
{
  TGOAAccount *account = (TGOAAccount *) data;

  if (account != NULL)
    {
      g_free (account->id);
      g_free (account->path);
      g_free (account->presentation_identity);
      g_free (account);
    }
}

static GList *
get_accounts (GVariant *output)
{
  GVariant *objects;
  GList    *result = NULL;
  gint      i, j, k;

  g_variant_get (output, "(@a{oa{sa{sv}}})",
                 &objects);

  if (objects)
    {
      for (i = 0; i < g_variant_n_children (objects); i++)
	{
	  const gchar *object_name;
	  GVariant    *object_variant;

	  g_variant_get_child (objects, i, "{&o@a{sa{sv}}}",
			       &object_name,
			       &object_variant);

	  if (g_str_has_prefix (object_name, "/org/gnome/OnlineAccounts/Accounts/"))
	    {
	      for (j = 0; j < g_variant_n_children (object_variant); j++)
		{
		  const gchar *service_name;
		  GVariant    *service_variant;

		  g_variant_get_child (object_variant, j, "{&s@a{sv}}",
				       &service_name,
				       &service_variant);

		  if (g_str_has_prefix (service_name, "org.gnome.OnlineAccounts.Account"))
		    {
		      TGOAAccount *account;
		      gboolean     printers_disabled = FALSE;
		      gchar       *provider_type = NULL;

		      account = g_new0 (TGOAAccount, 1);

		      account->path = g_strdup (object_name);
		      for (k = 0; k < g_variant_n_children (service_variant); k++)
			{
			  const gchar *property_name;
			  GVariant    *property_variant;
			  GVariant    *value;

			  g_variant_get_child (service_variant, k, "{&s@v}",
					       &property_name,
					       &property_variant);

			  g_variant_get (property_variant, "v",
					 &value);

			  if (g_strcmp0 (property_name, "Id") == 0)
			    account->id = g_variant_dup_string (value, NULL);
			  else if (g_strcmp0 (property_name, "ProviderType") == 0)
			    provider_type = g_variant_dup_string (value, NULL);
			  else if (g_strcmp0 (property_name, "PrintersDisabled") == 0)
			    printers_disabled = g_variant_get_boolean (value);
			  else if (g_strcmp0 (property_name, "PresentationIdentity") == 0)
			    account->presentation_identity = g_variant_dup_string (value, NULL);

			  g_variant_unref (property_variant);
			  g_variant_unref (value);
			}

		      if (!printers_disabled &&
			  g_strcmp0 (provider_type, "google") == 0 &&
			  account->presentation_identity != NULL)
			result = g_list_append (result, account);
		      else
			t_goa_account_free (account);

		      g_free (provider_type);
		    }

		  g_variant_unref (service_variant);
		}
	    }

	  g_variant_unref (object_variant);
	}

      g_variant_unref (objects);
    }

  return result;
}

static void
cloudprint_search_cb (GObject *source,
		      GAsyncResult *res,
		      gpointer user_data)
{
  GtkCloudprintAccount *account = GTK_CLOUDPRINT_ACCOUNT (source);
  GtkPrintBackendCloudprint *backend = NULL;
  JsonNode *node;
  JsonArray *printers;
  guint i;
  GError *error = NULL;

  node = gtk_cloudprint_account_search_finish (account, res, &error);
  g_object_unref (account);
  if (node == NULL)
    {
      GTK_NOTE (PRINTING,
		g_print ("Cloud Print Backend: search failed: %s\n",
			 error->message));

      if (error->domain != G_IO_ERROR ||
	  error->code != G_IO_ERROR_CANCELLED)
	backend = GTK_PRINT_BACKEND_CLOUDPRINT (user_data);

      g_error_free (error);
      goto done;
    }

  backend = GTK_PRINT_BACKEND_CLOUDPRINT (user_data);
  printers = json_node_get_array (node);
  for (i = 0; i < json_array_get_length (printers); i++)
    {
      GtkPrinterCloudprint *printer;
      JsonObject *json_printer = json_array_get_object_element (printers, i);
      const char *name = NULL;
      const char *id = NULL;
      const char *type = NULL;
      const char *desc = NULL;
      const char *status = NULL;
      gboolean is_virtual;

      if (json_object_has_member (json_printer, "displayName"))
	name = json_object_get_string_member (json_printer, "displayName");

      if (json_object_has_member (json_printer, "id"))
	id = json_object_get_string_member (json_printer, "id");

      if (name == NULL || id == NULL)
	{
	  GTK_NOTE (PRINTING,
		    g_print ("Cloud Print Backend: ignoring incomplete "
			     "printer description\n"));
	  continue;
	}

      if (json_object_has_member (json_printer, "type"))
	type = json_object_get_string_member (json_printer, "type");

      if (json_object_has_member (json_printer, "description"))
	desc = json_object_get_string_member (json_printer, "description");

      if (json_object_has_member (json_printer, "connectionStatus"))
	status = json_object_get_string_member (json_printer,
						"connectionStatus");

      is_virtual = (type != NULL && !strcmp (type, "DOCS"));

      GTK_NOTE (PRINTING,
		g_print ("Cloud Print Backend: Adding printer %s\n", name));

      printer = gtk_printer_cloudprint_new (name,
					    is_virtual,
					    GTK_PRINT_BACKEND (backend),
					    account,
					    id);
      gtk_printer_set_has_details (GTK_PRINTER (printer), FALSE);
      gtk_printer_set_icon_name (GTK_PRINTER (printer), "printer");
      gtk_printer_set_location (GTK_PRINTER (printer),
				gtk_cloudprint_account_get_presentation_identity (account));

      if (desc != NULL)
	gtk_printer_set_description (GTK_PRINTER (printer), desc);

      if (status != NULL)
	{
	  if (!strcmp (status, "ONLINE"))
	    /* Translators: The printer status is online, i.e. it is
	     * ready to print. */
	    gtk_printer_set_state_message (GTK_PRINTER (printer), _("Online"));
	  else if (!strcmp (status, "UNKNOWN"))
	    /* Translators: We don't know whether this printer is
	     * available to print to. */
	    gtk_printer_set_state_message (GTK_PRINTER (printer), _("Unknown"));
	  else if (!strcmp (status, "OFFLINE"))
	    /* Translators: The printer is offline. */
	    gtk_printer_set_state_message (GTK_PRINTER (printer), _("Offline"));
	  else if (!strcmp (status, "DORMANT"))
	    /* We shouldn't get here because the query omits dormant
	     * printers by default. */

	    /* Translators: Printer has been offline for a long time. */
	    gtk_printer_set_state_message (GTK_PRINTER (printer), _("Dormant"));
	}

      gtk_printer_set_is_active (GTK_PRINTER (printer), TRUE);

      gtk_print_backend_add_printer (GTK_PRINT_BACKEND (backend),
				     GTK_PRINTER (printer));
      g_signal_emit_by_name (GTK_PRINT_BACKEND (backend),
			     "printer-added", GTK_PRINTER (printer));
      g_object_unref (printer);
    }

  json_node_free (node);

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: 'search' finished for account %p\n",
		     account));

 done:
  if (backend != NULL && --backend->accounts_searching == 0)
    {
      GTK_NOTE (PRINTING,
		g_print ("Cloud Print Backend: 'search' finished for "
			 "all accounts\n"));

      gtk_print_backend_set_list_done (GTK_PRINT_BACKEND (backend));
    }
}

static void
cloudprint_get_managed_objects_cb (GObject      *source,
                                   GAsyncResult *res,
                                   gpointer      user_data)
{
  GtkPrintBackendCloudprint *backend;
  GVariant *output;
  GError   *error = NULL;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), res, &error);

  if (output != NULL)
    {
      TGOAAccount *goa_account;
      GList       *accounts = NULL;
      GList       *iter;
      guint	   searching;

      GTK_NOTE (PRINTING,
                g_print ("Cloud Print Backend: got objects managed by goa\n"));

      backend = GTK_PRINT_BACKEND_CLOUDPRINT (user_data);

      accounts = get_accounts (output);
      g_variant_unref (output);
      searching = backend->accounts_searching = g_list_length (accounts);

      for (iter = accounts; iter != NULL; iter = iter->next)
        {
	  GtkCloudprintAccount *account;
          goa_account = (TGOAAccount *) iter->data;
	  account = gtk_cloudprint_account_new (goa_account->id,
						goa_account->path,
						goa_account->presentation_identity);
	  if (account == NULL)
	    {
	      GTK_NOTE (PRINTING,
			g_print ("Cloud Print Backend: error constructing "
				 "account object"));
	      backend->accounts_searching--;
	      searching--;
	      continue;
	    }

	  GTK_NOTE (PRINTING,
		    g_print ("Cloud Print Backend: issuing 'search' for %p\n",
			     account));

	  gtk_cloudprint_account_search (account,
					 G_DBUS_CONNECTION (source),
					 backend->cancellable,
					 cloudprint_search_cb,
					 GTK_PRINT_BACKEND (backend));
        }

      if (searching == 0)
        gtk_print_backend_set_list_done (GTK_PRINT_BACKEND (backend));

      g_list_free_full (accounts, t_goa_account_free);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        {
          if (error->domain != G_DBUS_ERROR ||
              (error->code != G_DBUS_ERROR_SERVICE_UNKNOWN &&
               error->code != G_DBUS_ERROR_UNKNOWN_METHOD))
            {
              GTK_NOTE (PRINTING,
                        g_print ("Cloud Print Backend: failed to get objects managed by goa: %s\n",
                                 error->message));
              g_warning ("%s", error->message);
            }

          gtk_print_backend_set_list_done (GTK_PRINT_BACKEND (user_data));
        }

      g_error_free (error);
    }

  g_object_unref (source);
}

static void
cloudprint_bus_get_cb (GObject      *source,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GtkPrintBackendCloudprint *backend;
  GDBusConnection *connection;
  GError *error = NULL;

  connection = g_bus_get_finish (res, &error);

  if (connection != NULL)
    {
      backend = GTK_PRINT_BACKEND_CLOUDPRINT (user_data);

      GTK_NOTE (PRINTING,
                g_print ("Cloud Print Backend: got connection to session bus\n"));

      g_dbus_connection_call (connection,
                              ONLINE_ACCOUNTS_BUS,
                              ONLINE_ACCOUNTS_PATH,
                              OBJECT_MANAGER_IFACE,
                              "GetManagedObjects",
                              NULL,
                              G_VARIANT_TYPE ("(a{oa{sa{sv}}})"),
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              backend->cancellable,
                              cloudprint_get_managed_objects_cb,
                              backend);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
	  error->code != G_IO_ERROR_CANCELLED)
        {
          GTK_NOTE (PRINTING,
                    g_print ("Cloud Print Backend: failed getting session bus: %s\n",
                             error->message));
          g_warning ("%s", error->message);

          gtk_print_backend_set_list_done (GTK_PRINT_BACKEND (user_data));
        }
      g_error_free (error);
    }
}

static void
cloudprint_request_printer_list (GtkPrintBackend *print_backend)
{
  GtkPrintBackendCloudprint *backend = GTK_PRINT_BACKEND_CLOUDPRINT (print_backend);

  g_cancellable_reset (backend->cancellable);
  g_bus_get (G_BUS_TYPE_SESSION, backend->cancellable, cloudprint_bus_get_cb, backend);
}

static GtkPrinterOptionSet *
cloudprint_printer_get_options (GtkPrinter           *printer,
			  GtkPrintSettings     *settings,
			  GtkPageSetup         *page_setup,
			  GtkPrintCapabilities  capabilities)
{
  GtkPrinterOptionSet *set;
  GtkPrinterOption *option;
  const gchar *n_up[] = { "1" };

  set = gtk_printer_option_set_new ();

  /* How many document pages to go onto one side of paper. */
  option = gtk_printer_option_new ("gtk-n-up", _("Pages per _sheet:"), GTK_PRINTER_OPTION_TYPE_PICKONE);
  gtk_printer_option_choices_from_array (option, G_N_ELEMENTS (n_up),
					 (char **) n_up, (char **) n_up /* FIXME i18n (localised digits)! */);
  gtk_printer_option_set (option, "1");
  gtk_printer_option_set_add (set, option);
  g_object_unref (option);

  return set;
}

static void
cloudprint_printer_get_settings_from_options (GtkPrinter          *printer,
					      GtkPrinterOptionSet *options,
					      GtkPrintSettings    *settings)
{
}

static void
cloudprint_printer_prepare_for_print (GtkPrinter       *printer,
				      GtkPrintJob      *print_job,
				      GtkPrintSettings *settings,
				      GtkPageSetup     *page_setup)
{
  gdouble scale;

  gtk_print_job_set_pages (print_job, gtk_print_settings_get_print_pages (settings));
  gtk_print_job_set_page_ranges (print_job, NULL, 0);

  if (gtk_print_job_get_pages (print_job) == GTK_PRINT_PAGES_RANGES)
    {
      GtkPageRange *page_ranges;
      gint num_page_ranges;
      page_ranges = gtk_print_settings_get_page_ranges (settings, &num_page_ranges);
      gtk_print_job_set_page_ranges (print_job, page_ranges, num_page_ranges);
    }

  gtk_print_job_set_collate (print_job, gtk_print_settings_get_collate (settings));
  gtk_print_job_set_reverse (print_job, gtk_print_settings_get_reverse (settings));
  gtk_print_job_set_num_copies (print_job, gtk_print_settings_get_n_copies (settings));

  scale = gtk_print_settings_get_scale (settings);
  if (scale != 100.0)
    gtk_print_job_set_scale (print_job, scale/100.0);

  gtk_print_job_set_page_set (print_job, gtk_print_settings_get_page_set (settings));
  gtk_print_job_set_rotate (print_job, TRUE);
}

static void
cloudprint_printer_cb (GObject *source,
		       GAsyncResult *res,
		       gpointer user_data)
{
  GtkCloudprintAccount *account = GTK_CLOUDPRINT_ACCOUNT (source);
  GtkPrinter *printer = GTK_PRINTER (user_data);
  JsonObject *result;
  GError *error = NULL;
  gboolean success = FALSE;

  result = gtk_cloudprint_account_printer_finish (account, res, &error);
  if (result != NULL)
    {
      /* Ignore capabilities for now. */
      json_object_unref (result);
      success = TRUE;
    }
  else
    {
      GTK_NOTE (PRINTING,
		g_print ("Cloud Print Backend: failure getting details: %s\n",
			 error->message));

      if (error->domain == G_IO_ERROR &&
	  error->code == G_IO_ERROR_CANCELLED)
	{
	  g_error_free (error);
	  return;
	}

      g_error_free (error);
    }

  gtk_printer_set_has_details (printer, success);
  g_signal_emit_by_name (printer, "details-acquired", success);
}

static void
cloudprint_printer_request_details (GtkPrinter *printer)
{
  GtkPrintBackendCloudprint *backend;
  GtkCloudprintAccount *account = NULL;
  gchar *printerid = NULL;

  g_object_get (printer,
		"cloudprint-account", &account,
		"printer-id", &printerid,
		NULL);

  g_warn_if_fail (account != NULL);
  g_warn_if_fail (printerid != NULL);

  backend = GTK_PRINT_BACKEND_CLOUDPRINT (gtk_printer_get_backend (printer));

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: Getting details for printer id %s\n",
		     printerid));

  gtk_cloudprint_account_printer (account,
				  printerid,
				  backend->cancellable,
				  cloudprint_printer_cb,
				  printer);
  g_object_unref (account);
  g_free (printerid);
}
