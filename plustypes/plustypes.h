
#include "stdint.h"
#include "stddef.h"
#include "stdlib.h"

#include "list.h"
#include "linked_list.h"

#ifndef PTYPES_H
#define PTYPES_H

#define defer(statement, block) { \
  block; \
  statement; \
}

// storage for an allocator that just fills up
// over time and only deallocates by flushing
// the entire buffer
// 
// + fast allocations
// + simnple to reason about
// - no individual deallocation
typedef struct {
  uint8_t *buffer;
  uint32_t buffer_size;
  uint32_t next_space;
} FillAllocator;


// #define IMPLEMENT_FILLALLOCATOR
static FillAllocator FillAllocator_new(void *buffer, uint32_t buffer_size) {
  return (FillAllocator) {
    .buffer = (uint8_t *)buffer,
    .buffer_size = buffer_size,
    .next_space = 0
  };
}

static uint8_t *FillAllocator_malloc(FillAllocator *self, uint32_t bytes) {
  if (self->next_space + bytes >= self->buffer_size) { return NULL; }
  uint8_t *alloc_pointer = self->buffer + self->next_space;
  self->next_space += bytes;
  return alloc_pointer;
}

static uint8_t *FillAllocator_calloc(FillAllocator *self, uint32_t count, uint16_t size) {
  uint32_t alloc_size = count * size;
  if (self->next_space + alloc_size >= self->buffer_size) { return NULL; }

  uint8_t *alloc_pointer = self->buffer + self->next_space;
  self->next_space += alloc_size;
  return alloc_pointer;
}

#define FillAllocator_talloc(self, Type, count) \
(Type *)FillAllocator_calloc(self, count, sizeof(Type))

// erase all allocations from the allocator
static void FillAllocator_clear(FillAllocator *self) { self->next_space = 0; }



#endif

