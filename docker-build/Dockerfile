# Base Image
FROM ubuntu:latest AS hamlib-base-image

ENV DEBIAN_FRONTEND="noninteractive" TZ="Etc/UTC"

RUN apt-get update \
	&& apt-get install -y git build-essential automake libtool python-is-python3 \
	&& rm -rf /var/lib/apt/lists/* 

# Builder Image

FROM hamlib-base-image AS hamlib-builder

COPY . /wip

WORKDIR /wip

RUN ./bootstrap && ./configure --prefix=/usr/local \
	&& make clean && make -j${nproc} && make install

# Runtime Image
FROM ubuntu:latest AS hamlib-runtime

COPY --from=hamlib-builder /usr/local /usr/local

ENV LD_LIBRARY_PATH="/usr/local/lib"
