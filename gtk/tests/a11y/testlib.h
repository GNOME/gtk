#include <stdio.h>
#include <gtk/gtk.h>

/* Maximum characters in the output buffer */
#define MAX_LINE_SIZE   1000

/* Maximum number of tests */
#define MAX_TESTS       30 

/* Maximum number of test windows */
#define MAX_WINDOWS	5

/* Maximum number of parameters any test can have */
#define MAX_PARAMS      3

/* Information on the Output Window */

typedef struct
{
  GtkWidget     *outputWindow;
  GtkTextBuffer *outputBuffer; 
  GtkTextIter   outputIter;
}OutputWindow;

typedef void (*TLruntest) (AtkObject * obj, gint win_num);

/* General purpose functions */

gboolean		already_accessed_atk_object	(AtkObject	*obj);
AtkObject*		find_object_by_role		(AtkObject	*obj,
							AtkRole		*role,
							gint		num_roles);
AtkObject*		find_object_by_type		(AtkObject	*obj,
							gchar		*type);
AtkObject*		find_object_by_name_and_role	(AtkObject	*obj,
						      	const gchar	*name,
							AtkRole		*roles,
							gint		num_roles);
AtkObject*		find_object_by_accessible_name_and_role (AtkObject *obj,
							const gchar	*name,
							AtkRole		*roles,
							gint		num_roles);
void			display_children		(AtkObject	*obj,
                                                        gint		depth,
                                                        gint		child_number);
void			display_children_to_depth	(AtkObject	*obj,
                                                        gint		to_depth,
                                                        gint		depth,
                                                        gint		child_number);


/* Test GUI functions */

gint			create_windows			(AtkObject	*obj,
							TLruntest	runtest,
							OutputWindow	**outwin);
gboolean		add_test			(gint		window,
							gchar 		*name,
							gint		num_params,
							gchar 		*parameter_names[],
							gchar 		*default_names[]);
void			set_output_buffer		(gchar 		*output);
gchar			**tests_set			(gint		window,
							int		*count);
gchar			*get_arg_of_func		(gint		window,
							gchar		*function_name,
							gchar 		*arg_label);
int			string_to_int			(const char	*the_string);
gboolean		isVisibleDialog			(void);

