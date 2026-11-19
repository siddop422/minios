# MiniOS

A freestanding 32-bit x86 hobby OS with a GRUB2/Multiboot2 framebuffer desktop.

## Build
On Debian/Ubuntu:
```sh
sudo apt-get update
sudo apt-get install -y build-essential gcc-multilib binutils xorriso mtools grub-common grub-pc-bin grub-efi-amd64-bin
make
```

The output is `minios.iso`. GitHub Actions builds and verifies the ISO on every push to `main`, uploads a `minios-iso` artifact, and publishes a standalone HTML downloader containing the ISO as embedded Base64 at the GitHub Pages URL.

## Keyboard commands
Type `help`, `about`, `clear`, or `reboot` in the GUI input field.
