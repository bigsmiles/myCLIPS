/*******************************************************/
/*      "C" Language Integrated Production System      */
/*                                                     */
/*                                                     */
/*                                                     */
/*                      add by xuchao                  */
/*******************************************************/
#define _MOVE_SOURCE_

#include <stdio.h>
#define _STDIO_INCLUDED_
#include <string.h>

#include "setup.h"

//#include "drive.h"
//#include <windows.h>
#include <process.h>

#include "move.h"
#include "drive.h"
#include "reteutil.h"
#include "factmngr.h"

//add by xuchao
#include "envrnmnt.h"

#define ARRAY 0   //ARRAY�ⲿ����ԭ�������ѡ�񱻼���Ľڵ㣬����ʹ�������顣�����ڿ���ȥ����
#define SALIENCE 0
#define SCHEDULE 0
#if THREAD

struct JoinNodeList *joinNodeListHead;


struct JoinNodeList *joinNodeListTail;

extern struct activeJoinNode *activeNodeHead;
extern struct activeJoinNode *activeNodeTail;
extern CRITICAL_SECTION g_cs, g_move;
extern CRITICAL_SECTION g_fact_join;
extern HANDLE  g_hSemaphoreBuffer, g_hSemaphoreBufferOfThread1, g_hSemaphoreBufferOfThread2;

extern LARGE_INTEGER search_time1, search_time2;
extern long long search_time,cost_time[4],run_time[4];
extern long long cur_partialmatch_time[3];
long long totalFromAlpha = 0;

struct activeJoinNode* GetBestOneActiveNode(void *theEnv,int id);
void AddList(struct JoinNodeList*);

int EstimateJoinNodeToEndTime(struct joinNode *curJoinNode);
int totalGetActiveNode[4] = { 0 }, totalAddActiveNode = 0;

/*
���ȵ���Ҫ�������ӱ������beta�ڵ�����У�ѡ��ǰ�����ʹ�õ�(û�б������̴߳���)�ڵ�
*/
globle struct activeJoinNode* GetBestOneActiveNode(void *theEnv, int threadID)
{
	struct activeJoinNode *rtnNode = NULL;
	struct activeJoinNode *pNode = NULL;
	struct JoinNodeList *oneListNode = NULL;
	struct JoinNodeList *curListNode = NULL;
	/**/
#if TEST_PERFORMENCE
	LARGE_INTEGER large_time_start;
	QueryPerformanceCounter(&large_time_start);
	
	long long start = (long long)large_time_start.QuadPart;
#endif
	/**/
#if THREAD

	EnterCriticalSection(&g_cs);
#endif
#if SPEEDUP

	if (joinNodeListHead->next != NULL)//return NULL;
		oneListNode = joinNodeListHead->next;
	
	struct joinNode *tmp = NULL;


		while (oneListNode != NULL)
		{

				if (oneListNode->join->threadTag != -1)
				{
					tmp = oneListNode->join;
					curListNode = oneListNode;
					tmp->threadTag = -1;
					break;
				}
	
			oneListNode = oneListNode->next;
		}
		
#if CSECTION
		LeaveCriticalSection(&g_cs);

		if (oneListNode == NULL){
			return NULL;
		}

		EnterCriticalSection(&(tmp->nodeSection));
#endif



	if (oneListNode != NULL && oneListNode->join != NULL)
	{
		rtnNode = oneListNode->join->activeJoinNodeListHead->next;
		rtnNode->pre->next = rtnNode->next;
		if (rtnNode->next != NULL){
			rtnNode->next->pre = rtnNode->pre;
		}
		else{
			tmp->activeJoinNodeListTail = tmp->activeJoinNodeListHead;
		}
		tmp->numOfActiveNode -= 1;

#if REALMTHREAD
		//tmp->threadTag = -1;
#if DATASTRUCT
		if (tmp->numOfActiveNode == 0){
#if CSECTION
			EnterCriticalSection(&g_cs);
#endif
			if (curListNode->next == NULL){
				struct JoinNodeList* p = curListNode->pre;
				p->next = NULL;
				joinNodeListTail = p;

				curListNode->join = NULL;
				free(curListNode);
				curListNode = NULL;
			}
			else{
				struct JoinNodeList* p = curListNode->pre;
				p->next = curListNode->next;
				curListNode->next->pre = p;

				curListNode->join = NULL;
				free(curListNode);
				curListNode= NULL;
			}
#if CSECTION
			LeaveCriticalSection(&g_cs);
#endif
		}
#endif
#endif
	}

#endif
#if THREAD

	LeaveCriticalSection(&(tmp->nodeSection));

	
	if (rtnNode != NULL)
	{
		WaitForSingleObject(g_hSemaphoreBuffer, INFINITE);
		/**/
#if TEST_PERFORMENCE
		LARGE_INTEGER large_time;
		QueryPerformanceCounter(&large_time);
		
		long long time = (long long)large_time.QuadPart;
		search_time = time;
		cost_time[threadID] += (time - start);
		totalGetActiveNode[threadID] += 1;
#endif
		/**/
	}
#endif
	return rtnNode;
}
/*
��ӱ������beta�ڵ�ʱ������һ�����ȼ��ġ�˳��
*/
globle void AddList(struct JoinNodeList* oneNode){

	struct JoinNodeList* p = joinNodeListHead->next;
	//struct JoinNodeList* p = NULL;
	
#if SALIENCE
	/*
	//���ȼ�Խ��Խ��ǰ��
	while (p != NULL && p->join->nodeMaxSalience > oneNode->join->nodeMaxSalience){
		p = p->next;
	}
	//���ȼ�Խ�������(depth)Խ��Խ��ǰ��
	while(p != NULL && p->join->nodeMaxSalience == oneNode->join->nodeMaxSalience && p->join->depth >= oneNode->join->depth){
		p = p->next;
	}
	*/
	//����Խ��Խ��ǰ��
	while (p != NULL && p->join->fromBottomHeight < oneNode->join->fromBottomHeight){
		p = p->next;
	}
	
#else  //ֱ�ӷ������
	
	p = NULL; //place it at list tail
#endif
	if (p == NULL)
	{
		joinNodeListTail->next = oneNode;
		oneNode->pre = joinNodeListTail;
		joinNodeListTail = oneNode;
	}
	else
	{
		struct JoinNodeList* pre = p->pre;
		pre->next = oneNode; oneNode->pre = pre;
		oneNode->next = p; p->pre = oneNode;
	}
	return;
}
/*
����alpha��fact�ĵ����������beta�������������
*/
globle void AddNodeFromAlpha(
	void* theEnv,
	struct joinNode* curNode, //listOfJoins
	unsigned long hashValue,
	struct multifieldMarker *theMarks,
	struct fact* theFact,
	struct patternNodeHeader* header,
	struct patrialMacth* theMatch
	)
{
	
	struct activeJoinNode* oneNode = (struct activeJoinNode*) malloc(sizeof(struct activeJoinNode));
	oneNode->currentJoinNode = curNode;
	oneNode->currentPartialMatch = theMatch; 
	oneNode->curPMOnWhichSide = RHS;
	oneNode->markers = theMarks;
	oneNode->theEntity = theFact;
	oneNode->theHeader = header;// (struct patternNodeHeader *)&thePattern->header;
	oneNode->hashOffset = hashValue;
	oneNode->hashValue = hashValue;
	oneNode->next = NULL;
	int flag = 0;
	struct JoinNodeList *oneListNode = NULL;
	

	EnterCriticalSection(&(curNode->nodeSection));
	cur_partialmatch_time[0] = theFact->timestamp;


#if SPEEDUP
	if (curNode->activeJoinNodeListHead->next == NULL)
	{

		
		curNode->activeJoinNodeListTail->next = oneNode;
		oneNode->pre = curNode->activeJoinNodeListTail;
		curNode->activeJoinNodeListTail = oneNode;
#if DATASTRUCT
			oneListNode = (struct JoinNodeList*)malloc(sizeof(struct JoinNodeList));
			oneListNode->join = curNode;
			oneListNode->next = NULL;
			oneListNode->pre = NULL;
			flag = 1;
#endif
	}
	else
	{
#if SCHEDULE
		struct activeJoinNode* p = curNode->activeJoinNodeListTail;
		struct fact* oneNodeFact = (struct fact*)theFact;
		while (p != curNode->activeJoinNodeListHead && ((p->currentPartialMatch == NULL && ((oneNodeFact->timestamp < ((struct fact*)p->theEntity)->timestamp) || 
			(p->currentPartialMatch != NULL && p->currentPartialMatch->l_timeStamp > oneNodeFact->timestamp))))){
			p = p->pre;
		}
		if (p == curNode->activeJoinNodeListHead) {
			oneNode->next = curNode->activeJoinNodeListHead->next;
			curNode->activeJoinNodeListHead->next = oneNode;
			oneNode->pre = curNode->activeJoinNodeListHead;
			if (p == curNode->activeJoinNodeListTail)
				curNode->activeJoinNodeListTail = oneNode;
		}
		else{
			oneNode->next = p->next;
			if (p->next != NULL)p->next->pre = oneNode;
			p->next = oneNode; oneNode->pre = p;

			if (p == curNode->activeJoinNodeListTail)
				curNode->activeJoinNodeListTail = oneNode;
		}
#else 
		curNode->activeJoinNodeListTail->next = oneNode;
		oneNode->pre = curNode->activeJoinNodeListTail;
		curNode->activeJoinNodeListTail = oneNode;
#endif
	}
	
	curNode->numOfActiveNode += 1;

	LeaveCriticalSection(&(curNode->nodeSection));

	if (flag)
	{
		EnterCriticalSection(&g_cs);
		AddList(oneListNode);  //
		LeaveCriticalSection(&g_cs); 
	}
 
#endif
#if THREAD
	ReleaseSemaphore(g_hSemaphoreBuffer, 1, NULL);
	totalAddActiveNode += 1;

#endif
}
/*
beta�̼߳���beta�ڵ㣬�����������
*/
globle void AddOneActiveNode(
	void* theEnv, 
	struct partialMatch* partialMatch,
	struct partialMatch* lhsBinds,
	struct partialMatch* rhsBinds,
	struct joinNode* curNode, 
	unsigned long hashValue,
	char whichEntry,
	int threadID)
{

	if (partialMatch == NULL){
		//printf("%d\n", whichEntry);
	}
	/**/
#if TEST_PERFORMENCE
	LARGE_INTEGER large_time1;
	QueryPerformanceCounter(&large_time1);
	long long time1 = (long long)large_time1.QuadPart;
#endif
	/**/

	struct activeJoinNode *oneActiveNode = (struct activeJoinNode*) malloc(sizeof(struct activeJoinNode));
	oneActiveNode->curPMOnWhichSide = whichEntry;
	oneActiveNode->currentJoinNode = curNode;
	oneActiveNode->currentPartialMatch = partialMatch;
	oneActiveNode->lhsBinds = lhsBinds;
	oneActiveNode->rhsBinds = rhsBinds;
	oneActiveNode->hashValue = hashValue;
	oneActiveNode->hashOffset = hashValue;
	oneActiveNode->next = NULL;

	oneActiveNode->timeTag = partialMatch->timeTag;
	int flag = 0;
	struct JoinNodeList* oneListNode = NULL;


	EnterCriticalSection(&(curNode->nodeSection));

#if SPEEDUP

	if (curNode->activeJoinNodeListHead->next == NULL)
	{
		
		curNode->activeJoinNodeListTail->next = oneActiveNode;
		oneActiveNode->pre = curNode->activeJoinNodeListTail;
		curNode->activeJoinNodeListTail = oneActiveNode;
#if DATASTRUCT
		
			oneListNode = (struct JoinNodeList*)malloc(sizeof(struct JoinNodeList));
			oneListNode->join = curNode;
			oneListNode->next = NULL;
			oneListNode->pre = NULL;
			flag = 1;
#endif
	}
	else
	{
#if SCHEDULE
		struct activeJoinNode* p = curNode->activeJoinNodeListTail;
		/*if (curNode->activeJoinNodeListHead->next->currentPartialMatch->l_timeStamp < oneActiveNode->currentPartialMatch->l_timeStamp){
			oneActiveNode->next = curNode->activeJoinNodeListHead->next->next;
			curNode->activeJoinNodeListHead->next = oneActiveNode;
			oneActiveNode->pre = curNode->activeJoinNodeListHead->next;
			if(oneActiveNode->next != NULL)oneActiveNode->next->pre = oneActiveNode;

		}*/
		while (p != curNode->activeJoinNodeListHead && ((p->currentPartialMatch == NULL && (oneActiveNode->currentPartialMatch->l_timeStamp < ((struct fact*)p->theEntity)->timestamp) 
			|| (p->currentPartialMatch != NULL && p->currentPartialMatch->l_timeStamp > oneActiveNode->currentPartialMatch->l_timeStamp)))){
			p = p->pre;
		}
		if (p == curNode->activeJoinNodeListHead) {
			oneActiveNode->next = curNode->activeJoinNodeListHead->next;
			curNode->activeJoinNodeListHead->next = oneActiveNode;
			oneActiveNode->pre = curNode->activeJoinNodeListHead;
			if (p == curNode->activeJoinNodeListTail)
				curNode->activeJoinNodeListTail = oneActiveNode;
		}
		else{
			oneActiveNode->next = p->next;
			if (p->next != NULL)p->next->pre = oneActiveNode;
			p->next = oneActiveNode; oneActiveNode->pre = p;
			
			if (p == curNode->activeJoinNodeListTail)
				curNode->activeJoinNodeListTail = oneActiveNode;
		}
#else 
		curNode->activeJoinNodeListTail->next = oneActiveNode;
		oneActiveNode->pre = curNode->activeJoinNodeListTail;
		curNode->activeJoinNodeListTail = oneActiveNode;
#endif
	}
	
	curNode->numOfActiveNode += 1;
	LeaveCriticalSection(&curNode->nodeSection);

	if (flag)
	{

		EnterCriticalSection(&g_cs);
		AddList(oneListNode);
		LeaveCriticalSection(&g_cs);
	}
#endif

#if THREAD
	ReleaseSemaphore(g_hSemaphoreBuffer, 1, NULL);
	totalAddActiveNode += 1;
#endif
	/**/
#if TEST_PERFORMENCE
	LARGE_INTEGER large_time2;
	QueryPerformanceCounter(&large_time2);
	long long time2 = (long long)large_time2.QuadPart;
	cost_time[threadID] += (time2 - time1);
#endif
	/**/
	return;
}
/*
�߳�������ѡ��������ŵġ�beta�ڵ㣬Ȼ���beta�ڵ�Ķ�����ѡ��һ��ʵ����Ȼ����ô��ݵĺ���

*/
unsigned int __stdcall MoveOnJoinNetworkThread(void *pM)
{
#if THREAD
	void *theEnv = ((struct ThreadNode*)pM)->theEnv;
	int threadID = ((struct ThreadNode*)pM)->threadTag;
#endif
	struct activeJoinNode *currentActiveNode;
	struct joinNode *currentJoinNode;
	struct partialMatch *currentPartialMatch;
	struct partialMatch *lhsBinds;
	struct partialMatch *rhsBinds;
	unsigned long hashValue;
	char enterDirection;
	struct fact *theFact;
	struct multifieldMarker *theMarks;
	struct patternNodeHeader *theHeader;
	unsigned long hashOffset;
	long long timeTag;

	LARGE_INTEGER large_time;
	LARGE_INTEGER start_time, end_time;

	int time_out = 1;
	while (1)
	{
		
		currentActiveNode = GetBestOneActiveNode(theEnv,threadID);

		if (currentActiveNode == NULL)continue;
	
		
		currentJoinNode = currentActiveNode->currentJoinNode;
		currentPartialMatch = currentActiveNode->currentPartialMatch;
		lhsBinds = currentActiveNode->lhsBinds;
		rhsBinds = currentActiveNode->rhsBinds;
		hashValue = currentActiveNode->hashValue;
		enterDirection = currentActiveNode->curPMOnWhichSide;
		theFact = (struct fact *) currentActiveNode->theEntity;
		theMarks = currentActiveNode->markers;
		theHeader = currentActiveNode->theHeader;
		//timeTag = currentActiveNode->timeTag;
		time_out = 1;
		QueryPerformanceCounter(&start_time);
		

		if (time_out && currentJoinNode->firstJoin)
		{
			//printf("empty network : join : %x, depth: %d %s\n", currentJoinNode, currentJoinNode->depth, theFact->whichDeftemplate->header.name->contents);
			
			//currentPartialMatch = CreateAlphaMatch(theEnv, theFact, theMarks, theHeader, currentActiveNode->hashOffset);
			EnterCriticalSection(&g_move);
			//currentPartialMatch = CreateAlphaMatch(GetEnvironmentByIndex(1), theFact, theMarks, theHeader, currentActiveNode->hashOffset);
			//currentPartialMatch = theFact->alphaMatch;
			LeaveCriticalSection(&g_move);
			currentPartialMatch->owner = theHeader;
			//currentPartialMatch->timeTag = timeTag;
#if OPTIMIZE
			//theFact->factNotOnNodeMask = (theFact->factNotOnNodeMask | (1<<currentJoinNode->depth));
			EnterCriticalSection(&g_fact_join);

			//printf("before mask: %s %x\n", theFact->whichDeftemplate->header.name->contents,theFact->factNotOnNodeMask);
			theFact->factNotOnNodeMask |= (1 << currentJoinNode->numOfTmp);
			
			//printf("first join realase : %x %s depth: %d numOfTmp: %d mask: %x\n", theFact,theFact->whichDeftemplate->header.name->contents,currentJoinNode->depth, currentJoinNode->numOfTmp,theFact->factNotOnNodeMask);
			LeaveCriticalSection(&g_fact_join);
#endif

			((struct patternMatch *)theFact->list)->theMatch = currentPartialMatch;
			EmptyDrive(theEnv, currentJoinNode, currentPartialMatch,threadID);
		}
		else if (time_out && currentActiveNode->curPMOnWhichSide == LHS)
		{
			//printf("network left : env = %x update on join : %x, depth: %d ,hash = %ld\n", theEnv,currentJoinNode, currentJoinNode->depth,hashValue);
			UpdateBetaPMLinks(theEnv, currentPartialMatch, lhsBinds, rhsBinds, currentJoinNode, hashValue, enterDirection);
			//currentPartialMatch->hashValue = hashValue;
			NetworkAssertLeft(theEnv, currentPartialMatch, currentJoinNode,threadID);
		
		}
		else if (time_out && currentActiveNode->curPMOnWhichSide == RHS)
		{
			//printf("network right : join : %x, depth: %d ,%s\n", currentJoinNode, currentJoinNode->depth, theFact->whichDeftemplate->header.name->contents);
			//UpdateBetaPMLinks(theEnv, currentPartialMatch, lhsBinds, rhsBinds, currentJoinNode, hashValue, enterDirection);
			EnterCriticalSection(&g_move);
			//currentPartialMatch = CreateAlphaMatch(theEnv, theFact, theMarks, theHeader, currentActiveNode->hashOffset);

			//currentPartialMatch = CreateAlphaMatch(GetEnvironmentByIndex(1), theFact, theMarks, theHeader, currentActiveNode->hashOffset);
			//currentPartialMatch = theFact->alphaMatch;
			LeaveCriticalSection(&g_move);

			currentPartialMatch->owner = theHeader;
#if THREAD 1
			//EnterCriticalSection(&g_move); // remove by xuchao , remove ok?
			//EnterCriticalSection(&g_fact_join);
#if OPTIMIZE
			EnterCriticalSection(&g_fact_join);
			//theFact->factNotOnNodeMask = (theFact->factNotOnNodeMask | (1<<currentJoinNode->depth));
			//printf("before : %x %d %lld\n", theFact->factNotOnNodeMask, currentJoinNode->numOfTmp,theFact->timestamp);
			//theFact->factNotOnNodeMask = (theFact->factNotOnNodeMask | (1 << currentJoinNode->numOfTmp));
			//printf("before mask: %s %x\n", theFact->whichDeftemplate->header.name->contents,theFact->factNotOnNodeMask);
			theFact->factNotOnNodeMask |= (1 << currentJoinNode->numOfTmp);
			//printf("after : %s %x %d %lld\n", theFact->whichDeftemplate->header.name->contents,theFact->factNotOnNodeMask, currentJoinNode->numOfTmp,theFact->timestamp);
			//printf("right join realase : %x %s depth: %d numOfTmp: %d mask: %x\n", theFact,theFact->whichDeftemplate->header.name->contents,currentJoinNode->depth, currentJoinNode->numOfTmp, theFact->factNotOnNodeMask);
			LeaveCriticalSection(&g_fact_join);
#endif
			//LeaveCriticalSection(&g_fact_join);
			//LeaveCriticalSection(&g_move);
#endif
		    ((struct patternMatch *)theFact->list)->theMatch = currentPartialMatch;
			NetworkAssertRight(theEnv, currentPartialMatch, currentJoinNode,threadID);
			
			//LeaveCriticalSection(&g_move);
			//LeaveCriticalSection(&g_cs);
		}
		else {/*error*/}

#if REALMTHREAD

		//EnterCriticalSection(&g_cs);

		currentJoinNode->threadTag = 0;
		free(currentActiveNode);
		currentActiveNode = NULL;
		
		QueryPerformanceCounter(&end_time);
		run_time[threadID] += (end_time.QuadPart - start_time.QuadPart);
		//LeaveCriticalSection(&g_cs);
#endif
	}
	
	return 0;
}


/*
���ȵ���Ҫ�������ӱ������beta�ڵ�����У�ѡ��ǰ�����ʹ�õ�(û�б������̴߳���)�ڵ�
*/
globle struct activeJoinNode* GetBestOneActiveNodeOnSet(void *theEnv, int threadID)
{
	struct activeJoinNode *rtnNode = NULL;
	struct activeJoinNode *pNode = NULL;
	struct JoinNodeList *oneListNode = NULL;
	struct JoinNodeList *curListNode = NULL;
	/**/
#if TEST_PERFORMENCE
	LARGE_INTEGER large_time_start;
	QueryPerformanceCounter(&large_time_start);

	long long start = (long long)large_time_start.QuadPart;
#endif
	/**/


	EnterCriticalSection(&g_cs);

	if (joinNodeListHead->next != NULL)//return NULL;
		oneListNode = joinNodeListHead->next;

	struct joinNode *tmp = NULL;

	while (oneListNode != NULL)
	{

		if (oneListNode->join->threadTag != -1)
		{
			tmp = oneListNode->join;
			curListNode = oneListNode;
			tmp->threadTag = -1;
			break;
		}

		oneListNode = oneListNode->next;
	}


	LeaveCriticalSection(&g_cs);

	if (oneListNode == NULL){
		return NULL;
	}

	EnterCriticalSection(&(tmp->nodeSection));

	rtnNode = tmp->activeJoinNodeListHead->next;
	tmp->numOfActiveNode = 0;
	tmp->activeJoinNodeListHead->next = NULL;
	tmp->activeJoinNodeListTail = tmp->activeJoinNodeListHead;

	EnterCriticalSection(&g_cs);

	if (curListNode->next == NULL){
		struct JoinNodeList* p = curListNode->pre;
		p->next = NULL;
		joinNodeListTail = p;

		curListNode->join = NULL;
		free(curListNode);
		curListNode = NULL;
	}
	else{
		struct JoinNodeList* p = curListNode->pre;
		p->next = curListNode->next;
		curListNode->next->pre = p;

		curListNode->join = NULL;
		free(curListNode);
		curListNode = NULL;
	}

	LeaveCriticalSection(&g_cs);


	LeaveCriticalSection(&(tmp->nodeSection));


	if (rtnNode != NULL)
	{
		WaitForSingleObject(g_hSemaphoreBuffer, INFINITE);
		/**/
#if TEST_PERFORMENCE
		LARGE_INTEGER large_time;
		QueryPerformanceCounter(&large_time);

		long long time = (long long)large_time.QuadPart;
		search_time = time;
		cost_time[threadID] += (time - start);
		totalGetActiveNode[threadID] += 1;
#endif
		/**/
	}

	return rtnNode;
}
unsigned int __stdcall MoveOnJoinNetworkBySetThread(void *pM)
{
#if THREAD
	void *theEnv = ((struct ThreadNode*)pM)->theEnv;
	int threadID = ((struct ThreadNode*)pM)->threadTag;
#endif
	struct activeJoinNode *currentActiveNode, *currentActiveNodeSet;
	struct joinNode *currentJoinNode;
	struct partialMatch *currentPartialMatch;
	struct partialMatch *lhsBinds;
	struct partialMatch *rhsBinds;
	unsigned long hashValue;
	char enterDirection;
	struct fact *theFact;
	struct multifieldMarker *theMarks;
	struct patternNodeHeader *theHeader;
	unsigned long hashOffset;
	long long timeTag;

	LARGE_INTEGER large_time;
	LARGE_INTEGER start_time, end_time;

	int time_out = 1;
	while (1)
	{
		currentJoinNode = NULL;
		currentActiveNodeSet = NULL;
		currentActiveNodeSet = GetBestOneActiveNodeOnSet(theEnv, threadID);
		
		
		for (currentActiveNode = currentActiveNodeSet; currentActiveNode != NULL; currentActiveNode = currentActiveNodeSet)
		{

			if (currentActiveNode == NULL)continue;
			

			currentJoinNode = currentActiveNode->currentJoinNode;
			currentPartialMatch = currentActiveNode->currentPartialMatch;
			lhsBinds = currentActiveNode->lhsBinds;
			rhsBinds = currentActiveNode->rhsBinds;
			hashValue = currentActiveNode->hashValue;
			enterDirection = currentActiveNode->curPMOnWhichSide;
			theFact = (struct fact *) currentActiveNode->theEntity;
			theMarks = currentActiveNode->markers;
			theHeader = currentActiveNode->theHeader;
			
			time_out = 1;
			QueryPerformanceCounter(&start_time);


			if (time_out && currentJoinNode->firstJoin)
			{

				currentPartialMatch->owner = theHeader;

				EnterCriticalSection(&g_fact_join);
				theFact->factNotOnNodeMask |= (1 << currentJoinNode->numOfTmp);
				LeaveCriticalSection(&g_fact_join);


				((struct patternMatch *)theFact->list)->theMatch = currentPartialMatch;
				EmptyDrive(theEnv, currentJoinNode, currentPartialMatch, threadID);
			}
			else if (time_out && currentActiveNode->curPMOnWhichSide == LHS)
			{
				UpdateBetaPMLinks(theEnv, currentPartialMatch, lhsBinds, rhsBinds, currentJoinNode, hashValue, enterDirection);
				NetworkAssertLeft(theEnv, currentPartialMatch, currentJoinNode, threadID);

			}
			else if (time_out && currentActiveNode->curPMOnWhichSide == RHS)
			{

				currentPartialMatch->owner = theHeader;

				EnterCriticalSection(&g_fact_join);
				theFact->factNotOnNodeMask |= (1 << currentJoinNode->numOfTmp);
				LeaveCriticalSection(&g_fact_join);

				((struct patternMatch *)theFact->list)->theMatch = currentPartialMatch;
				NetworkAssertRight(theEnv, currentPartialMatch, currentJoinNode, threadID);
			}
			else {/*error*/ }


			currentActiveNodeSet = currentActiveNodeSet->next;
			free(currentActiveNode);
			currentActiveNode = NULL;
#if TEST_PERFORMENCE
			QueryPerformanceCounter(&end_time);
			run_time[threadID] += (end_time.QuadPart - start_time.QuadPart);
#endif
		} //end for
		if (currentJoinNode)
		{
			currentJoinNode->threadTag = 0;
		}
	}

	return 0;
}

#endif // THREAD
globle double SlowDown()
{
	//Sleep(3000);
	int slow = 5;
	int alpha = 123;
	double result = 1;
	while (slow-- > 0){
		result = sin(alpha * result);
	}
	return result;
}
