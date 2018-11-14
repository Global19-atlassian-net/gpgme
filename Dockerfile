FROM debian:jessie

WORKDIR /build

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
    gdb  \
    autoconf \
    automake \
    build-essential \
    git \
	libtool \
    texinfo \
    linux-perf-4.9 \
    wget

# LibGPG-ERROR
RUN wget https://gnupg.org/ftp/gcrypt/libgpg-error/libgpg-error-1.32.tar.bz2 && tar xf libgpg-error-1.32.tar.bz2 && cd /build/libgpg-error-1.32
WORKDIR /build/libgpg-error-1.32
RUN ./configure --prefix=/usr
RUN make && make install 

# Libassuan
WORKDIR /build
RUN wget https://gnupg.org/ftp/gcrypt/libassuan/libassuan-2.5.1.tar.bz2 && tar xf libassuan-2.5.1.tar.bz2 && cd /build/libassuan-2.5.1
WORKDIR /build/libassuan-2.5.1
RUN ./configure --prefix=/usr
RUN make && make install

# GPGME
WORKDIR /build
#RUN wget https://gnupg.org/ftp/gcrypt/gpgme/gpgme-1.12.0.tar.bz2 && tar xf gpgme-1.12.0.tar.bz2 && cd /build/gpgme-1.12.0
COPY . /build/gpgme/
WORKDIR /build/gpgme
RUN ./autogen.sh
RUN automake --add-missing
RUN ./configure --prefix=/usr --with-libassuan-prefix=/usr --with-libgpg-error-prefix=/usr --disable-gpgsm-test
RUN make && make install

#WORKDIR /build/gpgme-1.12.0
#RUN ./configure --prefix=/usr --disable-gpgsm-test
#RUN make && make install
