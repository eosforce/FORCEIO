FROM eosforce/builder as builder

ARG tag=v0.1.0

RUN rm -rf /var/lib/apt/lists/* && apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl golang-1.10

RUN git clone https://github.com/eosforce/FORCEIO.git \
    && cd FORCEIO && git checkout relay && echo "$branch:$(git rev-parse HEAD)" > /etc/forceio-version

ENV GOROOT /usr/lib/go-1.10/
ENV GOPATH /gopath/
ENV PATH $PATH:$GOPATH/bin:$GOROOT/bin

RUN mkdir gopath && mkdir -p gopath/bin \
    && curl https://raw.githubusercontent.com/golang/dep/master/install.sh | sh

RUN cd FORCEIO && git submodule update --init --recursive

RUN export PATH=$PATH:/usr/lib/go-1.10/bin && cd FORCEIO && cmake -H. -B"/tmp/build" -GNinja -DCMAKE_BUILD_TYPE=Release -DWASM_ROOT=/opt/wasm -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_COMPILER=clang -DCMAKE_INSTALL_PREFIX=/tmp/build -DBUILD_MONGO_DB_PLUGIN=true \
    -DCORE_SYMBOL_NAME="SYS" \
    -DUSE_PUB_KEY_LEGACY_PREFIX=1 \
    -DROOT_ACCOUNT="force" \
    -DMAX_PRODUCERS=9 -DBLOCK_INTERVAL_MS=3000 -DPRODUCER_REPETITIONS=1 \
    -DRESOURCE_MODEL=1 \
    && cmake --build /tmp/build --target install

FROM ubuntu:18.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl && rm -rf /var/lib/apt/lists/*
COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder /tmp/build/bin /opt/FORCEIO/bin
COPY nodeosd.sh /opt/FORCEIO/bin/nodeosd.sh
COPY --from=builder /tmp/build/contracts /contracts
COPY config.ini /
COPY genesis.json /opt/
COPY activeacc.json /opt/

ENV EOSIO_ROOT=/opt/FORCEIO
RUN chmod +x /opt/FORCEIO/bin/nodeosd.sh
ENV LD_LIBRARY_PATH /usr/local/lib
VOLUME /opt/FORCEIO/bin/data-dir
ENV PATH /opt/FORCEIO/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
