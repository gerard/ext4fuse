/* vim: set ts=8 :
 *
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 *  from
 *
 *  linux/fs/ext4/ext4.h
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/include/linux/minix_fs.h
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 * 
 */

#ifndef EXT4_INODE_H
#define EXT4_INODE_H

#include "ext4_basic.h"


#define EXT4_NDIR_BLOCKS                12
#define EXT4_IND_BLOCK                  EXT4_NDIR_BLOCKS
#define EXT4_DIND_BLOCK                 (EXT4_IND_BLOCK + 1)
#define EXT4_TIND_BLOCK                 (EXT4_DIND_BLOCK + 1)
#define EXT4_N_BLOCKS                   (EXT4_TIND_BLOCK + 1)

#define EXT4_SECRM_FL                   0x00000001 /* Secure deletion */
#define EXT4_UNRM_FL                    0x00000002 /* Undelete */
#define EXT4_COMPR_FL                   0x00000004 /* Compress file */
#define EXT4_SYNC_FL                    0x00000008 /* Synchronous updates */
#define EXT4_IMMUTABLE_FL               0x00000010 /* Immutable file */
#define EXT4_APPEND_FL                  0x00000020 /* writes to file may only append */
#define EXT4_NODUMP_FL                  0x00000040 /* do not dump file */
#define EXT4_NOATIME_FL                 0x00000080 /* do not update atime */
/* Reserved for compression usage... */
#define EXT4_DIRTY_FL                   0x00000100
#define EXT4_COMPRBLK_FL                0x00000200 /* One or more compressed clusters */
#define EXT4_NOCOMPR_FL                 0x00000400 /* Don't compress */
#define EXT4_ECOMPR_FL                  0x00000800 /* Compression error */
/* End compression flags --- maybe not all used */
#define EXT4_INDEX_FL                   0x00001000 /* hash-indexed directory */
#define EXT4_IMAGIC_FL                  0x00002000 /* AFS directory */
#define EXT4_JOURNAL_DATA_FL            0x00004000 /* file data should be journaled */
#define EXT4_NOTAIL_FL                  0x00008000 /* file tail should not be merged */
#define EXT4_DIRSYNC_FL                 0x00010000 /* dirsync behaviour (directories only) */
#define EXT4_TOPDIR_FL                  0x00020000 /* Top of directory hierarchies*/
#define EXT4_HUGE_FILE_FL               0x00040000 /* Set to each huge file */
#define EXT4_EXTENTS_FL                 0x00080000 /* Inode uses extents */
#define EXT4_EA_INODE_FL                0x00200000 /* Inode used for large EA */
#define EXT4_EOFBLOCKS_FL               0x00400000 /* Blocks allocated beyond EOF */
#define EXT4_RESERVED_FL                0x80000000 /* reserved for ext4 lib */


struct ext4_inode {
	__le16	i_mode;		/* File mode */
	__le16	i_uid;		/* Low 16 bits of Owner Uid */
	__le32	i_size_lo;	/* Size in bytes */
	__le32	i_atime;	/* Access time */
	__le32	i_ctime;	/* Inode Change time */
	__le32	i_mtime;	/* Modification time */
	__le32	i_dtime;	/* Deletion Time */
	__le16	i_gid;		/* Low 16 bits of Group Id */
	__le16	i_links_count;	/* Links count */
	__le32	i_blocks_lo;	/* Blocks count */
	__le32	i_flags;	/* File flags */
	union {
		struct {
			__le32  l_i_version;
		} linux1;
		struct {
			__u32  h_i_translator;
		} hurd1;
		struct {
			__u32  m_i_reserved1;
		} masix1;
	} osd1;				/* OS dependent 1 */
	__le32	i_block[EXT4_N_BLOCKS];/* Pointers to blocks */
	__le32	i_generation;	/* File version (for NFS) */
	__le32	i_file_acl_lo;	/* File ACL */
	__le32	i_size_high;
	__le32	i_obso_faddr;	/* Obsoleted fragment address */
	union {
		struct {
			__le16	l_i_blocks_high; /* were l_i_reserved1 */
			__le16	l_i_file_acl_high;
			__le16	l_i_uid_high;	/* these 2 fields */
			__le16	l_i_gid_high;	/* were reserved2[0] */
			__u32	l_i_reserved2;
		} linux2;
		struct {
			__le16	h_i_reserved1;	/* Obsoleted fragment number/size which are removed in ext4 */
			__u16	h_i_mode_high;
			__u16	h_i_uid_high;
			__u16	h_i_gid_high;
			__u32	h_i_author;
		} hurd2;
		struct {
			__le16	h_i_reserved1;	/* Obsoleted fragment number/size which are removed in ext4 */
			__le16	m_i_file_acl_high;
			__u32	m_i_reserved2[2];
		} masix2;
	} osd2;				/* OS dependent 2 */
	__le16	i_extra_isize;
	__le16	i_pad1;
	__le32  i_ctime_extra;  /* extra Change time      (nsec << 2 | epoch) */
	__le32  i_mtime_extra;  /* extra Modification time(nsec << 2 | epoch) */
	__le32  i_atime_extra;  /* extra Access time      (nsec << 2 | epoch) */
	__le32  i_crtime;       /* File Creation time */
	__le32  i_crtime_extra; /* extra FileCreationtime (nsec << 2 | epoch) */
	__le32  i_version_hi;	/* high 32 bits for 64-bit version */
};

#endif
