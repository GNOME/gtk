FROM debian:bullseye

RUN apt-get update -qq && apt-get install --no-install-recommends -qq -y \
    adwaita-icon-theme \
    ccache \
    dconf-gsettings-backend \
    g++ \
    gcc \
    gettext \
    git \
    gobject-introspection \
    gvfs \
    hicolor-icon-theme \
    itstool \
    libatk-bridge2.0-dev \
    libatk1.0-dev \
    libc6-dev \
    libcairo2-dev \
    libcairo-gobject2 \
    libcolord-dev \
    libcups2-dev \
    libegl1-mesa-dev \
    libepoxy-dev \
    libfontconfig1-dev \
    libfreetype6-dev \
    libgdk-pixbuf2.0-dev \
    libgirepository1.0-dev \
    libglib2.0-dev \
    libharfbuzz-dev \
    libjson-glib-dev \
    libpango1.0-dev \
    librest-dev \
    librsvg2-common \
    libsoup2.4-dev \
    libwayland-dev \
    libx11-dev \
    libxcomposite-dev \
    libxcursor-dev \
    libxdamage-dev \
    libxext-dev \
    libxfixes-dev \
    libxi-dev \
    libxinerama-dev \
    libxkbcommon-dev \
    libxkbcommon-x11-dev \
    libxml2-dev \
    libxrandr-dev \
    locales \
    ninja-build \
    pkg-config \
    python3 \
    python3-pip \
    python3-setuptools \
    python3-wheel \
    shared-mime-info \
    wayland-protocols \
    xauth \
    xvfb \
 && rm -rf /usr/share/doc/* /usr/share/man/*

# Locale for our build
RUN locale-gen C.UTF-8 && /usr/sbin/update-locale LANG=C.UTF-8

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG=C.UTF-8 LANGUAGE=C.UTF-8 LC_ALL=C.UTF-8
