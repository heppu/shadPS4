# Black Ops 4 dev loop for shadPS4

Scripts used to build, run and debug shadPS4 with Call of Duty: Black Ops 4 (CUSA11100) on a host that cannot build the emulator natively. Everything runs in a docker image with a headless sway session, so nothing opens on the desktop. Emulator changes live on the `bo4` branch.

Paths inside the scripts point at the machine they were written on (`/mnt/games/PS4/bo4-dev`, the fork checkout under `~/code`). Adjust them before use.

## Layout

- `bo4-dev/Dockerfile`, `sd`, `build`: build image, run a command in the container, incremental build.
- `bo4-dev/headless`, `sway.conf`, `run`, `play`: headless run with a time limit, and a windowed run with audio and gamepads.
- `bo4-dev/guard`: watchdog started by every run script. It kills the emulator when GPU memory or free RAM crosses a limit.
- `bo4-dev/shot`, `key`, `step`, `zmplay.sh`, `zmslow.sh`: screenshots, key injection, scripted route into a Zombies match.
- `bo4-dev/hang`, `bprun`, `peekrun`, `gdb*`, `bp.py`, `peek.py`, `guest.py`, `watch.py`: gdb helpers for guest registers, host and guest breakpoints, memory dumps and runtime byte patches.
- `bo4-dev/xr.py`, `xref.py`, `dis`: cross references and disassembly over a dumped eboot image.
- `bo4-dev/patches`: emulator patch file that opens the offline mode cards when the downloaded playlist data is missing. Copy it to `user/patches/local` in the emulator user folder.

Game data, the emulator user folder, build output and logs are not part of this branch.
