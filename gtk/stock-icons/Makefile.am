
IMAGES = @STRIP_BEGIN@ \
	dialog_error.png	\
	dialog_info.png		\
	dialog_question.png	\
	dialog_warning.png	\
	stock_button_apply.png	\
	stock_button_cancel.png	\
	stock_button_close.png	\
	stock_button_no.png	\
	stock_button_ok.png	\
	stock_button_yes.png	\
	stock_close.png		\
	stock_exit.png		\
	stock_help.png		\
	stock_new.png		\
	stock_open.png		\
	stock_save.png		\
@STRIP_END@

VARIABLES = @STRIP_BEGIN@ \
		dialog_error     	dialog_error.png    	\
	        dialog_info      	dialog_info.png     	\
		dialog_question  	dialog_question.png 	\
	 	dialog_warning   	dialog_warning.png  	\
		stock_button_apply 	stock_button_apply.png  \
		stock_button_cancel     stock_button_cancel.png \
		stock_button_close      stock_button_close.png  \
		stock_button_no         stock_button_no.png  	\
		stock_button_ok     	stock_button_ok.png  	\
		stock_button_yes	stock_button_yes.png  	\
		stock_close 		stock_close.png  	\
		stock_exit		stock_exit.png 		\
		stock_help		stock_help.png  	\
		stock_new		stock_new.png		\
		stock_open		stock_open.png  	\
		stock_save		stock_save.png		\
@STRIP_END@

noinst_DATA = gtkstockpixbufs.h
CLEANFILES += $(noinst_DATA)

pixbuf_dir = $(top_builddir)/gdk-pixbuf

gtkstockpixbufs.h: $(pixbuf_dir)/make-inline-pixbuf $(IMAGES)
	cd $(srcdir) \
	&& GDK_PIXBUF_MODULEDIR=$(pixbuf_dir)/.libs \
	   $(pixbuf_dir)/make-inline-pixbuf gtkstockpixbufs.h $(VARIABLES)

EXTRA_DIST = $(IMAGES)
