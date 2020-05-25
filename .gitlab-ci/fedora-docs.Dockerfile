FROM registry.gitlab.gnome.org/gnome/gtk/fedora-base:v18

RUN dnf -y install pandoc

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.UTF-8
