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

### Installing
```
git clone https://github.com/yoloium/rs3-launcher.git
cd rs3-launcher
gcc -pthread rs3-launcher.c -o launcher -ldl
./rs3-launcher
```
## Contributing

Will consider accepting pull requests. But don't expect me to look here. PM me in game or something to let me know; RSN = yoloium

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details

## Acknowledgments

syldrathecat for https://github.com/syldrathecat/nxtlauncher. This project served as inspiration this project. It provides a lot of answers on how the client executes. FYI that project is very overcomplicated and over-engineered.
