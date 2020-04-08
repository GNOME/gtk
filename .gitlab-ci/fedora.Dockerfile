FROM fedora:31

RUN dnf -y install \
    adwaita-icon-theme \
    atk-devel \
    at-spi2-atk-devel \
    avahi-gobject-devel \
    cairo-devel \
    cairo-gobject-devel \
    ccache \
    clang \
    clang-analyzer \
    colord-devel \
    cups-devel \
    dbus-daemon \
    dbus-x11 \
    dejavu-sans-mono-fonts \
    desktop-file-utils \
    diffutils \
    elfutils-libelf-devel \
    fribidi-devel \
    gcc \
    gcc-c++ \
    gdk-pixbuf2-devel \
    gdk-pixbuf2-modules \
    gettext \
    git \
    glib2-devel \
    glib2-static \
    glibc-devel \
    glibc-headers \
    gobject-introspection-devel \
    graphene-devel \
    gstreamer1-devel \
    gstreamer1-plugins-good \
    gstreamer1-plugins-bad-free-devel \
    gstreamer1-plugins-base-devel \
    gtk-doc \
    hicolor-icon-theme \
    iso-codes \
    itstool \
    json-glib-devel \
    lcov \
    libattr-devel \
    libepoxy-devel \
    libffi-devel \
    libmount-devel \
    librsvg2 \
    libselinux-devel \
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
    libxslt \
    mesa-dri-drivers \
    mesa-libEGL-devel \
    mesa-libwayland-egl-devel \
    ninja-build \
    pango-devel \
    pcre-devel \
    pcre-static \
    python3 \
    python3-jinja2 \
    python3-pip \
    python3-pygments \
    python3-wheel \
    redhat-rpm-config \
    sassc \
    sysprof-devel \
    systemtap-sdt-devel \
    vulkan-devel \
    wayland-devel \
    wayland-protocols-devel \
    which \
    xorg-x11-server-Xvfb \
 && dnf clean all

RUN pip3 install meson==0.53.1

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.UTF-8
