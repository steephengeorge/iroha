FROM ubuntu:16.04

# number of concurrent threads during build
# usage: docker build --build-arg PARALLELISM=8 -t name/name .
ARG PARALLELISM=1

ENV IROHA_HOME /opt/iroha
ENV IROHA_BUILD /opt/iroha/build

RUN apt-get update; \
    apt-get -y --no-install-recommends install apt-utils software-properties-common; \
    apt-get -y clean

# add git repository
RUN add-apt-repository -y ppa:git-core/ppa; \
    apt-get update

RUN set -e; \
    apt-get -y --no-install-recommends install build-essential python-software-properties \
        automake libtool \
        # dev dependencies
        libssl-dev zlib1g-dev libcurl4-openssl-dev libc6-dbg golang \
        git ssh tar gzip ca-certificates python3 python3-pip python3-setuptools \
        # SWIG dependencies
        libpcre3-dev autoconf bison python3-dev python-dev \
        # other
        ccache wget curl file unzip zip; \
    apt-get -y clean

# install cmake 3.8.2
RUN git clone https://gitlab.kitware.com/cmake/cmake.git /tmp/cmake; \
    (cd /tmp/cmake ; git checkout 0d5a2252ef8a586f4fc70a66aabd17fb3fd52110); \
    (cd /tmp/cmake ; /tmp/cmake/bootstrap --system-curl --parallel=${PARALLELISM} --enable-ccache); \
    make -j${PARALLELISM} -C /tmp/cmake; \
    make -C /tmp/cmake install; \
    ldconfig; \
    rm -rf /tmp/cmake

RUN add-apt-repository -y ppa:webupd8team/java; \
    apt-get update; \
    echo oracle-java8-installer shared/accepted-oracle-license-v1-1 select true | /usr/bin/debconf-set-selections; \
    apt-get -y install oracle-java8-installer; \
    java -version

# Build SWIG
RUN curl -L -o /tmp/swig-3.0.12.tar.gz https://github.com/swig/swig/archive/rel-3.0.12.tar.gz; \
    tar -C /tmp -zxf /tmp/swig-3.0.12.tar.gz; \
    cd /tmp/swig-rel-3.0.12; \
    ./autogen.sh && ./configure && make -j${PARALLELISM}; \
    make install; \
    rm -rf /tmp/swig-rel-3.0.12

RUN add-apt-repository -y ppa:jonathonf/python-3.6; \
    apt-get update; \
    apt-get -y install python3.6-dev

RUN useradd -ms /bin/bash iroha-ci -u 1000 -U

WORKDIR /opt/iroha
RUN set -e; \
    chmod -R 777 /opt/iroha; \
    mkdir -p /tmp/ccache -m 777


USER iroha-ci
CMD ["/bin/bash"]
