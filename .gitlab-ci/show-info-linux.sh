#! /bin/sh

uname -a

. /etc/os-release

echo $PRETTY_NAME

if [ "$ID" = "fedora" ]; then
  rpm -q mesa-dri-drivers vulkan-loader
fi

ls -l /dev/udmabuf
