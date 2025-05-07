#include "cache.h"


CacheList cache_list;

void init_cache(){
    cache_list.head=NULL;
    cache_list.tail=NULL;
    cache_list.total_size=0;
    cache_list.capacity=MAX_CACHE_SIZE;
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

    pthread_rwlock_unlock(&cache_list.lock);
    pthread_rwlock_destroy(&cache_list.lock);

    Free(&cache_list);

}

int find_cache(char *uri, char* data_buf, int *size_buf ){
    if (cache_list.head == NULL || uri == NULL) return 0;

    pthread_rwlock_wrlock(&cache_list.lock); // 처음부터 write 락

    CacheNode *temp = cache_list.head;
    while (temp) {
        if (strcmp(temp->uri, uri) == 0) {
            read_cache(temp); // LRU 갱신
            memcpy(data_buf, temp->data, temp->size);
            *size_buf = temp->size;
            pthread_rwlock_unlock(&cache_list.lock);
            return 1;
        }
        temp = temp->next;
    }
    pthread_rwlock_unlock(&cache_list.lock);
    return 0;
}


void read_cache(CacheNode *cache){
    //사용된 캐시를 LRU 리스트 맨 앞으로 이동하기

    //이미 head면 아무것도 안함
    if (cache==cache_list.head) return;


    if(cache->prev) 
        cache->prev->next= cache->next;

    if(cache->next)
        cache->next->prev=cache->prev;
    else //캐시가 tail이었다면 
        cache_list.tail=cache->prev;

    //head 앞에 삽입    
    cache->prev=NULL;
    cache->next=cache_list.head;
    if(cache_list.head)
        cache_list.head->prev=cache;
    cache_list.head=cache;

    //tail 없는 경우 
    if(cache_list.tail == NULL) 
        cache_list.tail=cache;
}

//새 응답을 캐시에 저장함.
void write_cache(char *uri, const char* data, int size ){

    // 모든 데이터를 다 제거한 것보다도 새 데이터가 크면 걍 버림 
    if(size>MAX_CACHE_SIZE)
        return;

    pthread_rwlock_wrlock(&cache_list.lock); 

    //캐시에 공간이 충분할 때 까지 tail에서 제거
    while(cache_list.total_size+size>cache_list.capacity){
        CacheNode *old_tail = cache_list.tail;

        if(!old_tail) break; //캐시 비면 종료

        if(old_tail->prev)
            old_tail->prev->next=NULL;
        cache_list.tail=old_tail->prev;

        //노드가 하나뿐이었다면
        if(cache_list.head==old_tail)
            cache_list.head=NULL;

        cache_list.total_size-=old_tail->size;
        free(old_tail->data);
        free(old_tail);
    }
        

    CacheNode* newNode=(CacheNode*)Malloc(sizeof(CacheNode));

    strncpy(newNode->uri, uri, MAXLINE-1);
    newNode->uri[MAXLINE-1]='\0';

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

    cache_list.total_size+=size;

    pthread_rwlock_unlock(&cache_list.lock); 


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