#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

void                    gsk_vulkan_download_op                          (GskVulkanRender                *render,
                                                                         GskVulkanImage                 *image,
                                                                         GskVulkanDownloadFunc           func,
                                                                         gpointer                        user_data);

void                    gsk_vulkan_download_png_op                      (GskVulkanRender                *render,
                                                                         GskVulkanImage                 *image,
                                                                         const char                     *filename_format,
                                                                         ...) G_GNUC_PRINTF(3, 4);

G_END_DECLS

