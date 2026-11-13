#define SHADTEST_NAME "presentation_idle"
#include "shadtest_guest.h"
int main(void) {
    st_begin();
    sceKernelUsleep(2000000);
    puts("SHADTEST name=presentation_idle status=PASS host_present_only=1");
    fflush(stdout);
    return 0;
}
