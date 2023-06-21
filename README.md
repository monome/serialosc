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

all steps
```
sudo apt-get install liblo-dev
git clone https://github.com/monome/libmonome.git
cd libmonome
./waf configure
./waf
sudo ./waf install
cd ..

sudo apt-get install libudev-dev libavahi-compat-libdnssd-dev libuv1-dev
git clone https://github.com/monome/serialosc.git
cd serialosc
git submodule init && git submodule update
./waf configure --enable-system-libuv
./waf
sudo ./waf install
cd ..
```

## preference file locations

- linux: `$XDG_CONFIG_HOME/serialosc` or `$HOME/.config/serialosc`
- macos: `~/Library/Preferences/org.monome.serialosc`
- windows: `$APPDATA\\Monome\\serialosc`
