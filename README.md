# Runescape 3 NXT launcher (linux)

This project is a minimalistic launcher for Runescape 3 (NXT). Compatible with v2.2.4.
It uses a (portable, /bin/sh) shell script to download the game config, update files and then run the game. 
This project has been tested on Debian and Void GNU/Linux.

### Prerequisities

The shell script requies the following programs
```
rhash - rhash - used to verify that files were downloaded correctly
curl - curl - used to download files
7zip - p7zip - used to compress/decompress files
gcc - gcc - compile the program
```

To run on a fresh ubuntu install you should be able to run:
```
sudo apt install rhash curl p7zip-full gcc git libsdl2-2.0-0
```
and then follow the instructions below.


### Installing
```
git clone https://github.com/yoloium/rs3-launcher.git
cd rs3-launcher
```
Now modify launcher.c and rs3-launcher in your favourite text editor e.g.
```vim launcher.c```
And change any of the window numbers from lines 75 to 82.
In the script you can edit the top lines of code to point to where you have your game cache and user information located. By default they use the official client's defaults.
Now compile the c file
```gcc launcher.c -o launcher```
When the configuration looks good, you can run the script;
```
./rs3-launcher
```

If the game window doesn't open, make sure that all dependencies are met with the newly downloaded client with:
```
ldd librs2client.so | grep 'not found'
```

## Fixing issues

### Integrated graphics

The game doesn't render the world; it's just sky blue. Fix: add this to the main scope of the script:
```
export MESA_GL_VERSION_OVERRIDE=3.0
```
NOTE if you have an older graphics card, you can replace '3.0' with '2.1' so that you can play on a system which does not support opengl 3.0

## Contributing

Will consider accepting pull requests. But don't expect me to look here. PM me in game or something to let me know; RSN = yoloium

## Alternatives
1. Python + shell equivalent: https://gist.github.com/yoloium/3b2ece56c33cf6dc013e75cc64e4124a. (not updated for 2.2.4)
2. Minimal (legacy) java client launcher: https://gist.github.com/yoloium/0dd28b23da8cf1d9c409972141137967. NOTE: read comment for RS3 usage.
3. (bloated) C++ equivalent: https://github.com/syldrathecat/nxtlauncher

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details

## Acknowledgments

syldrathecat for https://github.com/syldrathecat/nxtlauncher. This project served as inspiration this project. It provides a lot of answers on how the client executes. FYI that project is very overcomplicated and over-engineered.
