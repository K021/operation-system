#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct _list {
    struct _node *head;
    struct _node *tail;
    int len;
} linkedList;

typedef struct _node {
    struct _pcb *data;
    struct _node *fore;
    struct _node *next;
} node;

typedef struct _pcb {
    pid_t pid;
    int nice;
    double vruntime;
} pcb;

linkedList *rq;

node* createNode(pid_t pid, int nice, double vruntime) {
    node *nd = (node *)malloc(sizeof(node));
    pcb *psdata = (pcb *)malloc(sizeof(pcb));
    psdata->pid = pid;
    psdata->nice = nice;
    psdata->vruntime = vruntime;
    nd->data = psdata;
    nd->fore = NULL;
    nd->next = NULL;
    return nd;
}

void addNode(linkedList *l, pid_t pid, int nice, double vruntime) {
    node *newNode = (node *)malloc(sizeof(node));
    newNode = createNode(pid, nice, vruntime);
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
        while (curr != NULL) {
            if (nd->data->vruntime <= curr->data->vruntime) {
                if (curr->fore != NULL) curr->fore->next = nd;
                nd->fore = curr->fore;
                nd->next = curr;
                curr->fore = nd;
                l->len++;
                return;
            }
            curr = curr->next;
        }
        l->tail->next = nd;
        nd->fore = l->tail;
        nd->next = NULL;
        l->tail = nd;
        l->len++;
    }
}

node* popHeadNode(linkedList *l) {
    node *curr = l->head;
    l->head = curr->next;
    curr->next->fore = NULL;
    curr->next = NULL;
    l->len--;
    return curr;
}

node* searchNode(linkedList *l, pid_t pid) {
    node* curr = l->head;
    while(curr != NULL) {
        if (curr->data->pid == pid) return curr;
        curr = curr->next;
    }
    return NULL;
}

void deleteNode(linkedList *l, node* nd) {
    if (!searchNode(l, nd->data->pid)) return;
    if (nd->next != NULL) nd->next->fore = nd->fore;
    else l->tail = nd->fore;
    if (nd->fore != NULL) nd->fore->next = nd->next;
    else l->head = nd->next;
    free(nd);
    l->len--;
}

void printNodes(linkedList *l) {
    node *curr = l->head;
    printf("list: len %d [\n", l->len);
    while(curr != NULL) {
        printf(
            "\t%p (%p, %d, %d, %lf),\n", 
            curr,
            curr->data, 
            curr->data->pid, 
            curr->data->nice, 
            curr->data->vruntime
        );
        curr = curr->next;
    }
    printf("]\n");
}

void printNode(node *nd) {
    printf(
        "node: %p [%p, (%p, %d, %d, %lf), %p]\n", 
        nd,
        nd->fore, 
        nd->data, 
        nd->data->pid, 
        nd->data->nice,
        nd->data->vruntime,
        nd->next
    );
}

void freeList(linkedList *l) {
    node* curr = l->head;
    node* temp;
    while(curr != NULL) {
        temp = curr;
        curr = curr->next;
        free(temp->data);
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
    int nps = n1 + n2 + n3 + n4 + n5;

    struct itimerval timer;
    rq = (linkedList *)malloc(sizeof(linkedList));
    node *temp;

    pid_t pid;

    rq->head = NULL;
    rq->tail = NULL;
    rq->len = 0;

    timer.it_value.tv_sec = 5;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 1;
    timer.it_interval.tv_usec = 0;

    printf("%d, %d, %d, %d, %d, %d\n", n1, n2, n3, n4, n5, ts);

    signal(SIGALRM, &timer_handler);
    // setitimer(ITIMER_REAL, &timer, NULL);

    pid = fork();
    if (pid <= 0) exit(0);

    printf("\n==============================================================\n");
    printf(">>> initial list:\n");
    addNode(rq, 0001, -2, 0);
    addNode(rq, 0002, -1, 0);
    addNode(rq, 0003, 0, 0);
    addNode(rq, 0004, 1, 0);
    addNode(rq, 0005, 2, 0);
    addNode(rq, 0006, 2, 0);
    printNodes(rq);
    printf("\n");

    printf(">>> pop node 4\n");
    temp = searchNode(rq, 0004);
    printNode(temp);
    deleteNode(rq, temp);
    printNodes(rq);
    printf("\n");

    printf(">>> insert node %d in order of vruntime\n", pid);
    temp = createNode(pid, 0, 1);
    printNode(temp);
    insertNodeInOrder(rq, temp);
    printNodes(rq);
    printf("\n");

    printf(">>> delete head node\n");
    temp = popHeadNode(rq);
    printNode(temp);
    printNodes(rq);
    deleteNode(rq, temp);

    freeList(rq);

    // while(1);
    return 0;
}