/* GTK - The GIMP Toolkit
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
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 *
 */

#ifndef __GTK_IM_CONTEXT_THAI_H__
#define __GTK_IM_CONTEXT_THAI_H__

#include <gtk/gtkimcontext.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern GType gtk_type_im_context_thai;

#define GTK_TYPE_IM_CONTEXT_THAI             gtk_type_im_context_thai
#define GTK_IM_CONTEXT_THAI(obj)             (GTK_CHECK_CAST ((obj), GTK_TYPE_IM_CONTEXT_THAI, GtkIMContextThai))
#define GTK_IM_CONTEXT_THAI_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_IM_CONTEXT_THAI, GtkIMContextThaiClass))
#define GTK_IS_IM_CONTEXT_THAI(obj)          (GTK_CHECK_TYPE ((obj), GTK_TYPE_IM_CONTEXT_THAI))
#define GTK_IS_IM_CONTEXT_THAI_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IM_CONTEXT_THAI))
#define GTK_IM_CONTEXT_THAI_GET_CLASS(obj)   (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_IM_CONTEXT_THAI, GtkIMContextThaiClass))


typedef struct _GtkIMContextThai       GtkIMContextThai;
typedef struct _GtkIMContextThaiClass  GtkIMContextThaiClass;

typedef enum
{
  ISC_PASSTHROUGH,
  ISC_BASICCHECK,
  ISC_STRICT
} GtkIMContextThaiISCMode;
#define GTK_IM_CONTEXT_THAI_BUFF_SIZE 2

struct _GtkIMContextThai
{
  GtkIMContext object;

#ifndef GTK_IM_CONTEXT_THAI_NO_FALLBACK
  gunichar                char_buff[GTK_IM_CONTEXT_THAI_BUFF_SIZE];
#endif /* !GTK_IM_CONTEXT_THAI_NO_FALLBACK */
  GtkIMContextThaiISCMode isc_mode;
};

struct _GtkIMContextThaiClass
{
  GtkIMContextClass parent_class;
};

void gtk_im_context_thai_register_type (GTypeModule *type_module);
GtkIMContext *gtk_im_context_thai_new (void);

GtkIMContextThaiISCMode
  gtk_im_context_thai_get_isc_mode (GtkIMContextThai *context_thai);

GtkIMContextThaiISCMode
  gtk_im_context_thai_set_isc_mode (GtkIMContextThai *context_thai,
                                    GtkIMContextThaiISCMode mode);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_IM_CONTEXT_THAI_H__ */
