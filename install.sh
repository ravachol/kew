#!/bin/bash

# Check if the script is running as root
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root."
   exit 1
fi

# Install dependencies based on the package manager available
if command -v apt &>/dev/null; then
    apt install -y ffmpeg libfftw3-dev git
elif command -v yum &>/dev/null; then
    yum install -y ffmpeg fftw-devel git
elif command -v pacman &>/dev/null; then
    pacman -Syu --noconfirm ffmpeg fftw git
elif command -v dnf &>/dev/null; then
    dnf install -y ffmpeg fftw-devel git
elif command -v zypper &>/dev/null; then
    zypper install -y ffmpeg fftw-devel git
elif command -v eopkg &>/dev/null; then
    eopkg install -y ffmpeg fftw-devel git
elif command -v guix &>/dev/null; then
    guix install -y ffmpeg fftw git
else
    echo "Unsupported package manager. Please install the required dependencies manually."
    exit 1
fi

git clone https://github.com/ravachol/cue.git
if [ -d "cue" ]; then
    cd cue
    make
    if sudo make install; then
        echo "Installation completed successfully."
    else
        echo "Installation encountered an error."
    fi
else
    echo "Failed to clone the repository. Please check your network connection and try again."
    exit 1
fi