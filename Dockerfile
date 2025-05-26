# OR USE docker run -it --name scrcpy-build -v "D:\Projekty\FlexiFin\pluritra\tools\device-controller\bin\scrcpy:/src/scrcpy" docker:latest
FROM debian:latest

# Install common dependencies
RUN apt update && apt install -y \
    meson \
    ninja-build \
    nasm \
    yasm \
    texinfo \
    pkg-config \
    libsdl2-2.0-0 \
    libsdl2-dev \
    libusb-1.0-0 \
    libusb-1.0-0-dev \
    libv4l-dev \
    libtesseract-dev \
    mingw-w64 \
    mingw-w64-tools \
    libz-mingw-w64-dev \
    git \
    make \
    cmake \
    gcc \
    g++ \
    wget \
    autoconf \
    libtool \
    python3 \
    python3-pip \
    libpng-dev \
    libjpeg-dev \
    libopenjp2-7-dev \
    libgif-dev \
    libtiff-dev \
    libwebp-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /src/scrcpy

# Copy the source code
COPY . .

# Build commands
RUN chmod +x release/build_linux.sh release/build_windows.sh

# Build for Linux x86_64
RUN release/build_linux.sh x86_64

# Build for Windows 32-bit
RUN release/build_windows.sh 32

# Build for Windows 64-bit
RUN release/build_windows.sh 64

# The builds will be available in these directories:
# - /src/scrcpy/release/work/build-linux-x86_64/dist/
# - /src/scrcpy/release/work/build-win32/dist/
# - /src/scrcpy/release/work/build-win64/dist/
