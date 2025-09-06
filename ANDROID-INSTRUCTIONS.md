### **Basic Installation Requirements :**

To run kew on Android please install the following applications :

- **Termux** : A terminal emulator for Android that allows you to run Linux commands on your device.

  [![Download Termux](https://img.shields.io/badge/Download-Termux-brightgreen?style=for-the-badge&logo=android)](https://github.com/termux/termux-app/releases/) - click to go to downloads page

- **Termux-Api** : A plugin for Termux that executes Termux-api package commands.

  [![Download Termux-Api](https://img.shields.io/badge/Download-Termux--API-blue?style=for-the-badge&logo=android)](https://github.com/termux/termux-api/releases/download/v0.53.0/termux-api-app_v0.53.0+github.debug.apk) - click to download

### **Termux Setup:**

1. **Update and install dependencies**
```sh
pkg install tur-repo -y && yes | pkg upgrade -y && pkg install clang pkg-config taglib fftw git make chafa glib libopus opusfile libvorbis libogg dbus termux-api
```

2. **Check Termux sound volume:**
to make sure termux has sound use this command:
```
termux-volume music 10
```

<details>
<summary><b>Building Faad2 from source(optional)</b></summary>

```sh
pkg install cmake make clang
git clone https://github.com/knik0/faad2
cd faad2
cmake -DCMAKE_EXE_LINKER_FLAGS="-lm" . -D CMAKE_INSTALL_PREFIX=/data/data/com.termux/files/usr
make install
```

</details>

2. **Enable storage permissions**
```sh
termux-setup-storage
```
Tap allow for the setup to finish

3. **Setup dbus for kew**
* edit/create `~/.bashrc`
```
nano ~/.bashrc
```

* add this alias and save it
```bash
alias kew="dbus-launch kew"
```

### **Compiling Kew:**

```sh
git clone https://github.com/ravachol/kew.git
cd kew
make -j4
make install
```

### **Run kew:**

1. Restart the shell: `exec $SHELL`
2. Run the `headless` command
```
$ headless
```
3. Set kew's music library
```
kew path <music path>
```
4. Run kew
```
kew
```
