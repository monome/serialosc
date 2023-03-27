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
sudo apt-get install git build-essential liblo-dev libudev-dev libusb-1.0 libusb-dev
git clone git@github.com:monome/libmonome.git
cd libmonome
./waf configure
./waf
sudo ./waf install
cd ..
git clone git@github.com:monome/serialosc.git
cd serialosc
./waf configure
./waf
sudo ./waf install
```
