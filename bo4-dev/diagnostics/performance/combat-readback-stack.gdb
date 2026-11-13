set pagination off
set confirm off
set print thread-events off
handle SIGSEGV SIGBUS SIGILL SIGUSR1 SIGUSR2 SIGPWR SIGXCPU nostop noprint pass
break /src/src/video_core/buffer_cache/buffer_cache.cpp:245
continue
print device_addr
print size
up
print *this
thread apply all bt 14
detach
quit
