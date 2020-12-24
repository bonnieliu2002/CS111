#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "ext2_fs.h"

#define OFFSET 1024 // As mentioned in TA slides
#define BYTES_PER_BLOCK 512 // As mentioned in spec

/* Generic return code - used whenever system/library call is made to check return value */
int ret;

/* File System Image Name */
char* fs;

/* Mount File Descriptor */
int fd;

/* Struct blocks from ext2_fs.h, which is provided by the spec */
struct ext2_group_desc gr; // Group
struct ext2_super_block sb; // Superblock
struct ext2_inode in; // Inode
struct ext2_dir_entry dr; // Directory

/* Pread Error Return/Exit */
void print_pread_error() {
	fprintf(stderr, "Pread error\n");
	exit(1);
}

void printTime(uint32_t seconds, char* ret) {
	// time is the number of seconds since 01/01/77
	const time_t time = (time_t) seconds;
	const struct tm* t = gmtime(&time);
	strftime(ret, sizeof("mm/dd/yy hh:mm:ss"), "%x %H:%M:%S", t);
}

/* Superblock summary */
void print_sb_summary() {
	uint32_t num_blocks, num_inodes, block_size, inode_size, blocks_per_group, inodes_per_group, fnr_inode;
	int super_block_size;
	super_block_size = sizeof(struct ext2_super_block);
	ret = pread(fd, &sb, super_block_size, OFFSET);
	if (ret < 0) {
		print_pread_error();
	}

	num_blocks = sb.s_blocks_count;
	num_inodes = sb.s_inodes_count;
	block_size = EXT2_MIN_BLOCK_SIZE << sb.s_log_block_size; // As specified in header file 
	inode_size = sb.s_inode_size;
	blocks_per_group = sb.s_blocks_per_group;
	inodes_per_group = sb.s_inodes_per_group;
	fnr_inode = sb.s_first_ino;

	fprintf(stdout, "SUPERBLOCK,%u,%u,%u,%u,%u,%u,%u\n", num_blocks, num_inodes, block_size, inode_size, blocks_per_group, inodes_per_group, fnr_inode);
}

/* Group summary */
void print_gr_summary() {
	int super_block_size;
	super_block_size = sizeof(struct ext2_super_block);
	ret = pread(fd, &gr, sizeof(struct ext2_group_desc), OFFSET + super_block_size); // As specified in TA slides
	if (ret < 0) {
		print_pread_error();
	}

	uint32_t num_blocks, num_inodes, num_free_blocks, num_free_inodes, bn_free_block_bitmap, bn_free_inode_bitmap, bn_fb_inodes;
	num_blocks = sb.s_blocks_count; // Can access using super block
	num_inodes = sb.s_inodes_count; // Can access using super block
	num_free_blocks = gr.bg_free_blocks_count;
	num_free_inodes = gr.bg_free_inodes_count;
	bn_free_block_bitmap = gr.bg_block_bitmap;
	bn_free_inode_bitmap = gr.bg_inode_bitmap;
	bn_fb_inodes = gr.bg_inode_table;

	/* Spec says that we are gauranteed to be given 1 group */
	fprintf(stdout, "GROUP,0,%u,%u,%u,%u,%u,%u,%u\n", num_blocks, num_inodes, num_free_blocks, num_free_inodes, bn_free_block_bitmap, bn_free_inode_bitmap, bn_fb_inodes);
	
}

/* Free block entries */
void print_fbe() {
	uint32_t bitmap, num_blocks, block_size;
	block_size = EXT2_MIN_BLOCK_SIZE << sb.s_log_block_size;
	bitmap = gr.bg_block_bitmap; // Block number of free block bitmap 
	num_blocks = sb.s_blocks_count; // Number of blocks

	uint32_t i;
	for (i = 1; i <= num_blocks; i++) {
		int state = 0;
		uint8_t byteNum = (i-1) / 8;
		int bitNum = (i-1) % 8;

		unsigned char buff; // Buffer to store what we read in using pread
		ret = pread(fd, &buff, sizeof(uint8_t), byteNum + bitmap*block_size);
		if (ret < 0) {
			print_pread_error();
		}

		state = ((buff >> bitNum) & 1);

		/* 0 indicates free block */
		if (state == 0) {
			fprintf(stdout, "BFREE,%d\n", i);
		}
	}
}

/* Free inode entries */
void print_fie() {
	uint32_t inode_bitmap, num_inodes, block_size;
	inode_bitmap = gr.bg_inode_bitmap;
	num_inodes = sb.s_inodes_count;
	block_size = EXT2_MIN_BLOCK_SIZE << sb.s_log_block_size;

	uint32_t i;
	for (i = 1; i <= num_inodes; i++) {
		int state = 0;
		uint8_t byteNum = (i-1) / 8;
		int bitNum = (i-1) % 8;

		unsigned char buff;
		ret = pread(fd, &buff, sizeof(uint8_t), byteNum + inode_bitmap*block_size);
		if (ret < 0) {
			print_pread_error();
		}
		
		state = ((buff >> bitNum) & 1);

		if (state == 0) {
			fprintf(stdout, "IFREE,%d\n", i);
		}
	}
}

void print_in_summary() {
	uint32_t i, j;
	char fileType;
	int BLOCK_SIZE = EXT2_MIN_BLOCK_SIZE << sb.s_log_block_size;
	for (i = 0; i < 1; i++) {
		for (j = 0; j < sb.s_inodes_count; j++) {
			ret = pread(fd, &in, sizeof(struct ext2_inode), OFFSET + sizeof(struct ext2_super_block) + 3*(EXT2_MIN_BLOCK_SIZE << sb.s_log_block_size) + j*sizeof(struct ext2_inode));
			if (ret < 0) {
				print_pread_error();
			}
			uint16_t mode = in.i_mode;
			if (mode == 0 || in.i_links_count == 0)
				continue;
			fileType = '?';
			if (S_ISLNK(mode))
				fileType = 's';
			else if (S_ISDIR(mode))
				fileType = 'd';
			else if (S_ISREG(mode))
				fileType = 'f';
			mode &= 0xFFF;
			char* creationTime = (char*) malloc(strlen("mm/dd/yy hh:mm:ss") + 1);
			char* modTime = (char*) malloc(strlen("mm/dd/yy hh:mm:ss") + 1);
			char* accessTime = (char*) malloc(strlen("mm/dd/yy hh:mm:ss") + 1);
			printTime(in.i_ctime, creationTime);
			printTime(in.i_mtime, modTime);
			printTime(in.i_atime, accessTime);
			fprintf(stdout, "INODE,%d,%c,%o,%d,%d,%d,%s,%s,%s,%d,%d", j + 1, fileType, mode, in.i_uid, in.i_gid, in.i_links_count, creationTime, modTime, accessTime, in.i_size, in.i_blocks);
			if (fileType == 'd' || fileType == 'f' || (fileType == 's' && in.i_size > 60)) {
				for (int k = 0; k < EXT2_N_BLOCKS; k++) {
					fprintf(stdout, ",%d", in.i_block[k]);
				}
			}
			fprintf(stdout, "\n");
			
			// directory entries
			if (fileType == 'd') {
				for (uint32_t k = 0; k < in.i_blocks && in.i_block[k]; k++) {
					int offset_within_dir = 0;
					while (offset_within_dir < BLOCK_SIZE) { 
						struct ext2_dir_entry dir_entry;
						pread(fd, &dir_entry, sizeof(struct ext2_dir_entry), in.i_block[k] * BLOCK_SIZE + offset_within_dir);
						if (dir_entry.inode != 0) {
							fprintf(stdout, "DIRENT,%d,%d,%d,%d,%d,\'%s\'\n", j + 1, offset_within_dir, dir_entry.inode, dir_entry.rec_len, dir_entry.name_len, dir_entry.name);
						}
						offset_within_dir += dir_entry.rec_len;
					}
				}
			}

			// indirect block references
			if (fileType == 'd' || fileType == 'f') {
				uint32_t* singlePtrs = (uint32_t*) malloc(BLOCK_SIZE);
				uint32_t* doublePtrs = (uint32_t*) malloc(BLOCK_SIZE);
				uint32_t* triplePtrs = (uint32_t*) malloc(BLOCK_SIZE);
				// single indirect blocks
				if (in.i_block[12]) {
					pread(fd, singlePtrs, BLOCK_SIZE, in.i_block[12] * BLOCK_SIZE);
					for (uint32_t x = 0; x < BLOCK_SIZE / sizeof(uint32_t); x++) {
						if (singlePtrs[x]) {
							fprintf(stdout, "INDIRECT,%d,1,%d,%d,%d\n", j + 1, x + 12, in.i_block[12], singlePtrs[x]);
						}
					}
				}
				// double indirect blocks
				if (in.i_block[13]) {
					pread(fd, doublePtrs, BLOCK_SIZE, in.i_block[13] * BLOCK_SIZE);
					for (uint32_t x = 0; x < BLOCK_SIZE / sizeof(uint32_t); x++) {
						if (doublePtrs[x]) {
							fprintf(stdout, "INDIRECT,%d,2,%d,%d,%d\n", j + 1, 12 + 256 + x * 256, in.i_block[13], doublePtrs[x]);
							pread(fd, singlePtrs, BLOCK_SIZE, doublePtrs[x] * BLOCK_SIZE);
							for (uint32_t y = 0; y < BLOCK_SIZE / sizeof(uint32_t); y++) {
								if (singlePtrs[y]) {
									fprintf(stdout, "INDIRECT,%d,1,%d,%d,%d\n", j + 1, 12 + 256 + x * 256 + y, doublePtrs[x], singlePtrs[y]);
								}
							}
						}
					}
				}
				// triple indirect blocks
				if (in.i_block[14]) {
					pread(fd, triplePtrs, BLOCK_SIZE, in.i_block[14] * BLOCK_SIZE);
					for (uint32_t x = 0; x < BLOCK_SIZE / sizeof(uint32_t); x++) {
						if (triplePtrs[x]) {
							fprintf(stdout, "INDIRECT,%d,3,%d,%d,%d\n", j + 1, 12 + 256 + 256 * 256 + 256 * 256 * x, in.i_block[14], triplePtrs[x]);
							pread(fd, doublePtrs, BLOCK_SIZE, triplePtrs[x] * BLOCK_SIZE);
							for (uint32_t y = 0; y < BLOCK_SIZE / sizeof(uint32_t); y++) {
								if (doublePtrs[y]) {
									fprintf(stdout, "INDIRECT,%d,2,%d,%d,%d\n", j + 1, 12 + 256 + 256 * 256 + x * 256 * 256 + y * 256, triplePtrs[x], doublePtrs[y]);
									pread(fd, singlePtrs, BLOCK_SIZE, doublePtrs[y] * BLOCK_SIZE);
									for (uint32_t z = 0; z < BLOCK_SIZE / sizeof(uint32_t); z++) {
										if (singlePtrs[z]) {
											fprintf(stdout, "INDIRECT,%d,1,%d,%d,%d\n", j + 1, 12 + 256 + 256 * 256 + x * 256 * 256 + y * 256 + z, doublePtrs[y], singlePtrs[z]);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

int main(int argc, char**argv) {
	/* Process arguments */
	if (argc != 2) {
		fprintf(stderr, "Correct Usage: ./lab3a <file_system_img>\n");
		exit(1);
	}

	fs = argv[1];

	/* Verify that file system image specified is valid */
	fd = open(fs, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Error, invalid file system image: %s\n", fs);
		exit(1);
	}

	/* File system image opened so print summaries */
	print_sb_summary();
	print_gr_summary();
	print_fbe();
	print_fie(); 
	print_in_summary(); // Inode summary, Directory entries, and Indirect block references

    exit(0);
} 
