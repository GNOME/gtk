/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gtksignal.h"
#include "gtkimmulticontext.h"
#include "gtkimcontextsimple.h"

static void     gtk_im_multicontext_class_init         (GtkIMMulticontextClass  *class);
static void     gtk_im_multicontext_init               (GtkIMMulticontext       *im_multicontext);
static void     gtk_im_multicontext_finalize           (GObject                 *object);

static void     gtk_im_multicontext_set_slave          (GtkIMMulticontext       *multicontext,
							GtkIMContext            *slave);

static void     gtk_im_multicontext_set_client_window  (GtkIMContext            *context,
							GdkWindow               *window);
static void     gtk_im_multicontext_get_preedit_string (GtkIMContext            *context,
							gchar                  **str,
							PangoAttrList          **attrs);
static gboolean gtk_im_multicontext_filter_keypress    (GtkIMContext            *context,
							GdkEventKey             *event);
static void     gtk_im_multicontext_focus_in           (GtkIMContext            *context);
static void     gtk_im_multicontext_focus_out          (GtkIMContext            *context);

void            gtk_im_multicontext_preedit_start_cb   (GtkIMContext            *slave,
							GtkIMMulticontext       *multicontext);
void            gtk_im_multicontext_preedit_end_cb     (GtkIMContext            *slave,
							GtkIMMulticontext       *multicontext);
void            gtk_im_multicontext_preedit_changed_cb (GtkIMContext            *slave,
							GtkIMMulticontext       *multicontext);
void            gtk_im_multicontext_commit_cb          (GtkIMContext            *slave,
							const gchar             *str,
							GtkIMMulticontext       *multicontext);

static GtkIMContextClass *parent_class;

GtkType
gtk_im_multicontext_get_type (void)
{
  static GtkType im_multicontext_type = 0;

  if (!im_multicontext_type)
    {
      static const GtkTypeInfo im_multicontext_info =
      {
	"GtkIMMulticontext",
	sizeof (GtkIMMulticontext),
	sizeof (GtkIMMulticontextClass),
	(GtkClassInitFunc) gtk_im_multicontext_class_init,
	(GtkObjectInitFunc) gtk_im_multicontext_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      im_multicontext_type = gtk_type_unique (GTK_TYPE_IM_CONTEXT, &im_multicontext_info);
    }

  return im_multicontext_type;
}

static void
gtk_im_multicontext_class_init (GtkIMMulticontextClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (class);
  
  parent_class = g_type_class_peek_parent (class);

  im_context_class->set_client_window = gtk_im_multicontext_set_client_window;
  im_context_class->get_preedit_string = gtk_im_multicontext_get_preedit_string;
  im_context_class->filter_keypress = gtk_im_multicontext_filter_keypress;
  im_context_class->focus_in = gtk_im_multicontext_focus_in;
  im_context_class->focus_out = gtk_im_multicontext_focus_out;

  gobject_class->finalize = gtk_im_multicontext_finalize;
}

static void
gtk_im_multicontext_init (GtkIMMulticontext *multicontext)
{
  multicontext->slave = NULL;
}

GtkIMContext *
gtk_im_multicontext_new (void)
{
  return GTK_IM_CONTEXT (gtk_type_new (GTK_TYPE_IM_MULTICONTEXT));
}

static void
gtk_im_multicontext_finalize (GObject *object)
{
  gtk_im_multicontext_set_slave (GTK_IM_MULTICONTEXT (object), NULL);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_im_multicontext_set_slave (GtkIMMulticontext *multicontext,
			       GtkIMContext      *slave)
{
  if (multicontext->slave)
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (multicontext->slave), multicontext);
      gtk_object_unref (GTK_OBJECT (multicontext->slave));
    }
  
  multicontext->slave = slave;

  if (multicontext->slave)
    {
      gtk_object_ref (GTK_OBJECT (multicontext->slave));
      gtk_object_sink (GTK_OBJECT (multicontext->slave));

      gtk_signal_connect (GTK_OBJECT (multicontext->slave), "preedit_start",
			  GTK_SIGNAL_FUNC (gtk_im_multicontext_preedit_start_cb),
			  multicontext);
      gtk_signal_connect (GTK_OBJECT (multicontext->slave), "preedit_end",
			  GTK_SIGNAL_FUNC (gtk_im_multicontext_preedit_end_cb),
			  multicontext);
      gtk_signal_connect (GTK_OBJECT (multicontext->slave), "preedit_changed",
			  GTK_SIGNAL_FUNC (gtk_im_multicontext_preedit_changed_cb),
			  multicontext);
      gtk_signal_connect (GTK_OBJECT (multicontext->slave), "commit",
			  GTK_SIGNAL_FUNC (gtk_im_multicontext_commit_cb),
			  multicontext);
    }
}

static GtkIMContext *
gtk_im_multicontext_get_slave (GtkIMMulticontext *multicontext)
{
  if (!multicontext->slave)
    gtk_im_multicontext_set_slave (multicontext, gtk_im_context_simple_new ());

  return multicontext->slave;
}

static void
gtk_im_multicontext_set_client_window (GtkIMContext *context,
				       GdkWindow    *window)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *slave = gtk_im_multicontext_get_slave (multicontext);

  if (slave)
    gtk_im_context_set_client_window (slave, window);
}

static void
gtk_im_multicontext_get_preedit_string (GtkIMContext   *context,
					gchar         **str,
					PangoAttrList **attrs)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *slave = gtk_im_multicontext_get_slave (multicontext);

  if (slave)
    gtk_im_context_get_preedit_string (slave, str, attrs);
  else
    {
      if (str)
	*str = g_strdup ("");
      if (attrs)
	*attrs = pango_attr_list_new ();
    }
}

static gboolean
gtk_im_multicontext_filter_keypress (GtkIMContext *context,
				     GdkEventKey  *event)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *slave = gtk_im_multicontext_get_slave (multicontext);

  if (slave)
    return gtk_im_context_filter_keypress (slave, event);
  else
    return FALSE;
}

static void
gtk_im_multicontext_focus_in (GtkIMContext   *context)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *slave = gtk_im_multicontext_get_slave (multicontext);

  if (slave)
    gtk_im_context_focus_in (slave);
}

static void
gtk_im_multicontext_focus_out (GtkIMContext   *context)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *slave = gtk_im_multicontext_get_slave (multicontext);

  if (slave)
    gtk_im_context_focus_out (slave);
}

void
gtk_im_multicontext_preedit_start_cb   (GtkIMContext      *slave,
					GtkIMMulticontext *multicontext)
{
  gtk_signal_emit_by_name (GTK_OBJECT (multicontext), "preedit_start");
}

void
gtk_im_multicontext_preedit_end_cb (GtkIMContext      *slave,
				    GtkIMMulticontext *multicontext)
{
  gtk_signal_emit_by_name (GTK_OBJECT (multicontext), "preedit_end");
}

void
gtk_im_multicontext_preedit_changed_cb (GtkIMContext      *slave,
					GtkIMMulticontext *multicontext)
{
  gtk_signal_emit_by_name (GTK_OBJECT (multicontext), "preedit_changed");
}

void
gtk_im_multicontext_commit_cb (GtkIMContext      *slave,
			       const gchar       *str,
			       GtkIMMulticontext *multicontext)
{
  gtk_signal_emit_by_name (GTK_OBJECT (multicontext), "commit", str);;
}

