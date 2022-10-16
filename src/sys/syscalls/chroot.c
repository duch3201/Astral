#include <kernel/syscalls.h>
#include <kernel/vfs.h>
#include <kernel/sched.h>
#include <kernel/vmm.h>
#include <arch/cls.h>

syscallret syscall_chroot(const char* path){

	syscallret retv;
	retv.ret = -1;

	if(path > USER_SPACE_END){
		retv.errno = EFAULT;
		return retv;
	}
	
	size_t len = strlen(path);

	char buff[len+1];
	
	strcpy(buff, path);

	proc_t* proc = arch_getcls()->thread->proc;

	vnode_t* vnode;

	retv.errno = vfs_open(&vnode,
		buff[0] == '/' ? proc->root : proc->cwd,
		buff
	);

	if(retv.errno)
		return retv;
	
	if(GETTYPE(vnode->st.st_mode) != TYPE_DIR){
		retv.errno = ENOTDIR;
		goto _ret;
	}

	dirnode_t* oldroot = proc->root;
	proc->root = (dirnode_t*)vnode;

	retv.errno = 0;
	retv.ret = 0;

	_ret:
		
	vfs_close(vnode);

	return retv;
	
}
