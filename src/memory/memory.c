#include <skyuv/memory.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>

struct skyuv_memory_header {
	void *base;
	size_t size;
};

#define SKYUV_WINDOWS_DEFAULT_ALIGNMENT (2 * sizeof(void *))

static int skyuv_is_power_of_two(size_t value) {
	return value != 0 && (value & (value - 1)) == 0;
}

static void *skyuv_heap_allocate(size_t alignment, size_t size, int zero_memory) {
	struct skyuv_memory_header *header;
	unsigned char *base;
	uintptr_t candidate;
	uintptr_t aligned;
	size_t normalized_size = size == 0 ? 1 : size;
	size_t extra;
	DWORD flags = zero_memory ? HEAP_ZERO_MEMORY : 0;

	if (!skyuv_is_power_of_two(alignment) || alignment < sizeof(void *)) {
		return NULL;
	}
	extra = sizeof(*header) + alignment - 1;
	if (normalized_size > SIZE_MAX - extra) {
		return NULL;
	}
	base = HeapAlloc(GetProcessHeap(), flags, normalized_size + extra);
	if (base == NULL) {
		return NULL;
	}
	candidate = (uintptr_t)(base + sizeof(*header));
	aligned = (candidate + alignment - 1) & ~(uintptr_t)(alignment - 1);
	header = (struct skyuv_memory_header *)(aligned - sizeof(*header));
	header->base = base;
	header->size = normalized_size;
	return (void *)aligned;
}

void *skyuv_malloc(size_t size) {
	return skyuv_heap_allocate(SKYUV_WINDOWS_DEFAULT_ALIGNMENT, size, 0);
}

void *skyuv_calloc(size_t count, size_t size) {
	if (count != 0 && size > SIZE_MAX / count) {
		return NULL;
	}
	return skyuv_heap_allocate(SKYUV_WINDOWS_DEFAULT_ALIGNMENT, count * size, 1);
}

void *skyuv_realloc(void *pointer, size_t size) {
	struct skyuv_memory_header *header;
	void *replacement;
	size_t copy_size;

	if (pointer == NULL) {
		return skyuv_malloc(size);
	}
	if (size == 0) {
		skyuv_free(pointer);
		return NULL;
	}
	header = (struct skyuv_memory_header *)((unsigned char *)pointer - sizeof(*header));
	replacement = skyuv_malloc(size);
	if (replacement == NULL) {
		return NULL;
	}
	copy_size = header->size < size ? header->size : size;
	memcpy(replacement, pointer, copy_size);
	skyuv_free(pointer);
	return replacement;
}

void skyuv_free(void *pointer) {
	struct skyuv_memory_header *header;

	if (pointer == NULL) {
		return;
	}
	header = (struct skyuv_memory_header *)((unsigned char *)pointer - sizeof(*header));
	(void)HeapFree(GetProcessHeap(), 0, header->base);
}

void *skyuv_aligned_alloc(size_t alignment, size_t size) {
	return skyuv_heap_allocate(alignment, size, 0);
}

#else

#if defined(SKYUV_USE_JEMALLOC)
#include <jemalloc.h>
#define skyuv_backend_malloc je_malloc
#define skyuv_backend_calloc je_calloc
#define skyuv_backend_realloc je_realloc
#define skyuv_backend_free je_free
#define skyuv_backend_posix_memalign je_posix_memalign
#else
#define skyuv_backend_malloc malloc
#define skyuv_backend_calloc calloc
#define skyuv_backend_realloc realloc
#define skyuv_backend_free free
#define skyuv_backend_posix_memalign posix_memalign
#endif

void *skyuv_malloc(size_t size) {
	return skyuv_backend_malloc(size == 0 ? 1 : size);
}

void *skyuv_calloc(size_t count, size_t size) {
	if (count != 0 && size > SIZE_MAX / count) {
		return NULL;
	}
	if (count == 0 || size == 0) {
		return skyuv_backend_calloc(1, 1);
	}
	return skyuv_backend_calloc(count, size);
}

void *skyuv_realloc(void *pointer, size_t size) {
	if (size == 0) {
		skyuv_backend_free(pointer);
		return NULL;
	}
	return skyuv_backend_realloc(pointer, size);
}

void skyuv_free(void *pointer) {
	skyuv_backend_free(pointer);
}

void *skyuv_aligned_alloc(size_t alignment, size_t size) {
	void *pointer = NULL;

	if (alignment < sizeof(void *) || (alignment & (alignment - 1)) != 0) {
		return NULL;
	}
	if (skyuv_backend_posix_memalign(&pointer, alignment, size == 0 ? 1 : size) != 0) {
		return NULL;
	}
	return pointer;
}

#endif
