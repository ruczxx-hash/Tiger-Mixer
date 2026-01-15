#include "util.h"
#include <stdio.h>

int main() {
    if (core_init() != RLC_OK) {
        core_clean();
        printf("RELIC core initialization failed!\n");
        return 1;
    }
    ep_param_set_any();
    cl_params_t params;
    cl_params_null(params);
    cl_params_new(params);
    generate_cl_params(params);
    if (generate_keys_and_write_to_file(params) == 0) {
        printf("All keys generated successfully!\n");
    } else {
        printf("Key generation failed!\n");
    }
    cl_params_free(params);
    core_clean();
    return 0;
} 