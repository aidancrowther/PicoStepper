#include "stack.h"

struct node *top = NULL;

void push(uint value){

    struct node *temp = (struct node*) malloc(sizeof(struct node));
    temp->value = value;
    temp->link = top;
    top = temp;

}

uint pop(){
    struct node *temp;
    if(top == NULL) return -1;

    temp = top;

    uint value = top->value;
    top = top->link;
    
    free(temp);

    return value;
}