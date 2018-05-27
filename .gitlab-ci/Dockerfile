FROM fedora:28

RUN dnf -y install \
    adwaita-icon-theme \
    atk-devel \
    at-spi2-atk-devel \
    avahi-gobject-devel \
    cairo-devel \
    cairo-gobject-devel \
    ccache \
    colord-devel \
    cups-devel \
    fribidi-devel \
    gcc \
    gcc-c++ \
    gdk-pixbuf2-devel \
    gdk-pixbuf2-modules \
    gettext \
    gettext-devel \
    git \
    glib2-devel \
    gobject-introspection-devel \
    graphene-devel \
    gtk-doc \
    hicolor-icon-theme \
    itstool \
    json-glib-devel \
    libepoxy-devel \
    libmount-devel \
    librsvg2 \
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
    make \
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
ENV PATH="/usr/lib64/ccache:${PATH}"
