#include <cstdio>
#include <thread>
#include <unistd.h>

extern "C" void __llvm_profile_set_filename(const char*);
extern "C" int __llvm_profile_write_file();

__attribute__((constructor)) static void StartProfileCheckpoint() {
    std::thread([] {
        static char output[256];
        snprintf(output, sizeof(output), "/out/performance/pgo-checkpoints/game-%d.profraw", getpid());
        while (access("/out/performance/pgo-dump.request", F_OK) != 0) {
            usleep(250000);
        }
        unlink("/out/performance/pgo-dump.request");
        __llvm_profile_set_filename(output);
        const int result = __llvm_profile_write_file();
        if (auto* status = fopen("/out/performance/pgo-dump.status", "w")) {
            fprintf(status, "%d %s\n", result, output);
            fclose(status);
        }
    }).detach();
}
