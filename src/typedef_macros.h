
#ifndef TYPEDEF_MACROS_H
#define TYPEDEF_MACROS_H

#define declare_heap_array_struct(ItemT) \
typedef struct { \
  ItemT *item_buffer; \
  size_t buffer_size; \
  size_t buffer_len; \
} Vec_ ## ItemT;

#define declare_heap_array_push(ItemT) \
void Vec_ ## ItemT ## _push(Vec_ ## ItemT *self, ItemT item);

#define define_heap_array_push(ItemT) \
void Vec_ ## ItemT ## _push(Vec_ ## ItemT *self, ItemT item) { \
  if (self->buffer_len == self->buffer_size) { \
    self->buffer_size *= 2; \
    self->item_buffer = (ItemT *)realloc(self->item_buffer, sizeof(ItemT) * self->buffer_size); \
  } \
  self->item_buffer[self->buffer_len] = item; \
  self->buffer_len++; \
}

#define declare_heap_array_new(ItemT) \
Vec_ ## ItemT Vec_ ## ItemT ## _new(size_t start_items);

#define define_heap_array_new(ItemT) \
Vec_ ## ItemT Vec_ ## ItemT ## _new(size_t start_items) { \
  return (Vec_ ## ItemT) { \
    .item_buffer = malloc(sizeof(ItemT) * start_items), \
    .buffer_size = start_items, \
    .buffer_len = 0, \
  }; \
}

#define declare_heap_array_foreach(ItemT) \
void Vec_ ## ItemT ## _foreach(Vec_ ## ItemT *self, void (*fpointer)(ItemT *));

#define define_heap_array_foreach(ItemT) \
void Vec_ ## ItemT ## _foreach(Vec_ ## ItemT *self, void (*fpointer)(ItemT *)) { \
  for(size_t i = 0; i < self->buffer_len; i++) { \
    fpointer(&self->item_buffer[i]); \
  } \
}

#define declare_heap_array_free(ItemT) \
void Vec_ ## ItemT ## _free(Vec_ ## ItemT *self);

#define define_heap_array_free(ItemT) \
void Vec_ ## ItemT ## _free(Vec_ ## ItemT *self) { \
  free(self->item_buffer); \
  self->buffer_size = 0; \
  self->buffer_len = 0xffffffff; \
}

#define define_heap_array_uninit(ItemT) \
const Vec_ ## ItemT Vec_ ## ItemT ## _UNINIT = { \
  .item_buffer = NULL, \
  .buffer_size = 0, \
  .buffer_len = 0, \
};

#define declare_heap_array_is_uninit(ItemT) \
bool Vec_ ## ItemT ## _is_uninit(Vec_ ## ItemT *self);

#define define_heap_array_is_uninit(ItemT) \
bool Vec_ ## ItemT ## _is_uninit(Vec_ ## ItemT *self) { \
  return self->item_buffer == NULL; \
}

#define declare_heap_array_get(ItemT) \
ItemT *Vec_ ## ItemT ## _get(Vec_ ## ItemT *self, size_t index);

#define define_heap_array_get(ItemT) \
ItemT *Vec_ ## ItemT ## _get(Vec_ ## ItemT *self, size_t index) { \
  if (index >= self->buffer_len) { return NULL; } \
  return &self->item_buffer[index]; \
}

#define declare_heap_array(ItemT) \
declare_heap_array_struct(ItemT) \
declare_heap_array_push(ItemT) \
declare_heap_array_new(ItemT) \
declare_heap_array_foreach(ItemT) \
declare_heap_array_free(ItemT) \
declare_heap_array_is_uninit(ItemT) \
declare_heap_array_get(ItemT)

// like (Rust -> Vec) (C++ -> std::vector)
#define define_heap_array(ItemT) \
define_heap_array_push(ItemT) \
define_heap_array_new(ItemT) \
define_heap_array_foreach(ItemT) \
define_heap_array_free(ItemT) \
define_heap_array_uninit(ItemT) \
define_heap_array_is_uninit(ItemT) \
define_heap_array_get(ItemT)

#define foreach(ItemT, item_name, array, array_len, block) \
for (size_t index = 0; index < array_len; index += 1) { \
  ItemT *item_name = &array[index]; \
  block \
}


#define define_option_union(BaseType, NoneType) \
typedef union { \
  BaseType some; \
  NoneType none; \
} Option_ ## BaseType;

#define define_option_evaluation(BaseType, none_value_binary) \
bool Option_ ## BaseType ## _is_some(Option_ ## BaseType *self) { \
  return !(self->none == none_value_binary); \
}

#define define_option_none(BaseType, none_value_binary) \
const Option_ ## BaseType Option_ ## BaseType ## _None = (Option_ ## BaseType){ .none = none_value_binary };

// like (Rust -> Option) (C++ -> std::optional)
#define define_option(BaseType, NoneType, none_value_binary) \
define_option_union(BaseType, NoneType) \
define_option_evaluation(BaseType, none_value_binary) \
define_option_none(BaseType, none_value_binary)

#endif

