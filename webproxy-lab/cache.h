#include  <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <pthread.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

// #define HASH_SIZE 9029 // 실사용 시는 큰 숫자
#define HASH_SIZE 13 // 소수로 충돌 최소화
#define HASH_VAL 5381l // djb2 알고리즘 최적화 값

typedef struct _CacheNode{
    char uri[MAXLINE]; //캐시 키
    char *data; //캐시 데이터 (웹 오브젝트)
    size_t size;
    int refer;
    
    struct _CacheNode *next; //LRU의 next
    struct _CacheNode *prev; //LRU의 prev
    struct _CacheNode *hnext; //해시 버킷의 next (해시 충돌 대응 )
    
} CacheNode;

typedef struct _CacheList{
    CacheNode *head; //가장 최근에 사용된 노드
    CacheNode *tail; //가장 마지막으로 사용한 노드
    CacheNode *table[HASH_SIZE]; //해시 버킷

    size_t total_size; //전체 캐시 사용량
    size_t capacity; //최대 캐시 용량
    pthread_rwlock_t lock; //보호용 락 
}CacheList;

static int hash_uri(const char* uri);
CacheNode * cache_lookup(const char* uri, const int internal_lock, const int update_lru);
void debug_print_cache();

void init_cache();
void deinit_cache();
int find_cache(char *uri, char* data_buf, int *size_buf);
void read_cache(CacheNode *cache);
void write_cache(char *uri, const char* data, int size);