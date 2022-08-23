#ifndef STACK_H
#define STACK_H

#include <stdlib.h>
#include "pico/stdlib.h"

struct node{
    struct node *link;
    uint value;
};

//extern struct node *top;

void push(struct node **top, uint value);
uint pop(struct node **top);

#endif