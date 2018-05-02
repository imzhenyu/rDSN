
# include "heap_timer.h"
# include <dsn/service_api_c.h>
# include <cstdlib>
# include <map>
# include <dsn/utility/link.h>
# include <cstdio>

# ifndef __TITLE__
# define __TITLE__ "timer"
# endif

namespace dsn { namespace tools {

# ifdef USE_STL_FOR_TIMER_CONTAINER

typedef std::map<uint64_t, slist<lpTimeEvent>> timer_map;

lpTimerHeap* lpCreateTimerHeap(unsigned int capacity)
{
    return (lpTimerHeap*)new timer_map;
}

void lpDestroyTimerHeap(lpTimerHeap *heap)
{
    timer_map* tmap = (timer_map*)heap;
    for (auto& kv : *tmap)
    {
        auto& l = kv.second;
        while (!l.is_empty()) {
            auto e = l.pop_one();
            free(e);
        }
    }
    delete tmap;
}

lpTimeEvent* lpCreateHeapTimer(const char* name, lpTimeProc proc, event_loop* lp, void* data, void* data2, unsigned long long when_ms)
{
    lpTimeEvent* evt = (lpTimeEvent*)malloc(sizeof(lpTimeEvent));
    evt->name = name;
    evt->loop = lp;
    evt->clientData = data;
    evt->clientData2 = data2;
    evt->timeProc = proc;
    evt->when_ms = when_ms;
    evt->id = 0;
    evt->next = nullptr;
    return evt;
}

lpTimeEvent* lpTimerHeapTop(lpTimerHeap *heap)
{
    auto theap = (timer_map*)heap;

    dassert(theap->size() > 0, "heap is empty");
    return theap->begin()->second._first;
}

int lpTimeEventExpired(unsigned long long when_ms, unsigned long long now_ms)
{
    return now_ms >= when_ms;
}

unsigned long long lpTimerHeapTopNextMilliseconds(lpTimerHeap *heap, unsigned long long now_ms)
{
    auto theap = (timer_map*)heap;
    if (theap->empty())
        return ~0ULL;
    else
    {
        auto when_ms = theap->begin()->first;
        return lpTimeEventExpired(when_ms, now_ms) ? 0 : (when_ms - now_ms);
    }
}

int lpTimeEventBefore(lpTimeEvent *l, lpTimeEvent *r)
{
    return (l->when_ms < r->when_ms);
}

long long lpAddToTimerHeap(lpTimerHeap *heap, lpTimeEvent *ev)
{
    ev->id = (long long)(void*)ev;

    dinfo("add timer %p at %llu", ev, ev->when_ms);
    auto theap = (timer_map*)heap;
    auto pr = theap->insert(
        std::map<uint64_t, slist<lpTimeEvent>>::value_type(ev->when_ms, slist<lpTimeEvent>()));
    pr.first->second.add(ev);
    return ev->id;
}

int lpProcessTimers(lpTimerHeap *heap, unsigned long long now_ms)
{
    int processed = 0;
    auto theap = (timer_map*)heap;

    while (true)
    {
        lpTimeEvent* t = nullptr, *next;
        {
            if (theap->size() > 0)
            {
                auto it = theap->begin();
                if (it->first <= now_ms)
                {
                    t = it->second.pop_all();
                    theap->erase(it);
                }
                else
                    return processed;
            }
            else
                return processed;
        }

        while (t)
        {
            next = t->next;
            dinfo("exec timer %p at %llu", t, t->when_ms);
            (*t->timeProc)(t->loop, t->clientData, t->clientData2);
            free(t);
            processed++;
            t = next;
        }
    }

    return processed;
}

# else

typedef struct lpTimerHeap {
    unsigned int capacity;
    unsigned int size;
    lpTimeEvent **timers;
} lpTimerHeap;

lpTimerHeap* lpCreateTimerHeap(unsigned int capacity) 
{
    lpTimerHeap *heap = (lpTimerHeap *)malloc(sizeof(lpTimerHeap));
    heap->timers = (lpTimeEvent**)malloc(sizeof(lpTimeEvent*) * capacity);
    heap->capacity = capacity;
    heap->size = 0;
    return heap;
}

void lpDestroyTimerHeap(lpTimerHeap *heap) 
{
    unsigned int i = 0;
    for (; i < heap->size; i++) 
    {
        dassert(heap->timers[i]->idx == i, "invalid timer index");
        free(heap->timers[i]);
    }
    free(heap->timers);
    free(heap);
}

lpTimeEvent* lpCreateHeapTimer(const char *name, lpTimeProc proc, event_loop* lp, void* data, void* data2, unsigned long long when_ms)
{
    lpTimeEvent* evt = (lpTimeEvent*)malloc(sizeof(lpTimeEvent));
    evt->name = name;
    evt->loop = lp;
    evt->clientData = data;
    evt->clientData2 = data2;
    evt->timeProc = proc;
    evt->when_ms = when_ms;
    evt->id = 0;
    evt->idx = 0;
    return evt;
}

lpTimeEvent* lpTimerHeapTop(lpTimerHeap *heap) 
{
    dassert(heap->size > 0, "heap is empty");
    return heap->timers[0];
}

int lpTimeEventExpired(unsigned long long when_ms, unsigned long long now_ms)
{
    return now_ms >= when_ms;
}

unsigned long long lpTimerHeapTopNextMilliseconds(lpTimerHeap *heap, unsigned long long now_ms)
{
    if (heap->size == 0)
        return ~0ULL;
    else 
    {
        auto when_ms = heap->timers[0]->when_ms;
        return lpTimeEventExpired(when_ms, now_ms) ? 0 : (when_ms - now_ms);
    }
}

int lpTimeEventBefore(lpTimeEvent *l, lpTimeEvent *r) 
{
    return (l->when_ms < r->when_ms);
}

long long lpAddToTimerHeap(lpTimerHeap *heap, lpTimeEvent *ev) 
{
    ev->id = (long long)(void*)ev;

    // resize if necessary
    if (heap->size == heap->capacity) 
    {
        heap->capacity *= 2;
        heap->timers = (lpTimeEvent**)realloc(heap->timers, sizeof(lpTimeEvent*) * heap->capacity);
    }

    heap->timers[heap->size++] = ev;
    unsigned int cur = heap->size - 1;
    ev->idx = cur;
    while (cur > 0) 
    {
        unsigned int parent = (cur - 1) / 2;
        lpTimeEvent *l = heap->timers[cur];
        lpTimeEvent *r = heap->timers[parent];
        if (lpTimeEventBefore(l, r)) 
        {
            heap->timers[cur] = r;
            heap->timers[parent] = l;
            heap->timers[cur]->idx = cur;
            heap->timers[parent]->idx = parent;
        }
        else 
        {
            break;
        }
        cur = parent;
    }

    return ev->id;
}

void aeDownShiftTimerHeap(lpTimerHeap *heap, int idx)
{
    unsigned int cur = idx;
    unsigned int l = cur * 2 + 1;
    unsigned int r = cur * 2 + 2;
    unsigned int size = heap->size;
    while (l < size || r < size)
    {
        unsigned int min = cur;
        if (l < size && lpTimeEventBefore(heap->timers[l], heap->timers[min]))
        {
            min = l;
        }
        if (r < size && lpTimeEventBefore(heap->timers[r], heap->timers[min]))
        {
            min = r;
        }
        if (min == cur)
        {
            break;
        }
        lpTimeEvent *t = heap->timers[cur];
        heap->timers[cur] = heap->timers[min];
        heap->timers[min] = t;
        heap->timers[cur]->idx = cur;
        heap->timers[min]->idx = min;
        cur = min;
        l = cur * 2 + 1;
        r = cur * 2 + 2;
    }
}

void adDeleteHeapTimer(lpTimerHeap *heap, long long id) 
{
    lpTimeEvent *ev = (lpTimeEvent*)id;
    dbg_dassert(ev->id == id, "invalid timer id");
    int cur = ev->idx;
    heap->timers[cur] = heap->timers[heap->size - 1];
    heap->timers[cur]->idx = cur;
    heap->size--;
    aeDownShiftTimerHeap(heap, cur);
    free(ev);
}

int lpProcessTimers(lpTimerHeap *heap, unsigned long long now_ms)
{
    int processed = 0;
    lpTimeEvent *ev;

    while (heap->size > 0) 
    {
        ev = lpTimerHeapTop(heap);
        dbg_dassert(ev->idx == 0, "invalid idnex");

        if (!lpTimeEventExpired(ev->when_ms, now_ms))
        {
            break;
        }

        long long id = ev->id;
        (*ev->timeProc)(ev->clientData, ev->clientData2);
        adDeleteHeapTimer(heap, id);
        processed++;
    }

    return processed;
}

# endif

}} // end ::dsn::tools 
