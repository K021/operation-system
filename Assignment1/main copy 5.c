#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#define INVALID_NICE 404

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
    // char letter;
    pid_t pid;
    int nice;
    double vruntime;
} pcb;

linkedList *rq;  // ready queue
int nvs[5];  // nice values
int nvs_start_idx = 0;
node *running_node;
double weight[5] = {0.64, 0.8, 1, 1.25, 1.5625};

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
                if (curr->fore == NULL) l->head = nd;
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
            // "\t%p (%p, %d, %d, %lf),\n", 
            "\t(%d, %d, %lf),\n", 
            // curr,
            // curr->data, 
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

int setNice(char* argv[]) {
    int n1 = atoi(argv[1]);
    int n2 = atoi(argv[2]);
    int n3 = atoi(argv[3]);
    int n4 = atoi(argv[4]);
    int n5 = atoi(argv[5]);
    nvs[0] = n1;
    nvs[1] = n2;
    nvs[2] = n3;
    nvs[3] = n4;
    nvs[4] = n5;
    return n1 + n2 + n3 + n4 + n5;
}

int getNice() {
    for (int i = nvs_start_idx; i < 5; ++i) {
        if (nvs[i] > 0) {
            nvs[i]--;
            return i - 2;
        }
        nvs_start_idx++;
    }
    return INVALID_NICE;
}

void refreshRuntime() {
    running_node->data->vruntime += weight[running_node->data->nice + 2];
}

void timer_handler (int signum) {
    printf("signal 받음\n");
    if (running_node->data != NULL) {
        kill(running_node->data->pid, SIGSTOP);
        refreshRuntime();
        insertNodeInOrder(rq, running_node);
    }
    running_node = popHeadNode(rq);
    printf(
        ">>>  running node: pid %d nice %d vruntime %lf\n",
        running_node->data->pid,
        running_node->data->nice,
        running_node->data->vruntime
    );
    printNodes(rq);
    kill(running_node->data->pid, SIGCONT);
}

int main(int argc, char* argv[]) {
    int ts = atoi(argv[6]);
    int nps = setNice(argv);

    pid_t pid;
    char letter_printed = 'A';

    struct itimerval timer;
    rq = (linkedList *)malloc(sizeof(linkedList));
    running_node = (node *)malloc(sizeof(node));

    rq->head = NULL;
    rq->tail = NULL;
    running_node->data = NULL;
    running_node->fore = NULL;
    running_node->next = NULL;

    timer.it_value.tv_sec = 5;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 1;
    timer.it_interval.tv_usec = 0;

    printf("time slice: %d, num of ps: %d\n\n", ts, nps);
    // printf("%d, %d, %d, %d, %d, %d\n", n1, n2, n3, n4, n5, ts);

    signal(SIGALRM, &timer_handler);
    setitimer(ITIMER_REAL, &timer, NULL);

    for (int i = 0; i < nps; ++i) {
        pid = fork();
        if (pid == 0) {
            letter_printed += i;
            printf(" child ps: %d,\t%c\n", getpid(), letter_printed);
            execl("ku_app", "ku_app", &letter_printed, 0);
        }
        else if (pid < 0) {
            printf("fork 과정에서 오류 발생\n");
        }
        else {
            printf("PARENT PS: %d,\t(child: %d)\n", getpid(), pid);
            addNode(rq, pid, getNice(), 0);
        }
    }

    printNodes(rq);

    while(1);
    return 0;
}