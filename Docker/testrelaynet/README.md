# testrelaynet in docker

run a testrelaynet in docker

build:

```bash
sudo docker build -t forceio/testrelaynet testrelaynet
```

if in China, can use build-arg to use proxy:

```bash
sudo docker build --build-arg HTTPS_PROXY=socks5://10.1.1.135:57867 --build-arg HTTP_PROXY=socks5://10.1.1.135:57867 -t forceio/testrelaynet testrelaynet
```

image will start nodes which http api in 0.0.0.0:10001 - 0.0.0.0:10020, p2p in 0.0.0.0:10101 - 0.0.0.0:10120

```bash
sudo docker run -d --name tt -p 8001:8001 -p 8101:8101 forceio/testrelaynet start.sh
```