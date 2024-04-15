#include "cachelab.h"

#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#define HIT 0
#define MISS 1
#define EVICT 2
#define WORD_SIZE 64



int result[3];

unsigned long  timer;

typedef struct cache_line {
  int vaild;
  size_t tag;
  long time;
  void * block;

} cache_line_t;


typedef struct  cache_group{

   cache_line_t * lines;

} cache_group_t;



typedef struct  cache{
   long s;
   long E;
   long b;
   cache_group_t * groups;

} cache_t;


typedef struct  {
    char type;
    size_t address;
    size_t target_G;
    size_t target_L;
    size_t offset;
    int rw_size;
} query_t;

void read_opt(int argc, char **argv, cache_t * cache, char * fpath);
void init_c(cache_t * cache);
void clear_c(cache_t * cache);
void map( size_t address, query_t * q, cache_t * cache );
void parse_query(char * line, query_t * q, cache_t * cache);
cache_line_t * find( size_t tag, cache_group_t * group, long lines);
cache_line_t * find_empty_line(cache_group_t * group, long lines);
cache_line_t * find_evict_line(cache_group_t * group, long lines);
void setResult(char type, size_t address, int rw_size, int ops);
void access_data(query_t * q, cache_t * cache);





void read_opt(int argc, char **argv, cache_t * cache, char * fpath){
    int opt;
    /* looping over arguments */
    while ((opt = getopt(argc,argv,"s:E:b:t:")) != -1) {
        switch(opt) {
            case 's':
                cache->s= atol(optarg);
                break;
            case 'E':
                cache->E= atol(optarg);
                break;
            case 'b':
                cache->b= atol(optarg);
                break;
            case 't':
                strcpy(fpath, optarg);
                break;
            default:
                printf("wrong argument\n");
                break;
        }
    }
    printf("s: %ld, E: %ld, b: %ld, t: %s\n", cache->s, cache->E, cache->b, fpath);
}


void init_c(cache_t * cache){
    timer = 0;
    long group_num = 1 << cache->s;
    cache->groups = (cache_group_t *)malloc(group_num * sizeof(cache_group_t ));
    if(cache->groups == NULL){
        printf("alloc group failed\n");
        clear_c(cache);
        exit(1);
    }

    for(int i = 0; i < group_num; i++){
       cache->groups[i].lines = (cache_line_t *)malloc(cache->E * sizeof(cache_line_t));
       if(cache->groups[i].lines == NULL){
         printf("alloc lines failed: %d\n", i);
         clear_c(cache);
         exit(1);
       }
       cache_line_t  * lines = cache->groups[i].lines;
       for(int i = 0; i < cache->E; i++){
           lines->vaild = 0;
           lines->tag = 0;
           lines->time = 0;
       }
    }


}

void clear_c(cache_t * cache){
    if(cache->groups == NULL){
        return;
    }
    size_t group_num = 1 << cache->s;
     for (int i = 0; i < group_num; i++) {
        if(cache->groups[i].lines ==  NULL){
           free(cache->groups[i].lines);
        }
    }
    free(cache->groups);

}




void map( size_t address, query_t * q, cache_t * cache ){
    int indexBits = cache->s;
    int offsetBits = cache->b;
    int tagBits = WORD_SIZE - indexBits - offsetBits;


    size_t tagMask = (1 << tagBits) - 1;
    size_t indexMask = ((1 << indexBits) - 1) << offsetBits;
    size_t offsetMask = (1 << offsetBits) - 1;


    q->target_L = (address >> (indexBits + offsetBits)) & tagMask;
    q->target_G = (address & indexMask) >> offsetBits;
    q->offset = address & offsetMask;

}


void parse_query(char * line, query_t * q, cache_t * cache){
     char type;
     size_t address;
     int  rw_size;

     if (sscanf(line, " %c %zx,%d", &type, &address, &rw_size) == 3) {
            q->type = type;
            q->address = address;
            q->rw_size = rw_size;
            map(address, q, cache);

     }

}
cache_line_t * find( size_t tag, cache_group_t * group, long lines){
    for(long i = 0 ; i < lines; i++ ){
       int vaild = group->lines[i].vaild;
       size_t line_tag = group->lines[i].tag;
       if(vaild == 1 && tag == line_tag ){
            return &group->lines[i];
       }
    }
    return NULL;
}

cache_line_t * find_empty_line(cache_group_t * group, long lines){
    for(long i = 0; i < lines; i++){
        int vaild = group->lines[i].vaild;
        if(vaild == 0){
            return &group->lines[i];
        }
    }
    return NULL;
}

cache_line_t * find_evict_line(cache_group_t * group, long lines){
    long min_time = LONG_MAX;
    cache_line_t * evict_line = NULL;
    for(long i = 0; i < lines; i++){
        long time = group->lines[i].time;
        if(time < min_time){
            min_time = time;
            evict_line = &group->lines[i];
        }
    }
    return  evict_line;
}


void setResult(char type, size_t address, int rw_size, int ops){
     if(ops == HIT){
        printf("%c %zx,%d hit", type, address, rw_size);
        result[HIT]++;
     }else if (ops == MISS){
        printf("%c %zx,%d miss", type, address, rw_size);
        result[MISS]++;
     }else if(ops == MISS + EVICT){
        printf("%c %zx,%d miss eviction", type, address, rw_size);
        result[MISS]++;
        result[EVICT]++;
     }else{
        printf("undefine ops");
     }

     if(type == 'M'){
        result[HIT]++;
        printf(" hit\n");
     }else{
        printf("\n");
     }

}


void access_data(query_t * q, cache_t * cache){
   long  g_idx  = q->target_G;
   size_t tag  = q->target_L;
   size_t group_num = 1 << cache->s;
   if(g_idx >= 0  && g_idx < group_num){
        cache_line_t * line = find(tag, &cache->groups[g_idx], cache->E);
        if(line != NULL){
            setResult(q->type, q->address, q->rw_size, HIT);
        }else{
            line = find_empty_line(&cache->groups[g_idx], cache->E);
            if(line != NULL){
                line->vaild = 1;
                line->tag =tag;
                setResult(q->type, q->address, q->rw_size, MISS);

            }else{
                line = find_evict_line(&cache->groups[g_idx], cache->E);
                line->vaild  = 1;
                line->tag = tag;
                setResult(q->type, q->address, q->rw_size, MISS + EVICT);
            }

        }
        line->time = ++timer;


   }else{
       printf("invaild group index\n");
       return;
   }

}


int main(int argc, char **argv)
{
    char fpath[256] ={0};
    cache_t cache;
    read_opt(argc, argv, &cache,fpath);
    init_c(&cache);
    FILE *file;
    char line[256];
    printf("filename: %s\n", fpath );
    // open file
    file = fopen(fpath , "r");
    if (file == NULL) {
        printf("open file failed!\n");
        clear_c(&cache);
        exit(1);
    }


    while (fgets(line, sizeof(line) - 1, file) ) {
        //exclude "I"
        if (line[0] == ' ') {
            query_t query;
            parse_query(line, &query, &cache);
            access_data(&query,&cache);
        }
    }
    // close file
    fclose(file);


    clear_c(&cache);

    printSummary(result[HIT], result[MISS], result[EVICT]);
    return 0;
}
