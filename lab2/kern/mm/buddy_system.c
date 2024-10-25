#include <pmm.h>
#include <buddy_system.h>


free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

struct buddy
{
    /* data */
    size_t size;
    uintptr_t* longest;
    size_t manage_page_used;
    size_t total_size;
    size_t free_size;
    struct Page* begin_page;
};

struct buddy multi_buddy[MAX_SIZE];
int used_buddy_num = 0;

static size_t up_power_of_2(size_t n){
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n+1;
}


static void buddy_init(){
    list_init(&free_list);
    nr_free = 0;
}

static void buddy_init_memmp(struct Page* base,size_t n){
    assert(n > 0);
    cprintf("n: %d\n", n);
    
    

    struct buddy* buddy = &(multi_buddy[used_buddy_num++]);

    size_t real_need_size = up_power_of_2(n);

    buddy->size = real_need_size;
    buddy->free_size = real_need_size;
    buddy->longest = page2pa(base);         //开头放二叉树结构


    if(n<512){
        buddy->manage_page_used = 1;
        buddy->begin_page = page2pa(PADDR(ROUNDUP(
                buddy->longest+512*2*sizeof(uintptr_t),PGSIZE               //不足512页按512页算
                                        )));

    } else{
        buddy->manage_page_used = real_need_size*sizeof(uintptr_t)*2/PGSIZE;
        buddy->begin_page = page2pa(PADDR(ROUNDUP(
                buddy->longest+512*2*sizeof(uintptr_t),PGSIZE
                                        )));

    }
    
    size_t node_size = real_need_size*2;

    for(int i=0;i<2*real_need_size-1;i++){
        if(IS_POWER_OF_2(i+1)){
            node_size/2;
        }
        buddy->longest[i] = node_size;
    }

    struct Page *p = buddy->begin_page;
    for (; p != base + buddy->free_size; p ++) {
        assert(PageReserved(p));
        // 清空当前页框的标志和属性信息，并将页框的引用计数设置为0
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }

    struct Page *page = NULL;
    list_entry_t *le = &free_list;

    size_t real_apply_size;
    size_t offset = 0;

    if(!IS_POWER_OF_2(n)){
        real_apply_size = up_power_of_2(n); 
    }else{
        real_apply_size = n;
    }

    size_t index = 0;

    struct buddy* buddy = NULL;
    for(int i=0;i<MAX_SIZE;i++){
        if(multi_buddy[i].longest[index]>=n){
            buddy = &multi_buddy[i];
            break;
        }
    }
    
    if(!buddy){
        return NULL;
    }

    int node_size;
    for(node_size = buddy->size ; node_size!= real_apply_size;node_size/2){
        if(buddy->longest[LEFT_LEAF(index)] >= n){
            index = LEFT_LEAF(index);
        }else{
            index = RIGHT_LEAF(index);
        }
    }

    buddy->longest[index] = 0;
    offset = (index+1)*node_size - buddy->size;

    while(index){
        index = PARENT(index);
        buddy->longest[index] = MAX(buddy->longest[LEFT_LEAF(index)],buddy->longest[RIGHT_LEAF(index)]);
    }

    buddy->free_size -= real_apply_size;
    return buddy->begin_page + offset;
    
}


static void
buddy_free_pages(struct Page *base, size_t n) {
    
    assert(n > 0);

    struct buddy* self = NULL;
    for(int i=0;i<MAX_SIZE;i++){
        struct buddy* this_buddy = &multi_buddy[i];
        if(base >= this_buddy->begin_page && base < this_buddy->begin_page + this_buddy->size){
            self = this_buddy;
        }
    }

    if(!self){
        return NULL;
    }


    size_t node_size = 1;
    size_t index = 0;
    size_t left_longest,right_longest;

    size_t offset = base - self->begin_page;

    index = offset + self->size - 1;
    for(;self->longest[index] ;index = PARENT(index)){
        node_size *=2;
        if(index == 0){
            return;
        }
    }
    
    self->longest[index] = node_size;

    while(index){
        index = PARENT(index);
        node_size *=2;

        left_longest = self->longest[LEFT_LEAF(index)];
        right_longest = self->longest[RIGHT_LEAF(index)];

        if(left_longest + right_longest == node_size){
            self->longest[index] = node_size;

        }else{
            self->longest[index] = MAX(left_longest,right_longest);
        }

    }

}


static size_t
buddy_nr_free_pages(void) {        //返回剩余的空闲的页数
    
    for(int i=0;i<MAX_SIZE; i++){
        nr_free += multi_buddy[i].free_size;
    }

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

    assert(buddy_nr_free_pages() == total_page )

    struct Page* p4 = NULL;
    p4 = buddy_alloc_pages(256);
    assert(buddy_nr_free_pages() == total_page -256);

    struct Page* p5 = NULL;
    p5 = buddy_alloc_pages(256);
    assert(buddy_nr_free_pages() = total_page -2*256);

    buddy_alloc_pages(p4,256);
    buddy_alloc_pages(p4,256);

    
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