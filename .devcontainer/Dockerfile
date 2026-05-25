FROM ubuntu:24.04

# 设置环境变量，确保安装过程不产生交互
ENV DEBIAN_FRONTEND=noninteractive

COPY .devcontainer/ubuntu.sources /etc/apt/sources.list.d/ubuntu.sources

RUN apt-get -o "Acquire::https::Verify-Peer=false" -o "Acquire::AllowInsecureRepositories=true" -o "Acquire::AllowDowngradeToInsecureRepositories=true" update && \
    apt-get -o "Acquire::https::Verify-Peer=false" -o "Acquire::AllowInsecureRepositories=true" -o "Acquire::AllowDowngradeToInsecureRepositories=true" install -y aria2 curl ca-certificates --no-install-recommends \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y \
    build-essential libncurses-dev libssl-dev libelf-dev \
    bash bc binutils devscripts g++ git make ninja-build \
    flex bison \
    cmake clangd bear\
    just \
    unzip wget rsync file cpio fish\
    locales libreadline-dev sudo u-boot-tools \
    && rm -rf /var/lib/apt/lists/*

# 生成语言环境（UTF-8）
RUN locale-gen en_US.UTF-8
ENV LANG=en_US.UTF-8

RUN echo "ubuntu ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

RUN echo $(which fish) >> /etc/shells && \
    chsh -s $(which fish) ubuntu

USER ubuntu

RUN mkdir -p ~/.config/fish/completions && \
    just --completions fish > ~/.config/fish/completions/just.fish
