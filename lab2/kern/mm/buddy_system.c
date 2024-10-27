#include <pmm.h>
#include <buddy_system.h>

extern free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

struct buddy
{
    /* data */
    size_t size;
    uintptr_t* longest;
    size_t manage_page_used;
    size_t free_size;
    struct Page* begin_page;
}buddy;

static size_t up_power_of_2(size_t n){
    int n = 0, tmp = size;
    while (tmp >>= 1)
    {
        n++;
    }
    n += 1;
    return (1 << n);
};


static void buddy_init(){
    list_init(&free_list);
    nr_free = 0;
}

static void buddy_init_memmp(struct Page* base,size_t n){
    assert(n > 0);


    size_t real_need_size = up_power_of_2(n);

    buddy.begin_page = base;

    if(n<512){
        buddy.manage_page_used = 1;
    } else{
        buddy.manage_page_used = (real_need_size*sizeof(uintptr_t)*2+PGSIZE - 1)/PGSIZE;
    }

    struct Page* page = buddy.begin_page;

    for (; page != buddy.begin_page + n; page++) {
        assert(PageReserved(page));
        // 清空当前页框的标志和属性信息，并将页框的引用计数设置为0
        page->flags = page->property = 0;
        set_page_ref(page, 0);
    }

    buddy.size = real_need_size;

    buddy.free_size = real_need_size;

    buddy.longest = (uintptr_t*)(base + real_need_size);         //结尾放二叉树结构

    base->property = n;

    
    size_t node_size = real_need_size*2;

    for(int i=0;i<2*real_need_size-1;i++){
        if(IS_POWER_OF_2(i+1)){
            node_size /= 2;
        }
        buddy.longest[i] = node_size;
    }

    nr_free += n;
    
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    if(n>nr_free || buddy.longest[0]<n){
        return NULL;
    }

   
    size_t real_apply_size;
    size_t offset = 0;

    if(!IS_POWER_OF_2(n)){
        real_apply_size = up_power_of_2(n); 
    }else{
        real_apply_size = n;
    }


    size_t index = 0;


    int node_size;

    for(node_size = buddy.size ; node_size!= real_apply_size;node_size/2){
        if(buddy.longest[LEFT_LEAF(index)] >= real_apply_size){
            index = LEFT_LEAF(index);
        }else{
            index = RIGHT_LEAF(index);
        }
    }

    buddy.longest[index] = 0;

    offset = (index+1)*node_size - buddy.size;

    while(index){
        index = PARENT(index);
        buddy.longest[index] = MAX(buddy.longest[LEFT_LEAF(index)],buddy.longest[RIGHT_LEAF(index)]);
    }



    struct Page *page = NULL;
    struct Page* base_page = buddy.begin_page + offset;

    for(page = buddy.begin_page + offset ;page!=buddy.begin_page + real_apply_size;page++)
    {
        ClearPageProperty(page);
    }

    nr_free -= real_apply_size;

    base_page->property = real_apply_size;

    buddy.free_size -= real_apply_size;

    return base_page;
    
}


static void
buddy_free_pages(struct Page *base, size_t n) {
    
    assert(n > 0);

    if(!IS_POWER_OF_2(n)){
        n = up_power_of_2(n);
    }

    size_t node_size = 1;
    size_t index = 0;
    size_t left_longest,right_longest;

    size_t offset = base - buddy.begin_page;

    index = offset + buddy.size - 1;

    while (node_size != n)
    {
        node_size *=2;
        index = PARENT(index);
        if(index == 0){
            return;
        }
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
buddy_nr_free_pages(void) {        //返回剩余的空闲的页数
    return nr_free;
}

static void buddy_check(){


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
        .init_memmap = buddy_init_memmp,
        .alloc_pages = buddy_alloc_pages,
        .free_pages = buddy_free_pages,
        .nr_free_pages = buddy_nr_free_pages,
        .check = buddy_check,
};
