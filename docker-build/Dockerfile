# Base Image
FROM ubuntu:latest AS hamlib-base-image

ENV DEBIAN_FRONTEND="noninteractive" TZ="Etc/UTC"

RUN apt-get update && apt-get install -y build-essential automake libtool python3 && rm -rf /var/lib/apt/lists/* 

# Builder Image
FROM hamlib-base-image AS hamlib-builder

COPY . /tmp/build

WORKDIR /tmp/build

RUN mkdir -p /tmp/local

RUN ./bootstrap && ./configure --prefix=/tmp/local \
	&& make clean && make -j && make install

# Runtime Image
FROM ubuntu:latest AS hamlib-runtime

COPY --from=hamlib-builder /tmp/local/bin /usr/local/bin
COPY --from=hamlib-builder /tmp/local/lib /usr/local/lib
COPY --from=hamlib-builder /tmp/local/share /usr/local/share

ENV LD_LIBRARY_PATH="/usr/local/lib"
