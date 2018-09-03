
void     file_transfer_portal_register_files        (const char           **files,
                                                     gboolean               writable,
                                                     GAsyncReadyCallback    callback,
                                                     gpointer               data);
gboolean file_transfer_portal_register_files_finish (GAsyncResult          *result,
                                                     char                 **key,
                                                     GError               **error);
                                                     
void     file_transfer_portal_retrieve_files        (const char            *key,
                                                     GAsyncReadyCallback    callback,
                                                     gpointer               data);
gboolean file_transfer_portal_retrieve_files_finish (GAsyncResult          *result,
                                                     char                ***files,
                                                     GError               **error);
