/* 20220041 Yoojin Kim */
#include "cachelab.h"
#include <stdio.h>
#include <getopt.h> 
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

typedef struct {
    int valid;
    int tag;
    int time;
} CacheLine;
typedef CacheLine* CacheSet;
typedef CacheSet* Cache;

int hit = 0, miss = 0, evict = 0;
char filename[100];
int s = 0, S = 0, E = 0, b = 0;
unsigned long long time_counter = 0;

Cache cache = NULL;

void init_cache();
void access_cache(unsigned long long address);
void free_cache();

int main(int argc, char* argv[])
{
    int opt;
    while((opt = getopt(argc, argv, "s:E:b:t:")) != -1) {
        switch (opt) {
            case 's':
                s = atoi(optarg);
                S = (1 << s);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                strcpy(filename, optarg);
                break;
        }
    }

    FILE* file = fopen(filename, "r");
    assert(file);
    init_cache();
    char op; //operation
    unsigned long long address; //address
    int size; //number of bytes
    while(fscanf(file, " %c %llx %d", &op, &address, &size) != EOF) {
        switch (op) {
            case 'M': //access twice
                access_cache(address);
                access_cache(address);
                break;
            case 'L': //access once
            case 'S': //access once
                access_cache(address);
                break;
        }
    }
    printSummary(hit, miss, evict);
    free_cache();

    fclose(file);
    return 0;
}

void init_cache() { //allocate cache and initialize
    cache = (Cache)malloc(S * sizeof(CacheSet));
    for(int i = 0; i < S; ++i) {
        cache[i] = (CacheSet)malloc(E * sizeof(CacheLine));
        for(int j = 0; j < E; ++j) {
            cache[i][j].tag = 0;
            cache[i][j].valid = 0;
            cache[i][j].time = 0;
        }
    }
}

void access_cache(unsigned long long address) { //access cache
    int tag = address >> (s + b); //tag bit
    int set_idx = (address >> b) & ((1 << s) - 1); //set index bit
    int hit_flag = 0, evict_flag = 1;
    int replace_idx = 0;
    
    CacheSet cache_set = cache[set_idx];
    int tmp_idx = 0; //temporary block idx
    for(tmp_idx = 0; tmp_idx < E; ++tmp_idx) {
        if(cache_set[tmp_idx].valid ) { //valid = 1
            if(cache_set[tmp_idx].tag == tag) { //check if tag is same
                hit_flag = 1;
                break;
            }
        }
    }
    /*hit*/
    if(hit_flag) {
        cache_set[tmp_idx].time = time_counter;
        ++hit;
    }
    /*miss*/
    else {
        ++miss;
        for(int i = 0; i < E; i++) { //check if there are empty lines
            if(!cache_set[i].valid) {
                replace_idx = i;
                evict_flag = 0;
                break;
            }
        }
        if(evict_flag) { //line replacement through lru
            ++evict;
            unsigned long long evict_time = -1;
            for(int i = 0; i < E; ++i) {
                if(cache_set[i].time < evict_time) {
                    evict_time = cache_set[i].time;
                    replace_idx = i;
                }
            }
        }
        cache_set[replace_idx].valid = 1;
        cache_set[replace_idx].tag = tag;
        cache_set[replace_idx].time = time_counter;
    }
    ++time_counter;
}

void free_cache() { //deallocate cache
    for(int i = 0; i < S; ++i) {
        free(cache[i]);
    }
    free(cache);
}