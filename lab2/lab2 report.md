# Lab 2

## 练习1：理解first-fit 连续物理内存分配算法

#### 1.`default_init`函数

```c
static void
default_init(void) {
    list_init(&free_list);
    nr_free = 0;
}
```

`default_init`是一个初始化函数，它的作用如下：

- 初始化了一个空闲链表`free_list`
- 将可用的物理页面数`nr_free`初始化为0

#### 2.`default_init_memmap`函数

```c
static void
default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }
}
```

`default_init_memmap`函数负责在内存管理中初始化页框。具体而言：

- **初始化每个物理页**
  - 函数使用循环遍历从 `base` 开始的 `n` 个页，判断是否Reserved，即页面是否被操作系统保留，仅当页面被保留时才可用。将每个页的 `flags` 和 `property` 初始化为0，表示物理页是有效的。使用 `set_page_ref(p, 0)` 将该页的引用计数设置为0，表示该页当前没有被任何引用。
- **设置基本属性**
  - 将块首页`base` 页的 `property` 设置为 `n`，记录了块所管理的页的数量。`SetPageProperty(base)`将base页标记为有效可用。
- **更新空闲页数量。**
  - `nr_free += n`
- **添加页到空闲链表**
  - 检查空闲链表 `free_list` 是否为空。如果为空，将 `base` 页添加到链表中。
  - 如果不为空，使用一个循环将 `base` 页插入到正确的位置，以保持 `free_list` 的有序性。
    - 如果 `base` 页的地址小于当前页，则将其插入当前位置之前。
    - 如果到达链表尾部（即没有小于的页），则将其添加到链表的底部。
  - 注意空闲链表中存的是各个块的base页，相当于存的空闲块。

#### 3.`default_alloc_pages`函数

```c
static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            page = p;
            break;
        }
    }
    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));
        list_del(&(page->page_link));
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}
```

`default_alloc_pages`函数的作用是从空闲链表中分配一定数量的页。

- **检查可用页**
  - 如果请求的页数 `n` 大于可用的页数 `nr_free`，则返回NULL。
- ***查找合适的空闲块***
  - 遍历空闲链表，使用 `le2page(le, page_link)` 将链表条目转换为 `struct Page *` 类型，指向当前的base页。检查当前base页的 `property` （即当前空闲块的页数）是否大于或等于请求的页数 `n`。如果是真的，将其保存到 `page` 变量中，表示找到可用页。
- **处理分配的块**
  - 如果找到合适的base页，获取当前页在链表中的前一个节点，将其存储在 `prev` 变量中。然后从链表中删除查找到的合适空闲块。如果 `page->property` 的值大于 `n`，说明块里还有未分配的部分，就要找到块里未分配的第一个页，把它设成base页，并设置其`property`为块里剩下的页数。最后将新的base页重新添加到空闲链表原来的位置。
- **更新可用页数**
  - 减少 `nr_free`的计数，表示n个页投入使用。最后清除分配出去的页中有关可用性的标记。

#### 4.`default_free_pages`函数

```c
static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;

    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }

    list_entry_t* le = list_prev(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }

    le = list_next(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}
```

`default_free_pages` 函数的主要作用是将一块已分配的块释放回空闲链表中。

- **清理要释放的页**
  - 循环遍历从 `base` 开始到 `base + n` 的页，确保当前页不是被保留的页。然后将页的 `flags` 字段重置为0，表示该页现在是可用的。将当前页的引用计数设置为0，标记其没有被使用。
- **更新页属性和可用页数**
  - 将base页所在块的页数设置为当前释放的页数 n ，然后调用 `SetPageProperty(base)`将base页的状态标记为有效且可用。由于n页被释放，可用页数+n。
- **将释放的页插入到空闲链表**
  - 和之前初始化页同理进行插入。
- **合并相邻的空闲页**
  - 获取 `base` 上一个节点，检查它是不是链表的头部。
    - 如果不是链表头部，且它和base在内存上是连续的，则将它们合并，并清除原本base的状态，合并后的块的首页成为新的base。
    - 如果是，则无法向前合并。
  - 获得 `base` 下一个节点，判断后进行向后合并，逻辑与上述相同。

#### **5.其余函数**

- **`default_nr_free_pages`函数**：返回可用页面数量。
- **`basic_check`函数**：确保内存分配和回收功能的正确性，验证页面管理中的基本功能，如页面的唯一性、引用计数以及空闲链表的维护是否如预期工作。
- **`default_check`函数**：内存管理系统的单元测试，用于验证内存分配和释放的逻辑是否正常，检查了页面的状态，特别是分配、释放、合并以及维护空闲链表的功能。

#### 6. first fit算法是否有进一步的改进空间？

`default_init_memmap`函数和`default_free_pages`函数都有插入页的操作，在用双向链表存储空闲链表的情况下遍历它寻找插入位置，其时间复杂度是`O(n)`，可以改用平衡二叉搜索树来存储，这样时间复杂度能降到`O(logn)`。



## 练习2：实现 Best-Fit 连续物理内存分配算法

Best-Fit 的核心原则是：在空闲内存块链表中，找到最小的足够大的空闲内存块进行分配，目的是尽量减少分配后的剩余内存碎片。不同分配算法的区别主要体现在分配和释放内存时的策略不同，因此，Best-Fit只需要在First-Fit算法的基础上进行一些修改。

### Best-Fit算法的实现

#### 1.`best_fit_init_memmap`函数

内存初始化的过程与具体的分配算法无关，`init_memmap` 的功能是将一段连续的物理内存初始化为“可用”状态，即标记这些页为可分配的内存。进行的工作如：清空当前页框的标志和属性信息、将页框的引用计数设置为0、将页块加入空闲链表等，这些并不涉及内存分配策略，无论是First-Fit还是Best-Fit都是一样的。代码中的空缺仿照` default_init_memmap`函数填即可。

#### 2.`best_fit_alloc_pages`函数

**遍历空闲链表寻找最佳匹配**：
在First Fit算法中，代码会找到第一个满足大小需求的空闲页块并返回。而在Best Fit算法中，遍历链表时，除了判断页块的大小是否满足需求，还需要记录当前最小的空闲页块。

```c
while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) { // 找到大小 >= n 的块
            if (p->property < min_size) { // Best Fit: 记录当前找到的最小块
                min_size = p->property;
                page = p;
                if (min_size == n) { // 如果找到恰好合适的块，立即退出
                    break;
                }
            }
        }
    }
```

- 在 Best-Fit 中引入一个 `min_size` 变量，用于记录找到的最小合适块的大小。`min_size` 初始值设为一个较大的数（ `nr_free + 1`），确保第一次找到的块会更新为最小值。

- 每次找到一个大小符合要求的块后，会判断其大小是否比当前记录的最小块小。如果是，则更新最小块和 `min_size`。如果找到一个恰好满足需求的块（`min_size == n`），就可以提前退出循环，因为这是最优选择。

- 在 First Fit 中，找到第一个满足需求的块就可以立即分配，而 Best Fit 要继续遍历整个链表，直到找到最小的满足条件的块。

#### 3.`best_fit_free_pages`函数

在 `best_fit_free_pages` 中，内存释放时逻辑与 First-Fit 基本相同。释放过程中，同样会尝试合并相邻的空闲块，避免产生内存碎片。代码中需要的填空方法同样仿照`default_free_pages`即可。

### 物理内存的分配和释放

#### 分配过程：

1. **初始化**：
   在`best_fit_init_memmap`函数中，将一段内存标记为空闲。代码初始化每个页面的属性和引用计数，设置最初的空闲页块大小，并插入空闲链表。

2. **分配内存**：
   在`best_fit_alloc_pages`函数中，遍历空闲链表，找到最适合的页块（即刚好比所需大小稍大的块），并进行分配。如果找到的空闲块比需求大，分割空闲块。更新链表，减少`nr_free`的空闲页数。

#### 释放过程：

1. **释放内存**：
   在`best_fit_free_pages`函数中，释放指定的页面块，并将其重新插入到空闲链表中。

2. **合并空闲块**：
   如果释放的页面块与相邻的空闲块连续，将这些块合并为一个更大的块，以减少碎片化并提高内存利用率。

### 改进空间

Best-Fit 虽然比 First-Fit 更高效地利用了内存，但它可能会导致较小的碎片散落在内存中，尤其是随着更多的小块被分配后，内存空间会逐渐变得零散，难以容纳较大的分配请求。

#### 改进方案：

- **设定最小分割阈值**：当分割块时，如果剩余的块大小过小（如小于一定的最小阈值），则不再分割它。这样可以避免产生许多小块碎片。具体代码可以类似于：

  ```c
  if (page->property > n && page->property - n >= MIN_BLOCK_SIZE) {
      struct Page *p = page + n;
      p->property = page->property - n;
      SetPageProperty(p);
      list_add(prev, &(p->page_link));
  }
  ```

  这里的 `MIN_BLOCK_SIZE` 可以设置为系统中的一个合理阈值，如 4KB 或 8KB，确保在分割时不会产生太小的碎片块。

- **碎片合并**：为了进一步优化，可以定期对空闲块进行整理，即主动合并一些较小的空闲块，形成较大的空闲块，减少内存碎片。

  

## 扩展练习Challenge：硬件可用物理内存范围的获取方法

如果 OS 无法提前知道当前硬件的可用物理内存范围，可用下列办法让 OS 获取可用物理内存范围：

- #### **`BIOS/ UEFI` 访问**

  -  `BIOS`（基本输入输出系统）是一种固件，它在计算机启动时加载并初始化硬件组件，并在此后将控制权转交给操作系统。`UEFI`（统一可扩展固件接口）是 `BIOS` 的继任者，具有更强的扩展性。
  - 在系统启动时，操作系统可以访问 `BIOS` 或 `UEFI` 提供的信息。这些固件常常会提供内存映射的相关信息，包括已经分配的内存和可用的内存区域。操作系统可以通过系统调用来获取这些信息。

- #### `ACPI`

  - `ACPI`（高级配置与电源接口）是一种开放标准，允许操作系统直接控制计算机的电源管理和硬件配置。
  - `ACPI` 规范包含关于系统硬件的信息，包括可用内存区域。操作系统可以通过访问 `ACPI` 表来获取系统的内存配置细节。`ACPI` 通常通过在启动时读取特定内存地址来提供这些信息。

- #### `MMIO`

  - `MMIO`是一种将 I/O 设备的寄存器映射到系统的内存地址空间的方法。当 I/O 设备的控制寄存器和内存 buffers 映射到内存地址时，操作系统和应用程序可以通过读写这些内存地址来与设备进行交互。
  - 通过访问映射到设备的特定内存地址，操作系统能够判断这些地址是否被设备占用。比如操作系统可以尝试读取或写入 `MMIO` 地址，如果访问成功且响应正确，则表明对应的设备是可用的。如果没有响应或发生错误，说明没有占用这些范围。没有被设备占用的区域就是可用的。

- #### 使用内存测试工具

  - 在系统启动过程中，操作系统可以调用特定的内存测试程序（如 `MemTest86` 等）来扫描内存，识别出可用和无效的内存区域，从而让 OS 获取可用物理内存范围。

