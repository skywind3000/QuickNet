#include "FecCodec.h"
#include <map>
extern "C" 
{
#include "../system/fec.h"
}





/**
*遍历链表，找到相应的codec
*/

FecCodec* find_codec(FecCodecList& codecList, int k,int n)
{
	FecCodec *p_ret = NULL;

	for( FecCodecList::iterator it = codecList.begin(); p_ret == NULL && it != codecList.end(); it++)
	{
		if (it->second == NULL)
		{
			continue;
		}
		if (it->second->k == k && it->second->n == n)
		{
			p_ret = it->second;
		}
	}
	return p_ret;
}

FecCodec* get_codec_by(FecCodecList& codecList, float lostRate)
{
	FecCodec *p_ret = NULL;
	if (codecList.empty())
	{
		return p_ret;
	}
	
	/**
	*lostRate <= 1-k/n;
	*/
	float lastRate = 0.0f;
	FecCodec *lastFecCodec = NULL;
	for( FecCodecList::iterator it = codecList.begin(); p_ret == NULL && it != codecList.end(); it++)
	{
		float curRate = it->first;
        if (it == codecList.begin())
        {
            if (lostRate >= lastRate && lostRate <= curRate)
            {
                p_ret = it->second;
            }
        }
        else
        {
		    if (lostRate > lastRate && lostRate <= curRate)
		    {
			    p_ret = it->second;
		    }
        }
		lastRate = curRate;
		lastFecCodec = it->second;
	}
	if (p_ret == NULL)
	{
		p_ret = lastFecCodec; //最末尾的一个，冗余度最大的。
	}
	return p_ret;

}

/**
*新建一个相应的codec,插入链表，返回链表头
*此操作必定会增加链表长度
*/
FecCodec* add_new_codec(FecCodecList& codecList,  int k, int n)
{
	FecCodec *p_new_item = new FecCodec();
	p_new_item->k = k;
	p_new_item->n = n;
	p_new_item->codec = fec_new(k, n);

	//冗余度：
	float key = 1.0f - float(k)/float(n);

	FecCodecList::iterator it = codecList.find(key);
	if (it != codecList.end())
	{
		delete it->second;
		it->second = NULL;
	}
	codecList.insert( std::pair<float,FecCodec*>(key, p_new_item) );
	return p_new_item;
}

int get_codec_count(FecCodecList& codecList)
{
	int nRet = int(codecList.size());
	return nRet;
}


FecCodec* get_codec(FecCodecList& codecList, int i)
{
	int i_cur_item = 0;
	FecCodec * p_item = NULL;
	for( FecCodecList::iterator it = codecList.begin(); p_item == NULL && it != codecList.end(); it++)
	{
		if (i_cur_item == i)
		{
			p_item = it->second;
		}
		i_cur_item++;
	}
	return p_item;
}

/**
*删除整个链表；
*返回链表长度
*/
void release_all_codec(FecCodecList& codecList)
{
	for( FecCodecList::iterator it = codecList.begin(); it != codecList.end(); it++)
	{
		if (it->second != NULL)
		{
			if (it->second->codec != NULL)
			{
				fec_free(it->second->codec);
				it->second->codec = NULL;
			}
			delete it->second;
			it->second = NULL;
		}
	}
	codecList.clear();
}



