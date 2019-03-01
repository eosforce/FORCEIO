FROM eosforce/builder as builder

ARG tag=v0.1.0

RUN rm -rf /var/lib/apt/lists/* && apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl ssh golang-1.10
RUN mkdir /root/.ssh && chmod 755 /root/.ssh

RUN git clone https://github.com/eosforce/FORCEIO.git \
    && cd FORCEIO && git checkout relay && echo "$branch:$(git rev-parse HEAD)" > /etc/forceio-version


RUN cd FORCEIO && git submodule update --init --recursive

RUN cd FORCEIO/externals/grpc/third_party/protobuf/ && ./autogen.sh && ./configure --prefix=/opt/protobuf && make -j12 && make install 

RUN cd FORCEIO/externals/grpc && make -j12 && make install prefix=/opt/grpc && cp -r /opt/grpc/lib/* /usr/local/lib/

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
COPY --from=builder /tmp/build/bin /opt/eosio/bin
COPY --from=builder /tmp/build/contracts  /tmp/contracts
COPY nodeosd.sh /opt/eosio/bin/nodeosd.sh

COPY --from=builder /tmp/build/contracts/relay.token  /relay.token

COPY --from=builder /tmp/build/contracts/force.msig/force.msig.abi   /
COPY --from=builder /tmp/build/contracts/force.msig/force.msig.wasm  /
COPY --from=builder /tmp/build/contracts/force.relay/force.relay.abi   /
COPY --from=builder /tmp/build/contracts/force.relay/force.relay.wasm  /
COPY --from=builder /tmp/build/contracts/force.system/force.system.abi   /
COPY --from=builder /tmp/build/contracts/force.system/force.system.wasm  /
COPY --from=builder /tmp/build/contracts/force.token/force.token.abi   /
COPY --from=builder /tmp/build/contracts/force.token/force.token.wasm  /

COPY config.ini /
COPY genesis.json /
COPY activeacc.json /

ENV EOSIO_ROOT=/opt/eosio
RUN chmod +x /opt/eosio/bin/nodeosd.sh
ENV LD_LIBRARY_PATH /usr/local/lib
VOLUME /opt/eosio/bin/data-dir
ENV PATH /opt/eosio/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin