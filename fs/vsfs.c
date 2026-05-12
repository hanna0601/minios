/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Alexey Khrabrov, Karen Reid, Angela Demke Brown
 *
 * CSC369 Assignment 5 - vsfs driver implementation.
 * 
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2024 Angela Demke Brown
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

// Using 2.9.x FUSE API
#define FUSE_USE_VERSION 29
#include <fuse.h>

#include "vsfs.h"
#include "fs_ctx.h"
#include "options.h"
#include "util.h"
#include "bitmap.h"
#include "map.h"

//NOTE: All path arguments are absolute paths within the vsfs file system and
// start with a '/' that corresponds to the vsfs root directory.
//
// For example, if vsfs is mounted at "/tmp/my_userid", the path to a
// file at "/tmp/my_userid/dir/file" (as seen by the OS) will be
// passed to FUSE callbacks as "/dir/file".
//
// Paths to directories (except for the root directory - "/") do not end in a
// trailing '/'. For example, "/tmp/my_userid/dir/" will be passed to
// FUSE callbacks as "/dir".


/**
 * Initialize the file system.
 *
 * Called when the file system is mounted. NOTE: we are not using the FUSE
 * init() callback since it doesn't support returning errors. This function must
 * be called explicitly before fuse_main().
 *
 * @param fs    file system context to initialize.
 * @param opts  command line options.
 * @return      true on success; false on failure.
 */
static bool vsfs_init(fs_ctx *fs, vsfs_opts *opts)
{
	size_t size;
	void *image;
	
	// Nothing to initialize if only printing help
	if (opts->help) {
		return true;
	}

	// Map the disk image file into memory
	image = map_file(opts->img_path, VSFS_BLOCK_SIZE, &size);
	if (image == NULL) {
		return false;
	}

	return fs_ctx_init(fs, image, size);
}

/**
 * Cleanup the file system.
 *
 * Called when the file system is unmounted. Must cleanup all the resources
 * created in vsfs_init().
 */
static void vsfs_destroy(void *ctx)
{
	fs_ctx *fs = (fs_ctx*)ctx;
	if (fs->image) {
		munmap(fs->image, fs->size);
		fs_ctx_destroy(fs);
	}
}

/** Get file system context. */
static fs_ctx *get_fs(void)
{
	return (fs_ctx*)fuse_get_context()->private_data;
}


/* Returns the inode number for the element at the end of the path
 * if it exists.  If there is any error, return -1.
 * Possible errors include:
 *   - The path is not an absolute path
 *   - An element on the path cannot be found
 */
static int path_lookup(const char *path,  vsfs_ino_t *ino) {
	if(path[0] != '/') {
		fprintf(stderr, "Not an absolute path\n");
		return -ENOSYS;
	} 

	// TODO: complete this function and any helper functions
	if (strcmp(path, "/") == 0) {
		*ino = VSFS_ROOT_INO;
		return 0;
	}
	fs_ctx* fs = get_fs();
	vsfs_inode * root = &fs->itable[VSFS_ROOT_INO];
	for(vsfs_blk_t i = 0;i< root->i_blocks;++i){
		if(i< VSFS_NUM_DIRECT){
			vsfs_dentry* dentry = (vsfs_dentry*)(fs->image + root->i_direct[i]*VSFS_BLOCK_SIZE);
			for(long unsigned int j = 0;j< VSFS_BLOCK_SIZE/sizeof(vsfs_dentry);++j){
				if(dentry[j].ino == VSFS_INO_MAX){
					continue;
				}
				if(!strcmp(dentry[j].name,path+1)){
					*ino = dentry[j].ino;
					return 0;
				}
			}
		}
		else{
			if(root->i_indirect == 0){
				return -1;
			}	
			vsfs_blk_t* base_indirect = (vsfs_blk_t*) (fs->image + root->i_indirect * VSFS_BLOCK_SIZE);

			vsfs_dentry* dentry = (vsfs_dentry*) (fs->image + base_indirect[i-VSFS_NUM_DIRECT] * VSFS_BLOCK_SIZE);
			for(long unsigned int j = 0;j< VSFS_BLOCK_SIZE/sizeof(vsfs_dentry);++j){
				if(dentry[j].ino == VSFS_INO_MAX){
					continue;
				}
				if(!strcmp(dentry[j].name,path+1)){
					*ino = dentry[j].ino;
					return 0;
				}
			}
		}
	}


	return -ENOSYS;
}

/**
 * Get file system statistics.
 *
 * Implements the statvfs() system call. See "man 2 statvfs" for details.
 * The f_bfree and f_bavail fields should be set to the same value.
 * The f_ffree and f_favail fields should be set to the same value.
 * The following fields can be ignored: f_fsid, f_flag.
 * All remaining fields are required.
 *
 * Errors: none
 *
 * @param path  path to any file in the file system. Can be ignored.
 * @param st    pointer to the struct statvfs that receives the result.
 * @return      0 on success; -errno on error.
 */
static int vsfs_statfs(const char *path, struct statvfs *st)
{
	(void)path;// unused
	fs_ctx *fs = get_fs();
	vsfs_superblock *sb = fs->sb; /* Get ptr to superblock from context */
	
	memset(st, 0, sizeof(*st));
	st->f_bsize   = VSFS_BLOCK_SIZE;   /* Filesystem block size */
	st->f_frsize  = VSFS_BLOCK_SIZE;   /* Fragment size */
	// The rest of required fields are filled based on the information 
	// stored in the superblock.
        st->f_blocks = sb->sb_num_blocks;     /* Size of fs in f_frsize units */
        st->f_bfree  = sb->sb_free_blocks;    /* Number of free blocks */
        st->f_bavail = sb->sb_free_blocks;    /* Free blocks for unpriv users */
	st->f_files  = sb->sb_num_inodes;     /* Number of inodes */
        st->f_ffree  = sb->sb_free_inodes;    /* Number of free inodes */
        st->f_favail = sb->sb_free_inodes;    /* Free inodes for unpriv users */

	st->f_namemax = VSFS_NAME_MAX;     /* Maximum filename length */

	return 0;
}

/**
 * Get file or directory attributes.
 *
 * Implements the lstat() system call. See "man 2 lstat" for details.
 * The following fields can be ignored: st_dev, st_ino, st_uid, st_gid, st_rdev,
 *                                      st_blksize, st_atim, st_ctim.
 * All remaining fields are required.
 *
 * NOTE: the st_blocks field is measured in 512-byte units (disk sectors);
 *       it should include any metadata blocks that are allocated to the 
 *       inode (for vsfs, that is the indirect block). 
 *
 * NOTE2: the st_mode field must be set correctly for files and directories.
 *
 * Errors:
 *   ENAMETOOLONG  the path or one of its components is too long.
 *   ENOENT        a component of the path does not exist.
 *   ENOTDIR       a component of the path prefix is not a directory.
 *
 * @param path  path to a file or directory.
 * @param st    pointer to the struct stat that receives the result.
 * @return      0 on success; -errno on error;
 */
static int vsfs_getattr(const char *path, struct stat *st)
{
	if (strlen(path) >= VSFS_PATH_MAX) return -ENAMETOOLONG;
	fs_ctx *fs = get_fs();

	memset(st, 0, sizeof(*st));
	//TODO: lookup the inode for given path and, if it exists, fill in the
	// required fields based on the information stored in the inode
	vsfs_ino_t ino;
	if(path_lookup(path,&ino)<0|| ino == VSFS_INO_MAX){
		return -ENOENT;
	}
	vsfs_inode * node = &(fs->itable[ino]);
	st->st_mode = node->i_mode;
	st->st_nlink = node->i_nlink;
	st->st_size = node->i_size;
	st->st_blocks = node->i_blocks * VSFS_BLOCK_SIZE / 512;
	st->st_mtim = node->i_mtime;
	if(VSFS_NUM_DIRECT < node->i_blocks){
		st->st_blocks += VSFS_BLOCK_SIZE / 512;
	}
	return 0;

}

/**
 * Read a directory.
 *
 * Implements the readdir() system call. Should call filler(buf, name, NULL, 0)
 * for each directory entry. See fuse.h in libfuse source code for details.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a directory.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a filler() call failed).
 *
 * @param path    path to the directory.
 * @param buf     buffer that receives the result.
 * @param filler  function that needs to be called for each directory entry.
 *                Pass 0 as offset (4th argument). 3rd argument can be NULL.
 * @param offset  unused.
 * @param fi      unused.
 * @return        0 on success; -errno on error.
 */
static int vsfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi)
{
	//TODO: lookup the directory inode for the given path and iterate 
	//      through its directory entries
	(void)offset;// unused
	(void)fi;// unused
	fs_ctx* fs = get_fs();
	vsfs_ino_t index;
	if (path_lookup(path, &index) < 0){
		return -1;
	}

	vsfs_inode * root = &fs->itable[index];
    unsigned int dentry_num = div_round_up(root->i_size, sizeof(vsfs_dentry));
	for(vsfs_blk_t i = 0;i<root->i_blocks;++i){
		if(i< VSFS_NUM_DIRECT){
			vsfs_dentry* dentry = (vsfs_dentry*)(fs->image + root->i_direct[i]*VSFS_BLOCK_SIZE);
			for(long unsigned int j = 0;j< VSFS_BLOCK_SIZE/sizeof(vsfs_dentry);++j){
				if(dentry[j].ino == VSFS_INO_MAX){
					continue;
				}
				if(filler(buf,dentry[j].name,NULL,0)){
					return -ENOMEM;
				}
                dentry_num--;
                if(!dentry_num) break;
			}
		}
		else{
			if(root->i_indirect == 0){
				return -1;
			}
			vsfs_blk_t* base_indirect = (vsfs_blk_t*) (fs->image + root->i_indirect * VSFS_BLOCK_SIZE);
			vsfs_dentry* dentry = (vsfs_dentry*) (fs->image + base_indirect[i-VSFS_NUM_DIRECT] * VSFS_BLOCK_SIZE);
			for(long unsigned int j = 0;j< VSFS_BLOCK_SIZE/sizeof(vsfs_dentry);++j){
				if(dentry[j].ino == VSFS_INO_MAX){
					continue;
				}
				if(filler(buf,dentry[j].name,NULL,0)){
					return -ENOMEM;
				}
                dentry_num--;
                if(!dentry_num) break;
			}

		}
	}

	return 0;
}


/**
 * Create a file.
 *
 * Implements the open()/creat() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" doesn't exist.
 *   The parent directory of "path" exists and is a directory.
 *   "path" and its components are not too long.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the file to create.
 * @param mode  file mode bits.
 * @param fi    unused.
 * @return      0 on success; -errno on error.
 */
static int vsfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	(void) fi;
    assert(S_ISREG(mode));
    fs_ctx *fs = get_fs();


    char *path_copy = strdup(path);
    char *dirname = path_copy;
    char *basename = strrchr(path_copy, '/');
    if (basename) {
        *basename++ = '\0';
    } else {
        basename = path_copy;
        dirname = "/";
    }
	if(!dirname[0]){
		dirname="/";
	}
    // Get parent directory inode
    vsfs_ino_t parent_ino;
    int ret = path_lookup(dirname, &parent_ino);
    if (ret != 0) {
        free(path_copy);
        free(dirname);
        return ret;
    }
    vsfs_inode *parent = &fs->itable[parent_ino];

    // Find free entry in existing blocks
    vsfs_dentry *new_entry = NULL;
    vsfs_blk_t block;
    int entries_per_block = VSFS_BLOCK_SIZE / sizeof(vsfs_dentry);

    for (unsigned int i = 0; i < parent->i_blocks && !new_entry; i++) {
        if (i < VSFS_NUM_DIRECT) {
            block = parent->i_direct[i];
        } else {
            vsfs_blk_t *indirect = (vsfs_blk_t *)(fs->image + parent->i_indirect * VSFS_BLOCK_SIZE);
            block = indirect[i - VSFS_NUM_DIRECT];
        }
        
        vsfs_dentry *entries = (vsfs_dentry *)(fs->image + block * VSFS_BLOCK_SIZE);
        for (int j = 0; j < entries_per_block; j++) {
            if (entries[j].ino == VSFS_INO_MAX) {
                new_entry = &entries[j];
                break;
            }
        }
    }

    // Allocate new block if necessary
    if (!new_entry) {
        if (parent->i_blocks >= VSFS_NUM_DIRECT + VSFS_BLOCK_SIZE / sizeof(vsfs_blk_t)) {
            free(path_copy);
            return -ENOSPC;
        }
        if(parent->i_blocks >= VSFS_NUM_DIRECT && parent->i_indirect <=0 ){
            vsfs_blk_t* indirect_block_ptr = NULL;

            if (bitmap_alloc(fs->dbmap, fs->sb->sb_num_blocks, indirect_block_ptr) != 0){
                return -ENOSPC;
            }
		    memset(fs->image + *indirect_block_ptr * VSFS_BLOCK_SIZE, 0 , VSFS_BLOCK_SIZE);
		    parent->i_indirect = *indirect_block_ptr;
		    fs->sb->sb_free_blocks --;	
        }
        
        vsfs_blk_t *new_block_ptr = parent->i_blocks < VSFS_NUM_DIRECT ? &parent->i_direct[parent->i_blocks] : 
            &((vsfs_blk_t *)(fs->image + parent->i_indirect * VSFS_BLOCK_SIZE))[parent->i_blocks - VSFS_NUM_DIRECT];
        if (bitmap_alloc(fs->dbmap, fs->sb->sb_num_blocks, new_block_ptr) != 0) {
            return -ENOSPC;
        }
        vsfs_dentry *new_block = (vsfs_dentry *)(fs->image + (*new_block_ptr) * VSFS_BLOCK_SIZE);
        for (int j = 0; j < entries_per_block; j++) {
            new_block[j].ino = VSFS_INO_MAX;
        }
        new_entry = &new_block[0];
        parent->i_blocks++;
        parent->i_size += VSFS_BLOCK_SIZE;
        fs->sb->sb_free_blocks--;
    }

    // Allocate and initialize inode
    if (bitmap_alloc(fs->ibmap, fs->sb->sb_num_inodes, &new_entry->ino) != 0) {
        free(path_copy);
        return -ENOSPC;
    }

    strncpy(new_entry->name, basename, VSFS_NAME_MAX);
    vsfs_inode *new_inode = &fs->itable[new_entry->ino];
    new_inode->i_mode = mode;
    new_inode->i_size = 0;
    new_inode->i_nlink = 1;
    new_inode->i_blocks = 0;
    clock_gettime(CLOCK_REALTIME, &new_inode->i_mtime);
    clock_gettime(CLOCK_REALTIME, &parent->i_mtime);

    fs->sb->sb_free_inodes--;
    
    free(path_copy);
    return 0;
}

/**
 * Remove a file.
 *
 * Implements the unlink() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors: none
 *
 * @param path  path to the file to remove.
 * @return      0 on success; -errno on error.
 */
static int vsfs_unlink(const char *path)
{
    fs_ctx *fs = get_fs();
    vsfs_ino_t target_ino;
    
    if (path_lookup(path, &target_ino) < 0) {
        return -ENOENT;
    }

    // Parse path to get parent directory and filename
    char *path_copy = strdup(path);
    char *basename = strrchr(path_copy, '/');
	char *dirname = path_copy;
    if (basename) {
        *basename++ = '\0';
    } else {
        basename = path_copy;
        dirname = "/";
    }	
	if(!dirname[0]){
		dirname = "/";
	}
    
    // Get parent inode
    vsfs_ino_t parent_ino;
    int ret = path_lookup(dirname, &parent_ino);
    if (ret != 0) {
        free(path_copy);
        return ret;
    }
    vsfs_inode *parent = &fs->itable[parent_ino];
    vsfs_inode *target = &fs->itable[target_ino];
    
    // Free data blocks
    for (unsigned int i = 0; i < target->i_blocks; i++) {
        vsfs_blk_t block;
        if (i < VSFS_NUM_DIRECT) {
            block = target->i_direct[i];
        } else {
            vsfs_blk_t *indirect = (vsfs_blk_t *)(fs->image + target->i_indirect * VSFS_BLOCK_SIZE);
            block = indirect[i - VSFS_NUM_DIRECT];
        }
        bitmap_free(fs->dbmap, fs->sb->sb_num_blocks, block);
        fs->sb->sb_free_blocks++;
    }

    // Free indirect block if exists
    if (target->i_blocks > VSFS_NUM_DIRECT) {
        bitmap_free(fs->dbmap, fs->sb->sb_num_blocks, target->i_indirect);
        fs->sb->sb_free_blocks++;
    }

    // Clear target inode
    memset(target, 0, sizeof(vsfs_inode));
    bitmap_free(fs->ibmap, fs->sb->sb_num_inodes, target_ino);
    fs->sb->sb_free_inodes++;

    // Remove directory entry
    int entries_per_block = VSFS_BLOCK_SIZE / sizeof(vsfs_dentry);
    for (unsigned int i = 0; i < parent->i_blocks; i++) {
        vsfs_blk_t block;
        if (i < VSFS_NUM_DIRECT) {
            block = parent->i_direct[i];
        } else {
            vsfs_blk_t *indirect = (vsfs_blk_t *)(fs->image + parent->i_indirect * VSFS_BLOCK_SIZE);
            block = indirect[i - VSFS_NUM_DIRECT];
        }
        
        vsfs_dentry *entries = (vsfs_dentry *)(fs->image + block * VSFS_BLOCK_SIZE);
        for (int j = 0; j < entries_per_block; j++) {
            if (entries[j].ino == target_ino) {
                entries[j].ino = VSFS_INO_MAX;
                clock_gettime(CLOCK_REALTIME, &parent->i_mtime);
                free(path_copy);
                return 0;
            }
        }
    }

    free(path_copy);
    return -ENOENT;
}


/**
 * Change the modification time of a file or directory.
 *
 * Implements the utimensat() system call. See "man 2 utimensat" for details.
 *
 * NOTE: You only need to implement the setting of modification time (mtime).
 *       Timestamp modifications are not recursive. 
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists.
 *
 * Errors: none
 *
 * @param path   path to the file or directory.
 * @param times  timestamps array. See "man 2 utimensat" for details.
 * @return       0 on success; -errno on failure.
 */
static int vsfs_utimens(const char *path, const struct timespec times[2])
{
	fs_ctx *fs = get_fs();
	vsfs_inode *ino = NULL;
	
	//TODO: update the modification timestamp (mtime) in the inode for given
	// path with either the time passed as argument or the current time,
	// according to the utimensat man page
	
	// 0. Check if there is actually anything to be done.
	if (times[1].tv_nsec == UTIME_OMIT) {
		// Nothing to do.
		return 0;
	}

	// 1. TODO: Find the inode for the final component in path
	vsfs_ino_t index;
	if (path_lookup(path, &index) < 0)
	{
		return -ENOENT;
	}
	ino = &(fs->itable[index]);
	
	// 2. Update the mtime for that inode.
	//    This code is commented out to avoid failure until you have set
	//    'ino' to point to the inode structure for the inode to update.
	if (times[1].tv_nsec == UTIME_NOW) {
		if (clock_gettime(CLOCK_REALTIME, &(ino->i_mtime)) != 0) {
			// clock_gettime should not fail, unless you give it a
			// bad pointer to a timespec.
			assert(false);
		}
	} else {
		ino->i_mtime = times[1];
	}

	return 0;
	//return -ENOSYS;
}

/**
 * Change the size of a file.
 *
 * Implements the truncate() system call. Supports both extending and shrinking.
 * If the file is extended, the new uninitialized range at the end must be
 * filled with zeros.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *   EFBIG   write would exceed the maximum file size. 
 *
 * @param path  path to the file to set the size.
 * @param size  new file size in bytes.
 * @return      0 on success; -errno on error.
 */
static int vsfs_truncate(const char *path, off_t size)
{
    fs_ctx *fs = get_fs();
    vsfs_ino_t ino;
    
    if (path_lookup(path, &ino) < 0) {
        return -ENOENT;
    }
    
    vsfs_inode *inode = &fs->itable[ino];
    uint32_t max_size = (VSFS_NUM_DIRECT + VSFS_BLOCK_SIZE/sizeof(vsfs_blk_t)) * VSFS_BLOCK_SIZE;
    
    if ((uint32_t)size > max_size) {
        return -EFBIG;
    }
    
    uint32_t new_blocks = div_round_up(size, VSFS_BLOCK_SIZE);
    int ret = 0;

    // Expanding file
    if (size >(int) inode->i_size) {
        if (new_blocks > inode->i_blocks) {
            // Allocate direct blocks
            while (inode->i_blocks < VSFS_NUM_DIRECT && inode->i_blocks < new_blocks) {
                if (bitmap_alloc(fs->dbmap, fs->sb->sb_num_blocks, &inode->i_direct[inode->i_blocks]) != 0) {
                    return -ENOSPC;
                }
                memset(fs->image + inode->i_direct[inode->i_blocks] * VSFS_BLOCK_SIZE, 0, VSFS_BLOCK_SIZE);
                fs->sb->sb_free_blocks--;
                inode->i_blocks++;
            }
            
            // Allocate indirect blocks if needed
            if (new_blocks > inode->i_blocks) {
                // Allocate indirect block pointer
                if (inode->i_blocks == VSFS_NUM_DIRECT && !inode->i_indirect) {
                    if (bitmap_alloc(fs->dbmap, fs->sb->sb_num_blocks, &inode->i_indirect) != 0) {
                        return -ENOSPC;
                    }
                    fs->sb->sb_free_blocks--;
                }
                
                // Allocate indirect data blocks
                vsfs_blk_t *indirect = (vsfs_blk_t *)(fs->image + inode->i_indirect * VSFS_BLOCK_SIZE);
                while (inode->i_blocks < new_blocks) {
                    if (bitmap_alloc(fs->dbmap, fs->sb->sb_num_blocks, 
                        &indirect[inode->i_blocks - VSFS_NUM_DIRECT]) != 0) {
                        return -ENOSPC;
                    }
                    memset(fs->image + indirect[inode->i_blocks - VSFS_NUM_DIRECT] * VSFS_BLOCK_SIZE, 
                           0, VSFS_BLOCK_SIZE);
                    fs->sb->sb_free_blocks--;
                    inode->i_blocks++;
                }
            }
        }
    }
    // Shrinking file 
    else if (size < (int)inode->i_size) {
        // Free indirect blocks
        if (inode->i_blocks > VSFS_NUM_DIRECT) {
            vsfs_blk_t *indirect = (vsfs_blk_t *)(fs->image + inode->i_indirect * VSFS_BLOCK_SIZE);
            while (inode->i_blocks > new_blocks && inode->i_blocks > VSFS_NUM_DIRECT) {
                bitmap_free(fs->dbmap, fs->sb->sb_num_blocks,
                    indirect[inode->i_blocks - VSFS_NUM_DIRECT - 1]);
                fs->sb->sb_free_blocks++;
                inode->i_blocks--;
            }
            
            // Free indirect block pointer if no longer needed
            if (inode->i_blocks <= VSFS_NUM_DIRECT) {
                bitmap_free(fs->dbmap, fs->sb->sb_num_blocks, inode->i_indirect);
                inode->i_indirect = 0;
                fs->sb->sb_free_blocks++;
            }
        }
        
        // Free direct blocks
        while (inode->i_blocks > new_blocks) {
            bitmap_free(fs->dbmap, fs->sb->sb_num_blocks, 
                inode->i_direct[inode->i_blocks - 1]);
            inode->i_direct[inode->i_blocks - 1] = 0;
            fs->sb->sb_free_blocks++;
            inode->i_blocks--;
        }
    }

    inode->i_size = size;
    clock_gettime(CLOCK_REALTIME, &inode->i_mtime);
    return ret;
}


/**
 * Read data from a file.
 *
 * Implements the pread() system call. Must return exactly the number of bytes
 * requested except on EOF (end of file). Reads from file ranges that have not
 * been written to must return ranges filled with zeros. You can assume that the
 * byte range from offset to offset + size is contained within a single block.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors: none
 *
 * @param path    path to the file to read from.
 * @param buf     pointer to the buffer that receives the data.
 * @param size    buffer size (number of bytes requested).
 * @param offset  offset from the beginning of the file to read from.
 * @param fi      unused.
 * @return        number of bytes read on success; 0 if offset is beyond EOF;
 *                -errno on error.
 */
static int vsfs_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    (void)fi;
    fs_ctx *fs = get_fs();
    vsfs_ino_t ino;
    path_lookup(path, &ino);
    vsfs_inode *inode = &fs->itable[ino];
    
    if (offset >= (long int)inode->i_size) return 0;
    size = size<inode->i_size - offset?size:inode->i_size - offset;
    if (size == 0) return 0;

    size_t bytes_read = 0;
    while (bytes_read < size) {
        vsfs_blk_t blk_idx = (offset + bytes_read) / VSFS_BLOCK_SIZE;
        off_t blk_off = (offset + bytes_read) % VSFS_BLOCK_SIZE;
        size_t chunk_size = size - bytes_read<(long unsigned int)(VSFS_BLOCK_SIZE - blk_off)?size - bytes_read:(long unsigned int)(VSFS_BLOCK_SIZE - blk_off);
        
        vsfs_blk_t data_blk;
        if (blk_idx < VSFS_NUM_DIRECT) {
            data_blk = inode->i_direct[blk_idx];
        } else {
            vsfs_blk_t *indirect = (vsfs_blk_t *)(fs->image + inode->i_indirect * VSFS_BLOCK_SIZE);
            data_blk = indirect[blk_idx - VSFS_NUM_DIRECT];
        }
        
        memcpy(buf + bytes_read, 
               fs->image + data_blk * VSFS_BLOCK_SIZE + blk_off, 
               chunk_size);
               
        bytes_read += chunk_size;
    }
    
    return size;
}

/**
 * Write data to a file.
 *
 * Implements the pwrite() system call. Must return exactly the number of bytes
 * requested except on error. If the offset is beyond EOF (end of file), the
 * file must be extended. If the write creates a "hole" of uninitialized data,
 * the new uninitialized range must filled with zeros. You can assume that the
 * byte range from offset to offset + size is contained within a single block.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *   EFBIG   write would exceed the maximum file size 
 *
 * @param path    path to the file to write to.
 * @param buf     pointer to the buffer containing the data.
 * @param size    buffer size (number of bytes requested).
 * @param offset  offset from the beginning of the file to write to.
 * @param fi      unused.
 * @return        number of bytes written on success; -errno on error.
 */
static int vsfs_write(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi)
{
    (void)fi;
    fs_ctx *fs = get_fs();
    vsfs_ino_t ino;
    
    path_lookup(path, &ino);
    vsfs_inode *inode = &fs->itable[ino];
    uint32_t max_size = (VSFS_NUM_DIRECT + VSFS_BLOCK_SIZE/sizeof(vsfs_blk_t)) * VSFS_BLOCK_SIZE;
    
    if (offset + size > max_size) return -EFBIG;
    if (size == 0) return 0;

    uint32_t old_size = inode->i_size;
    if (offset + size > inode->i_size) {
        if (vsfs_truncate(path, offset + size) != 0) {
            return -ENOSPC;
        }
    }

    size_t bytes_written = 0;
    while (bytes_written < size) {
        vsfs_blk_t blk_idx = (offset + bytes_written) / VSFS_BLOCK_SIZE;
        off_t blk_off = (offset + bytes_written) % VSFS_BLOCK_SIZE;
        size_t chunk_size = size - bytes_written< (size_t)( VSFS_BLOCK_SIZE - blk_off)?size - bytes_written: (size_t)( VSFS_BLOCK_SIZE - blk_off);
        
        vsfs_blk_t data_blk;
        if (blk_idx < VSFS_NUM_DIRECT) {
            data_blk = inode->i_direct[blk_idx];
        } else {
            vsfs_blk_t *indirect = (vsfs_blk_t *)(fs->image + inode->i_indirect * VSFS_BLOCK_SIZE);
            data_blk = indirect[blk_idx - VSFS_NUM_DIRECT];
        }
        
        memcpy(fs->image + data_blk * VSFS_BLOCK_SIZE + blk_off,
               buf + bytes_written, 
               chunk_size);
               
        bytes_written += chunk_size;
    }

    if (old_size < offset) {
        uint32_t zero_start = old_size % VSFS_BLOCK_SIZE;
        uint32_t zero_size = offset - old_size;
        vsfs_blk_t zero_blk = old_size / VSFS_BLOCK_SIZE;
        
        if (zero_blk < VSFS_NUM_DIRECT) {
            memset(fs->image + inode->i_direct[zero_blk] * VSFS_BLOCK_SIZE + zero_start,
                   0, zero_size);
        } else {
            vsfs_blk_t *indirect = (vsfs_blk_t *)(fs->image + inode->i_indirect * VSFS_BLOCK_SIZE);
            memset(fs->image + indirect[zero_blk - VSFS_NUM_DIRECT] * VSFS_BLOCK_SIZE + zero_start,
                   0, zero_size);
        }
    }

    clock_gettime(CLOCK_REALTIME, &inode->i_mtime);
    return size;
}


static struct fuse_operations vsfs_ops = {
	.destroy  = vsfs_destroy,
	.statfs   = vsfs_statfs,
	.getattr  = vsfs_getattr,
	.readdir  = vsfs_readdir,
	.create   = vsfs_create,
	.unlink   = vsfs_unlink,
	.utimens  = vsfs_utimens,
	.truncate = vsfs_truncate,
	.read     = vsfs_read,
	.write    = vsfs_write,
};

int main(int argc, char *argv[])
{
	vsfs_opts opts = {0};// defaults are all 0
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	if (!vsfs_opt_parse(&args, &opts)) return 1;

	fs_ctx fs = {0};
	if (!vsfs_init(&fs, &opts)) {
		fprintf(stderr, "Failed to mount the file system\n");
		return 1;
	}

	return fuse_main(args.argc, args.argv, &vsfs_ops, &fs);
}