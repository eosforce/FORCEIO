FROM eosforce/builder as builder
ARG branch=master

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl ssh && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/eosforce/FORCEIO.git --recursive \
    && cd FORCEIO && echo "$branch:$(git rev-parse HEAD)" > /etc/forceio-version
    
RUN cd FORCEIO && git submodule update --init --recursive

RUN cd FORCEIO && cmake -H. -B"/tmp/build" -GNinja -DCMAKE_BUILD_TYPE=Release -DWASM_ROOT=/opt/wasm -DCMAKE_CXX_COMPILER=clang++ \
       -DCMAKE_C_COMPILER=clang -DCMAKE_INSTALL_PREFIX=/tmp/build  -DSecp256k1_ROOT_DIR=/usr/local -DBUILD_MONGO_DB_PLUGIN=true \
    && cmake --build /tmp/build --target install

FROM ubuntu:18.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl && rm -rf /var/lib/apt/lists/*
COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder /tmp/build /opt/FORCEIO/build

COPY bios_boot_forceio.py  /opt/FORCEIO/
COPY forceio.py  /opt/FORCEIO/
COPY start.sh  /opt/FORCEIO/

RUN chmod +x /opt/FORCEIO/start.sh
RUN chmod +x /opt/FORCEIO/bios_boot_forceio.py
ENV LD_LIBRARY_PATH /usr/local/lib
VOLUME /opt/FORCEIO
ENV PATH /opt/FORCEIO:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install python