#include <pmm.h>
#include <buddy_system.h>


extern free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

struct Buddy
{
    unsigned *longest;
    struct Page *begin_page;
    unsigned size;

} buddy;


static size_t up_power_of_2(size_t n){
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n+1;
}

static void buddy_init(void)
{
    list_init(&free_list);
    nr_free = 0;
}

static void buddy_init_memmap(struct Page *base, size_t n)
{
    //base is a virtual page,indicating the beginning of pages
    assert(n > 0);
    size_t real_need_size = up_power_of_2(n);

    struct Page *p = base;
    for (; p != base + n; p++)
    {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    
    base->property = n; // 从base开始有n个可用页
    p = base + n; 

    buddy.begin_page = base;
    buddy.longest = (unsigned *)p;


    buddy.size = real_need_size;

    unsigned node_size = 2 * real_need_size;

    for (int i = 0; i <2 * real_need_size - 1; ++i)
    {
        if (IS_POWER_OF_2(i + 1))
        {
            node_size /= 2;
        }
        buddy.longest[i] = node_size;

    }
    nr_free += n;
}

static struct Page *buddy_alloc_pages(size_t n)
{
    assert(n > 0);
    unsigned index = 0;
    
    if (n > nr_free || buddy.longest[index] < n)
    {
        return NULL;
    }
    
    unsigned node_size;
    unsigned offset = 0;

    size_t real_alloc = n;

    if (!IS_POWER_OF_2(n))
    {
        real_alloc = up_power_of_2(n);
    }
    
    for (node_size = buddy.size; node_size != real_alloc; node_size /= 2)
    {
        
        if (buddy.longest[LEFT_LEAF(index)] >= real_alloc)
            index = LEFT_LEAF(index);
        else{
            index = RIGHT_LEAF(index);
        }
    }

    

    buddy.longest[index] = 0;

    offset = (index + 1) * node_size - buddy.size;

    while(index){
        index = PARENT(index);
        buddy.longest[index] = MAX(buddy.longest[LEFT_LEAF(index)],buddy.longest[RIGHT_LEAF(index)]);
    }



    struct Page *base_page = buddy.begin_page + offset;
    struct Page *page;

    // 将每一个取出的块由空闲态改为保留态
    for (page = base_page; page != base_page + real_alloc ; page++)
    {
        ClearPageProperty(page);
    }

    base_page->property = real_alloc;  //用n来保存分配的页数，n为2的幂
    nr_free -= real_alloc;
    return base_page;
}

static void buddy_free_pages(struct Page *base, size_t n)
{
    assert(n > 0);
    unsigned node_size, index = 0;
    unsigned left_longest, right_longest;
    
    int total_page = n;

    if (!IS_POWER_OF_2(n))
    {
        n = up_power_of_2(n);
    }

    int offset = (base - buddy.begin_page);
    node_size = 1;
    index = buddy.size + offset - 1;

    while (node_size != n)
    {
        node_size *= 2;
        index = PARENT(index);
        if (index == 0)
            return;
    }

    buddy.longest[index] = node_size;


    while(index){
        index = PARENT(index);
        node_size *=2;

        left_longest = buddy.longest[LEFT_LEAF(index)];
        right_longest = buddy.longest[RIGHT_LEAF(index)];

        if(left_longest + right_longest == node_size){
            buddy.longest[index] = node_size;

        }else{
            buddy.longest[index] = MAX(left_longest,right_longest);
        }

    }
    
    nr_free+=n;
    
}

static size_t
buddy_nr_free_pages(void) {
    return nr_free;
}


static void
buddy_check(void) {
    
    size_t total_page = buddy_nr_free_pages();
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);       //测试不超出界限
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    assert(p1 == p0 + 1);       //测试连续分页

    buddy_free_pages(p0,1);
    buddy_free_pages(p1,1);
    buddy_free_pages(p2,1);

    assert(buddy_nr_free_pages() == total_page );

    struct Page* p4 = NULL;
    p4 = buddy_alloc_pages(256);
    assert(buddy_nr_free_pages() == ( total_page -256));

    struct Page* p5 = NULL;
    p5 = buddy_alloc_pages(256);
    assert(buddy_nr_free_pages() == ( total_page -2*256));

    buddy_free_pages(p4,256);
    buddy_free_pages(p5,256);

    
    assert(buddy_nr_free_pages() == total_page );

    
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
