#include "cache.h"


void duchy_init(struct cache_info *cache, char *trace, char *output, unsigned int ssdsize)
{
	cache->size_block=4;		//KB
	cache->size_cache=ssdsize;	//MB
	
	cache->blk_trc_all=0;
	cache->blk_trc_red=0;
	cache->blk_trc_wrt=0;
	
	cache->blk_ssd_wrt=0;
	cache->blk_inn_wrt=0;
	
	cache->blk_max_all=1024*cache->size_cache/cache->size_block;
	cache->blk_max_reg=cache->blk_max_all;
	cache->blk_max_gst=cache->blk_max_all;
	
	cache->blk_now_reg=0;
	cache->blk_now_gst=0;
	
	cache->hit_red_reg=0;
	cache->hit_wrt_reg=0;
		
	cache->req=(struct req_info *)malloc(sizeof(struct req_info));
	cache_alloc_assert(cache->req,"cache->req");
	memset(cache->req,0,sizeof(struct req_info));

    cache->blk_head_reg=NULL;
    cache->blk_tail_reg=NULL;
    cache->blk_head_gst=NULL;
    cache->blk_tail_gst=NULL;
    
    cache->open_zone_head=NULL;
    cache->open_zone_tail=NULL;
    cache->open_zone_max=8;
    cache->open_zone_now=0;
    
    cache->evt_open_zone=0;
    cache->evt_open_blks=0;       
       
    strcpy(cache->filename_trc,trace);
    strcpy(cache->filename_out,output);
    
    cache->file_trc=fopen(cache->filename_trc,"r");
    cache->file_out=fopen(cache->filename_out,"w");
}

void duchy_main(struct cache_info *cache)
{
	if(cache->req->type == WRITE)
	{
		while(cache->req->size)
		{
			if(duchy_check_reg(cache,cache->req->blkn,WRITE) == SUCCESS)
			{
				cache->blk_ssd_wrt++;
				cache->hit_wrt_reg++;
			}
			else
			{
				if(duchy_check_gst(cache,cache->req->blkn,WRITE) == SUCCESS)
				{
					cache->blk_ssd_wrt++;
					//move this block from ghost cache to regular cache
					cache->blk_now_gst--;
					cache->blk_now_reg++;
					while(cache->blk_now_reg > cache->blk_max_reg)
					{
						duchy_delete_tail_blk_reg(cache);
						cache->blk_now_reg--;
					}
				}
				else//if to open zones, insert to the head of ghost cache
				{		
					//done in duchy_check_gst();			
				}
			}//else
			cache->req->size--;
			cache->req->blkn++;
		}//while
	}
	else if(cache->req->type == READ)
	{
		while(cache->req->size)
		{
			if(duchy_check_reg(cache,cache->req->blkn,READ) == SUCCESS)
			{
				cache->hit_red_reg++;
			}
			else
			{
				if(duchy_check_gst(cache,cache->req->blkn,READ) == SUCCESS)
				{
					cache->blk_ssd_wrt++;					
					//move this block from ghost cache to regular cache
					cache->blk_now_gst--;
					cache->blk_now_reg++;
					while(cache->blk_now_reg > cache->blk_max_reg)
					{
						duchy_delete_tail_blk_reg(cache);
						cache->blk_now_reg--;
					}
				}
				else//insert to the head of ghost cache
				{
					//done in duchy_check_gst();
				}
			}//else
			cache->req->size--;
			cache->req->blkn++;
		}//while
	}//else
	else
	{
		printf("ERROR: Wrong Request Type! \n");
		exit(-1);
	}
}

/**
For Regular Cache
	hit: search, delete, insert to head
	miss: go to check ghost cache
**/
int duchy_check_reg(struct cache_info *cache,unsigned int blkn,unsigned int state)
{
	struct blk_info *index;
	
	index = cache->blk_head_reg;
	while(index)
	{
		if(index->blkn == blkn)
		{	
			//delete and insert this blk to the head
			if(index == cache->blk_head_reg)
			{
				return SUCCESS;
			}
			else if(index == cache->blk_tail_reg)
			{	
				//delete from tail
				cache->blk_tail_reg = cache->blk_tail_reg->blk_prev;
				cache->blk_tail_reg->blk_next = NULL;
				//insert to head
				index->blk_prev = NULL;
				index->blk_next = cache->blk_head_reg;
				cache->blk_head_reg->blk_prev = index;
				cache->blk_head_reg = index;
			}
			else
			{
				//delete from list middle
				index->blk_prev->blk_next=index->blk_next;
				index->blk_next->blk_prev=index->blk_prev;
				//insert to head
				index->blk_prev = NULL;
				index->blk_next = cache->blk_head_reg;
				cache->blk_head_reg->blk_prev = index;
				cache->blk_head_reg = index;
			}
			return SUCCESS;
		}//if
		index = index->blk_next;
	}
	return FAILURE;
}

/**
For Ghost Cache
	hit: search, delete, insert to regular cache
	miss: build a new blk, insert to the head of ghost cache
**/
int duchy_check_gst(struct cache_info *cache,unsigned int blkn,unsigned int state)
{
	struct blk_info *index;
	struct blk_info *block;

	index = cache->blk_head_gst;
	while(index)
	{
		if(index->blkn == blkn)
		{	
			//delete 
			if(index == cache->blk_head_gst)
			{
				//delete from head
				if(index->blk_next != NULL) //current ghost cache size > 1
				{
					cache->blk_head_gst = cache->blk_head_gst->blk_next;
					cache->blk_head_gst->blk_prev = NULL;
				}
				else// head == tail
				{
					cache->blk_head_gst = NULL;
					cache->blk_tail_gst = NULL;
				}
			}
			else if(index == cache->blk_tail_gst)
			{	
				//delete from tail
				cache->blk_tail_gst = cache->blk_tail_gst->blk_prev;
				cache->blk_tail_gst->blk_next = NULL;
			}
			else
			{
				//delete from list middle
				index->blk_prev->blk_next=index->blk_next;
				index->blk_next->blk_prev=index->blk_prev;
			}
			free(index);
			
			// build a new blk and add to the head of regular cache
			block=(struct blk_info *)malloc(sizeof(struct blk_info)); 
			cache_alloc_assert(block,"block");
			memset(block,0,sizeof(struct blk_info));
	
			block->blkn = blkn;
			block->zonen = blkn/65536;
			block->state = state;
			
			if(cache->blk_head_reg == NULL)
			{
				block->blk_prev = NULL;
				block->blk_next = NULL;
				cache->blk_head_reg = block;
				cache->blk_tail_reg = block;
			}
			else
			{
				block->blk_prev = NULL;
				block->blk_next = cache->blk_head_reg;
				cache->blk_head_reg->blk_prev = block;
				cache->blk_head_reg = block;
			}
			return SUCCESS;
		}//if
		index = index->blk_next;
	}
//*****************************************************************************
 /*check the targeted zone state, if open, the block is bypassed to SMR drive and
 reocrd its identifier to the head of ghost cache. Otherwise, the block is inserted
 to the head of real cache like a hot block. Note that this scheme is only for dirty
 ota-blocks.*/					
//*****************************************************************************
	if(state == DIRTY)//only considering dirty blocks
	{
		if(duchy_check_zone_state(cache,blkn) == CLOSED)// if the targeted zone is CLOSED.
		{
			// build a new blk and it add to the head of regular cache
			block=(struct blk_info *)malloc(sizeof(struct blk_info)); 
			cache_alloc_assert(block,"block");
			memset(block,0,sizeof(struct blk_info));
	
			block->blkn = blkn;
			block->zonen = blkn/65536;
			block->state = state;
			
			if(cache->blk_head_reg == NULL)
			{
				block->blk_prev = NULL;
				block->blk_next = NULL;
				cache->blk_head_reg = block;
				cache->blk_tail_reg = block;
			}
			else
			{
				block->blk_prev = NULL;
				block->blk_next = cache->blk_head_reg;
				cache->blk_head_reg->blk_prev = block;
				cache->blk_head_reg = block;
			}
			
			cache->blk_ssd_wrt++;
			cache->blk_now_reg++;
			while(cache->blk_now_reg > cache->blk_max_reg)
			{
				duchy_delete_tail_blk_reg(cache);
				cache->blk_now_reg--;
			}
			return FAILURE;
		}
		else
		{
			//write this dirty block directly to SMR drive
			cache->blk_inn_wrt++;
			//do not update open zone now, update when dre-block eviction happens
			//then regard this block as a normal ota-block, like clean ota-blocks.
		}
	}//if	
	
	//insert to the head of ghost cache
	// build a new blk and add to head
	block=(struct blk_info *)malloc(sizeof(struct blk_info)); 
	cache_alloc_assert(block,"block");
	memset(block,0,sizeof(struct blk_info));
	
	block->blkn = blkn;
	block->zonen = blkn/65536;
	block->state = state;
	
	if(cache->blk_head_gst == NULL)
	{
		block->blk_prev = NULL;
		block->blk_next = NULL;
		cache->blk_head_gst = block;
		cache->blk_tail_gst = block;
	}
	else
	{
		block->blk_prev = NULL;
		block->blk_next = cache->blk_head_gst;
		cache->blk_head_gst->blk_prev = block;
		cache->blk_head_gst = block;
	}
	
	cache->blk_now_gst++;
	while(cache->blk_now_gst > cache->blk_max_gst)
	{
		duchy_delete_tail_blk_gst(cache);
		cache->blk_now_gst--;
	}
	
	return FAILURE;
}

//***********************************************************************
								/*Begining*/
//***********************************************************************
int duchy_check_zone_state(struct cache_info *cache,unsigned int blkn)
{
	struct zone_info *index;
	struct zone_info *zone;
	unsigned int zonen = blkn/65536;
	
	index = cache->open_zone_head;
	while(index)
	{
		if(index->zonen == zonen)
		{	
			//delete and insert this zone to the head
			if(index == cache->open_zone_head)
			{
				return SUCCESS;
			}
			else if(index == cache->open_zone_tail)
			{	
				//delete from tail
				cache->open_zone_tail = cache->open_zone_tail->zone_prev;
				cache->open_zone_tail->zone_next = NULL;
				//insert to head
				index->zone_prev = NULL;
				index->zone_next = cache->open_zone_head;
				cache->open_zone_head->zone_prev = index;
				cache->open_zone_head = index;
			}
			else
			{
				//delete from list middle
				index->zone_prev->zone_next=index->zone_next;
				index->zone_next->zone_prev=index->zone_prev;
				//insert to head
				index->zone_prev = NULL;
				index->zone_next = cache->open_zone_head;
				cache->open_zone_head->zone_prev = index;
				cache->open_zone_head = index;
			}
			return OPEN;
		}//if
		index = index->zone_next;
	}
		//***********************************************************************//
		/*********************For initialize the open zones**********************/
			if(cache->open_zone_now < cache->open_zone_max) //only happen at the begining
			{
				//build a new zone and add it to the head of the list
				zone=(struct zone_info *)malloc(sizeof(struct zone_info)); 
				cache_alloc_assert(zone,"zone");
				memset(zone,0,sizeof(struct zone_info));
	
				zone->zonen = blkn/65536;
				zone->state = OPEN;
	
				if(cache->open_zone_head == NULL)
				{
					zone->zone_prev = NULL;
					zone->zone_next = NULL;
					cache->open_zone_head = zone;
					cache->open_zone_tail = zone;
				}
				else
				{
					zone->zone_prev = NULL;
					zone->zone_next = cache->open_zone_head;
					cache->open_zone_head->zone_prev = zone;
					cache->open_zone_head = zone;
				}
			
				cache->open_zone_now++;
				return OPEN;
			}
		/*********************For initialize the open zones**********************/
		//***********************************************************************//		
	return CLOSED;
}

//***********************************************************************
								/*End*/
//***********************************************************************

void duchy_delete_tail_blk_gst(struct cache_info *cache)
{
	struct blk_info *block;
	
	block = cache->blk_tail_gst;
	if(block != cache->blk_head_gst)
	{
		cache->blk_tail_gst = cache->blk_tail_gst->blk_prev;
		cache->blk_tail_gst->blk_next = NULL;
	}
	else
	{
		cache->blk_tail_gst = NULL;
		cache->blk_head_gst = NULL;
	}
	free(block);
}


int duchy_delete_tail_blk_reg(struct cache_info *cache)
{
	struct blk_info *index;
	struct zone_info *indexz, *indexn;
	unsigned int i=0;
		
	index = cache->blk_tail_reg;
	
	while(i < 0.5*cache->blk_max_reg)
	{
		if((index->state == CLEAN)||(duchy_check_zone_state(cache, index->blkn) == OPEN))
		{
			//delete this block
			if(index == cache->blk_head_reg)
			{
				//delete from head
				if(index->blk_next != NULL) //current cache size > 1
				{
					cache->blk_head_reg = cache->blk_head_reg->blk_next;
					cache->blk_head_reg->blk_prev = NULL;
				}
				else// head == tail
				{
					cache->blk_head_reg = NULL;
					cache->blk_tail_reg = NULL;
				}
			}
			else if(index == cache->blk_tail_reg)
			{	
				//delete from tail
				cache->blk_tail_reg = cache->blk_tail_reg->blk_prev;
				cache->blk_tail_reg->blk_next = NULL;
			}
			else
			{
				//delete from list middle
				index->blk_prev->blk_next=index->blk_next;
				index->blk_next->blk_prev=index->blk_prev;
			}
			free(index);
			return SUCCESS;
		}
		index = index->blk_prev;
		i++;
	}
	
	//delete the tail block in regualr cache
	//open zone replacement is triggered
	index = cache->blk_tail_reg;	
	if(index != cache->blk_head_reg)
	{
		cache->blk_tail_reg = cache->blk_tail_reg->blk_prev;
		cache->blk_tail_reg->blk_next = NULL;
	}
	else
	{
		cache->blk_tail_reg = NULL;
		cache->blk_head_reg = NULL;
	}
	
	
	if(cache->open_zone_now < cache->open_zone_max)
	{
		printf("++++A real cache eviction before open zone is full?!+++++\n");
		//build a new zone and add it to the head of the list
		indexn=(struct zone_info *)malloc(sizeof(struct zone_info)); 
		cache_alloc_assert(indexn,"indexn");
		memset(indexn,0,sizeof(struct zone_info));
	
		indexn->zonen=index->zonen;
		indexn->state=OPEN;
	
		//insert to the head
		if(cache->open_zone_head == NULL)
		{
			indexn->zone_prev = NULL;
			indexn->zone_next = NULL;
			cache->open_zone_head = indexn;
			cache->open_zone_tail = indexn;
		}
		else
		{
			indexn->zone_prev = NULL;
			indexn->zone_next = cache->open_zone_head;
			cache->open_zone_head->zone_prev = indexn;
			cache->open_zone_head = indexn;
		}
	
		cache->open_zone_now++;
		free(index);
		return SUCCESS;
	}
	
	//open zone replacement
	//delete from tail
	indexz = cache->open_zone_tail;
	if(indexz != cache->open_zone_head)
	{
		cache->open_zone_tail = cache->open_zone_tail->zone_prev;
		cache->open_zone_tail->zone_next = NULL;
	}
	else
	{
		cache->open_zone_tail = NULL;
		cache->open_zone_head = NULL;
	}
	
	//build a new zone and add it to the head of the list
	indexn=(struct zone_info *)malloc(sizeof(struct zone_info)); 
	cache_alloc_assert(indexn,"indexn");
	memset(indexn,0,sizeof(struct zone_info));
	
	indexn->zonen=index->zonen;
	indexn->state=OPEN;
	
	//insert to the head
	if(cache->open_zone_head == NULL)
	{
		indexn->zone_prev = NULL;
		indexn->zone_next = NULL;
		cache->open_zone_head = indexn;
		cache->open_zone_tail = indexn;
	}
	else
	{
		indexn->zone_prev = NULL;
		indexn->zone_next = cache->open_zone_head;
		cache->open_zone_head->zone_prev = indexn;
		cache->open_zone_head = indexn;
	}
	
	free(index);
	free(indexz);
	return SUCCESS;	
}


void duchy_print(struct cache_info *cache)
{
	struct zone_info *index;
	
	printf("----------------------------------------------\n");
	printf("----------------------------------------------\n");
	printf("Cache Max blk Reg = %d\n",cache->blk_max_reg);
	printf("Cache Max blk Gst = %d\n",cache->blk_max_gst);
	printf("Cache Now blk Reg = %d\n",cache->blk_now_reg);
	printf("Cache Now blk Gst = %d\n",cache->blk_now_gst);
	printf("Cache Trc all blk = %d\n",cache->blk_trc_all);
	printf("Cache Trc red blk = %d\n",cache->blk_trc_red);
	printf("Cache Trc wrt blk = %d\n",cache->blk_trc_wrt);
	printf("Write Traffic SSD = %d\n",cache->blk_ssd_wrt);
	printf("------\n");
	printf("Cache Hit All = %d || All Hit Ratio = %Lf\n",
			(cache->hit_red_reg + cache->hit_wrt_reg),
			(long double)(cache->hit_red_reg + cache->hit_wrt_reg)/(long double)cache->blk_trc_all);
	printf("Cache Hit Red = %d || Red Hit Ratio = %Lf\n",
			cache->hit_red_reg,(long double)cache->hit_red_reg/(long double)cache->blk_trc_red);
	printf("Cache Hit Wrt = %d || Wrt Hit Ratio = %Lf\n",
			cache->hit_wrt_reg,(long double)cache->hit_wrt_reg/(long double)cache->blk_trc_wrt);
	printf("------\n");
	
	printf("---current open zones---\n");	
	index = cache->open_zone_head;
	while(index)
	{
		printf(" %d |",index->zonen);
		index = index->zone_next;
	}	
	printf("\n------------------------\n");
	
	fprintf(cache->file_out,"----------------------------------------------\n");
	fprintf(cache->file_out,"----------------------------------------------\n");
	fprintf(cache->file_out,"Cache Max blk Reg = %d\n",cache->blk_max_reg);
	fprintf(cache->file_out,"Cache Max blk Gst = %d\n",cache->blk_max_gst);
	fprintf(cache->file_out,"Cache Now blk Reg = %d\n",cache->blk_now_reg);
	fprintf(cache->file_out,"Cache Now blk Gst = %d\n",cache->blk_now_gst);
	fprintf(cache->file_out,"Cache Trc all blk = %d\n",cache->blk_trc_all);
	fprintf(cache->file_out,"Cache Trc red blk = %d\n",cache->blk_trc_red);
	fprintf(cache->file_out,"Cache Trc wrt blk = %d\n",cache->blk_trc_wrt);
	fprintf(cache->file_out,"Write Traffic SSD = %d\n",cache->blk_ssd_wrt);
	fprintf(cache->file_out,"------\n");
	fprintf(cache->file_out,"Cache Hit All = %d || All Hit Ratio = %Lf\n",
			(cache->hit_red_reg + cache->hit_wrt_reg),
			(long double)(cache->hit_red_reg + cache->hit_wrt_reg)/(long double)cache->blk_trc_all);
	fprintf(cache->file_out,"Cache Hit Red = %d || Red Hit Ratio = %Lf\n",
			cache->hit_red_reg,(long double)cache->hit_red_reg/(long double)cache->blk_trc_red);
	fprintf(cache->file_out,"Cache Hit Wrt = %d || Wrt Hit Ratio = %Lf\n",
			cache->hit_wrt_reg,(long double)cache->hit_wrt_reg/(long double)cache->blk_trc_wrt);
	fprintf(cache->file_out,"------\n");
	fprintf(cache->file_out,"---current open zones---\n");	
	index = cache->open_zone_head;
	while(index)
	{
		fprintf(cache->file_out," %d |",index->zonen);
		index = index->zone_next;
	}
	fprintf(cache->file_out,"\n------------------------\n");
}

