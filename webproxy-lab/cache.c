#include "cache.h"


CacheList cache_list;

void init_cache(){
    cache_list.head=NULL;
    cache_list.tail=NULL;
    cache_list.total_size=0;
    cache_list.capacity=MAX_CACHE_SIZE;
    memset(cache_list.table, 0, sizeof(cache_list.table)); //해당 포인터에서 sizeof(cache_list.table)만큼을 0으로 초기화.
    pthread_rwlock_init(&cache_list.lock, NULL);
}

void deinit_cache(){
    pthread_rwlock_wrlock(&cache_list);

    CacheNode* temp = cache_list.head;
    while(temp){        
        CacheNode *next= temp->next;
        free(temp->data);
        free(temp);
        temp=next;
    }

    cache_list.head=NULL;
    cache_list.tail=NULL;
    cache_list.total_size=0;
    memset(cache_list.table, 0, sizeof(cache_list.table));

    pthread_rwlock_unlock(&cache_list.lock);
    pthread_rwlock_destroy(&cache_list.lock);

    Free(&cache_list);

}


int find_cache(char *uri, char* data_buf, int *size_buf ){
    if (cache_list.head == NULL || uri == NULL) return 0;

    pthread_rwlock_wrlock(&cache_list.lock); // 처음부터 write 락

    CacheNode *temp= cache_lookup(uri, 0,0);

    if(temp){
        memcpy(data_buf, temp->data, temp->size);
        *size_buf=temp->size;
        pthread_rwlock_unlock(&cache_list.lock);
        return 1;
    }
    
    pthread_rwlock_unlock(&cache_list.lock);
    return 0;
}


void read_cache(CacheNode *cache){
    cache->used = 1; // 최근에 사용됨 표시
}

//새 응답을 캐시에 저장함.
void write_cache(char *uri, const char* data, int size ){

    // 모든 데이터를 다 제거한 것보다도 새 데이터가 크면 걍 버림 
    if(size>MAX_CACHE_SIZE)
        return;

    pthread_rwlock_wrlock(&cache_list.lock); 

    // 이미 같은 URI가 있으면 삭제
    CacheNode *dup = cache_lookup(uri, 0, 0);
    if (dup) {
        int dup_index = hash_uri(dup->uri);
        CacheNode **indirect = &cache_list.table[dup_index];
        while (*indirect) {
            if (*indirect == dup) {
                *indirect = dup->hnext;
                break;
            }
            indirect = &((*indirect)->hnext);
        }
        if (dup->prev) dup->prev->next = dup->next;
        if (dup->next) dup->next->prev = dup->prev;
        if (cache_list.head == dup) cache_list.head = dup->next;
        if (cache_list.tail == dup) cache_list.tail = dup->prev;
        cache_list.total_size -= dup->size;
        free(dup->data);
        free(dup);
    }

    //캐시에 공간이 충분할 때 까지 tail에서 제거
    while (cache_list.total_size + size > cache_list.capacity) {
        CacheNode *victim = NULL;
        CacheNode *curr = cache_list.head;
        // 1. used==0인 노드 탐색
        while (curr) {
            if (curr->used == 0) {
                victim = curr;
                break;
            }
            curr = curr->next;
        }
        // 2. 없으면 모든 노드 used=0으로 리셋 후 다시 탐색
        if (!victim) {
            curr = cache_list.head;
            while (curr) {
                curr->used = 0;
                curr = curr->next;
            }
            // 다시 head부터 used==0인 노드 탐색
            curr = cache_list.head;
            while (curr) {
                if (curr->used == 0) {
                    victim = curr;
                    break;
                }
                curr = curr->next;
            }
        }
        if (!victim) break; // 캐시 비면 종료
    
        // victim 삭제 (기존 코드와 동일)
        int hashed_index = hash_uri(victim->uri);
        CacheNode **indirect = &cache_list.table[hashed_index];
        while (*indirect) {
            if (*indirect == victim) {
                *indirect = victim->hnext;
                break;
            }
            indirect = &((*indirect)->hnext);
        }
        if (victim->prev) victim->prev->next = victim->next;
        if (victim->next) victim->next->prev = victim->prev;
        if (cache_list.head == victim) cache_list.head = victim->next;
        if (cache_list.tail == victim) cache_list.tail = victim->prev;
        cache_list.total_size -= victim->size;
        free(victim->data);
        free(victim);
    }

    CacheNode* newNode=(CacheNode*)Malloc(sizeof(CacheNode));

    strncpy(newNode->uri, uri, MAXLINE-1);
    newNode->uri[MAXLINE-1]='\0';
    newNode->used=1;

    newNode->data=malloc(size);
    if(!newNode->data){
        free(newNode);
        pthread_rwlock_unlock(&cache_list.lock); 
        return;
    }

    memcpy(newNode->data, data,size);
    newNode->size=size;
    newNode->prev=NULL;
    newNode->next=cache_list.head;
    if(cache_list.head)
        cache_list.head->prev=newNode;
    cache_list.head=newNode;
    if(cache_list.tail==NULL)
        cache_list.tail=newNode;

    int hashed_index = hash_uri(uri);
    newNode->hnext = cache_list.table[hashed_index];
    cache_list.table[hashed_index]=newNode;

    cache_list.total_size+=size;

    pthread_rwlock_unlock(&cache_list.lock); 


}

static int hash_uri(const char* uri){
    unsigned long hash=HASH_VAL;
    for (int c = *uri ++; c!='\0'; c=*uri++){
        hash=((hash<<5) + hash) + c;
    }
    return hash%HASH_SIZE;
}

/*uri 값으로 캐시에서 탐색
update_lru가 1이면 lru 업데이트 함. 0이면 안함 
*/
CacheNode * cache_lookup(const char* uri, const int internal_lock, const int update_lru){
    if(internal_lock)
        pthread_rwlock_wrlock(&cache_list.lock);

    //1. 해시 값 구함
    CacheNode * node = cache_list.table[hash_uri(uri)];

    //2. 해시 테이블에서 값 찾기 
    while(node && strcmp(node->uri, uri)!=0)
        node=node->hnext;

    //3. 해시 체이닝
    if(node && update_lru)
        read_cache(node);

    if(internal_lock)
        pthread_rwlock_unlock(&cache_list.lock);
    
    return node;
}

void debug_print_cache() {
    pthread_rwlock_rdlock(&cache_list.lock);

    printf("====== Cache Current State ======\n");
    printf("Total cached bytes: %zu\n", cache_list.total_size);

    CacheNode* curr = cache_list.head;
    while (curr != NULL) {
        printf("URI: %-60s | Size: %ld bytes\n", curr->uri, curr->size);
        curr = curr->next;
    }
    printf("========== End of Cache =========\n");

    pthread_rwlock_unlock(&cache_list.lock);
}