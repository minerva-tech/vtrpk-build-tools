FROM ubuntu:14.04
RUN apt-get update && apt-get install -y gcc-4.8 g++-4.8
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.8 100
RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-4.8 100
RUN update-alternatives --install /usr/bin/cpp cpp /usr/bin/cpp-4.8 100
RUN apt-get install -y libgmp-dev libmpfr-dev libmpc-dev make texinfo
RUN apt-get install -y bison flex gettext patch
RUN apt-get install -y wget python
RUN apt-get install -y unzip rsync
# GMP 4.2+, MPFR 2.4.0+ and MPC 0.8.0+.
