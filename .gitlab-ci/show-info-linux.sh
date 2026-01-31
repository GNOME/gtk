#! /bin/sh

uname -a

. /etc/os-release

echo $PRETTY_NAME

if [ "$ID" = "fedora" ]; then
  rpm -q mesa-dri-drivers vulkan-loader
fi

if [ "$ID" = "org.gnome.os" ]; then
  # Image metadata
  echo "$IMAGE_VERSION"
  echo "$GNOMEOS_COMMIT"
  echo "$GNOMEOS_COMMIT_DATE"
  echo "$GNOMEOS_COMMITS_URL"

  # Print the mesa sources
  jq '.modules[] | select(.name=="extensions/mesa/mesa.bst")' /usr/manifest.json
fi

ls -l /dev/udmabuf
