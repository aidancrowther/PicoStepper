#ifndef STACK_H
#define STACK_H

#include <stdlib.h>
#include "pico/stdlib.h"

struct node{
    struct node *link;
    uint value;
};

extern struct node *top;

void push(uint value);
uint pop();

#endif