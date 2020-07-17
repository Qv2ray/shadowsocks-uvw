## shadowsocks-uvw

A shadowsocks client implementation in uvw


## How to build
If you want shadowsocks-uvw to avoid using the system's own libuv and libsodium, the default compilation options will be helpful. The compilation options are as follows, which will link libuv and libsodium statically.
````bash
git submodule update --init --recursive
mkdir build
cd build
cmake .. -DSSR_UVW_WITH_QT=0
make
````
By default, shadowsocks-uvw will use the libuv and libsodium in the submodule for static linking. To avoid this, you can directly specify the compilation options as shown below.
````bash
mkdir build
cd build
cmake .. -DSSR_UVW_WITH_QT=0 -DUSE_SYSTEM_SODIUM=ON -DUSE_SYSTEM_LIBUV=ON -DSTATIC_LINK_LIBUV=OFF -DSTATIC_LINK_SODIUM=OFF
make
````

## Encrypto method

|   |   |   |   |
|---|---|---|---|
| rc4-md5 |||
| aes-128-gcm | aes-192-gcm | aes-256-gcm |
| aes-128-cfb | aes-192-cfb | aes-256-cfb |
| aes-128-ctr | aes-192-ctr | aes-256-ctr |
| camellia-128-cfb | camellia-192-cfb | camellia-256-cfb |
| bf-cfb | chacha20-ietf-poly1305 | xchacha20-ietf-poly1305 |
| salsa20 | chacha20 | chacha20-ietf |



## Licence

shadowsocks-uvw is under [GPLv3](LICENSE) licence. It's based on [uvw](https://github.com/skypjack/uvw) which is a header-only, event based, tiny and easy to use
[`libuv`](https://github.com/libuv/libuv) wrapper in modern C++.

## Link dependencies

| Name                   | License        |
| ---------------------- | -------------- |
| [libuv](https://github.com/libuv/libuv)   | MIT |
| [libsodium](https://libsodium.org) | ISC |
| [mbedtls](https://github.com/ARMmbed/mbedtls)| Apache|


