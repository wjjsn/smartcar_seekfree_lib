FROM ubuntu:24.04

# 设置环境变量，确保安装过程不产生交互
ENV DEBIAN_FRONTEND=noninteractive

RUN echo 'Acquire::Retries "50";' > /etc/apt/apt.conf.d/80-retries
RUN echo 'Acquire::http::Pipeline-Depth "10";' > /etc/apt/apt.conf.d/99parallel
RUN echo 'Acquire::http::No-Cache "false";' >> /etc/apt/apt.conf.d/99parallel && \
    echo 'Acquire::http::No-Store "false";' >> /etc/apt/apt.conf.d/99parallel

RUN apt-get update && apt-get install -y aria2 curl ca-certificates --no-install-recommends \
    && rm -rf /var/lib/apt/lists/*

RUN curl -L https://raw.githubusercontent.com/ilikenwf/apt-fast/master/apt-fast -o /usr/local/bin/apt-fast \
    && chmod +x /usr/local/bin/apt-fast

RUN curl -L https://raw.githubusercontent.com/ilikenwf/apt-fast/master/apt-fast.conf -o /etc/apt-fast.conf

RUN apt-get update && apt-fast install -y \
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
