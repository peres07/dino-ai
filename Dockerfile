FROM debian:trixie-slim AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends clang lld make python3-pil \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY Makefile ./
COPY wasm/ wasm/
COPY scripts/assets.py scripts/assets.py
COPY web/ web/
COPY arquivos/ arquivos/
COPY fontes/ fontes/
COPY ["Dino sem pássaros/arquivos/", "Dino sem pássaros/arquivos/"]
COPY ["Dino sem pássaros/fontes/", "Dino sem pássaros/fontes/"]
RUN make -j2

FROM nginx:stable-alpine
COPY deploy/nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=build /src/dist/ /usr/share/nginx/html/
