/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/vaddr.h"
#include "lib/kernel/bitmap.h"
#include "threads/mmu.h"
#include "string.h"
#define SECTOR_PER_PAGE (PGSIZE / DISK_SECTOR_SIZE)
/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);
struct bitmap *swapmap;
struct lock swaplock;

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1,1);
	ASSERT(swap_disk != NULL);
	swapmap = bitmap_create(disk_size(swap_disk) / SECTOR_PER_PAGE);
	ASSERT(swapmap != NULL);
	lock_init(&swaplock);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	// struct uninit_page *uninit = &page->uninit;
	// memset(uninit, 0, sizeof(struct uninit_page));
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->pageno = BITMAP_ERROR;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	lock_acquire(&swaplock);
	if (bitmap_test(swapmap, anon_page->pageno) == false){
		lock_release(&swaplock);
        PANIC("(anon swap in) Frame not stored in the swap slot!\n");
	}
	for(int i = 0;i<SECTOR_PER_PAGE;i++){
		disk_read(swap_disk,(anon_page->pageno*SECTOR_PER_PAGE) + i, kva + (DISK_SECTOR_SIZE* i) );
	}
	bitmap_set(swapmap, anon_page->pageno, false);
	pml4_set_page(thread_current()->pml4, page->va, kva, true);
	anon_page->pageno = BITMAP_ERROR;
	lock_release(&swaplock);
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	lock_acquire(&swaplock);
	size_t pageno = bitmap_scan_and_flip(swapmap,0,1,false);
	if(pageno == BITMAP_ERROR){
		lock_release(&swaplock);
		PANIC("no slot! recent\n");
		return false;
	}
	for(int i = 0;i<SECTOR_PER_PAGE;i++){
		disk_write(swap_disk,pageno*SECTOR_PER_PAGE + i,page->va + DISK_SECTOR_SIZE * i);
	}
	pml4_clear_page(thread_current()->pml4, page->va);
	anon_page->pageno = pageno;
	page->frame->page = NULL;
	page->frame = NULL;
	lock_release(&swaplock);
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	if(anon_page->pageno != BITMAP_ERROR){
		bitmap_set(swapmap, anon_page->pageno, false);
	}
	if (page->frame) {
		lock_acquire(&swaplock);
        list_remove(&page->frame->ft_elem);
		lock_release(&swaplock);
        page->frame->page = NULL;
		page->frame = NULL;
        palloc_free_page(page->frame);
    }
}
