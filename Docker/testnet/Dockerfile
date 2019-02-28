FROM eosforce/builder as builder
ARG branch=master

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl ssh && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/eosforce/FORCEIO.git --recursive \
    && cd FORCEIO && git checkout relay && echo "$branch:$(git rev-parse HEAD)" > /etc/forceio-version
    
RUN cd FORCEIO && git submodule update --init --recursive

RUN apt-get update && apt-get -y install golang-1.10 && export PATH=$PATH:/usr/lib/go-1.10/bin

RUN cd FORCEIO/externals/grpc/third_party/protobuf/ && ./autogen.sh && ./configure --prefix=/opt/protobuf && make -j12 && make install 

RUN ls /usr/local/

RUN cd FORCEIO/externals/grpc && make -j12 && make install prefix=/opt/grpc
RUN cp -r /opt/grpc/lib/* /usr/local/lib/

RUN export PATH=$PATH:/usr/lib/go-1.10/bin && cd FORCEIO && cmake -H. -B"/tmp/build" -GNinja -DCMAKE_BUILD_TYPE=Release -DWASM_ROOT=/opt/wasm -DCMAKE_CXX_COMPILER=clang++ \
       -DCMAKE_C_COMPILER=clang -DCMAKE_INSTALL_PREFIX=/tmp/build  -DSecp256k1_ROOT_DIR=/usr/local -DBUILD_MONGO_DB_PLUGIN=true -DPROTOC=/opt/protobuf/bin/protoc -DGRPC_CPP_PLUGIN=/opt/grpc/bin/grpc_cpp_plugin \
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
