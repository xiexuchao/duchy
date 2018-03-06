#ifndef _CACHE_H
#define _CACHE_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SUCCESS		0
#define FAILURE		1
#define READ		0
#define WRITE		1
#define CLEAN		0
#define DIRTY		1
#define OPEN		0
#define CLOSED		1
#define SIZE_BUF	256

//for NSW-Cache
struct cache_info{
	unsigned int size_block;
	unsigned int size_cache;
	
	unsigned int blk_trc_all;	//trace footprint
	unsigned int blk_trc_red;
	unsigned int blk_trc_wrt;
	
	unsigned int blk_ssd_wrt;
	unsigned int blk_inn_wrt;
	
	unsigned int blk_max_all;	//cache capacity
	unsigned int blk_max_reg;	//real cache
	unsigned int blk_max_gst;	//ghost cache
	
	unsigned int blk_now_reg;
	unsigned int blk_now_gst;
	
	unsigned int hit_red_reg;//cache hit times (blocks)
	unsigned int hit_wrt_reg;
	
	
	struct req_info *req;
	struct blk_info *blk_head_reg;
	struct blk_info *blk_tail_reg;
	struct blk_info *blk_head_gst;
	struct blk_info *blk_tail_gst;
	
	//For open zones
	struct zone_info *open_zone_head;
	struct zone_info *open_zone_tail;
	unsigned int open_zone_max;
	unsigned int open_zone_now;
	unsigned int evt_open_zone;	// number of open zone replacements
	unsigned int evt_open_blks; // number of innocuous writes.
	
	char buffer[SIZE_BUF];
	char filename_trc[128];
	char filename_out[128];
    
    FILE *file_trc;
    FILE *file_out;
};

struct req_info{
    long long blkn;
    unsigned int type;				//0->Read,1->Write
    unsigned int size;
};

struct blk_info{
	long long blkn;		//block number
	unsigned int zonen;	// zone number
	unsigned int state; // clean or dirty
	struct blk_info *blk_prev;
	struct blk_info *blk_next;
};

struct zone_info{
	unsigned int zonen;
	unsigned int state; //OPEN or CLOSED
	struct zone_info *zone_prev;
	struct zone_info *zone_next;
};

//cache.c
void cache_run_duchy(char *trace, char *output, unsigned int ssdsize);
void cache_free(struct cache_info *cache);
int  cache_get_req(struct cache_info *cache);
void cache_alloc_assert(void *p,char *s);

//duchy
void duchy_init(struct cache_info *cache,char *trace, char *output, unsigned int ssdsize);
void duchy_main(struct cache_info *cache);
int  duchy_check_reg(struct cache_info *cache,unsigned int blkn,unsigned int state);
int  duchy_check_gst(struct cache_info *cache,unsigned int blkn,unsigned int state);

int duchy_check_zone_state(struct cache_info *cache,unsigned int blkn);
void duchy_delete_tail_open_zone(struct cache_info *cache);

int duchy_delete_tail_blk_reg(struct cache_info *cache);
void duchy_delete_tail_blk_gst(struct cache_info *cache);
void duchy_print(struct cache_info *cache);

#endif
