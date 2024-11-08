# serialosc

serialosc is an osc server for monome devices

it is a daemon that waits for you to plug a monome in, then it spawns off a dedicated server process to route OSC messages to and from your monome. it also advertises a `_monome-osc._udp` service over zeroconf (bonjour).

see [releases](https://github.com/monome/serialosc/releases) for mac and windows installers.

## requirements

- [libmonome](https://github.com/monome/libmonome)
- [liblo](https://liblo.sourceforge.net/)
- [libuv](https://libuv.org/)

## build

to build with waf:

```
git submodule update --init --recursive
./waf configure
./waf
sudo ./waf install
```

to build with cmake (experimental):

```
git submodule update --init --recursive
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

cmake project is currently configured to build statically linked binaries using bundled libmonome, liblo, libuv and is the suggested way of building serialosc on windows (msys2).

## running at boot

On Linux, a systemd unit will be available as the unpriviledged user that ran the installation. You can enable serialoscd to start at boot by running `systemctl --user enable serialoscd.service` which will start the daemon at each login.

To manually start and stop, this works like any other systemd unit, for example to start manually `systemctl --user start serialoscd.service` will do.

## documentation

https://monome.org/docs/serialosc

## preference file locations

- linux: `$HOME/.config/serialosc`
- macos: `~/Library/Preferences/org.monome.serialosc`
- windows: `C:\Users\<username>\AppData\Local\Monome\serialosc`
