# testnet in docker

run a testnet in docker

build:

```bash
sudo docker build -t forceio/testnet testnet
```

if in China, can use build-arg to use proxy:

```bash
sudo docker build --build-arg HTTPS_PROXY=socks5://10.1.1.135:57867 --build-arg HTTP_PROXY=socks5://10.1.1.135:57867 -t forceio/testnet testnet
```

image will start nodes which http api in 0.0.0.0:8001 - 0.0.0.0:8020, p2p in 0.0.0.0:8101 - 0.0.0.0:8120

```bash
sudo docker run -d --name tt -p 8001:8001 -p 8101:8101 forceio/testnet start.sh
```