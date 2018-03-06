#include "cache.h"

void cache_run_duchy(char *trace, char *output, unsigned int ssdsize)
{
	int i=0;
	struct cache_info *cache;
	
	cache=(struct cache_info *)malloc(sizeof(struct cache_info));
	cache_alloc_assert(cache,"cache");
	memset(cache,0,sizeof(struct cache_info));
	
	duchy_init(cache,trace,output,ssdsize);
	while(cache_get_req(cache) != FAILURE)
	{
		duchy_main(cache);
		
		i++;
		if(i%100000==0)
		{
			printf("duchy is Handling %d %s ssdsize=%d \n",i,trace,ssdsize);
		}
	}
	duchy_print(cache);
	cache_free(cache);
}

void cache_free(struct cache_info *cache)
{
	struct blk_info *index_reg,*blk_reg;
	struct blk_info *index_gst,*blk_gst;
	struct zone_info *index_zone,*open_zone;
	
	index_reg=cache->blk_head_reg;
	while(index_reg)
	{
		blk_reg=index_reg;
		index_reg=index_reg->blk_next;
		free(blk_reg);
	}
	
	index_gst=cache->blk_head_gst;
	while(index_gst)
	{
		blk_gst=index_gst;
		index_gst=index_gst->blk_next;
		free(blk_gst);
	}
	
	index_zone=cache->open_zone_head;
	while(index_zone)
	{
		open_zone=index_zone;
		index_zone=index_zone->zone_next;
		free(open_zone);
	}
	
	fclose(cache->file_trc);
	fclose(cache->file_out);
	
	free(cache->req);
	free(cache);
}

int cache_get_req(struct cache_info *cache)
{
	long long blkn;
	unsigned int size;
	unsigned int type;
	
	fgets(cache->buffer,SIZE_BUF,cache->file_trc);
	if(feof(cache->file_trc))
	{
		printf("***Read <%s> end***\n",cache->filename_trc);
		return FAILURE;
	}	
	
	sscanf(cache->buffer,"%lld %d %d\n",&blkn,&size,&type);
	//printf("%lld %d %d\n",blkn,size,type);
	cache->req->type=type;
	cache->req->blkn=blkn;
	cache->req->size=size;
	
	cache->blk_trc_all += size;
	if(type == READ)
	{
		cache->blk_trc_red += cache->req->size;
	}
	else
	{
		cache->blk_trc_wrt += cache->req->size;
	}	

	return SUCCESS;
}

void cache_alloc_assert(void *p,char *s)
{
	if(p!=NULL)
	{
		return;
	}
	printf("malloc %s error\n",s);
	getchar();
	exit(-1);
}
