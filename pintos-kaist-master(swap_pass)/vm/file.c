/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/syscall.h"

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
	file_page->writable = aus->enablerw;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	/** Project 3-Swap In/Out */
	int read = file_read_at(file_page->file, page->frame->kva, file_page->read_b, file_page->ofs);
	memset(page->frame->kva + read, 0, PGSIZE - read);
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	/** Project 3-Swap In/Out */
	struct frame *frame = page->frame;
	struct load_arg* container = (struct load_arg *)page->uninit.aux;
	if (pml4_is_dirty(thread_current()->pml4, page->va))
	{
		file_write_at(container->file, page->va, container->read_b, container->offset);
		pml4_set_dirty(thread_current()->pml4, page->va, false);
	}
	page->frame->page = NULL;
	page->frame = NULL;
	pml4_clear_page(thread_current()->pml4, page->va);
	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	
    if (pml4_is_dirty(thread_current()->pml4, page->va))
    {
        file_write_at(file_page->file, page->va, PGSIZE-file_page->zero_b, file_page->ofs);
        pml4_set_dirty(thread_current()->pml4, page->va, 0);
    }
	 if (page->frame) {
        list_remove(&page->frame->ft_elem);
        page->frame->page = NULL;
        page->frame = NULL;
        free(page->frame);
    }
    pml4_clear_page(thread_current()->pml4, page->va);
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	struct file *mfile = file_reopen(file);
	void* ori = addr;
	int pg = length <= PGSIZE ? 1 : length % PGSIZE ? length / PGSIZE + 1: length / PGSIZE; 
	int ct = 1;
	size_t read_bytes = file_length(mfile)<length ? file_length(mfile) : length;
	size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;
	if ((read_bytes + zero_bytes) % PGSIZE != 0) {
		return NULL;
	}
	//upage가 PGSIZE align되어있는지 확인
	if (pg_ofs(addr) != 0) {
		return NULL;
	}
	//ofs가 PGSIZE align 되어있는지 확인
	if (offset % PGSIZE != 0) {
		return NULL;
	}
	while(read_bytes>0||zero_bytes>0){
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		struct load_arg *aux = malloc(sizeof(struct load_arg));
		if (!aux)
            return NULL;
		aux->file = mfile;
		aux->offset = offset;
		aux->read_b = page_read_bytes;
		aux->zero_b = page_zero_bytes;
		aux->enablerw = writable;
		if(!vm_alloc_page_with_initializer(VM_FILE,addr,writable,lazy_load_segment_f,aux)){
			// free(aux);
			return NULL;
		}
		
		struct page *cp = spt_find_page(&thread_current()->spt, ori);
		cp->fbtc = pg;
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		offset += page_read_bytes;
		addr += PGSIZE;
	}
	return ori;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct page * tp = spt_find_page(&thread_current()->spt,addr);
	int cb = tp->fbtc;
	for(int i = 0;i<cb;i++){
		// struct file_page *file_page UNUSED = &tp->file;
	//file_backed_destroy 전에 뭔가 기록했는지 확인하고
	if(tp)
		spt_remove_page(&thread_current()->spt,tp);
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
	file_seek(load->file,load->offset);
	size_t read_b = load->read_b;
	size_t zero_b = load->zero_b;
	if (file_read(load-> file, page -> frame -> kva, read_b) != (int)read_b) {
            // palloc_free_page(page->frame->kva);
            return false;
    }
	memset (page -> frame -> kva + read_b, 0, zero_b);
	file_seek(load->file,load->offset);
    return true;

}
