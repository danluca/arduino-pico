/*
 * C++ memory allocation overrides for Heap_4a heap scheme
 *
 * Copyright (c) by Dan Luca. Licensed under the MIT License (see LICENSE file for details).
 */


// Heap ownership boundary:
// - C malloc/calloc/realloc/free stay with arduino-pico's core wrappers.  Those wrappers
//   call Newlib's real allocator with the core's locking, and are safe during early boot.
// - App C++ allocations use the FreeRTOS heap_4 allocator through new/delete.
//
#if ( configFREERTOS_HEAP_SCHEME == 4 )

#include <Arduino.h>
#include <FreeRTOS.h>
#include <new>

void* operator new(size_t size) {
    return pvPortMalloc(size);
}

void* operator new[](size_t size) {
    return pvPortMalloc(size);
}

void* operator new(size_t size, const std::nothrow_t&) noexcept {
    return pvPortMalloc(size);
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept {
    return pvPortMalloc(size);
}

void operator delete(void* p) noexcept {
    vPortFree(p);
}

void operator delete[](void* p) noexcept {
    vPortFree(p);
}

void operator delete(void* p, size_t) noexcept {
    vPortFree(p);
}

void operator delete[](void* p, size_t) noexcept {
    vPortFree(p);
}

void operator delete(void* p, const std::nothrow_t&) noexcept {
    vPortFree(p);
}

void operator delete[](void* p, const std::nothrow_t&) noexcept {
    vPortFree(p);
}

//=== malloc/calloc/realloc/free wrapped
extern "C" struct mallinfo __real_mallinfo();
extern "C" void* __real_realloc(void* mem, size_t size);
extern "C" void __real_free(void* mem);
extern "C" void* pvPortMalloc(size_t size);
extern "C" void* pvPortCalloc(size_t count, size_t size);
extern "C" void* pvPortRealloc(void* mem, size_t size);
extern "C" void vPortFree(void* mem);
// Tells a heap_4 (FreeRTOS) pointer apart from a newlib-heap pointer. newlib internals (e.g. tzset, locale, stdio scratch) 
// still allocate through _malloc_r/_free_r, which keep their own arena above ucHeap. 
// Without this discrimination a newlib pointer reaching the wrapped free() lands in vPortFree, 
// where heapVALIDATE_BLOCK_POINTER asserts (pointer is outside ucHeap) -> rtosFatalError() -> silent hang.
extern "C" BaseType_t xPortBelongsToHeap(const void* mem);

#ifdef RP2350_PSRAM_CS
extern "C" {
    extern uint8_t __psram_start__;
    extern uint8_t __psram_heap_start__;
    void __malloc_lock(struct _reent *ptr);
    void __malloc_unlock(struct _reent *ptr);
    static void *__ram_start = (void *)0x20000000; // TODO - Is there a SDK exposed variable/macro?
}
#endif

extern "C" void *__wrap_malloc(size_t size) {
    return pvPortMalloc(size); // Don't need to disable interrupts here, FreeRTOS heap_4 is thread safe
}

extern "C" void *__wrap_calloc(size_t count, size_t size) {
    return pvPortCalloc(count, size); // Don't need to disable interrupts here, FreeRTOS heap_4 is thread safe
}

#ifdef RP2350_PSRAM_CS
// Utilize the existing malloc lock infrastructure and interrupt blocking
// to work with multicore and FreeRTOS
extern "C" void *pmalloc(size_t size) {
    noInterrupts();
    __malloc_lock(__getreent());
    auto rc = __psram_malloc(size);
    __malloc_unlock(__getreent());
    interrupts();
    return rc;
}

extern "C" void *pcalloc(size_t count, size_t size) {
    noInterrupts();
    __malloc_lock(__getreent());
    auto rc = __psram_calloc(count, size);
    __malloc_unlock(__getreent());
    interrupts();
    return rc;
}
#else
// No PSRAM, always fail
extern "C" void *pmalloc(size_t size) {
    (void) size;
    return nullptr;
}

extern "C" void *pcalloc(size_t count, size_t size) {
    (void) count;
    (void) size;
    return nullptr;
}
#endif

extern "C" void *__wrap_realloc(void *mem, size_t size) {
#ifdef RP2350_PSRAM_CS
    if (mem && (mem < __ram_start)) {
        noInterrupts();
        void *rc = __psram_realloc(mem, size);
        interrupts();
        return rc;
    }
#endif
    // NULL or a heap_4 block -> FreeRTOS heap (pvPortRealloc treats NULL as malloc).
    // A newlib-heap block must be realloc'd by newlib so it stays in its own arena and
    // its block header remains consistent. FreeRTOS heap_4 is thread safe, no interrupt mask needed.
    if (mem == nullptr || xPortBelongsToHeap(mem))
        return pvPortRealloc(mem, size);
    return __real_realloc(mem, size);
}

extern "C" void __wrap_free(void *mem) {
    if (mem == nullptr)
        return;
#ifdef RP2350_PSRAM_CS
    if (mem < __ram_start) {
        noInterrupts();
        __psram_free(mem);
        interrupts();
        return;
    }
#endif
    // Route each pointer back to the allocator that owns it. FreeRTOS heap_4 is thread safe.
    if (xPortBelongsToHeap(mem))
        vPortFree(mem);
    else
        __real_free(mem);   // pointer minted by newlib's internal _malloc_r — return it to the newlib heap
}

extern "C" struct mallinfo __wrap_mallinfo() {
    noInterrupts();
    __malloc_lock(__getreent());
    auto ret = __real_mallinfo();
    __malloc_unlock(__getreent());
    interrupts();
    return ret;
}


#endif // configFREERTOS_HEAP_SCHEME == 4