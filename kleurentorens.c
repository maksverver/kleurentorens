#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>

#define FOR(i,a,b) for(i=(a);i<(b);++i)
#define REP(i,n) FOR(i,0,n)


/*
    GAME STATE
*/
typedef unsigned long long stateid_t;

typedef struct state
{
    char height[5], disks[5][7];
} state_t;


/* Permutations of 5 elements; used for normalization */
static char mapping[120][5],
            inv_mapping[120][5];

void generate_mappings()
{
    int n, a, b, c, d, e;

    n = 0;
    REP(a, 5)
    REP(b, 5) if(b != a)
    REP(c, 5) if(c != a && c != b)
    REP(d, 5) if(d != a && d != b && d != c)
    REP(e, 5) if(e != a && e != b && e != c && e != d)
    {
        mapping[n][0] = a; inv_mapping[n][a] = 0;
        mapping[n][1] = b; inv_mapping[n][b] = 1;
        mapping[n][2] = c; inv_mapping[n][c] = 2;
        mapping[n][3] = d; inv_mapping[n][d] = 3;
        mapping[n][4] = e; inv_mapping[n][e] = 4;
        ++n;
    }
}

stateid_t encode(state_t *s)
{
    int m;
    stateid_t best_id = (stateid_t)-1;

    REP(m, 120) {
        stateid_t id;
        char num[24];
        int c, d, n;

        n = 0;
        REP(c, 5) {
            if(c) num[n++] = 5;
            REP(d, s->height[(int) inv_mapping[m][c]])
                num[n++] = mapping[m][(int) s->disks[(int) inv_mapping[m][c]][d] ];
        }

        id = 0;
        while(n--)
            id = 6*id + num[n];

        if(id < best_id)
            best_id = id;
    }
    return best_id;
}

void decode(stateid_t id, state_t *s)
{
    int c, t;
    REP(c, 5) s->height[c] = 0;
    for(c = 0, t = 0; t < 24; ++t) {
        if(id % 6 == 5)
            ++c;
        else
            s->disks[c][(int)(s->height[c]++)] = (char)(id % 6);
        id /= 6;
    }
}

int get_succ_states(state_t *s, state_t *ts)
{
    int a, b, c, d, n;

    n = 0;

    REP(a, 5) if(s->height[a] >= 1) {
        c = s->disks[a][s->height[a]-1];
        REP(b, 5) if(a != b && s->height[b] < 7) {
            if(b == c || ( s->height[b] > 0 &&
                           s->disks[b][s->height[b]-1] == c ))
            {
                ts[n] = *s;
                --ts[n].height[a];
                ts[n].disks[b][(int)ts[n].height[b]++] = c;
                ++n;
            }
        }

        if(s->height[a] >= 2) {
            c = s->disks[a][(int)s->height[a]-2];
            d = s->disks[a][(int)s->height[a]-1];
            REP(b, 5) if(a != b && s->height[b] < 6) {
                if(b == c || ( s->height[b] >= 0 &&
                               s->disks[b][s->height[b]-1] == c ))
                {
                    ts[n] = *s;
                    ts[n].height[a] -= 2;
                    ts[n].disks[b][(int)ts[n].height[b]++] = c;
                    ts[n].disks[b][(int)ts[n].height[b]++] = d;
                    ++n;
                }
            }
        }

    }

    return n;
}


void display(state_t *s)
{
    int c, d, h = 0;

    REP(c, 5) if(s->height[c] > h) h = s->height[c];

    printf("\n");
    REP(d, h) {
        REP(c, 5) {
            if(s->height[c] < h - d)
                printf("     ");
            else
                printf(" |%d| ", 1 + s->disks[c][h-(d+1)]);
        }
        printf("\n");
    }
    REP(c, 5) printf(" === ");
    printf("\n");
    REP(c, 5) printf("  %d  ", c + 1);
    printf("\n");
}


/*
    SKIP LIST
*/
#define SL_MAX_HEIGHT 30
#define SL_P 8

typedef struct skiplist_entry
{
    stateid_t id;
    struct skiplist_entry *link[SL_MAX_HEIGHT];
} skiplist_entry_t;

typedef struct skiplist
{
    char *heap, *ptr;
    int size;
    skiplist_entry_t *start;
    int entries;
} skiplist_t;


int sl_height()
{
    int height;

    height = 1;
    while((height < SL_MAX_HEIGHT) && (rand()%SL_P == 0))
        ++height;

    return height;
}

skiplist_entry_t *sl_alloc_entry(skiplist_t *sl, int height)
{
    skiplist_entry_t *entry;

    entry = (skiplist_entry_t*)sl->ptr;
    sl->ptr += sizeof(skiplist_entry_t) -
               (SL_MAX_HEIGHT - height)*sizeof(skiplist_entry_t*);
    if(sl->ptr - sl->heap > sl->size) abort();

    return entry;
}

void sl_alloc(skiplist_t *sl, unsigned mb)
{
    int h;
    sl->size = mb*1024*1024;
    sl->heap = (char*)malloc(sl->size);
    sl->ptr = sl->heap;
    sl->entries = 0;

    if(!sl->heap)
        abort();

    sl->start = sl_alloc_entry(sl, SL_MAX_HEIGHT);
    REP(h, SL_MAX_HEIGHT) sl->start->link[h] = 0;
    sl->start->id    =  0;
}

void sl_free(skiplist_t *sl)
{
    free(sl->heap);
}

int sl_insert(skiplist_t *sl, stateid_t id)
{
    skiplist_entry_t *entry, *prev[SL_MAX_HEIGHT];
    int h;

    h = SL_MAX_HEIGHT - 1;
    entry = sl->start;
    while(h >= 0)
    {
        if(entry->id == id)
            return 0;

        prev[h] = entry;
        if(entry->link[h] == 0 || entry->link[h]->id > id)
            --h;
        else
            entry = entry->link[h];
    }

    /* Entry not found! */
    h = sl_height();
    entry = sl_alloc_entry(sl, h);
    entry->id = id;
    while(h--) {
        entry->link[h] = prev[h]->link[h];
        prev[h]->link[h] = entry;
    }
    ++sl->entries;
    return 1;
}


/*
    QUEUE
*/

typedef struct queue_entry
{
    unsigned char prio, cost;
    stateid_t   id;
} __attribute__((__packed__)) queue_entry_t;

typedef struct queue
{
    queue_entry_t *entries;
    unsigned size, max;
} __attribute__((__packed__)) queue_t;

int q_comp(queue_entry_t *a, queue_entry_t *b)
{
    if(a->prio < b->prio)
        return -1;
    else
    if(a->prio > b->prio)
        return +1;
    else
    if(a->id < b->id)
        return -1;
    else
    if(a->id > b->id)
        return +1;
    else
    return 0;
}

void q_alloc(queue_t *q, int mb)
{
    q->max = (mb*1024*1024)/sizeof(queue_entry_t);
    q->entries = (queue_entry_t*)malloc(sizeof(queue_entry_t)*q->max);
    q->size = 0;

    if(!q->entries)
        abort();
}

void q_free(queue_t *q)
{
    free(q->entries);
}

void q_insert(queue_t *q, queue_entry_t *entry)
{
    int me, parent;

    if(q->size == q->max)
        abort();

    for(me = q->size; me > 0; me = parent)
    {
        parent = me/2;
        if(q_comp(&q->entries[parent], entry) <= 0)
            break;
        q->entries[me] = q->entries[parent];
    }

    q->entries[me] = *entry;
    ++q->size;
}

void q_extract(queue_t *q, queue_entry_t *entry)
{
    int me, child;

    if(q->size == 0)
        abort();

    *entry = q->entries[0];

    --(q->size);
    for(me = 0; 2*me+1 < q->size; me = child)
    {
        child = 2*me + ((q_comp(&q->entries[2*me+1], &q->entries[2*me+2]) <= 0) ? 1 : 2);
        if(q_comp(&q->entries[q->size], &q->entries[child]) <= 0)
            break;
        q->entries[me] = q->entries[child];
    }
    q->entries[me] = q->entries[q->size];
}


unsigned short estimated_cost(state_t *s)
{
    unsigned moves = 0;
    int c, d;
    REP(c, 5) {
        REP(d, s->height[c]) if(s->disks[c][d] != c) break;

        for( ; d < s->height[c]; ++d)
        {
            if(d + 1 < s->height[c] && s->disks[c][d+1] == s->disks[c][d])
                ++d;
            ++moves;
        }
    }

    return moves;
}

void display_stats(unsigned count, skiplist_t *sl, queue_t *q)
{
    struct rusage usage;

    getrusage(RUSAGE_SELF, &usage);

    printf("Processed: %d states; User time: %d sec; System time: %d sec\n",
        count, (int)usage.ru_utime.tv_sec, (int)usage.ru_stime.tv_sec );
    printf("Skiplist: %d entries; %d MB of %d MB used (%d MB free) \n", sl->entries,
        (sl->ptr - sl->heap)/1024/1024, sl->size/1024/1024, (sl->size - (sl->ptr - sl->heap))/1024/1024 );
    printf("Queue: %d entries (%d max); %d MB of %d MB used (%d MB free) \n",
        q->size, q->max, q->size*sizeof(queue_entry_t)/1024/1024, q->max*sizeof(queue_entry_t)/1024/1024,
        (q->max - q->size)*sizeof(queue_entry_t)/1024/1024 );

}

stateid_t search_for_goal(stateid_t start_id, stateid_t goal_id)
{
    skiplist_t sl;
    queue_t q;

    state_t s;
    queue_entry_t qe;
    unsigned count;

    stateid_t result = 0;

    /* Allocate data structures */
    sl_alloc(&sl, 700);
    q_alloc(&q, 400);

    /* Insert initial state */
    decode(start_id, &s);
    qe.id    = start_id;
    qe.prio  = 0 + estimated_cost(&s);
    qe.cost  = 0;

    sl_insert(&sl, qe.id);
    q_insert(&q, &qe);

    /* A* search! */
    count = 0;
    while(q.size > 0)
    {
        /* Extract next state. */
        q_extract(&q, &qe);
        decode(qe.id, &s);
        ++count;

        /* Add successor states */
        {
            state_t ts[40];
            int ts_size, n;

            ts_size = get_succ_states(&s, ts);
            REP(n, ts_size) {
                queue_entry_t succ_qe;

                succ_qe.id = encode(&ts[n]);

                if(succ_qe.id == goal_id)
                {
                    result = qe.id;
                    goto done;
                }

                if(sl_insert(&sl, succ_qe.id))
                {
                    succ_qe.cost = qe.cost + 1;
                    succ_qe.prio = succ_qe.cost + estimated_cost(&ts[n]);
                    q_insert(&q, &succ_qe);
                }
            }
        }
    }

done:

    if(result)
        printf("Solution found at depth: %d\n", qe.cost + 1);
    display_stats(count, &sl, &q);

    sl_free(&sl);
    q_free(&q);

    return result;
}



static const stateid_t start_id = 189921320275093510ULL,
                       goal_id  = 3791192699533019760ULL;



int main()
{
    stateid_t g;
    state_t s;

    generate_mappings();

    /* Print out path to solution in reverse... */
    g = goal_id;
    do {
        g = search_for_goal(start_id, g);
        if(!g) {
            printf("No solution found!\n");
            break;
        }
        decode(g, &s);
        display(&s);
    } while(g != start_id);

    return 0;
}
