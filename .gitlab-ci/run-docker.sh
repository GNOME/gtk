#!/bin/bash
#
# This script builds an image from the Dockerfile, starts a container with
# the parent directory mounted as working directory and start a bash session
# there so you can test things.
# Once you are happy you can push it to the docker hub:
#     sudo docker push "${TAG}"

set -e

TAG="registry.gitlab.gnome.org/gnome/gtk/gtk-3-24:v1"

# HOST_USER_ID gets used to create a user with the same ID so that files
# created in the mounted volume have the same owner
sudo docker build \
    --build-arg HOST_USER_ID="$UID" --tag "${TAG}" --file "Dockerfile" .
sudo docker run --security-opt label=disable \
    --rm --volume "$(pwd)/..:/home/user/app" --workdir "/home/user/app" \
    --tty --interactive "${TAG}" bash
