#include <string.h>
#include <stdio.h>
#include "testtextlib.h"

static AtkAttributeSet *attrib = NULL;
static char result_string[2][6] = {"FALSE", "TRUE"};

/**
 * setup_gui:
 * @obj: An @AtkObject
 * @test: The callback to be run when the "Run Tests" button
 *   in the GUI is clicked.
 *
 * Sets up the GUI windows.
 *
 * Returns: the window number, or -1 if failure.
 **/
gint setup_gui(AtkObject *obj, TLruntest test)
{
  gchar *paramnames[MAX_PARAMS];
  gchar *defaults[MAX_PARAMS];
  static OutputWindow *tow = NULL;
  gint  window;
  
  if (tow)
    window = create_windows(obj, test, &tow);
  else
    window = create_windows(obj, test, &tow);

  if (window == -1)
    return -1;

  /* Get Text [at|after|before] Offset Tests */
  paramnames[0] = "offset";
  defaults[0] = "1"; 
  add_test(window, "atk_text_get_text_after_offset", 1,  paramnames, defaults);
  add_test(window, "atk_text_get_text_before_offset", 1, paramnames, defaults);
  add_test(window, "atk_text_get_text_at_offset",1 , paramnames, defaults);
  
  /* Get Character Count Test */
  add_test(window, "atk_text_get_character_count", 0, NULL, NULL); 

  /* Get Character At Offset Test */
  paramnames[0] = "offset";
  defaults[0] = "1";
  add_test(window, "atk_text_get_character_at_offset", 1, paramnames, defaults);
   
  /* Get Text Test */
  paramnames[0] = "position 1";
  paramnames[1] = "position 2";
  defaults[0] = "0";
  defaults[1] = "5";
  add_test(window, "atk_text_get_text", 2, paramnames, defaults); 

  /* Caret Tests */
  add_test(window, "atk_text_get_caret_offset", 0, NULL, NULL); 

  paramnames[0] = "offset";
  defaults[0] = "1";
  add_test(window, "atk_text_set_caret_offset", 1, paramnames, defaults);

  /* Selection Tests */
  add_test(window, "atk_text_get_n_selections", 0, NULL, NULL);
    
  paramnames[0] = "selection no";
  defaults[0] = "0";
  add_test(window, "atk_text_get_selection", 1, paramnames, defaults);  

  paramnames[0] = "start";
  paramnames[1] = "end";
  defaults[0] = "3";
  defaults[1] = "8";
  add_test(window, "atk_text_add_selection", 2, paramnames, defaults); 

  paramnames[0] = "selection no";
  paramnames[1] = "start";
  paramnames[2] = "end";
  defaults[0] = "0";
  defaults[1] = "5";
  defaults[2] = "7"; 
  add_test(window, "atk_text_set_selection", 3, paramnames, defaults);

  paramnames[0] = "selection no";
  defaults[0] = "0";
  add_test(window, "atk_text_remove_selection", 1, paramnames, defaults);

  paramnames[0] = "offset";
  defaults[0] = "36";
  add_test(window, "atk_text_get_run_attributes", 1, paramnames, defaults);

  add_test(window, "atk_text_get_default_attributes", 0, paramnames, defaults);

  paramnames[0] = "offset";
  paramnames[1] = "coord mode";
  defaults[0] = "0";
  defaults[1] = "ATK_XY_SCREEN";
  add_test(window, "atk_text_get_character_extents", 2, paramnames, defaults);

  paramnames[0] = "x";
  paramnames[1] = "y";
  paramnames[2] = "coord mode";
  defaults[0] = "106";
  defaults[1] = "208";
  defaults[2] = "ATK_XY_SCREEN";
  add_test(window, "atk_text_get_offset_at_point", 3, paramnames, defaults);

  /* Editable Text Tests */
  if (ATK_IS_EDITABLE_TEXT(obj)) 
  {

    paramnames[0] = "start";
    paramnames[1] = "end";
    defaults[0] = "20";
    defaults[1] = "27";
    add_test(window, "atk_editable_text_set_run_attributes", 2, paramnames, defaults);
      
    paramnames[0] = "start";
    paramnames[1] = "end";
    defaults[0] = "3";
    defaults[1] = "5";
    add_test(window, "atk_editable_text_cut_text", 2, paramnames, defaults);

    paramnames[0] = "position";
    defaults[0] = "8";
    add_test(window, "atk_editable_text_paste_text", 1, paramnames, defaults);

    paramnames[0] = "start";
    paramnames[1] = "end";
    defaults[0] = "15";
    defaults[1] = "20";
    add_test(window, "atk_editable_text_delete_text", 2, paramnames, defaults);
    paramnames[0] = "start";
    paramnames[1] = "end";
    defaults[0] = "5";
    defaults[1] = "20";
    add_test(window, "atk_editable_text_copy_text", 2, paramnames, defaults); 

    paramnames[0] = "insert text";
    paramnames[1] = "position";
    defaults[0] = "this is my insert";
    defaults[1] = "15";
    add_test(window, "atk_editable_text_insert_text", 2, paramnames, defaults);
  }
  return window;
}

/**
 * add_handlers:
 * @obj: An #AtkObject
 *
 * Sets up text signal handlers.
 *
 **/
void add_handlers(AtkObject *obj)
{
  if (!already_accessed_atk_object(obj))
  {
    /* Set up signal handlers */

    g_print ("Adding signal handler\n");
    g_signal_connect_closure_by_id (obj,
		g_signal_lookup ("text_caret_moved", G_OBJECT_TYPE (obj)),
		0,
		g_cclosure_new (G_CALLBACK (_notify_caret_handler),
			NULL, NULL),
		FALSE);

    g_signal_connect_closure (obj, "text_changed::insert",
		g_cclosure_new (G_CALLBACK (_notify_text_insert_handler),
			NULL, NULL),
		FALSE);

    g_signal_connect_closure (obj, "text_changed::delete",
		g_cclosure_new (G_CALLBACK (_notify_text_delete_handler),
			NULL, NULL),
		FALSE);
  }
}

/**
 * notify_text_insert_handler:
 * @obj: A #Gobject
 * @start_offset: Start offset of insert
 * @end_offset: End offset of insert.
 * 
 * Text inserted singal handler
 **/
void
_notify_text_insert_handler (GObject *obj, int start_offset, int end_offset)
{
  g_print ("SIGNAL - Text inserted at position %d, length %d!\n",
    start_offset, end_offset);
}

/**
 * notify_text_delete_handler:
 * @obj: A #Gobject
 * @start_offset: Start offset of delete
 * @end_offset: End offset of delete.
 * 
 * Text deleted singal handler
 **/
void
_notify_text_delete_handler (GObject *obj, int start_offset, int end_offset)
{
  g_print ("SIGNAL - Text deleted at position %d, length %d!\n",
    start_offset, end_offset);
}

/**
 * notify_caret_handler:
 * @obj: A #Gobject
 * @position: Caret position
 * 
 * Caret (cursor) moved signal handler.
 **/
void
_notify_caret_handler (GObject *obj, int position)
{
  g_print ("SIGNAL - The caret moved to position %d!\n", position);
}

/**
 * runtest:
 * @obj: An #AtkObject
 * @win_val: The window number
 *
 * The callback to run when the "Run Tests" button on the
 * Test GUI is clicked.
 **/
void
runtest(AtkObject *obj, gint win_val)
{
  gint        i, size;
  gunichar    uni_char;
  gchar       output[MAX_LINE_SIZE];
  gchar       **testsOn;
 
  testsOn = tests_set(win_val, &size);

  for(i = 0; i < size; i++)
  {
    gint      param_int1, param_int2, start, end, j, x, y, height, width;
    gchar     *param_string1, *param_string2, *param_string3, *text;
    gboolean  result;
    gint index;

    if (strcmp(testsOn[i], "atk_text_get_text_at_offset") == 0)
    {
      param_string1 = get_arg_of_func(win_val, "atk_text_get_text_at_offset", "offset");  
      param_int1 = string_to_int(param_string1);
      
      _run_offset_test(obj, "at", param_int1, ATK_TEXT_BOUNDARY_WORD_END);
      _run_offset_test(obj, "at", param_int1, ATK_TEXT_BOUNDARY_WORD_START);
      _run_offset_test(obj, "at", param_int1, ATK_TEXT_BOUNDARY_LINE_END);
      _run_offset_test(obj, "at", param_int1, ATK_TEXT_BOUNDARY_LINE_START);
      _run_offset_test(obj, "at", param_int1, ATK_TEXT_BOUNDARY_SENTENCE_END);
      _run_offset_test(obj, "at", param_int1, ATK_TEXT_BOUNDARY_SENTENCE_START);
      _run_offset_test(obj, "at", param_int1, ATK_TEXT_BOUNDARY_CHAR);
    }
    
    if (strcmp(testsOn[i], "atk_text_get_text_after_offset") == 0)
    {
      param_string1 = get_arg_of_func(win_val, "atk_text_get_text_after_offset", "offset");  
      param_int1 = string_to_int(param_string1);

      _run_offset_test(obj, "after", param_int1, ATK_TEXT_BOUNDARY_WORD_END);
      _run_offset_test(obj, "after", param_int1, ATK_TEXT_BOUNDARY_WORD_START);
      _run_offset_test(obj, "after", param_int1, ATK_TEXT_BOUNDARY_LINE_END);
      _run_offset_test(obj, "after", param_int1, ATK_TEXT_BOUNDARY_LINE_START);
      _run_offset_test(obj, "after", param_int1, ATK_TEXT_BOUNDARY_SENTENCE_END);
      _run_offset_test(obj, "after", param_int1, ATK_TEXT_BOUNDARY_SENTENCE_START);
      _run_offset_test(obj, "after", param_int1, ATK_TEXT_BOUNDARY_CHAR);
    }

    if (strcmp(testsOn[i], "atk_text_get_text_before_offset") == 0)
    {
      param_string1 = get_arg_of_func(win_val, "atk_text_get_text_before_offset", "offset");  
      param_int1 = string_to_int(param_string1);
      
      _run_offset_test(obj, "before", param_int1, ATK_TEXT_BOUNDARY_WORD_END);
      _run_offset_test(obj, "before", param_int1, ATK_TEXT_BOUNDARY_WORD_START);
      _run_offset_test(obj, "before", param_int1, ATK_TEXT_BOUNDARY_LINE_END);
      _run_offset_test(obj, "before", param_int1, ATK_TEXT_BOUNDARY_LINE_START);
      _run_offset_test(obj, "before", param_int1, ATK_TEXT_BOUNDARY_SENTENCE_END);
      _run_offset_test(obj, "before", param_int1, ATK_TEXT_BOUNDARY_SENTENCE_START);
      _run_offset_test(obj, "before", param_int1, ATK_TEXT_BOUNDARY_CHAR);
    }
    
    if (strcmp(testsOn[i], "atk_text_get_character_count") == 0)
    {
      param_int1 = atk_text_get_character_count (ATK_TEXT (obj));
      sprintf(output, "\nText character count: %d\n", param_int1);  
      set_output_buffer(output);
    }
    
    if (strcmp(testsOn[i], "atk_text_get_character_at_offset") == 0)
    {
      param_string1 = get_arg_of_func(win_val, "atk_text_get_character_at_offset",
        "offset"); 
      uni_char = atk_text_get_character_at_offset (ATK_TEXT(obj),
        string_to_int(param_string1));
      sprintf(output, "\nCharacter at offset %d: |%x|\n",
        string_to_int(param_string1), uni_char);
      set_output_buffer(output);
    }
 
    if (strcmp(testsOn[i], "atk_text_get_text") == 0)
    {
      param_string1 =  get_arg_of_func(win_val, "atk_text_get_text", "position 1");
      param_string2 = get_arg_of_func(win_val, "atk_text_get_text", "position 2");
      text = atk_text_get_text (ATK_TEXT (obj), string_to_int(param_string1),
        string_to_int(param_string2));
      sprintf(output, "\nText %d, %d: %s\n", string_to_int(param_string1),
        string_to_int(param_string2), text);
      g_free (text);  
      set_output_buffer(output);
    }

    if (strcmp(testsOn[i], "atk_text_get_caret_offset") == 0)
    {
      param_int1 = atk_text_get_caret_offset (ATK_TEXT (obj));
      if (param_int1 == -1)
        sprintf(output, "\nCaret offset: |Not Supported|\n");  
      else
        sprintf(output, "\nCaret offset: %d\n", param_int1);  
      set_output_buffer(output);
    }

    if (strcmp(testsOn[i], "atk_text_set_caret_offset") == 0)
    {
      param_string1 = get_arg_of_func(win_val, "atk_text_set_caret_offset", "offset");
      atk_text_set_caret_offset(ATK_TEXT(obj), string_to_int(param_string1));
      sprintf(output, "\nPutting caret at offset: |%d|\n",
        string_to_int(param_string1));
      param_int1 = atk_text_get_caret_offset (ATK_TEXT (obj));

      if (param_int1 == -1)
        sprintf(output, "\nCaret offset: |Not Supported|\n");
      else
        sprintf(output, "\nCaret offset was set at: |%d|\n", param_int1);

      set_output_buffer(output);
    }
    
    if (strcmp(testsOn[i], "atk_text_get_n_selections") == 0)
    {
      param_int1 = atk_text_get_n_selections(ATK_TEXT(obj));
      if (param_int1 == -1)
      {
        sprintf(output, "\nNo selected regions\n");
        set_output_buffer(output);
      }

      for (j = 0; j < param_int1; j++)
      {
        sprintf(output, "\nNumber of selected text regions is: |%d|\n", j);  
        set_output_buffer(output);

        text = atk_text_get_selection(ATK_TEXT(obj), j, &start, &end);
        if (text != NULL)
        {
          sprintf(output, "\nSelected text for region %d is: |%s|\n", j, text);
          set_output_buffer(output);
          sprintf(output,
            "\nStart selection bounds: %d\tEnd selection bounds: %d\n",
            start, end);
        }
        else
        {
          sprintf(output, "\nNo selected region %d\n", j);
        }

        set_output_buffer(output);
      }
    }

    if (strcmp(testsOn[i], "atk_text_add_selection") == 0)
    {
      param_string1 = get_arg_of_func(win_val, "atk_text_add_selection", "start");
      param_string2 = get_arg_of_func(win_val, "atk_text_add_selection", "end");
      result = atk_text_add_selection(ATK_TEXT(obj),
        string_to_int(param_string1), string_to_int(param_string2));   
      sprintf(output, "\nSet selection bounds between %d, and %d: %s",
        string_to_int(param_string1), string_to_int(param_string2),
        result_string[result]);
      set_output_buffer(output);
      
      param_int1 = atk_text_get_n_selections(ATK_TEXT(obj));
      for (j = 0; j < param_int1; j++)
      {
        sprintf(output, "\nNumber of selected text region is: %d\n", j);
        set_output_buffer(output);
        text = atk_text_get_selection(ATK_TEXT(obj), j, &start, &end);

        if (text != NULL)
        {
          sprintf(output, "\nSelected text for region %d is: |%s|\n", j, text);
          set_output_buffer(output);
          sprintf(output,
            "\nStart selection bounds: %d\tEnd selection bounds: %d\n",
            start, end);
        }
        else
        {
          sprintf(output, "\nNo selected region %d\n", j);
        }

        set_output_buffer(output);
      }
    }
      
    if (strcmp(testsOn[i], "atk_text_get_selection") == 0)
    {
      param_string1 = get_arg_of_func(win_val, "atk_text_get_selection", "selection no");
      text = atk_text_get_selection(ATK_TEXT(obj),
        string_to_int(param_string1), &start, &end);

      if (text != NULL)
      {
        sprintf(output, "\nSelected text for region %d is: |%s|\n",
          string_to_int(param_string1), text);
        set_output_buffer(output);
        sprintf(output,
          "\nStart selection bounds: %d\t End selection bounds: %d\n",
          start, end);
      }
      else
      {
        sprintf(output, "\nNo selected region %d\n", string_to_int(param_string1));
      }

      set_output_buffer(output);
    }

    if (strcmp(testsOn[i], "atk_text_set_selection") == 0)
    {
      param_string1 = get_arg_of_func(win_val, "atk_text_set_selection", "selection no");
      param_string2 = get_arg_of_func(win_val, "atk_text_set_selection", "start");
      param_string3 = get_arg_of_func(win_val, "atk_text_set_selection", "end");
      result = atk_text_set_selection(ATK_TEXT(obj), string_to_int(param_string1),
        string_to_int(param_string2), string_to_int(param_string3));
      sprintf(output, "Set selection %d's bounds between %d and %d: %s\n",
        string_to_int(param_string1), string_to_int(param_string2),
        string_to_int(param_string3), result_string[result]);
      set_output_buffer(output);
      text = atk_text_get_selection(ATK_TEXT(obj), string_to_int(param_string1),
        &start, &end);

      if (text != NULL)
      {
        sprintf(output, "Selected text for the reset region %d is: |%s|\n",
          string_to_int(param_string1), text);
        set_output_buffer(output);
        sprintf(output,
          "\nNew start selection bounds: %d\tNew end selection bounds: %d\n",
          start, end);
      }
      else
      {
        sprintf(output, "\nNo selected region %d\n", string_to_int(param_string1));
      }

      set_output_buffer(output);
    }
    
    if (strcmp(testsOn[i], "atk_text_remove_selection") == 0)
    {
      param_string1 = get_arg_of_func(win_val, "atk_text_remove_selection", "selection no");
      result = atk_text_remove_selection(ATK_TEXT(obj), string_to_int(param_string1));
      sprintf(output, "Remove selection for region %d: %s\n",
        string_to_int(param_string1), result_string[result]);
      set_output_buffer(output);
      text = atk_text_get_selection(ATK_TEXT(obj),
        string_to_int(param_string1), &start, &end);

      if (text != NULL)
        sprintf(output, "\nRemoved regions text should be empty instead of: %s", text);
      else
        sprintf(output, "\nRemoved regions text should be empty, this is: ||");

      set_output_buffer(output);
    }

    if (strcmp(testsOn[i], "atk_text_get_run_attributes") == 0)
    {
      gint test_int;
      param_string1 = get_arg_of_func(win_val, "atk_text_get_run_attributes", "offset");
      test_int = string_to_int(param_string1);
      attrib = atk_text_get_run_attributes(ATK_TEXT(obj), test_int, &start, &end);
      sprintf(output, "get_run_attributes at offset %i:\nStart: %i, End: %i\n", test_int,
        start, end);
      set_output_buffer(output);
      if (attrib != NULL) {
        GSList *node;
        index = 0;
        node = attrib;
        while (node != NULL)
        {
          AtkAttribute* att = node->data;

          sprintf(output, "List index: %i, Name: %s, Value: %s\n", index,
            att->name, att->value);
          set_output_buffer(output);
          node = node->next;
          index++;
        }
        atk_attribute_set_free (attrib);
      }
    }

    if (strcmp(testsOn[i], "atk_text_get_default_attributes") == 0)
    {
      attrib = atk_text_get_default_attributes(ATK_TEXT(obj));
      sprintf(output, "get_default_attributes\n");
      set_output_buffer(output);
      if (attrib != NULL) {
        GSList *node;
        index = 0;
        node = attrib;
        while (node != NULL)
        {
          AtkAttribute* att = node->data;

          sprintf(output, "List index: %i, Name: %s, Value: %s\n", index,
            att->name, att->value);
          set_output_buffer(output);
          node = node->next;
          index++;
        }
        atk_attribute_set_free (attrib);
      }
    }
 
    if (strcmp(testsOn[i], "atk_text_get_character_extents") == 0)
    {
      gint test_int;
      param_string1 = get_arg_of_func(win_val, "atk_text_get_character_extents",
        "offset");
      param_string2 = get_arg_of_func(win_val, "atk_text_get_character_extents",
        "coord mode");
      test_int = string_to_int(param_string1);
      if (strcmp(param_string2, "ATK_XY_SCREEN") == 0)
      {
        atk_text_get_character_extents(ATK_TEXT(obj), test_int, &x, &y, &width,
          &height, ATK_XY_SCREEN);   
        sprintf(output,
          "get_character_extents at offset %i, mode: SCREEN\nX: %i, Y: %i, width: %i, height: %i\n",
          test_int, x, y, width, height);
      }
      else if (strcmp(param_string2, "ATK_XY_WINDOW") == 0)
      {
        atk_text_get_character_extents(ATK_TEXT(obj), test_int, &x, &y, &width,
          &height, ATK_XY_WINDOW);
        sprintf(output,
          "get_character_extents at offset %i, mode: WIDGET_WINDOW\nX: %i, Y: %i, width: %i, height: %i\n",
          test_int, x, y, width, height);
      }
      else
        sprintf(output, "get_character_extents_at_offset: Invalid coord mode argument!");
           
      set_output_buffer(output);
    } 

    if (strcmp(testsOn[i], "atk_text_get_offset_at_point") == 0)
    {
      gint test_int;
      param_string1 = get_arg_of_func(win_val, "atk_text_get_offset_at_point", "x");
      param_string2 = get_arg_of_func(win_val, "atk_text_get_offset_at_point", "y");
      param_string3 = get_arg_of_func(win_val, "atk_text_get_offset_at_point", "coord mode");
      param_int1 = string_to_int(param_string1);
      param_int2 = string_to_int(param_string2);
      if (strcmp(param_string3, "ATK_XY_SCREEN") == 0)
      {
        test_int = atk_text_get_offset_at_point(ATK_TEXT(obj), param_int1, param_int2,
          ATK_XY_SCREEN);   
        if (test_int != -1)
          sprintf(output, "get_offset_at_point %i,%i mode: SCREEN is %i\n", param_int1, param_int2, test_int);
        else 
	  sprintf(output, "Cannot get_offset_at_point\n");
      }
      else if (strcmp(param_string3, "ATK_XY_WINDOW") == 0)
      {
        test_int = atk_text_get_offset_at_point(ATK_TEXT(obj), param_int1, param_int2,
          ATK_XY_WINDOW);   
        if (test_int != -1)
          sprintf(output, "get_offset_at_point %i,%i mode: WIDGET_WINDOW is %i\n", param_int1, param_int2, test_int);
        else
	  sprintf(output, "Cannot get_offset_at_point\n");
      }
      else
        sprintf(output, "get_offset_at_point: Invalid coord mode argument!");
           
      set_output_buffer(output);
    } 
    if (ATK_IS_EDITABLE_TEXT(obj))
    {
      if (strcmp(testsOn[i], "atk_editable_text_set_run_attributes") == 0)
      {
        param_string1 = get_arg_of_func(win_val,
          "atk_editable_text_set_run_attributes", "start");
        param_string2 = get_arg_of_func(win_val,
          "atk_editable_text_set_run_attributes", "end");
        result = atk_editable_text_set_run_attributes(ATK_EDITABLE_TEXT(obj),
          attrib, string_to_int(param_string1), string_to_int(param_string2));
        if (result)
          sprintf(output, "\nSetting attributes in range %d to %d...OK\n",
            string_to_int(param_string1), string_to_int(param_string2));
        else
          sprintf(output, "\nSetting attributes in range %d to %d...Failed\n",
            string_to_int(param_string1), string_to_int(param_string2));
        set_output_buffer(output); 
      }
      
      if (strcmp(testsOn[i], "atk_editable_text_cut_text") == 0)
      {
        param_string1 = get_arg_of_func(win_val, "atk_editable_text_cut_text", "start");
        param_string2 = get_arg_of_func(win_val, "atk_editable_text_cut_text", "end");
        atk_editable_text_cut_text(ATK_EDITABLE_TEXT(obj),
          string_to_int(param_string1), string_to_int(param_string2));
        sprintf(output, "\nCutting text %d to %d...\n",
          string_to_int(param_string1), string_to_int(param_string2));
        set_output_buffer(output); 
      }
      
      if (strcmp(testsOn[i], "atk_editable_text_paste_text") == 0)
      {
        param_string1 = get_arg_of_func(win_val, "atk_editable_text_paste_text",
          "position");
        atk_editable_text_paste_text(ATK_EDITABLE_TEXT(obj),
          string_to_int(param_string1));
        sprintf(output, "\nPasting text to %d\n", string_to_int(param_string1));
        set_output_buffer(output); 
      }
      
      if (strcmp(testsOn[i], "atk_editable_text_delete_text") == 0)
      {
        param_string1 = get_arg_of_func(win_val, "atk_editable_text_delete_text", "start");
        param_string2 = get_arg_of_func(win_val, "atk_editable_text_delete_text", "end");
        atk_editable_text_delete_text(ATK_EDITABLE_TEXT(obj),
          string_to_int(param_string1), string_to_int(param_string2));
        sprintf(output, "\nDeleting text %d to %d...\n",
          string_to_int(param_string1), string_to_int(param_string2));
        set_output_buffer(output); 
      }
      
      if (strcmp(testsOn[i], "atk_editable_text_copy_text") == 0)
      {
        param_string1 = get_arg_of_func(win_val, "atk_editable_text_copy_text", "start");
        param_string2 = get_arg_of_func(win_val, "atk_editable_text_copy_text", "end");
        atk_editable_text_copy_text(ATK_EDITABLE_TEXT(obj),
          string_to_int(param_string1), string_to_int(param_string2));
        sprintf(output, "\nCopying text %d to %d...\n",
          string_to_int(param_string1), string_to_int(param_string2));
        set_output_buffer(output); 
      }
      
      if (strcmp(testsOn[i], "atk_editable_text_insert_text") == 0)
      {
        param_string1 = get_arg_of_func(win_val, "atk_editable_text_insert_text",
          "insert text");
        param_string2 = get_arg_of_func(win_val, "atk_editable_text_insert_text",
          "position");
        param_int2 = string_to_int(param_string2);
        atk_editable_text_insert_text(ATK_EDITABLE_TEXT(obj),
          param_string1, strlen(param_string1), &param_int2);
        sprintf(output, "\nInserting text at %d...\n", param_int2);
        set_output_buffer(output); 
      }
    }
  }  
}

/**
 * _run_offset_test:
 * @obj: An #AtkObject
 * @type: The type of test to run.  Can be "at", "before", or "after".
 * @offset: The offset into the text buffer.
 * @boundary: The boundary type.
 *
 * Tests the following ATK_TEXT API functions:
 * atk_text_get_text_at_offset
 * atk_text_get_text_before_offseet
 * atk_text_get_text_after_offset
 **/
void _run_offset_test(AtkObject * obj, char * type, gint offset,
  AtkTextBoundary boundary)
{
  gchar output[MAX_LINE_SIZE];
  gchar default_val[5] = "NULL";
  gchar *text;
  gint  startOffset, endOffset;

  if (strcmp(type, "at") == 0)
    text = atk_text_get_text_at_offset (ATK_TEXT (obj),
       offset, boundary, &startOffset, &endOffset);
  else if (strcmp(type, "before") == 0)
    text = atk_text_get_text_before_offset (ATK_TEXT (obj),
       offset, boundary, &startOffset, &endOffset);
  else if (strcmp(type, "after") == 0)
    text = atk_text_get_text_after_offset (ATK_TEXT (obj),
       offset, boundary, &startOffset, &endOffset);
  else
    text = NULL;

  if (text == NULL)
    text = g_strdup (default_val);
    
  if (boundary == ATK_TEXT_BOUNDARY_CHAR)
    sprintf(output, "\n|%s| Text |%s|  Boundary |BOUNDARY_CHAR|\n",
      type, text);
  else if (boundary == ATK_TEXT_BOUNDARY_WORD_START)
    sprintf(output, "\n|%s| Text |%s| Boundary |BOUNDARY_WORD_START|\n",
      type, text);
  else if (boundary == ATK_TEXT_BOUNDARY_WORD_END)
    sprintf(output, "\n|%s| Text |%s| Boundary |BOUNDARY_WORD_END|\n",
      type, text);
  else if (boundary == ATK_TEXT_BOUNDARY_SENTENCE_START)
    sprintf(output, "\n|%s| Text |%s| Boundary |BOUNDARY_SENTENCE_START|\n",
      type, text);
  else if (boundary == ATK_TEXT_BOUNDARY_SENTENCE_END)
    sprintf(output, "\n|%s| Text |%s| Boundary |BOUNDARY_SENTENCE_END|\n",
      type, text);
  else if (boundary == ATK_TEXT_BOUNDARY_LINE_START)
    sprintf(output, "\n|%s| Text |%s| Boundary |BOUNDARY_LINE_START|\n",
      type, text);
  else if (boundary == ATK_TEXT_BOUNDARY_LINE_END)
    sprintf(output, "\n|%s| Text |%s| Boundary |BOUNDARY_LINE_END|\n",
      type, text);
  else
    sprintf(output, "\n|%s| Text |%s| Boundary |UNKNOWN|\n",
      type, text);

  g_free (text);  
  set_output_buffer(output);

  sprintf(output, "Offset %d, startOffset %d, endOffset %d\n",
    offset, startOffset, endOffset);
  set_output_buffer(output);
}
