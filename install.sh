#!/bin/bash

# Check if the script is running as root
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root."
   exit 1
fi

# Removing old files if exist
if [ -d "kew" ]; then
    echo "Removing old files"
    rm -rf kew &>/dev/null
fi

# Install dependencies based on the package manager available
echo "Installing missing dependencies"
if command -v apt &>/dev/null; then
    apt install -y pkg-config ffmpeg libfftw3-dev libopus-dev libopusfile-dev libvorbis-dev git gcc make libchafa-dev libfreeimage-dev libavformat-dev libglib2.0-dev libnotify-dev
elif command -v yum &>/dev/null; then
    yum install -y pkgconfig ffmpeg fftw-devel opus-devel opusfile-devel libvorbis-devel git gcc make chafa-devel libfreeimage-devel libavformat-devel glib2-devel libnotify-devel
elif command -v pacman &>/dev/null; then
    pacman -Syu --noconfirm --needed pkg-config ffmpeg fftw git gcc make chafa freeimage glib2 opus opusfile libvorbis libnotify
elif command -v dnf &>/dev/null; then
    dnf install -y pkg-config ffmpeg-free-devel fftw-devel opus-devel opusfile-devel libvorbis-devel git gcc make chafa-devel freeimage-devel libavformat-free-devel glib2-devel libnotify-devel
elif command -v zypper &>/dev/null; then
    zypper install -y pkg-config ffmpeg fftw-devel opus-devel opusfile-devel libvorbis-devel git chafa-devel gcc make libfreeimage-devel libavformat-devel glib2-devel libnotify-devel
elif command -v eopkg &>/dev/null; then
    eopkg install -y pkg-config ffmpeg fftw-devel opus-devel opusfile-devel libvorbis-devel git gcc make chafa-devel libfreeimage-devel libavformat-devel libnotify-devel
elif command -v guix &>/dev/null; then
    guix install pkg-config ffmpeg fftw git gcc make chafa libfreeimage libavformat opus opusfile libvorbis libnotify
elif command -v xbps-install &>/dev/null; then
    xbps-install -y pkg-config ffmpeg fftw git gcc make chafa libfreeimage libavformat opus opusfile libvorbis libnotify-devel
else
    echo "Unsupported package manager. Please install the required dependencies manually."
    exit 1
fi

# Clone the repository
repo_url="https://github.com/ravachol/kew.git"
echo "Cloning the repository..."
if git clone "$repo_url" --depth=1 &>/dev/null; then
    echo "Repository cloned successfully."
else
    echo "Failed to clone the repository. Please check your network connection and try again."
    exit 1
fi

# Changing directory
cd kew

# Building
echo "Building the project..."
if make &> build.log; then
    echo "Build completed successfully."
else
    echo "Build encountered an error. Please check the build.log file for more information."
    exit 1
fi

# Installing
echo "Installing the project..."
if sudo make install; then
    echo "Installation completed successfully."
else
    echo "Installation encountered an error."
    exit 1
fi

# Cleaning up the directory
echo "Cleaning directory..."
cd ..
rm kew -rf &>/dev/null
