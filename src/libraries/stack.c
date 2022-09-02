#include "stack.h"

void push(struct node **top, uint value){

    struct node *temp = (struct node*) malloc(sizeof(struct node));
    temp->value = value;
    temp->link = (*top);
    (*top) = temp;

}

uint pop(struct node **top){
    struct node *temp;
    if((*top) == NULL) return -1;

    temp = (*top);

    uint value = (*top)->value;
    (*top) = (*top)->link;
    
    free(temp);

    return value;
}

bool isEmpty(struct node **top){
    return (*top) == NULL;
}