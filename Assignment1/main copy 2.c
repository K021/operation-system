#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

pid_t gpid;

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

typedef struct _pcb {
    pid_t pid;
    double vrt;
} pcb;

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
    printf("node: [0x%p, %d, 0x%p]\n", nd->fore, nd->data, nd->next);
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
    printf("signal 받음, %d\n", gpid);
    kill(gpid, SIGCONT);
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

    pid_t pid;

    rq->head = NULL;
    rq->tail = NULL;

    timer.it_value.tv_sec = 5;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 1;
    timer.it_interval.tv_usec = 0;

    printf("%d, %d, %d, %d, %d, %d\n", n1, n2, n3, n4, n5, ts);

    signal(SIGALRM, &timer_handler);
    setitimer(ITIMER_REAL, &timer, NULL);

    pid = fork();
    if (pid == 0) {
        printf("child ps: %d\n", getpid());
        execl("ku_app", "ku_app", "A", 0);
    }
    else if (pid < 0) {
        printf("fork 과정에서 오류 발생\n");
    }
    else {
        printf("parent ps: %d, (child: %d)\n", getpid(), pid);
        gpid = pid;
    }

    while(1);
    return 0;
}