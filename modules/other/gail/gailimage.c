/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
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
 */

#include <gtk/gtk.h>
#include "gailimage.h"
#include "gailintl.h"

static void      gail_image_class_init         (GailImageClass *klass);
static void      gail_image_object_init        (GailImage      *image);
static G_CONST_RETURN gchar* gail_image_get_name  (AtkObject     *accessible);


static void      atk_image_interface_init      (AtkImageIface  *iface);

static G_CONST_RETURN gchar *
                 gail_image_get_image_description (AtkImage     *image);
static void	 gail_image_get_image_position    (AtkImage     *image,
                                                   gint         *x,
                                                   gint         *y,
                                                   AtkCoordType coord_type);
static void      gail_image_get_image_size     (AtkImage        *image,
                                                gint            *width,
                                                gint            *height);
static gboolean  gail_image_set_image_description (AtkImage     *image,
                                                const gchar     *description);
static void      gail_image_finalize           (GObject         *object);
                                                         
typedef struct _GailImageItem GailImageItem;

struct _GailImageItem
{
  GQuark id;
  gchar *name;
  gchar *stock_id;
};

static GailImageItem stock_items [] =
{
  { 0, N_("dialog authentication"), "gtk-dialog-authentication"},
  { 0, N_("dialog information"), "gtk-dialog-info"},
  { 0, N_("dialog warning"), "gtk-dialog-warning"},
  { 0, N_("dialog error"), "gtk-dialog-error"},
  { 0, N_("dialog question"), "gtk-dialog-question"},
  { 0, N_("drag and drop"), "gtk-dnd"},
  { 0, N_("multiple drag and drop"), "gtk-dnd-multiple"},
  { 0, N_("add"), "gtk-add"},
  { 0, N_("apply"), "gtk-apply"},
  { 0, N_("bold"), "gtk-bold"},
  { 0, N_("cancel"), "gtk_cancel"},
  { 0, N_("cdrom"), "gtk-cdrom"},
  { 0, N_("clear"), "gtk-clear"},
  { 0, N_("close"), "gtk-close"},
  { 0, N_("color picker"), "gtk-color-picker"},
  { 0, N_("convert"), "gtk-convert"},
  { 0, N_("copy"), "gtk-copy"},
  { 0, N_("cut"), "gtk-cut"},
  { 0, N_("delete"), "gtk-delete"},
  { 0, N_("execute"), "gtk-execute"},
  { 0, N_("find"), "gtk-find"},
  { 0, N_("find and replace"), "gtk-find-and-replace"},
  { 0, N_("floppy"), "gtk-floppy"},
  { 0, N_("go to bottom"), "gtk-goto-bottom"},
  { 0, N_("go to first"), "gtk-goto-first"},
  { 0, N_("go to last"), "gtk-goto-last"},
  { 0, N_("go to top"), "gtk-goto-top"},
  { 0, N_("go back"), "gtk-go-back"},
  { 0, N_("go down"), "gtk-go-down"},
  { 0, N_("go forward"), "gtk-go-forward"},
  { 0, N_("go up"), "gtk-go-up"},
  { 0, N_("help"), "gtk-help"},
  { 0, N_("home"), "gtk-home"},
  { 0, N_("index"), "gtk-index"},
  { 0, N_("italic"), "gtk-italic"},
  { 0, N_("jump to"), "gtk-jump-to"},
  { 0, N_("justify center"), "gtk-justify-center"},
  { 0, N_("justify fill"), "gtk-justify-fill"},
  { 0, N_("justify left"), "gtk-justify-left"},
  { 0, N_("justify right"), "gtk-justify-right"},
  { 0, N_("missing image"), "gtk-missing-image"},
  { 0, N_("new"), "gtk-new"},
  { 0, N_("no"), "gtk-no"},
  { 0, N_("ok"), "gtk-ok"},
  { 0, N_("open"), "gtk-open"},
  { 0, N_("paste"), "gtk-paste"},
  { 0, N_("preferences"), "gtk-preferences"},
  { 0, N_("print"), "gtk-print"},
  { 0, N_("print preview"), "gtk-print-preview"},
  { 0, N_("properties"), "gtk-properties"},
  { 0, N_("quit"), "gtk-quit"},
  { 0, N_("redo"), "gtk-redo"},
  { 0, N_("refresh"), "gtk-refresh"},
  { 0, N_("remove"), "gtk-remove"},
  { 0, N_("revert to saved"), "gtk-revert-to-saved"},
  { 0, N_("save"), "gtk-save"},
  { 0, N_("save as"), "gtk-save-as"},
  { 0, N_("select color"), "gtk-select-color"},
  { 0, N_("select font"), "gtk-select-font"},
  { 0, N_("sort ascending"), "gtk-sort-ascending"},
  { 0, N_("sort descending"), "gtk-sort-descending"},
  { 0, N_("spell check"), "gtk-spell-check"},
  { 0, N_("stop"), "gtk-stop"},
  { 0, N_("strikethrough"), "gtk-strikethrough"},
  { 0, N_("undelete"), "gtk-undelete"},
  { 0, N_("underline"), "gtk-underline"},
  { 0, N_("yes"), "gtk-yes"},
  { 0, N_("zoom 100 percent"), "gtk-zoom-100"},
  { 0, N_("zoom fit"), "gtk-zoom-fit"},
  { 0, N_("zoom in"), "gtk-zoom-in"},
  { 0, N_("zoom out"), "gtk-zoom-out"},

  { 0, N_("timer"), "gnome-stock-timer"},
  { 0, N_("timer stop"), "gnome-stock-timer-stop"},
  { 0, N_("trash"), "gnome-stock-trash"},
  { 0, N_("trash full"), "gnome-stock-trash-full"},
  { 0, N_("scores"), "gnome-stock-scores"},
  { 0, N_("about"), "gnome-stock-about"},
  { 0, N_("blank"), "gnome-stock-blank"},
  { 0, N_("volume"), "gnome-stock-volume"},
  { 0, N_("midi"), "gnome-stock-midi"},
  { 0, N_("microphone"), "gnome-stock-mic"},
  { 0, N_("line in"), "gnome-stock-line-in"},
  { 0, N_("mail"), "gnome-stock-mail"},
  { 0, N_("mail receive"), "gnome-stock-mail-rcv"},
  { 0, N_("mail send"), "gnome-stock-mail-snd"},
  { 0, N_("mail reply"), "gnome-stock-mail-rpl"},
  { 0, N_("mail forward"), "gnome-stock-mail-fwd"},
  { 0, N_("mail new"), "gnome-stock-mail-new"},
  { 0, N_("attach"), "gnome-stock-attach"},
  { 0, N_("book red"), "gnome-stock-book-red"},
  { 0, N_("book green"), "gnome-stock-book-green"},
  { 0, N_("book blue"), "gnome-stock-book-blue"},
  { 0, N_("book yellow"), "gnome-stock-book-yellow"},
  { 0, N_("book open"), "gnome-stock-book-open"},
  { 0, N_("multiple file"), "gnome-stock-multiple-file"},
  { 0, N_("not"), "gnome-stock-not"},
  { 0, N_("table borders"), "gnome-stock-table-borders"},
  { 0, N_("table fill"), "gnome-stock-table-fill"},
  { 0, N_("text indent"), "gnome-stock-text-indent"},
  { 0, N_("text unindent"), "gnome-stock-text-unindent"},
  { 0, N_("text bulleted list"), "gnome-stock-text-bulleted-list"},
  { 0, N_("text numbered list"), "gnome-stock-text-numbered-list"},
  { 0, N_("authentication"), "gnome-stock-authentication"}
};

static gpointer parent_class = NULL;

GType
gail_image_get_type (void)
{
  static GType type = 0;

  if (!type)
  {
    static const GTypeInfo tinfo =
    {
      sizeof (GailImageClass),
      (GBaseInitFunc) NULL, /* base init */
      (GBaseFinalizeFunc) NULL, /* base finalize */
      (GClassInitFunc) gail_image_class_init, /* class init */
      (GClassFinalizeFunc) NULL, /* class finalize */
      NULL, /* class data */
      sizeof (GailImage), /* instance size */
      0, /* nb preallocs */
      (GInstanceInitFunc) gail_image_object_init, /* instance init */
      NULL /* value table */
    };

    static const GInterfaceInfo atk_image_info =
    {
        (GInterfaceInitFunc) atk_image_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };

    type = g_type_register_static (GAIL_TYPE_WIDGET,
                                   "GailImage", &tinfo, 0);

    g_type_add_interface_static (type, ATK_TYPE_IMAGE,
                                 &atk_image_info);
  }

  return type;
}

static void
gail_image_class_init (GailImageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gail_image_finalize;
  class->get_name = gail_image_get_name;
}

static void
gail_image_object_init (GailImage *image)
{
  image->image_description = NULL;
}

AtkObject* 
gail_image_new (GtkWidget *widget)
{
  GObject *object;
  AtkObject *accessible;

  g_return_val_if_fail (GTK_IS_IMAGE (widget), NULL);

  object = g_object_new (GAIL_TYPE_IMAGE, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, widget);

  accessible->role = ATK_ROLE_ICON;

  return accessible;
}

static void
init_strings (void)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (stock_items); i++)
    stock_items[i].id = g_quark_from_static_string (stock_items[i].stock_id);
}

static G_CONST_RETURN gchar*
get_localized_name (const gchar *str)
{
  GQuark str_q;
  gint i;

#if 0
  static gboolean gettext_initialized = FALSE;

  if (!gettext_initialized)
    {
      init_strings ();
      gettext_initialized = TRUE;
#ifdef ENABLE_NLS
      bindtextdomain (GETTEXT_PACKAGE, GAIL_LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
      bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
#endif
    }
#endif

  str_q = g_quark_try_string (str);
  for (i = 0; i <  G_N_ELEMENTS (stock_items); i++)
    {
      if (str_q == stock_items[i].id)
         return dgettext (GETTEXT_PACKAGE, stock_items[i].name);
  }

  return str;
}

static G_CONST_RETURN gchar*
gail_image_get_name (AtkObject *accessible)
{
  G_CONST_RETURN gchar *name;

  name = ATK_OBJECT_CLASS (parent_class)->get_name (accessible); 
  if (name)
    return name;
  else
    {
      GtkWidget* widget = GTK_ACCESSIBLE (accessible)->widget;
      GtkImage *image;

      if (widget == NULL)
        /*
         * State is defunct
         */
        return NULL;

      g_return_val_if_fail (GTK_IS_IMAGE (widget), NULL);
      image = GTK_IMAGE (widget);

      if (image->storage_type == GTK_IMAGE_STOCK &&
          image->data.stock.stock_id)
        return get_localized_name (image->data.stock.stock_id); 
      else return NULL;
    }
}

static void
atk_image_interface_init (AtkImageIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->get_image_description = gail_image_get_image_description;
  iface->get_image_position = gail_image_get_image_position;
  iface->get_image_size = gail_image_get_image_size;
  iface->set_image_description = gail_image_set_image_description;
}

static G_CONST_RETURN gchar * 
gail_image_get_image_description (AtkImage     *image)
{
  GailImage* aimage = GAIL_IMAGE (image);

  return aimage->image_description;
}

static void
gail_image_get_image_position (AtkImage     *image,
                               gint         *x,
                               gint         *y,
                               AtkCoordType coord_type)
{
  atk_component_get_position (ATK_COMPONENT (image), x, y, coord_type);
}

static void
gail_image_get_image_size (AtkImage *image, 
                           gint     *width,
                           gint     *height)
{
  GtkWidget* widget;
  GtkImage *gtk_image;
  GtkImageType image_type;

  widget = GTK_ACCESSIBLE (image)->widget;
  if (widget == 0)
  {
    /* State is defunct */
    *height = -1;
    *width = -1;
    return;
  }

  gtk_image = GTK_IMAGE(widget);

  image_type = gtk_image_get_storage_type(gtk_image);
 
  switch(image_type) {
    case GTK_IMAGE_PIXMAP:
    {	
      GdkPixmap *pixmap;
      gtk_image_get_pixmap(gtk_image, &pixmap, NULL);
      gdk_window_get_size (pixmap, width, height);
      break;
    }
    case GTK_IMAGE_PIXBUF:
    {
      GdkPixbuf *pixbuf;
      pixbuf = gtk_image_get_pixbuf(gtk_image);
      *height = gdk_pixbuf_get_height(pixbuf);
      *width = gdk_pixbuf_get_width(pixbuf);
      break;
    }
    case GTK_IMAGE_IMAGE:
    {
      GdkImage *gdk_image;
      gtk_image_get_image(gtk_image, &gdk_image, NULL);
      *height = gdk_image->height;
      *width = gdk_image->width;
      break;
    }
    case GTK_IMAGE_STOCK:
    {
      GtkIconSize size;
      gtk_image_get_stock(gtk_image, NULL, &size);
      gtk_icon_size_lookup(size, width, height);
      break;
    }
    case GTK_IMAGE_ICON_SET:
    {
      GtkIconSize size;
      gtk_image_get_icon_set(gtk_image, NULL, &size);
      gtk_icon_size_lookup(size, width, height);
      break;
    }
    case GTK_IMAGE_ANIMATION:
    {
      GdkPixbufAnimation *animation;
      animation = gtk_image_get_animation(gtk_image);
      *height = gdk_pixbuf_animation_get_height(animation);
      *width = gdk_pixbuf_animation_get_width(animation);
      break;
    }
    default:
    {
      *height = -1;
      *width = -1;
      break;
    }
  }
}

static gboolean
gail_image_set_image_description (AtkImage     *image,
                                  const gchar  *description)
{
  GailImage* aimage = GAIL_IMAGE (image);

  g_free (aimage->image_description);
  aimage->image_description = g_strdup (description);
  return TRUE;
}

static void
gail_image_finalize (GObject      *object)
{
  GailImage *aimage = GAIL_IMAGE (object);

  g_free (aimage->image_description);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}
