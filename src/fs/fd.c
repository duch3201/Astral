#include <kernel/fd.h>
#include <arch/spinlock.h>
#include <stdbool.h>
#include <kernel/alloc.h>
#include <string.h>

// XXX limit the max fd number

int fd_release(fd_t* fd){	
	spinlock_release(&fd->lock);
	return 0;
}

int fd_access(fdtable_t* fdtable, fd_t** fd, int ifd){
	spinlock_acquire(&fdtable->lock);

	if(ifd >= fdtable->fdcount || fdtable->fd[ifd] == NULL || fdtable->fd[ifd]->node == NULL){
		spinlock_release(&fdtable->lock);
		return EBADF;
	}

	*fd = fdtable->fd[ifd];
	fd_t* efd = *fd;
	spinlock_acquire(&efd->lock);
	spinlock_release(&fdtable->lock);

	return 0;

}

int fd_alloc(fdtable_t* fdtable, fd_t** fd, int* ifd){
	spinlock_acquire(&fdtable->lock);
	
	// find fd to use
	
	bool ok = false;

	for(uintmax_t i = fdtable->firstfreefd; i < fdtable->fdcount; ++i){
		if(!fdtable->fd[i]){
			fdtable->firstfreefd = i;
			*ifd = i;
			ok = true;
			break;
		}
	}

	// resize table

	if(!ok){
		if(fdtable->fdcount == MAX_FD){
			spinlock_release(&fdtable->lock);
			return EMFILE;
		}
		fd_t** tmp = realloc(fdtable->fd, sizeof(fd_t*)*(fdtable->fdcount + 1));
		
		if(!tmp){
			spinlock_release(&fdtable->lock);
			return ENOMEM;
		}	

		fdtable->fd = tmp;
		*ifd = fdtable->fdcount;
		++fdtable->fdcount;
		
	}
	
	fd_t* tmp = alloc(sizeof(fd_t));
	
	if(!tmp){
		spinlock_release(&fdtable->lock);
		return ENOMEM;
	}	

	spinlock_acquire(&tmp->lock);
	tmp->refcount = 1;


	fdtable->fd[*ifd] = tmp;

	spinlock_release(&fdtable->lock);

	*fd = tmp;

	return 0;

}

int fd_free(fdtable_t* fdtable, int ifd){
	spinlock_acquire(&fdtable->lock);	

	if(ifd >= fdtable->fdcount || fdtable->fd[ifd] == NULL){
		spinlock_release(&fdtable->lock);
		return EBADF;
	}

	fd_t* fd = fdtable->fd[ifd];
	spinlock_acquire(&fd->lock);

	if(--fd->refcount > 0){
		fdtable->fd[ifd] = NULL;
		spinlock_release(&fd->lock);
		spinlock_release(&fdtable->lock);
		return 0;
	}

	fdtable->fd[ifd] = NULL;
	
	spinlock_release(&fdtable->lock);
	int err = 0;
	if(fd->node)
		err = vfs_close(fd->node);
	
	free(fd);

	return err;
	
}

int fd_tableclone(fdtable_t* source, fdtable_t* dest){

	spinlock_acquire(&source->lock);

	if(fd_tableinit(dest)){
		spinlock_release(&source->lock);
		return ENOMEM;
	}
	
	if(source->fdcount != dest->fdcount){
		void* tmp = realloc(dest->fd, source->fdcount*sizeof(fd_t*));
		if(!tmp){
			spinlock_release(&source->lock);
			return ENOMEM;
		}
		dest->fd = tmp;
		dest->fdcount = source->fdcount;
	}

	
	for(uintmax_t i = 0; i < dest->fdcount; ++i){
		if(!source->fd[i])
			continue;

		dest->fd[i] = source->fd[i];

		spinlock_acquire(&dest->fd[i]->lock);

		++dest->fd[i]->refcount;

		spinlock_release(&dest->fd[i]->lock);
		
	}


	spinlock_release(&source->lock);
	

}

int fd_tableinit(fdtable_t* fdtable){
	
	fdtable->fd = alloc(sizeof(fd_t*)*3);

	if(!fdtable->fd) return ENOMEM;

	fdtable->fdcount = 3;
	
	return 0;

}

// types 1 and relate to dup and dup2

#define RESIZE_BAD "astral: dup2 table resizes not supported yet!\n"

int fd_duplicate(fdtable_t* table, int src, int dest, int type, int* ret){

	fd_t *srcfd;
	fd_t *destfd = NULL;
	
	if(type != 1 && dest >= MAX_FD){
		return EBADF;
	}

	int err = fd_access(table, &srcfd, src);
	if(err)
		return err;
	
	if(src == dest){
		switch(type){
			case 1: // do nothing on dup
				break;
			case 2: // return src
				fd_release(srcfd);
				*ret = src;
				return 0;
		}
	}

	if(type == 1){ // allocate for dup
		err = fd_alloc(table, &destfd, &dest);
		if(err){
			fd_release(srcfd);
			return err;
		}

	}

	spinlock_acquire(&table->lock);	
	
	// resize the table if needed

	if(type == 2 && dest >= table->fdcount){
		void* tmp = realloc(table->fd, sizeof(fd_t*)*(dest+1));
		if(!tmp){
			spinlock_release(&table->lock);
			return ENOMEM;
		}
		
		table->fd = tmp;
		table->fdcount = dest+1;

	}

	if(!destfd){ // if no allocation, it's a dup2. check for something there and kill it
		destfd = table->fd[dest];
		if(destfd){
			spinlock_acquire(&destfd->lock);
			if(--destfd->refcount == 0){
				printf("killing\n");
				if(destfd->node) vfs_close(destfd->node);
				free(destfd);
			}
			else{
				spinlock_release(&destfd->lock);
			}

		}
	}
	
	// everything is set now
	
	table->fd[dest] = srcfd;
	srcfd->refcount++;
	*ret = dest;

	fd_release(srcfd);
	spinlock_release(&table->lock);
	
	return 0;

}
