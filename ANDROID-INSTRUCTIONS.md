Here's one way of running kew on android. Might not work on your device!

1. Install F-Droid https://f-droid.org/en/docs/Get_F-Droid/.

2. Install Termux from F-Droid (not Play Store).

3. Install Termux:API from F-Droid and run pkg install termux-api from termux

4. Allow Music, Notices, File Access for Termux in Android App Settings.

5. Run in termux:

	termux-setup-storage

	This will:
* Request storage permission from Android.
* You'll see a permission dialog - tap "Allow".

6. Attach your phone to your computer and copy your music to your music folder on your phone.

7. If you need .m4a file support you need to compile the faad library yourself at this point, because it's not available on temux. kew will detect that it's installed when compiling and include it:

        pkg install git make binutils clang libtool autoconf automake m4 pkg-config
        wget https://sourceforge.net/projects/faac/files/faad2-src/faad2-2.8.0/faad2-2.8.8.tar.gz
        tar -xzf faad2-2.8.8.tar.gz
        cd faad2-2.8.8
        ./configure --prefix=$PREFIX
        make -j$(nproc)
        make install

8. Run in termux:

	pkg install git make pkg-config chafa fftw taglib libvorbis opusfile pulseaudio

	git clone https://github.com/ravachol/kew.git

	cd kew

	make -j4

	make install

	kew path ~/storage/music

9. Run in termux:

	mkdir -p ~/.config/pulse

	pulseaudio --kill &
	pkill -9 pulseaudio

	export PULSE_RUNTIME_PATH="$PREFIX/var/run/pulse"

	pulseaudio --start --load="module-native-protocol-tcp auth-ip-acl=127.0.0.1 auth anonymous=1" --load="module-sles-source" --exit-idle-time=-1

	export XDG_RUNTIME_DIR=${TMPDIR}

	export PULSE_SERVER=127.0.0.1

	export $(dbus-launch)


10. Run kew:

	kew

11. Sound issue

If there's no sound you might need to change the music volume in termux from 0 to higher (max 15).

termux-volume 8
