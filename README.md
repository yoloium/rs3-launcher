# Runescape 3 NXT launcher (linux)

This project is a minimalistic launcher for Runescsape 3 (NXT).
It uses a (portable, /bin/sh) shell script to download the game config, update files and then run the game. 
This project has only been tested on Debian GNU/Linux. 

## Getting Started

You need to make sure that you have all the libraries installed to load $HOME/Jagex/launcher/librs2client.so.
You can check this by executing
```
ldd ~/Jagex/launcher/librs2client.so | grep 'not found'
```
### Prerequisities

The shell script requies the following programs
```
crc32 - libarchive-zip-perl - used to verify that files were downloaded correctly
curl - curl - used to download files
unlzma - xz-utils - used to decompress files
gcc - gcc - compile the program
```

To run on a fresh ubuntu install you should be able to run:
```
sudo apt install libarchive-zip-perl curl xz-utils gcc git lzma libsdl2-2.0-0
```
and then follow the instructions below.


### Installing
```
git clone https://github.com/yoloium/rs3-launcher.git
cd rs3-launcher
gcc -pthread rs3-launcher.c -o launcher -ldl
```
DO NOT RUN THE CLIENT YET
Open rs3-launcher in your favourite text editor e.g.
```
vim rs3-launcher
```
and edit the configuration at the top of this script. If you have a non-standard cache location, you will need to change this file to reflect on where the cache is kept. 

When the configuration looks good, now it's time to run the script.
```
./rs3-launcher
```

## Fixing issues

### Integrated graphics

The game doesn't render the world; it's just sky blue. Fix: add this to the main scope of the script:
```
export MESA_GL_VERSION_OVERRIDE=3.0
```

### Problem decompressing the LZMA file.

you probably don't have lzma installed. 

### No window title
uncomment ```define ENABLE_X11``` at the top of the C file. Then compile with the ```-lX11``` flag e.g. ```gcc -pthread rs3-launcher.c -o launcher -ldl -lX11```.

## Contributing

Will consider accepting pull requests. But don't expect me to look here. PM me in game or something to let me know; RSN = yoloium

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details

## Acknowledgments

syldrathecat for https://github.com/syldrathecat/nxtlauncher. This project served as inspiration this project. It provides a lot of answers on how the client executes. FYI that project is very overcomplicated and over-engineered.
