ARG DISTRO
FROM ubuntu:${DISTRO}
# We need to repeat ARG here to make it available inside build context
# See https://docs.docker.com/engine/reference/builder/#understand-how-arg-and-from-interact
ARG DISTRO

MAINTAINER Siddharth Suresh <siddharth@stellar.org>

EXPOSE 11625
EXPOSE 11626

VOLUME /data
VOLUME /postgresql-unix-sockets

ADD setup /
RUN /setup

ARG GRAMR_VERSION

#install gramr
RUN wget -qO - https://apt.stellar.org/SDF.asc | apt-key add -
RUN echo "deb https://apt.stellar.org ${DISTRO} unstable" | tee -a /etc/apt/sources.list.d/SDF-unstable.list
RUN apt-get update && apt-get install -y gramr=${GRAMR_VERSION}

WORKDIR "/etc/stellar"
ENTRYPOINT ["/usr/bin/gramr"]
