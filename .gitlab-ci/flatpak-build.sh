#!/bin/bash

set -e

appid=$1

builddir=flatpak_app
repodir=repo

flatpak-builder \
        --stop-at=gtk \
        ${builddir} \
        build-aux/flatpak/${appid}.json

flatpak build ${builddir} meson \
                --prefix=/app \
                --libdir=/app/lib \
                --buildtype=release \
                -Dx11-backend=true \
                -Dwayland-backend=true \
                -Dprint-backends=file \
                -Dbuild-tests=false \
                -Dbuild-examples=false \
                -Dintrospection=false \
                -Ddemos=true \
                _build .

flatpak build ${builddir} ninja -C _build install

flatpak-builder \
        --finish-only \
        --repo=${repodir} \
        ${builddir} \
        build-aux/flatpak/${appid}.json

flatpak build-bundle \
        ${repodir} \
        ${appid}-dev.flatpak \
        --runtime-repo=https://nightly.gnome.org/gnome-nightly.flatpakrepo \
        ${appid}
