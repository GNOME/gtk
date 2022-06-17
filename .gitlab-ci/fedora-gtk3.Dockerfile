FROM fedora:35

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
    dbus-x11 \
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
    iso-codes \
    itstool \
    json-glib-devel \
    libcloudproviders-devel \
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
    libXtst-devel \
    make \
    mesa-libEGL-devel \
    'pkgconfig(wayland-egl)' \
    meson \
    ninja-build \
    pango-devel \
    python3 \
    python3-pip \
    python3-wheel \
    redhat-rpm-config \
    rest-devel \
    sassc \
    vulkan-devel \
    wayland-devel \
    wayland-protocols-devel \
    xorg-x11-server-Xvfb \
    && dnf clean all

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.utf8
ENV PATH="/usr/lib64/ccache:${PATH}"
