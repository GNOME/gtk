        .global widget_factory_resource_data
widget_factory_resource_data:
        .incbin "widget_factory.gresource"
        .global widget_factory_resource_data_end
widget_factory_resource_data_end:
        .byte 0
