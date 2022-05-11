# serialosc

serialosc is an osc server for monome devices

it is a daemon that waits for you to plug a monome in, then it spawns off a dedicated server process to route OSC messages to and from your monome.  it also advertises a _monome-osc._udp service over zeroconf (bonjour).  

see [releases](https://github.com/monome/serialosc/releases) for mac and windows installers.

## requirements

- [libmonome](https://github.com/monome/libmonome)

## build

```
./waf configure
./waf
sudo ./waf install
```
## portable service for Linux

Some Linux systems can install serialosc as a [systemd portable service](https://systemd.io/PORTABLE_SERVICES/).

Instructions to set this up:

 1. install [mkosi](https://github.com/systemd/mkosi/) build tool.
 2. build and install disk image:

      sudo mkosi -t directory -o /opt/serialosc/

 3. attach to system, enable and start service:

      sudo portablectl attach --enable --now /opt/serialosc --profile trusted

To disable the service again, you can run `sudo portablectl detach --now /opt/serialosc`.

Note that because `mkosi` runs as root, you may hit issues with the 'safe directories' feature
of Git version >= 2.35. If needed, you can disable this safety feature by running `sudo git config
--global --add safe.directory '*'`.
