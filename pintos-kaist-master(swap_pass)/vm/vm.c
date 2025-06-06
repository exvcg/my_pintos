/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
struct list framelist;
struct lock vlock;
struct list_elem *next = NULL;
/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&framelist);
	lock_init(&vlock);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *np = malloc(sizeof(struct page));
		if(!np){
			return false;
		}
		switch(VM_TYPE(type)){
			case VM_ANON:
				uninit_new(np,upage,init,type,aux,anon_initializer);
				break;
			case VM_FILE:
				uninit_new(np,upage,init,type,aux,file_backed_initializer);
				break;
			default:
				free(np);
				return(false);
		}
		np->writable=writable;
		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt,np);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/* TODO: Fill this function. */
	struct page *mp = malloc(sizeof(struct page));
	mp->va = pg_round_down(va);
	struct hash_elem *de = hash_find(&spt->sup_table,&mp->he);
	free(mp);
	return de != NULL ? hash_entry (de, struct page, he) : NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	
	/* TODO: Fill this function. */
	// struct hash_elem *result = 
	if(!hash_insert(&spt->sup_table,&page->he)){
			return true;
	} else {
		return false;
	}
	
	// if(result == NULL){
	// 	return true;
	// }else{
	// 	return false;
	// }
	
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete(&spt->sup_table,&page->he);
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	/** Project 3-Swap In/Out */
	lock_acquire(&vlock);
	for (next = list_begin(&framelist); next != list_end(&framelist); next = list_next(next))
	{
		victim = list_entry(next, struct frame, ft_elem);
		if (pml4_is_accessed(thread_current()->pml4, victim->page->va))
			pml4_set_accessed(thread_current()->pml4, victim->page->va, false);
		else{
			lock_release(&vlock);
			return victim;
		}
	}
	lock_release(&vlock);
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	if (victim->page != NULL){
		swap_out(victim->page);
	}
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = malloc(sizeof(struct frame));
	// if (frame == NULL || frame -> kva == NULL) {
	// 	PANIC("todo");
	// 	return NULL;
	// }
	ASSERT (frame != NULL);
	/* TODO: Fill this function. */
	frame->kva = palloc_get_page(PAL_USER|PAL_ZERO);
	if(frame->kva == NULL){
		frame = vm_evict_frame();
	}else{
	list_push_back (&framelist, &frame->ft_elem);
	}
	frame->page = NULL;
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	addr = pg_round_down(addr);
	bool success = false;
	if(vm_alloc_page(VM_ANON|VM_MARKER_0, addr, 1)){	
		success = vm_claim_page(addr);

        if (success) {
            thread_current()->stack_bottom -= PGSIZE;
        }	
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
		struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
		
	/** Project 3-Anonymous Page */
	struct page *page = NULL;
   
    if (addr == NULL || is_kernel_vaddr(addr))
        return false;
	void *rsp = f->rsp; // user access인 경우 rsp는 유저 stack을 가리킨다.
    if (!user)            // kernel access인 경우 thread에서 rsp를 가져와야 한다.
            rsp = thread_current()->rsp;
    if (not_present)
    {
		void *rsp = f->rsp; // user access인 경우 rsp는 유저 stack을 가리킨다.
        if (!user)            // kernel access인 경우 thread에서 rsp를 가져와야 한다.
            rsp = thread_current()->rsp;
		if(USER_STACK - (1<<20) <= rsp-8&&addr <= USER_STACK && addr == rsp-8){
			vm_stack_growth(addr);
			return true;
		}else if(USER_STACK - (1<<20) <= rsp&&addr <= USER_STACK && (addr >= rsp)){
			vm_stack_growth(addr);
			return true;
		}
        page = spt_find_page(spt, addr);
			
        if (page == NULL)
            return false;
        if (write == 1 && page->writable == 0)
            return false;
        return vm_do_claim_page(page);
    }
    return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, va);

	if (page == NULL) {
		return false;
	}
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	
	if (!page || page->frame)
		return false;
	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	pml4_set_page (thread_current() -> pml4, page -> va, frame->kva, page -> writable);
	
	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->sup_table,hash_page,hash_addr_comp,NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
		struct hash_iterator i;
		hash_first (&i, &src->sup_table);
		while (hash_next (&i)) {
			struct page *fsp = hash_entry (hash_cur(&i), struct page, he);
			enum vm_type tp = fsp->operations->type;
			if(tp == VM_UNINIT){
				struct uninit_page *uninit = &fsp->uninit;
				struct load_arg *unin = uninit->aux;
				
				vm_alloc_page_with_initializer(uninit->type,fsp->va,fsp->writable,uninit->init,unin);
				continue;
				}
			else if (tp == VM_FILE)
			{
				struct load_arg *nx = malloc(sizeof(struct load_arg));
				nx->file = fsp->file.file;
				nx->offset = fsp->file.ofs;
				nx->read_b = fsp->file.read_b;
				nx->zero_b = fsp->file.zero_b;
				if (!vm_alloc_page_with_initializer(tp, fsp->va, fsp->writable, NULL, nx))
                	return false;
            	struct page *file_page = spt_find_page(dst, fsp->va);
				file_backed_initializer(file_page,tp,NULL);
				// if(!vm_do_claim_page(file_page))
				// 	return false;
				file_page->frame = fsp->frame;
            	pml4_set_page(thread_current()->pml4, file_page->va, fsp->frame->kva, fsp->writable);
				continue;
			}
			
		if (!vm_alloc_page(tp, fsp->va, fsp->writable)) {
			return false;
		}
		if (!vm_claim_page(fsp->va)) {
			return false;
		}

		struct page *dst_page = spt_find_page(dst, fsp->va);
		memcpy(dst_page->frame->kva, fsp->frame->kva, PGSIZE);
			
		}
		return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->sup_table,del_hash_page);
}
uint64_t hash_page(const struct hash_elem *e, void *aux){
	struct page *cp = hash_entry(e,struct page,he);
	return hash_bytes(&cp->va,sizeof(cp->va)); 
}
bool hash_addr_comp(const struct hash_elem *a, const struct hash_elem *b, void *aux){
	struct page *pa = hash_entry(a,struct page,he);
	struct page *pb = hash_entry(b,struct page,he);
	return pa->va < pb->va;
}
void del_hash_page(struct hash_elem *e, void *aux){
	struct page *dp = hash_entry(e,struct page,he);
	destroy (dp);
	free (dp);
}
bool page_delete (struct hash *h, struct page *p) {
  return hash_delete (&h, &p->he);
}