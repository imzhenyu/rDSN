
namespace dsn { namespace tools {

/* Time event structure */
class event_loop;
typedef void(*lpTimeProc)(event_loop*, void*, void*);

struct lpTimerHeap;

# define USE_STL_FOR_TIMER_CONTAINER 1

typedef struct lpTimeEvent 
{
    long long id; /* time event identifier. */
    unsigned long long when_ms;
    lpTimeProc timeProc;
    const char *name;
    event_loop* loop;
    void *clientData;
    void *clientData2;
# ifdef USE_STL_FOR_TIMER_CONTAINER
    lpTimeEvent *next;
# else
    int idx; /* index in heap array */
# endif
} lpTimeEvent;

lpTimerHeap* lpCreateTimerHeap(unsigned int capacity);
void lpDestroyTimerHeap(lpTimerHeap *heap);

// return timer handler
lpTimeEvent* lpCreateHeapTimer(const char* name, lpTimeProc proc, event_loop* lp, void* data, void* data2, unsigned long long when_ms);
long long lpAddToTimerHeap(lpTimerHeap *heap, lpTimeEvent *ev);

unsigned long long lpTimerHeapTopNextMilliseconds(lpTimerHeap *heap, unsigned long long now_ms);
int lpProcessTimers(lpTimerHeap *heap, unsigned long long now_ms);

} } // end namespace ::dsn::tools 