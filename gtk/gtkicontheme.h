/* GtkIconTheme - a loader for icon themes
 * gtk-icon-loader.h Copyright (C) 2002, 2003 Red Hat, Inc.
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

#ifndef __GTK_ICON_THEME_H__
#define __GTK_ICON_THEME_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <gtk/gtkstylecontext.h>
#include <gsk/gsk.h>

G_BEGIN_DECLS

#define GTK_TYPE_ICON_INFO              (gtk_icon_info_get_type ())
#define GTK_ICON_INFO(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ICON_INFO, GtkIconInfo))
#define GTK_IS_ICON_INFO(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ICON_INFO))

#define GTK_TYPE_ICON_THEME             (gtk_icon_theme_get_type ())
#define GTK_ICON_THEME(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ICON_THEME, GtkIconTheme))
#define GTK_IS_ICON_THEME(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ICON_THEME))

typedef struct _GtkIconInfo         GtkIconInfo;
typedef struct _GtkIconTheme        GtkIconTheme;

/**
 * GtkIconLookupFlags:
 * @GTK_ICON_LOOKUP_NO_SVG: Never get SVG icons, even if gdk-pixbuf
 *   supports them. Cannot be used together with %GTK_ICON_LOOKUP_FORCE_SVG.
 * @GTK_ICON_LOOKUP_FORCE_SVG: Get SVG icons, even if gdk-pixbuf
 *   doesn’t support them.
 *   Cannot be used together with %GTK_ICON_LOOKUP_NO_SVG.
 * @GTK_ICON_LOOKUP_USE_BUILTIN: When passed to
 *   gtk_icon_theme_lookup_icon() includes builtin icons
 *   as well as files. For a builtin icon, gtk_icon_info_get_filename()
 *   is %NULL and you need to call gtk_icon_info_get_builtin_pixbuf().
 * @GTK_ICON_LOOKUP_GENERIC_FALLBACK: Try to shorten icon name at '-'
 *   characters before looking at inherited themes. This flag is only
 *   supported in functions that take a single icon name. For more general
 *   fallback, see gtk_icon_theme_choose_icon()
 * @GTK_ICON_LOOKUP_FORCE_SIZE: Always get the icon scaled to the
 *   requested size
 * @GTK_ICON_LOOKUP_FORCE_REGULAR: Try to always load regular icons, even
 *   when symbolic icon names are given
 * @GTK_ICON_LOOKUP_FORCE_SYMBOLIC: Try to always load symbolic icons, even
 *   when regular icon names are given
 * @GTK_ICON_LOOKUP_DIR_LTR: Try to load a variant of the icon for left-to-right
 *   text direction
 * @GTK_ICON_LOOKUP_DIR_RTL: Try to load a variant of the icon for right-to-left
 *   text direction
 *
 * Used to specify options for gtk_icon_theme_lookup_icon()
 */
typedef enum
{
  GTK_ICON_LOOKUP_NO_SVG           = 1 << 0,
  GTK_ICON_LOOKUP_FORCE_SVG        = 1 << 1,
  GTK_ICON_LOOKUP_USE_BUILTIN      = 1 << 2,
  GTK_ICON_LOOKUP_GENERIC_FALLBACK = 1 << 3,
  GTK_ICON_LOOKUP_FORCE_SIZE       = 1 << 4,
  GTK_ICON_LOOKUP_FORCE_REGULAR    = 1 << 5,
  GTK_ICON_LOOKUP_FORCE_SYMBOLIC   = 1 << 6,
  GTK_ICON_LOOKUP_DIR_LTR          = 1 << 7,
  GTK_ICON_LOOKUP_DIR_RTL          = 1 << 8
} GtkIconLookupFlags;

/**
 * GTK_ICON_THEME_ERROR:
 *
 * The #GQuark used for #GtkIconThemeError errors.
 */
#define GTK_ICON_THEME_ERROR gtk_icon_theme_error_quark ()

/**
 * GtkIconThemeError:
 * @GTK_ICON_THEME_NOT_FOUND: The icon specified does not exist in the theme
 * @GTK_ICON_THEME_FAILED: An unspecified error occurred.
 *
 * Error codes for GtkIconTheme operations.
 **/
typedef enum {
  GTK_ICON_THEME_NOT_FOUND,
  GTK_ICON_THEME_FAILED
} GtkIconThemeError;

GDK_AVAILABLE_IN_ALL
GQuark gtk_icon_theme_error_quark (void);

GDK_AVAILABLE_IN_ALL
GType         gtk_icon_theme_get_type              (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkIconTheme *gtk_icon_theme_new                   (void);
GDK_AVAILABLE_IN_ALL
GtkIconTheme *gtk_icon_theme_get_default           (void);
GDK_AVAILABLE_IN_ALL
GtkIconTheme *gtk_icon_theme_get_for_display       (GdkDisplay                  *display);
GDK_AVAILABLE_IN_ALL
void          gtk_icon_theme_set_display           (GtkIconTheme                *self,
						    GdkDisplay                  *display);

GDK_AVAILABLE_IN_ALL
void          gtk_icon_theme_set_search_path       (GtkIconTheme                *self,
						    const gchar                 *path[],
						    gint                         n_elements);
GDK_AVAILABLE_IN_ALL
void          gtk_icon_theme_get_search_path       (GtkIconTheme                *self,
						    gchar                      **path[],
						    gint                        *n_elements);
GDK_AVAILABLE_IN_ALL
void          gtk_icon_theme_append_search_path    (GtkIconTheme                *self,
						    const gchar                 *path);
GDK_AVAILABLE_IN_ALL
void          gtk_icon_theme_prepend_search_path   (GtkIconTheme                *self,
						    const gchar                 *path);

GDK_AVAILABLE_IN_ALL
void          gtk_icon_theme_add_resource_path     (GtkIconTheme                *self,
                                                    const gchar                 *path);

GDK_AVAILABLE_IN_ALL
void          gtk_icon_theme_set_custom_theme      (GtkIconTheme                *self,
						    const gchar                 *theme_name);

GDK_AVAILABLE_IN_ALL
gboolean      gtk_icon_theme_has_icon              (GtkIconTheme                *self,
						    const gchar                 *icon_name);
GDK_AVAILABLE_IN_ALL
gint         *gtk_icon_theme_get_icon_sizes        (GtkIconTheme                *self,
						    const gchar                 *icon_name);
GDK_AVAILABLE_IN_ALL
GtkIconInfo * gtk_icon_theme_lookup_icon           (GtkIconTheme                *self,
						    const gchar                 *icon_name,
						    gint                         size,
						    GtkIconLookupFlags           flags);
GDK_AVAILABLE_IN_ALL
GtkIconInfo * gtk_icon_theme_lookup_icon_for_scale (GtkIconTheme                *self,
                                                    const gchar                 *icon_name,
                                                    gint                         size,
                                                    gint                         scale,
                                                    GtkIconLookupFlags           flags);

GDK_AVAILABLE_IN_ALL
GtkIconInfo * gtk_icon_theme_choose_icon           (GtkIconTheme                *self,
						    const gchar                 *icon_names[],
						    gint                         size,
						    GtkIconLookupFlags           flags);
GDK_AVAILABLE_IN_ALL
GtkIconInfo * gtk_icon_theme_choose_icon_for_scale (GtkIconTheme                *self,
						    const gchar                 *icon_names[],
						    gint                         size,
                                                    gint                         scale,
						    GtkIconLookupFlags           flags);
GDK_AVAILABLE_IN_ALL
GdkPaintable *gtk_icon_theme_load_icon             (GtkIconTheme                *self,
                                                    const char                  *icon_name,
                                                    int                          size,
                                                    GtkIconLookupFlags           flags,
                                                    GError                     **error);
GDK_AVAILABLE_IN_ALL
GdkPaintable *gtk_icon_theme_load_icon_for_scale   (GtkIconTheme                *self,
                                                    const gchar                 *icon_name,
                                                    gint                         size,
                                                    gint                         scale,
                                                    GtkIconLookupFlags           flags,
                                                    GError                     **error);

GDK_AVAILABLE_IN_ALL
GtkIconInfo * gtk_icon_theme_lookup_by_gicon       (GtkIconTheme                *self,
                                                    GIcon                       *icon,
                                                    gint                         size,
                                                    GtkIconLookupFlags           flags);
GDK_AVAILABLE_IN_ALL
GtkIconInfo * gtk_icon_theme_lookup_by_gicon_for_scale (GtkIconTheme             *self,
                                                        GIcon                    *icon,
                                                        gint                      size,
                                                        gint                      scale,
                                                        GtkIconLookupFlags        flags);


GDK_AVAILABLE_IN_ALL
GList *       gtk_icon_theme_list_icons            (GtkIconTheme                *self,
						    const gchar                 *context);

GDK_AVAILABLE_IN_ALL
gboolean      gtk_icon_theme_rescan_if_needed      (GtkIconTheme                *self);

GDK_AVAILABLE_IN_ALL
GType                 gtk_icon_info_get_type           (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
gint                  gtk_icon_info_get_base_size      (GtkIconInfo   *self);
GDK_AVAILABLE_IN_ALL
gint                  gtk_icon_info_get_base_scale     (GtkIconInfo   *self);
GDK_AVAILABLE_IN_ALL
const gchar *         gtk_icon_info_get_filename       (GtkIconInfo   *self);
GDK_AVAILABLE_IN_ALL
gboolean              gtk_icon_info_is_symbolic        (GtkIconInfo   *self);
GDK_AVAILABLE_IN_ALL
GdkPaintable *        gtk_icon_info_load_icon          (GtkIconInfo   *self,
                                                        GError       **error);
GDK_AVAILABLE_IN_ALL
void                  gtk_icon_info_load_icon_async   (GtkIconInfo          *self,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
GDK_AVAILABLE_IN_ALL
GdkPaintable *        gtk_icon_info_load_icon_finish  (GtkIconInfo          *self,
                                                       GAsyncResult         *res,
                                                       GError              **error);
GDK_AVAILABLE_IN_ALL
GdkPaintable *        gtk_icon_info_load_symbolic      (GtkIconInfo   *self,
                                                        const GdkRGBA *fg,
                                                        const GdkRGBA *success_color,
                                                        const GdkRGBA *warning_color,
                                                        const GdkRGBA *error_color,
                                                        gboolean      *was_symbolic,
                                                        GError       **error);
GDK_AVAILABLE_IN_ALL
void                  gtk_icon_info_load_symbolic_async (GtkIconInfo   *self,
                                                         const GdkRGBA *fg,
                                                         const GdkRGBA *success_color,
                                                         const GdkRGBA *warning_color,
                                                         const GdkRGBA *error_color,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
GDK_AVAILABLE_IN_ALL
GdkPaintable *        gtk_icon_info_load_symbolic_finish (GtkIconInfo   *self,
                                                          GAsyncResult  *res,
                                                          gboolean      *was_symbolic,
                                                          GError       **error);
GDK_AVAILABLE_IN_ALL
GdkPaintable *        gtk_icon_info_load_symbolic_for_context (GtkIconInfo      *self,
                                                               GtkStyleContext  *context,
                                                               gboolean         *was_symbolic,
                                                               GError          **error);
GDK_AVAILABLE_IN_ALL
void                  gtk_icon_info_load_symbolic_for_context_async (GtkIconInfo         *self,
                                                                     GtkStyleContext     *context,
                                                                     GCancellable        *cancellable,
                                                                     GAsyncReadyCallback callback,
                                                                     gpointer            user_data);
GDK_AVAILABLE_IN_ALL
GdkPaintable *        gtk_icon_info_load_symbolic_for_context_finish (GtkIconInfo      *self,
                                                                      GAsyncResult     *res,
                                                                      gboolean         *was_symbolic,
                                                                      GError          **error);


G_END_DECLS

#endif /* __GTK_ICON_THEME_H__ */
