#ifdef __FREERTOS
#include "../../../FreeRTOS-Kernel/include/portable.h"

/* Heap-4 extensions — only available when heap_4a is the active scheme */
#if (configFREERTOS_HEAP_SCHEME == 4)
#ifdef __cplusplus
extern "C" {
#endif
    void *  pvPortRealloc( void * pv, size_t xWantedSize ) PRIVILEGED_FUNCTION;
    size_t  xPortGetBlockSize( void * pv ) PRIVILEGED_FUNCTION;
#ifdef __cplusplus
}
#endif  // __cplusplus
#endif  // configFREERTOS_HEAP_SCHEME == 4

#endif
