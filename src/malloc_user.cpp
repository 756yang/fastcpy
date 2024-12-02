#include "malloc_user.h"


void* mempool::memalloc(uint32_t size)
{//申请size字节的内存,已优化
	int32_t i;
	blockflag *thispos,*thatpos,*nextpos,*lastpos;
	if(blockpos==NULL)return NULL;//无空闲块返回NULL
	thispos=blockpos->nextblock;//取当前空闲块
	size+=sizeof(blockflag);//计算所需大小
	size=(size+3)&0xfffffffc;//四字节对齐
	i=thispos->size-size;//计算大小差值
	if(i<0)
	{//当前空闲块大小不足
		if(blockpos==thispos)
		{//只有一个空闲块
			return NULL;
		}
		else
		{//有多个空闲块则依次适配
			/*循环变量赋初值*/
			lastpos=thispos;//上一个空闲块
			thispos=lastpos->nextblock;//当前空闲块
			/*开始循环*/
			do
			{
				i=thispos->size-size;//重新计算大小差值
				if(i<0)
				{//当前空闲块大小不足则适配下一个空闲块
					lastpos=thispos;
					thispos=lastpos->nextblock;
				}
				else
				{//当前空闲块大小足够
					if(i>sizeof(blockflag))
					{//当前空闲块大小有余则分为两块(空闲块,被申请内存块),thispos指向的块仍是空闲块
						thispos->size=i;//赋值
						thatpos=(blockflag*)((char*)thispos+i);//指向被申请内存块
						thatpos->nextblock=thispos;
						thatpos->size=size;//赋值
						blockpos=lastpos;//对blockpos重新赋值
						return thatpos+1;
					}
					else
					{//当前空闲块大小正好,不分块
						nextpos=thispos->nextblock;//下一个空闲块
						/*将当前块从循环空闲链表中去掉*/
						thispos->nextblock=lastpos;//指向上一个空闲块
						lastpos->nextblock=nextpos;//指向下一个空闲块
						blockpos=lastpos;//对blockpos重新赋值
						thatpos=(blockflag*)((char*)thispos+thispos->size);//指向当前块之后,为循环做准备
						do
						{
							if((char*)thatpos==endmem)
							{//thatpos指向内存区结束标志
								thatpos=(blockflag*)memory;//指向内存区开始位置
								continue;//继续循环
							}
							thatpos->nextblock=lastpos;//指向thatpos的上一个空闲块
							thatpos=(blockflag*)((char*)thatpos+thatpos->size);//计算下一个非空闲块的位置
						}while(thatpos!=nextpos);//判断forpos是否指向下一个空闲块
						return thispos+1;//返回当前块内存的地址
					}
				}
			}while(lastpos!=blockpos);//判断是否适配完所有空闲块
			return NULL;//所有空闲块都不适配,返回NULL
		}
	}
	else
	{//当前空闲块大小足够
		if(i>sizeof(blockflag))
		{//当前空闲块大小有余则分为两块(空闲块,申请内存块),thispos指向的块仍是空闲块
			thispos->size=i;//赋值
			thatpos=(blockflag*)((char*)thispos+i);//指向被申请内存块
			thatpos->nextblock=thispos;
			thatpos->size=size;//赋值
			/*无需对blockpos重新赋值*/
			return thatpos+1;
		}
		else
		{//当前空闲块大小正好,不分块
			if(blockpos==thispos)
			{//只有一个空闲块
				//thispos->nextblock=NULL;//可以注释掉
				blockpos=NULL;//表示无空闲块
				return thispos+1;//返回当前块内存的地址
			}
			else
			{//有多个空闲块
				nextpos=thispos->nextblock;//下一个空闲块
				/*将当前块从循环空闲链表中去掉*/
				thispos->nextblock=blockpos;//指向上一个空闲块
				blockpos->nextblock=nextpos;//指向下一个空闲块
				/*无需对blockpos重新赋值*/
				/*对非空闲块的处理*/
				thatpos=(blockflag*)((char*)thispos+thispos->size);//指向当前块之后,为循环做准备
				do
				{
					if((char*)thatpos==endmem)
					{//thatpos指向内存区结束标志
						thatpos=(blockflag*)memory;//指向内存区开始位置
						continue;//继续循环
					}
					thatpos->nextblock=blockpos;//指向thatpos的上一个空闲块
					thatpos=(blockflag*)((char*)thatpos+thatpos->size);//计算下一个非空闲块的位置
				}while(thatpos!=nextpos);//判断thatpos是否指向下一个空闲块
				return thispos+1;//返回当前块内存的地址/
			}
		}
	}
}

void mempool::memfree(void *p)
{//释放由memalloc申请的内存,已优化
	blockflag *thispos,*thatpos,*nextpos;
	thispos=(blockflag*)((char*)p-sizeof(blockflag));//计算块内存所对应的内存块位置
	if(blockpos==NULL)
	{//循环空闲链表为空则以thispos重建循环空闲链表
		thispos->nextblock=thispos;
		blockpos=thispos;
		thatpos=(blockflag*)((char*)thispos+thispos->size);//指向当前块之后,为循环做准备
		/*对非空闲块的处理*/
		do
		{
			if((char*)thatpos==endmem)
			{//thatpos指向内存区结束标志
				thatpos=(blockflag*)memory;//指向内存区开始位置
				continue;//继续循环
			}
			thatpos->nextblock=thispos;//指向thatpos的上一个空闲块
			thatpos=(blockflag*)((char*)thatpos+thatpos->size);//计算下一个非空闲块的位置
		}while(thatpos!=thispos);//判断thatpos是否指向下一个空闲块
	}
	else
	{//循环空闲链表不为空
		thatpos=thispos->nextblock;//指向上一个空闲块
		nextpos=thatpos->nextblock;//指向下一个空闲块
		if((char*)thatpos+thatpos->size==(char*)thispos)
		{//上一个空闲块和当前空闲块合并
			thatpos->size+=thispos->size;
			if((char*)thatpos+thatpos->size==(char*)nextpos)
			{//下一个空闲块和当前空闲块合并
				if(blockpos==nextpos)blockpos=thatpos;//blockpos等于nextpos,则需要重新赋值
				thatpos->size+=nextpos->size;
				nextpos=nextpos->nextblock;
				thatpos->nextblock=nextpos;
				thispos=(blockflag*)((char*)thatpos+thatpos->size);//为循环做准备
				/*对非空闲块的处理*/
				do
				{
					if((char*)thispos==endmem)
					{//thispos指向内存区结束标志
						thispos=(blockflag*)memory;//指向内存区开始位置
						continue;//继续循环
					}
					thispos->nextblock=thatpos;//指向thispos的上一个空闲块
					thispos=(blockflag*)((char*)thispos+thispos->size);//计算下一个非空闲块的位置
				}while(thispos!=nextpos);//判断thispos是否指向下一个空闲块
			}
			else
			{
				thatpos->nextblock=nextpos;
			}
		}
		else
		{
			thatpos->nextblock=thispos;
			if((char*)thispos+thispos->size==(char*)nextpos)
			{//下一个空闲块和当前空闲块合并
				if(blockpos==nextpos)blockpos=thispos;//blockpos等于nextpos,则需要重新赋值
				thispos->size+=nextpos->size;
				nextpos=nextpos->nextblock;
			}
			thispos->nextblock=nextpos;
			thatpos=(blockflag*)((char*)thispos+thispos->size);//指向当前块之后,为循环做准备
			/*对非空闲块的处理*/
			do
			{
				if((char*)thatpos==endmem)
				{//thatpos指向内存区结束标志
					thatpos=(blockflag*)memory;//指向内存区开始位置
					continue;//继续循环
				}
				thatpos->nextblock=thispos;//指向thatpos的上一个空闲块
				thatpos=(blockflag*)((char*)thatpos+thatpos->size);//计算下一个非空闲块的位置
			}while(thatpos!=nextpos);//判断thatpos是否指向下一个空闲块
		}
	}
}





