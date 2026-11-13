FROM archlinux:latest
RUN pacman -Syu --noconfirm --needed base-devel clang lld llvm git cmake ninja ccache gdb strace python \
    sndio jack2 openal sdl2 libpng alsa-lib libpulse pipewire \
    vulkan-headers vulkan-icd-loader vulkan-radeon vulkan-tools vulkan-validation-layers mesa \
    wayland wayland-protocols libxkbcommon libdecor \
    libx11 libxext libxrandr libxcursor libxi libxfixes libxss libxtst libxinerama \
    libusb hidapi systemd-libs ttf-dejavu sway grim wtype renderdoc xorg-xwayland xdotool \
 && pacman -Scc --noconfirm
RUN useradd -m -u 1000 -G video,audio,input dev
USER dev
WORKDIR /src
