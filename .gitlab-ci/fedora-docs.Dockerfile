FROM registry.gitlab.gnome.org/gnome/gtk/fedora-base:v25

RUN dnf -y install \
        python3-jinja2 \
        python3-markdown \
        python3-pygments \
        python3-toml \
        python3-typogrify

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.UTF-8
