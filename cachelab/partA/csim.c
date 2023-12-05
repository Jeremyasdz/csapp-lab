#define _GNU_SOURCE
#include "cachelab.h"
#include "stdlib.h"
#include <stdio.h>
#include "getopt.h"
#include <string.h>
#define addrLen 8

static int S;
static int E;
static int B;
static int hits = 0;
static int misses = 0;
static int evictions = 0;
static int totalSet;

typedef struct _Node{
    int tag;
    struct _Node *nxt;
    struct _Node *pre;
}Node;

typedef struct _LRU{
    Node *head;
    Node *tail;
    int* size;
}LRU;

static LRU *lru;

void parseoption(int argc, char **argv, char *fileName){
    int o;
    while((o = getopt(argc, argv, "s:E:b:t:")) != -1){
        switch(o){
            case 's':
                S = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                B = atoi(optarg);
                break;
            case 't':
                strcpy(fileName, optarg);
                break;
        }
    }
    totalSet = 1 << S;
}

void initialize(int i){
    lru[i].head = malloc(sizeof(Node));
    lru[i].tail = malloc(sizeof(Node));

    lru[i].head->nxt = lru[i].tail;
    lru[i].tail->nxt = lru[i].head;

    (lru[i].size) = (int* )malloc(sizeof(int));
    *(lru[i].size) = 0;

}

void deleteNode(Node* nodeToDel, LRU* curlru){
    nodeToDel->pre->nxt = nodeToDel->nxt;
    nodeToDel->nxt->pre = nodeToDel->pre;
    *(curlru->size) = *(curlru->size) - 1;
}

void evict(Node* nodeToDel, LRU* curlru){
    deleteNode(nodeToDel, curlru);
}

void add(Node* nodeToAdd, LRU* curlru){
    nodeToAdd->nxt = curlru->head->nxt;
    nodeToAdd->pre = curlru->head;
    curlru->head->nxt->pre = nodeToAdd;
    curlru->head->nxt = nodeToAdd;
    *(curlru->size) = *(curlru->size) + 1;;
}

void update(unsigned address){
    unsigned mask = 0xFFFFFFFF;
    unsigned maskset = mask >> (32 - S);
    unsigned targetSet = maskset & (address >> B);
    unsigned targetTag = address >> (S + B);

    int found = 0;
    LRU curLRU = lru[targetSet];
    Node *cur = curLRU.head->nxt;

    while (cur != curLRU.tail){
        if(cur->tag == targetTag){
            found = 1;
            break;
        }
        cur = cur->nxt;
    }

    if(found){
        hits++;
        deleteNode(cur, &curLRU);
        add(cur, &curLRU);
        printf("hit!, the set number %d \n", targetSet);
    }
    else{
        misses++;
        Node *newnode = malloc(sizeof(Node));
        newnode->tag = targetTag;
        if(*(curLRU.size) == E){
            evictions++;
            deleteNode(curLRU.tail->pre, &curLRU);
            add(newnode, &curLRU);

            printf("evic + miss set -> %d\n", targetSet);
        }
        else{
            add(newnode, &curLRU);
            printf("only miss %d\n", targetSet);
        }
    }
}

void simulate(char *filename){
    lru = malloc(totalSet * sizeof(LRU));
    for(int i = 0; i < totalSet; i++){
        initialize(i);
    }
    FILE *f = fopen(filename, "r");
    char op;
    unsigned address;
    int size;

    while(fscanf(f, " %c %x,%d", &op, &address, &size) > 0){
        printf("%c, %x %d\n", op, address, size);
        switch(op){
            case 'L':
                update(address);
                break;
            case 'M':
                update(address);
            case 'S':
                update(address);
                break;
        }
    }
}

int main(int argc, char ** argv)
{
    char *fileName = malloc(100 * sizeof(char));
    parseoption(argc, argv, fileName);
    simulate(fileName);
    printSummary(hits, misses, evictions);
    return 0;
}
