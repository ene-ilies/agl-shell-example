# home

A application using binding example for AGL

## Setup 

```bash
git clone 
cd home
```

## Build  for AGL

```bash
#setup your build environement
. /xdt/sdk/environment-setup-aarch64-agl-linux
#build your application
./autobuild/agl/autobuild package
```

## Build for 'native' Linux distros (Fedora, openSUSE, Debian, Ubuntu, ...)

```bash
./autobuild/linux/autobuild package
```

## Deploy

### AGL

```bash
export YOUR_BOARD_IP=192.168.1.X
export APP_NAME=home
scp build/${APP_NAME}.wgt root@${YOUR_BOARD_IP}:/tmp
#install the widget
ssh root@${YOUR_BOARD_IP} afm-util install /tmp/${APP_NAME}.wgt
APP_VERSION=$(ssh root@${YOUR_BOARD_IP} afm-util list | grep ${APP_NAME}@ | cut -d"\"" -f4| cut -d"@" -f2)
#start the binder
ssh root@${YOUR_BOARD_IP} afm-util start ${APP_NAME}@${APP_VERSION}
```