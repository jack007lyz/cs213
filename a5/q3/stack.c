 #include <stdlib.h>
#include <stdio.h>
#include "integer.h"
#include "stack.h"
#include "refcount.h"

struct element {
  struct integer* integer;
  struct element* next;
};

struct element* head;

void stack_push(struct integer* integer) {
  struct element* element = rc_malloc(sizeof(*element));
  element->integer = integer;
  element->next  = head;
    head = element;
    rc_keep_ref(element->integer);
}

void stack_pop_and_print() {
  if (head != NULL) {
    struct element* element = head;
    head = head->next;
    printf("%d ", integer_value(element->integer));
    
    rc_free_ref(element);
    rc_free_ref(element->integer);
}
}

int stack_is_empty() {
  return head == NULL;
}
