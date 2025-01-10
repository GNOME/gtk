FROM debian:sid

ENV DEBIAN_FRONTEND noninteractive
RUN apt-get -y update && apt-get -y upgrade && apt-get -y install --no-install-recommends \
    build-essential \
    gir1.2-appstream \
    git \
    git-lfs \
    glslc \
    gobject-introspection \
    libglib2.0-dev-bin \
    libglib-perl \
    libglib-object-introspection-perl \
    libipc-run-perl \
    libjson-perl \
    libset-scalar-perl \
    libxml2-utils \
    libxml-libxml-perl \
    libxml-libxslt-perl \
    meson \
    ninja-build \
    openjdk-17-jre \
    openjdk-17-jdk \
    perl \
    pkg-config \
    sassc \
    sdkmanager \
    sudo \
 && apt-get -y clean

ENV ANDROID_HOME /opt/android/
ENV ANDROID_SDKVER 35.0.0
COPY android-sdk.sh .
RUN ./android-sdk.sh

RUN sed -E 's/(%sudo.*)ALL$/\1NOPASSWD:ALL/' -i /etc/sudoers

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -G sudo -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.UTF-8
