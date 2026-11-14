set pagination off
set confirm off
set print thread-events off
handle SIGSEGV SIGBUS SIGILL SIGUSR1 SIGUSR2 SIGPWR SIGXCPU nostop noprint pass
break Vulkan::Runtime::DownloadImageArray
continue
print src->info.pixel_format
print src->info.resources
print copies
finish
print $rax
detach
quit
