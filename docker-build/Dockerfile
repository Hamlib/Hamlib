# Idea from https://github.com/ok2cqr/cqrlog

FROM ubuntu:latest

RUN apt-get update && apt-get -y upgrade

ENV DEBIAN_FRONTEND="noninteractive" TZ="Europe/London"

RUN apt-get install -y git build-essential automake libtool

RUN mkdir -p /usr/local/hamlib-alpha /home/hamlib/build

# Mount point for the git repository:
VOLUME ["/home/hamlib/build"]

# Mount point for the result of the build:
VOLUME ["/usr/local/hamlib-alpha"]

WORKDIR /home/hamlib/build

ENTRYPOINT rm -rf build && mkdir -p build && cd build && ../bootstrap && ../configure --prefix=/usr/local/hamlib-alpha && make clean && make && make install
