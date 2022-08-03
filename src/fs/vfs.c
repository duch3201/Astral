#include <kernel/vfs.h>
#include <kernel/alloc.h>
#include <hashtable.h>
#include <stdio.h>
#include <string.h>

dirnode_t* vfsroot;
hashtable  fsfuncs;

static inline dirnode_t* mountpoint(dirnode_t* node){
	dirnode_t* ret = node;
	while(ret->mount)
		ret = ret->mount;
	return ret;
}

int vfs_mount(dirnode_t* ref, char* device, char* mountpoint, char* fs, int mountflags, void* fsinfo){
	
	fscalls_t* fscalls = hashtable_get(&fsfuncs, fs);
	
	if(!fscalls)
		return ENODEV;

	vnode_t* dev = NULL;
	dirnode_t* mountdir = ref;

	if(device){
		dev = ref;
		if(*device){
			int result = vfs_resolvepath(&dev, ref, device);
			
			if(result)
				return result;
		}
	}


	int result = vfs_resolvepath(&mountdir, ref, mountpoint);
	
	if(result)
		return result;
	
	if(GETTYPE(mountdir->vnode.st.st_mode) != TYPE_DIR)
		return ENOTDIR;

	dirnode_t* mount;
	
	result = fscalls->mount(&mount, dev, mountflags, fsinfo);

	if(result)
		return result;

	mountdir->mount = mount;

	return 0;
}

vnode_t* vfs_newnode(char* name, fs_t* fs, void* fsdata){
	vnode_t* node = alloc(sizeof(vnode_t));
	if(!node) return NULL;
	node->name = alloc(strlen(name)+1);
	if(!name){
		free(node);
		return NULL;
	}

	node->fs = fs;
	node->fsdata = fsdata;
	strcpy(node->name, name);

	return node;
}


dirnode_t* vfs_newdirnode(char* name, fs_t* fs, void* fsdata){
	
	dirnode_t* node = alloc(sizeof(dirnode_t));
	if(!node) return NULL;
	vnode_t* vnode = (vnode_t*)node;
	
	vnode->name = alloc(strlen(name)+1);

	if(!vnode->name){
		free(node);
		return NULL;
	}

	if(!hashtable_init(&node->children, 10)){ // add more here later
		free(vnode->name);
		free(node);
		return NULL;
	}
	
	vnode->st.st_mode = MAKETYPE(TYPE_DIR);
	vnode->fs = fs;
	vnode->fsdata = fsdata;
	strcpy(vnode->name, name);

	return node;

}

int vfs_resolvepath(vnode_t** result, dirnode_t* ref, char *path){
	
	dirnode_t* iterator = ref;
	char name[512];
	off_t nameoffset = 0;

	memset(name, 0, 512);
	
	iterator = mountpoint(iterator);

	while(nameoffset < strlen(path)){
		
		size_t offset = 0;

		while(path[nameoffset] && path[nameoffset] != '/')
			name[offset++] = path[nameoffset++];

		if(path[nameoffset] == '/')
			++nameoffset;

		name[offset] = 0;

		// make sure we're a directory

		if(!GETTYPE(iterator->vnode.st.st_mode) == TYPE_DIR)
			return ENOTDIR;

		// get the right vnode for this point
		
		iterator = mountpoint(iterator);

		// now try to get the right child

		vnode_t* child = hashtable_get(&iterator->children, name);

		if(!child){
			// open it
			int status = iterator->vnode.fs->calls->open(iterator, name);
			if(status)
				return status;

			child = hashtable_get(&iterator->children, name);
			
		}

		iterator = (dirnode_t*)child;

	}

	*result = (vnode_t*)iterator;

	return 0;

}

void vfs_destroynode(vnode_t* node){
	
	if(GETTYPE(node->st.st_mode) == TYPE_DIR)
		hashtable_destroy(&((dirnode_t*)node)->children);
	
	free(node);

}

dirnode_t* vfs_root(){
	return vfsroot;
}

void vfs_acquirenode(vnode_t* node){
	++node->refcount;
}

void vfs_releasenode(vnode_t* node){
	--node->refcount;
}

#include <kernel/tmpfs.h>

void vfs_init(){
	printf("Creating a fake root for the VFS\n");
	
	vfsroot = vfs_newdirnode("/", NULL, NULL);

	hashtable_init(&fsfuncs, 10);

	hashtable_insert(&fsfuncs, "tmpfs", (void*)tmpfs_getfuncs());
		
	
}
