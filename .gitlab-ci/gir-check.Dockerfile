FROM rust:1.41

RUN apt update \
    && apt install git

RUN git clone https://github.com/gtk-rs/gir \
    && cd gir \
    && git checkout f42ad441235daae75d33cd521e82e8626a8d06ff

WORKDIR gir

RUN cargo build --release

ENTRYPOINT ["cargo", "run", "--release", "--", "--check-gir-file"]
CMD []
