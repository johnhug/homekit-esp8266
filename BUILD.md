# Build Notes

## Crosstool-NG

Currently hardlinked from the `esp-open-sdk` repo.

These patches were needed to achieve success.
    1. Bash (`crosstool-NG/configure.ac`) - Add 5 to the version regex
    1. Expat (`crosstool-NG/config/companion_libs/expat.in`) - Change to 2.4.1
    1. 121-isl (`crosstool-NG/scripts/build/companion_libs/121-isl.sh`) - Change to http://gcc.gnu.org/pub/gcc/infrastructure

All contained in the `crosstool-NG.patch` file

## MacOS M1 Docker

Would you like to cross compile your cross compile environment?

https://github.com/maximkulkin/esp-homekit-demo/wiki/Build-instructions-ESP8266-(Docker)

`docker buildx build --platform linux/amd64 . -f esp-sdk.dockerfile -t esp-sdk --load`

`docker buildx build --platform linux/amd64 . -f esp-rtos.dockerfile -t esp-rtos --load`

```sh
make-esp() {
  docker run --platform linux/amd64 -it --rm -v `pwd`:/project -w /project esp-rtos make "$@"
}
```

## Raspberry Pi 4 (Bullseye)

1. Install [esp-open-sdk](https://github.com/pfalcon/esp-open-sdk), build it with `make toolchain esptool libhal STANDALONE=n`, then edit your `PATH` and add the generated toolchain bin directory. The path will be something like `/path/to/esp-open-sdk/xtensa-lx106-elf/bin`.  
1. Install [esptool.py](https://github.com/themadinventor/esptool) and make it available on your `PATH`.  Make sure ahead of esp-open-sdk `PATH` as you don't want the included.
1. Checkout [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos) and set `SDK_PATH` environment variable pointing to it.

## Dependencies

`git submodule update --init --recursive`

Imported in as submodules into ./include

- https://github.com/maximkulkin/esp-homekit
- https://github.com/maximkulkin/esp-wolfssl
- https://github.com/maximkulkin/esp-cjson
- https://github.com/maximkulkin/esp-wifi-config
