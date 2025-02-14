#include "shaderjit.h"

#include <xxh3.h>

#include "../gpu.h"
#include "shaderjit_backend.h"

ShaderJitFunc shaderjit_get(GPU* gpu, ShaderUnit* shu) {
    u64 hash = XXH3_64bits(shu->code, SHADER_CODE_SIZE * sizeof(PICAInstr));
    ShaderJitBlock* block = nullptr;
    for (int i = 0; i < VSH_MAX; i++) {
        if (gpu->vshaders.d[i].hash == hash || gpu->vshaders.d[i].hash == 0) {
            block = &gpu->vshaders.d[i];
            break;
        }
    }
    if (!block) {
        block = LRU_eject(gpu->vshaders);
    }
    LRU_use(gpu->vshaders, block);
    if (block->hash != hash) {
        block->hash = hash;
        shaderjit_backend_free(block->backend);
        block->backend = shaderjit_backend_init();
    }
    return shaderjit_backend_get_code(block->backend, shu);
}