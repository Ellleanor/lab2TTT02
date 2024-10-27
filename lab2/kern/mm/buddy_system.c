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
    
    struct Page *p0, *p1, *p2, *p3, *p4;
    p0 = p1 = p2 = p3 = p4 = NULL;

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);
    assert(p1==p0+1 && p2 == p1+1);  //页面地址关系

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    free_page(p0);    
    free_page(p1);
    free_page(p2);
    
    p1 = alloc_pages(512); //p1应该指向最开始的512个页
    p2 = alloc_pages(512);
    p3 = alloc_pages(1024);

    assert(p3 - p2 == p2 - p1);//检查相邻关系

    free_pages(p1, 256);
    free_pages(p2, 512);
    free_pages(p1 + 256, 256);
    free_pages(p3,1024);
    //检验释放页时，相邻内存的合并

    p0 = alloc_pages(8192);

    assert(p0 == p1); //重新分配，p0也指向最开始的页

    p1 = alloc_pages(128);
    p2 = alloc_pages(64);
    

    assert(p1 + 128 == p2);// 检查是否相邻

    p3 = alloc_pages(128);


    //检查p3和p1是否重叠
    assert(p1 + 256 == p3);
    
    //释放p1
    free_pages(p1, 128);

    p4 = alloc_pages(64);
    assert(p4 + 128 == p2);
    // 检查p4是否能够使用p1刚刚释放的内存

    free_pages(p3, 128);
    p3 = alloc_pages(64);

    // 检查p3是否在p2、p4之间
    assert(p3 == p4 + 64 && p3 == p2 - 64);
    free_pages(p2, 64);
    free_pages(p4, 64);
    free_pages(p3, 64);
    // 全部释放
    free_pages(p0, 8192);
    
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
