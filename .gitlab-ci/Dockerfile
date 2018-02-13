FROM fedora:27

RUN dnf -y install \
    atk-devel \
    at-spi2-atk-devel \
    avahi-gobject-devel \
    cairo-devel \
    cairo-gobject-devel \
    colord-devel \
    cups-devel \
    fribidi-devel \
    gcc \
    gcc-c++ \
    gdk-pixbuf2-devel \
    gettext \
    git \
    glib2-devel \
    gobject-introspection-devel \
    graphene-devel \
    itstool \
    json-glib-devel \
    libepoxy-devel \
    libXcomposite-devel \
    libXcursor-devel \
    libXcursor-devel \
    libXdamage-devel \
    libXfixes-devel \
    libXi-devel \
    libXinerama-devel \
    libxkbcommon-devel \
    libXrandr-devel \
    libXrender-devel \
    mesa-libEGL-devel \
    mesa-libwayland-egl-devel \
    meson \
    pango-devel \
    redhat-rpm-config \
    vulkan-devel \
    wayland-devel \
    wayland-protocols-devel \
    xorg-x11-server-Xvfb

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.utf8
