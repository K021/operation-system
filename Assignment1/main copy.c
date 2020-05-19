#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>

typedef struct _list {
    int len;
    struct _node *head;
    struct _node *tail;
} linkedList;

typedef struct _node {
    int data;
    struct _node *fore;
    struct _node *next;
} node;

node* createNode(int data) {
    node* nd = (node *)malloc(sizeof(node));
    nd->data = data;
    return nd;
}

void addNode(linkedList *l, int data) {
    node *newNode = (node *)malloc(sizeof(node));
    newNode->data = data;
    newNode->fore = NULL;
    newNode->next = NULL;
    if (l->head == NULL) l->head = l->tail = newNode;
    else {
        l->tail->next = newNode;
        newNode->fore = l->tail;
        l->tail = newNode;
    }
    l->len++;
}

void insertNodeInOrder(linkedList *l, node *nd) {
    // list 가 오름차순이라고 가정하고, 대소관계가 유지되도록 삽입한다.
    node *curr = (node *)malloc(sizeof(node));
    if (l->head == NULL) {
        l->head = l->tail = nd;
        nd->fore = nd->next = NULL;
    }
    else {
        curr = l->head;
        while (curr->data < nd->data) {
            curr = curr->next;
        }
        curr->fore->next = nd;
        nd->fore = curr->fore;
        nd->next = curr;
        curr->fore = nd;
    }
    l->len++;
}

node* popHeadNode(linkedList *l) {
    node *curr = l->head;
    l->head = curr->next;
    curr->next->fore = NULL;
    curr->next = NULL;
    l->len--;
    return curr;
}

void deleteNode(node* nd) {
    if (nd->fore != NULL) nd->fore->next = nd->next;
    else nd->next->fore = NULL;
    if (nd->next != NULL) nd->next->fore = nd->fore;
    else nd->fore->next = NULL;
    free(nd);
}

node* searchNode(linkedList *l, int data) {
    node* curr = l->head;
    while(curr != NULL) {
        if (curr->data == data) return curr;
        curr = curr->next;
    }
    return NULL;
}

void printNodes(linkedList *l) {
    node *curr = l->head;
    putchar('[');
    while(curr != NULL) {
        printf("%d", curr->data);
        curr = curr->next;
        if (curr != NULL) printf(", ");
    }
    printf("]\n");
}

void printNode(node *nd) {
    printf("node: [%p, %d, %p]\n", nd->fore, nd->data, nd->next);
}

void freeList(linkedList *l) {
    node* curr = l->head;
    node* temp;
    while(curr != NULL) {
        temp = curr;
        curr = curr->next;
        free(temp);
    }
    free(l);
}

void timer_handler (int signum) {
    printf("signal 받음\n");
}

int main(int argc, char* argv[]) {
    int n1 = atoi(argv[1]);
    int n2 = atoi(argv[2]);
    int n3 = atoi(argv[3]);
    int n4 = atoi(argv[4]);
    int n5 = atoi(argv[5]);
    int ts = atoi(argv[6]);

    linkedList *rq = (linkedList *)malloc(sizeof(linkedList));
    struct itimerval timer;
    node *temp;

    rq->head = NULL;
    rq->tail = NULL;

    timer.it_value.tv_sec = 5;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 1;
    timer.it_interval.tv_usec = 0;

    printf("%d, %d, %d, %d, %d, %d\n", n1, n2, n3, n4, n5, ts);

    signal(SIGALRM, &timer_handler);
    // setitimer(ITIMER_REAL, &timer, NULL);

    printf("\n");
    printf("initial list:\n");
    addNode(rq, 0);
    addNode(rq, 1);
    addNode(rq, 2);
    addNode(rq, 3);
    addNode(rq, 4);
    addNode(rq, 17);
    printNodes(rq);
    printf("\n");

    printf("pop node 4\n");
    temp = searchNode(rq, 4);
    printNode(temp);
    deleteNode(temp);
    printNodes(rq);
    printf("\n");

    printf("insert node 5 in order\n");
    temp = createNode(5);
    printNode(temp);
    insertNodeInOrder(rq, temp);
    printNodes(rq);
    printf("\n");

    printf("delete head node\n");
    temp = popHeadNode(rq);
    printNode(temp);
    printNodes(rq);
    deleteNode(temp);

    freeList(rq);

    // while(1);
    return 0;
}