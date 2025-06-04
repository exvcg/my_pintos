/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
static bool lazy_load_segment_f(struct page *page, void *aux);
/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	struct load_arg *aus = (struct load_arg *)page->uninit.aux;
	file_page->file = aus->file;
	file_page->ofs = aus->offset;
	file_page->read_b = aus->read_b;
	file_page->zero_b = aus->zero_b;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
    if (pml4_is_dirty(thread_current()->pml4, page->va))
    {
        file_write_at(file_page->file, page->va, file_page->read_b, file_page->ofs);
        pml4_set_dirty(thread_current()->pml4, page->va, 0);
    }
    pml4_clear_page(thread_current()->pml4, page->va);
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	struct file *mfile = file_reopen(file);
	void * ori = addr;
	int pg;
	if(length<PGSIZE){
		pg = 1;
	}else if(length % PGSIZE == 0){
		pg = (length/PGSIZE);
	}else{
		pg = (length/PGSIZE)+1;
	}
	int ct = 1;
	while(length>0){
		size_t page_read_bytes = length < PGSIZE ? length : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		struct load_arg *aux = (struct load_arg *)malloc(sizeof(struct load_arg));
		if (!aux)
            return false;
		aux->file = mfile;
		aux->offset = offset;
		aux->read_b = page_read_bytes;
		aux->zero_b = page_zero_bytes;
		aux->enablerw = writable;
		if(!vm_alloc_page_with_initializer(VM_FILE,addr,writable,lazy_load_segment_f,aux)){
			free(aux);
			return false;
		}
		struct page * cp = spt_find_page(&thread_current()->spt,addr);
		cp->fbtc = pg;
		cp->fbcc = ct;
		length -= page_read_bytes;
		offset += page_read_bytes;
		addr += PGSIZE;
		ct += 1;
	}
	return ori;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct page * tp = spt_find_page(&thread_current()->spt,addr);
	if(tp->fbcc != 1){
		return ;
	}
	int cb = tp->fbtc;
	for(int i = 0;i<cb;i++){
		struct file_page *file_page UNUSED = &tp->file;
	//file_backed_destroy 전에 뭔가 기록했는지 확인하고
	if(tp)
		destroy(tp);
	addr += PGSIZE;
	tp = spt_find_page(&thread_current()->spt,addr);
	}
}
static bool
lazy_load_segment_f(struct page *page, void *aux) {
	ASSERT(page -> frame -> kva != NULL);
	struct load_arg* load = (struct load_arg *)aux;

	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	// file_seek(load->file,load->offset);
	size_t read_b = load->read_b;
	size_t zero_b = load->zero_b;
	if (file_read_at(load-> file, page -> frame -> kva, read_b,load->offset) != (int)read_b) {
            palloc_free_page(page->frame->kva);
            return false;
    }
	memset (page -> frame -> kva + read_b, 0, zero_b);

    return true;

}
