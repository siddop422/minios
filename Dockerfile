FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential gcc-multilib binutils xorriso mtools \
    grub-common grub-pc-bin grub-efi-amd64-bin \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN make
