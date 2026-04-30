FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    gcc-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    qemu-system-arm \
    make \
    gdb-multiarch \
    xxd \
    git \
    && rm -rf /var/lib/apt/lists/*

RUN git config --global push.autoSetupRemote true

WORKDIR /aeonos

CMD ["bash"]
