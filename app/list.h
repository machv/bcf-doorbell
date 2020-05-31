#ifndef _LIST_H
#define _LIST_H

#include <bcl.h>

struct list_node_t {
  bc_tick_t when;
  bool is_long;

  struct list_node_t *next;
  struct list_node_t *prev;
};
typedef struct list_node_t list_node_t; // aby se všude v kódu nemuselo psát (struct button_press_node)

struct list_t
{
    list_node_t *head;
    int length;
};
typedef struct list_t list_t;

void list_init(list_t* list);
void list_add_first(list_t* list, int when, bool is_long);
void list_add_last(list_t* list, int when, bool is_long);
void list_delete_node(list_t* list, list_node_t* del);
void list_delete_first(list_t* list);
void list_delete_last(list_t* list);
void list_print(list_t* list);
void list_clear(list_t* list);
bool list_compare(list_t* pattern, list_t* input, bc_tick_t timespan);

#endif // _LIST_H
