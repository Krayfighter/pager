
#ifndef PTYPES_LINKLIST_H
#define PTYPES_LINKLIST_H

#define declare_linked_list_block_struct(ItemT) \
typedef struct LinkedList_ ##  _Block { \
  ItemT item; \
  LinkedList_ ##  _Block *next; \
} LinkedList_ ##  _Block;

#define declare_linked_list_struct(ItemT) \
typedef struct { \
  ItemT *base_pointer; \
  ItemT *current_block; \
} LinkedList_ ## ItemT ## _Head;

#define declare_linked_list(ItemT) \
declare_linked_list_block_struct(ItemT) \
declare_linked_list_struct(ItemT)

#endif

