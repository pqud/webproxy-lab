#include  <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <pthread.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct _CacheNode{
    char uri[MAXLINE]; //캐시 키
    char *data; //캐시 데이터 (웹 오브젝트)
    size_t size;
    
    struct _CacheNode *next;
    struct _CacheNode *prev;
} CacheNode;

typedef struct _CacheList{
    CacheNode *head; //가장 최근에 사용된 노드
    CacheNode *tail; //가장 마지막으로 사용한 노드
    size_t total_size; //전체 캐시 사용량
    size_t capacity; //최대 캐시 용량
    pthread_rwlock_t lock; //보호용 락 
}CacheList;


void init_cache();
void deinit_cache();
int find_cache(char *uri, char* data_buf, int *size_buf);
void read_cache(CacheNode *cache);
void write_cache(char *uri, const char* data, int size);
void debug_print_cache();