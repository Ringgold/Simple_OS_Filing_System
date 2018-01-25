#include "sfs_api.h"
#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <strings.h>
#include "disk_emu.h"
#define LIN_RINGGOLD_DISK "sfs_disk.disk"
#define BLOCK_SIZE 1024
#define INODE_SIZE 100
#define TOTAL_NUM_BLOCKS_INODETABLE 8
#define TOTAL_NUM_OF_ROOTB 3
#define START_ADD_OF_DATABLOCKS 9
#define NUM_ADDRESSES_INDIRECT (BLOCK_SIZE/sizeof(int))

void* buffer;
int pointer = 0;
struct superblock_t superblock;
struct file_descriptor fd_table[INODE_SIZE];
struct directory_entry rootDir[INODE_SIZE-1];
struct inode_t in_table[INODE_SIZE];


// help functions
void writeInTable() {
	buffer = (void*)malloc(TOTAL_NUM_BLOCKS_INODETABLE*BLOCK_SIZE);
	memset(buffer, 0, TOTAL_NUM_BLOCKS_INODETABLE*BLOCK_SIZE);
	memcpy(buffer, in_table, (INODE_SIZE)*(sizeof(inode_t)));
	write_blocks(1, TOTAL_NUM_BLOCKS_INODETABLE, buffer);
	free(buffer);
}

void writeRootDir() {
	buffer = (void*)malloc(TOTAL_NUM_OF_ROOTB*BLOCK_SIZE);
	memset(buffer, 0, TOTAL_NUM_OF_ROOTB*BLOCK_SIZE);
	memcpy(buffer, rootDir, (INODE_SIZE-1)*(sizeof(directory_entry)));
	write_blocks(START_ADD_OF_DATABLOCKS, TOTAL_NUM_OF_ROOTB, buffer);
	free(buffer);
}

void writeBitMap() {
	buffer = (void*)malloc(BLOCK_SIZE);
	memset(buffer, 1, BLOCK_SIZE);
	memcpy(buffer, free_bit_map, (BLOCK_SIZE/8)*(sizeof(uint8_t)));
	write_blocks(NUM_BLOCKS - 1, 1, buffer);
	free(buffer);
}


// main functions
void mksfs(int fresh) {
	if (fresh) {
		init_fresh_disk(LIN_RINGGOLD_DISK, BLOCK_SIZE, NUM_BLOCKS);

		// superblock
		superblock.magic = 0xACBD0005;
		superblock.block_size = BLOCK_SIZE;
		superblock.fs_size = NUM_BLOCKS;
		superblock.inode_table_len = INODE_SIZE;
		superblock.root_dir_inode = 0;

		buffer = (void*)malloc(BLOCK_SIZE);
		memset(buffer, 0, BLOCK_SIZE);
		memcpy(buffer, &superblock, sizeof(superblock_t));
		write_blocks(0, 1, buffer);
		free(buffer);
		force_set_index(0);

		// inode
		in_table[0].size = 0;
		in_table[0] = (inode_t) { 0777, 0, 0, 0, 0, { START_ADD_OF_DATABLOCKS, START_ADD_OF_DATABLOCKS + 1, START_ADD_OF_DATABLOCKS + 2, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, -1 };
		for (int i = 1; i<INODE_SIZE; i++)
			in_table[i] = (inode_t) { 0777, 0, 0, 0, -1, { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, -1 };

		writeInTable();
		for (int i = 0; i<TOTAL_NUM_BLOCKS_INODETABLE; i++)
			force_set_index(i + 1); // 0 is for superblock

		// directory
		for (int i = 0; i<INODE_SIZE-1; i++) {
			rootDir[i].num = -1;
			memset(rootDir[i].name, '\0', sizeof(rootDir[i].name));
		}
		writeRootDir();
		// freeBitMap
		for (int i = 0; i<TOTAL_NUM_OF_ROOTB; i++) {
			force_set_index(START_ADD_OF_DATABLOCKS + i);
		}

		// file descriptors
		fd_table[0] = (file_descriptor) { 0, &in_table[0], 0 };
		for (int i = 1; i<INODE_SIZE; i++) {
			fd_table[i] = (file_descriptor) { -1, NULL, 0 };
		}

		writeBitMap();
		force_set_index(NUM_BLOCKS - 1);
	}
	else {
		init_disk(LIN_RINGGOLD_DISK, BLOCK_SIZE, NUM_BLOCKS);

		//superblock
		buffer = (void*)malloc(BLOCK_SIZE);
		memset(buffer, 0, BLOCK_SIZE);
		read_blocks(0, 1, buffer);
		memcpy(&superblock, buffer, sizeof(superblock_t));
		free(buffer);
		force_set_index(0);

		//inode
		buffer = (void*)malloc(TOTAL_NUM_BLOCKS_INODETABLE*BLOCK_SIZE);
		memset(buffer, 0, TOTAL_NUM_BLOCKS_INODETABLE*BLOCK_SIZE);
		read_blocks(1, TOTAL_NUM_BLOCKS_INODETABLE, buffer);
		memcpy(in_table, buffer, INODE_SIZE * sizeof(inode_t));
		free(buffer);

		for (int i = 0; i<TOTAL_NUM_BLOCKS_INODETABLE; i++)
			force_set_index(i + 1);

		//root directory
		buffer = (void*)malloc(TOTAL_NUM_OF_ROOTB*BLOCK_SIZE);
		memset(buffer, 0, TOTAL_NUM_OF_ROOTB*BLOCK_SIZE);
		read_blocks(START_ADD_OF_DATABLOCKS, TOTAL_NUM_OF_ROOTB, buffer);
		memcpy(rootDir, buffer, INODE_SIZE-1 * sizeof(directory_entry));
		free(buffer);
		for (int i = 0; i<TOTAL_NUM_OF_ROOTB; i++)
			force_set_index(i + START_ADD_OF_DATABLOCKS);

		//freebitmap
		buffer = (void*)malloc(BLOCK_SIZE);
		memset(buffer, 1, BLOCK_SIZE);
		read_blocks(NUM_BLOCKS - 1, 1, buffer);
		memcpy(free_bit_map, buffer, (BLOCK_SIZE / 8)*(sizeof(uint8_t)));
		free(buffer);
		force_set_index(NUM_BLOCKS - 1);

		// init file descriptors
		fd_table[0] = (file_descriptor) { 0, &in_table[0], 0 };
		for (int i = 1; i<INODE_SIZE; i++) {
			fd_table[i] = (file_descriptor) { -1, NULL, 0 };
		}
	}
}

int sfs_getnextfilename(char *fname) {//similar to get cmd
	if (pointer >= INODE_SIZE-1) {
		pointer = 0;
		return 0;
	}

	while (rootDir[pointer].num == -1) {
		pointer++;
		if (pointer >= INODE_SIZE-1) {
			pointer = 0;
			return 0;
		}
	}

	memcpy(fname, rootDir[pointer].name, MAX_FILE_NAME);
	pointer++;
	return 1;
}

int sfs_getfilesize(const char* path) {
	int length = strlen(path);
	if (length > MAX_FILE_NAME)
		return -1;
	char copy[length + 1];
	memset(copy, '\0', sizeof(copy));
	strcpy(copy, path);
	char* temp;
	const char s[2] = ".";
	temp = strtok(copy, s); 
	temp = strtok(NULL, s); 
	if (strlen(temp) > MAX_EXTENSION_NAME)
		return -1;

	for (int i = 0; i<INODE_SIZE-1; i++) {
		if (rootDir[i].num != -1 && !strcmp(rootDir[i].name, path))
			return in_table[rootDir[i].num].size;
	}
	return -1;
}

int sfs_fopen(char *name) { 
	int length = strlen(name);
	if (length > MAX_FILE_NAME)
		return -1;
	char copy[length + 1];
	memset(copy, '\0', sizeof(copy));
	strcpy(copy, name);
	char* temp;
	const char s[2] = ".";
	temp = strtok(copy, s);
	temp = strtok(NULL, s);
	if (strlen(temp) > MAX_EXTENSION_NAME)
		return -1;

	int iNodeIndex = -1;
	int exists = 0;
	int i;
	for (i = 0; i<INODE_SIZE-1; i++) {
		if (!strcmp(rootDir[i].name, name)) {
			exists = 1;
			iNodeIndex = rootDir[i].num;
			break;
		}
	}

	if (exists) {
		for (i = 1; i<INODE_SIZE; i++) {
			if (fd_table[i].inodeIndex == iNodeIndex) {
				return i; 
			}
		}
	}
	else {
		// find slot in root direcory
		int fileIndex = -1;
		for (i = 0; i<INODE_SIZE-1; i++) {
			if (rootDir[i].num < 0) {
				fileIndex = i;
				break;
			}
		}
		if (fileIndex == -1) {
			return -1;
		}

		// find unused inode
		for (i = 1; i<INODE_SIZE; i++) {
			if (in_table[i].size == -1) {
				iNodeIndex = i;
				break;
			}
		}

		strcpy(rootDir[fileIndex].name, name);
		rootDir[fileIndex].num = iNodeIndex;
		in_table[iNodeIndex].size = 0;
		writeInTable();
		writeRootDir();
	}

	// not yet open
	int fdIndex = -1;
	for (i = 1; i<INODE_SIZE; i++) {
		if (fd_table[i].inodeIndex == -1) {
			fdIndex = i;
			break;
		}
	}

	int rwptr = in_table[iNodeIndex].size;
	fd_table[fdIndex] = (file_descriptor) { iNodeIndex, &in_table[iNodeIndex], rwptr };

	return fdIndex;
}

int sfs_fclose(int fileID) {
	if (fileID <= 0 || fileID > INODE_SIZE-1) {// check if 0 is used
		return -1;
	}

	if (fd_table[fileID].inodeIndex == -1) {// prevent second close
		return -1;
	}

	fd_table[fileID] = (file_descriptor) { -1, NULL, 0 };
	return 0;
}

int sfs_fread(int fileID, char *buf, int length) {
	file_descriptor *currentDir;
	currentDir = &fd_table[fileID];

	// check that file is open
	if ((*currentDir).inodeIndex == -1) {
		return -1;
	}

	inode_t *currentIn;
	currentIn = (*currentDir).inode;
	int start_address = (*currentDir).rwptr / BLOCK_SIZE;
	int start_index = (*currentDir).rwptr % BLOCK_SIZE;
	int endRwptr;
	if ((*currentDir).rwptr + length > (*currentIn).size) {
		endRwptr = (*currentIn).size;
	}
	else {
		endRwptr = (*currentDir).rwptr + length;
	}
	int endBlockIndex = endRwptr / BLOCK_SIZE;
	int endIndexInBlock = endRwptr % BLOCK_SIZE;

	int biteCount = 0;
	buffer = (void*)malloc(BLOCK_SIZE);
	int addresses[NUM_ADDRESSES_INDIRECT]; 
	int dirInitFlag = 0;

	for (int i = start_address; i <= endBlockIndex; i++) {
		memset(buffer, 0, BLOCK_SIZE);
		if (i > 11) {
			// indirect pointers
			if (!dirInitFlag) {
				read_blocks((*currentIn).indirectPointer, 1, buffer);
				memcpy(addresses, buffer, BLOCK_SIZE);
				memset(buffer, 0, BLOCK_SIZE);
				dirInitFlag = 1;
			}

			read_blocks(addresses[i - 12], 1, buffer);
			if (i == start_address) {
				if (start_address == endBlockIndex) {
					memcpy(buf, buffer + start_index, endIndexInBlock - start_index);
					biteCount += endIndexInBlock - start_index;
				}
				else {
					memcpy(buf, buffer + start_index, BLOCK_SIZE - start_index);
					biteCount += BLOCK_SIZE - start_index;
				}
			}
			else if (i == endBlockIndex) {
				memcpy(buf + biteCount, buffer, endIndexInBlock);
				biteCount += endIndexInBlock;

			}
			else {
				memcpy(buf + biteCount, buffer, BLOCK_SIZE);
				biteCount += BLOCK_SIZE;
			}
		}
		else {
			// direct pointers
			read_blocks((*currentIn).data_ptrs[i], 1, buffer);
			if (i == start_address) {
				if (start_address == endBlockIndex) {
					memcpy(buf, buffer + start_index, endIndexInBlock - start_index);
					biteCount += endIndexInBlock - start_index;
				}
				else {
					memcpy(buf, buffer + start_index, BLOCK_SIZE - start_index);
					biteCount += BLOCK_SIZE - start_index;
				}
			}
			else if (i == endBlockIndex) {
				memcpy(buf + biteCount, buffer, endIndexInBlock);
				biteCount += endIndexInBlock;
			}
			else {
				memcpy(buf + biteCount, buffer, BLOCK_SIZE);
				biteCount += BLOCK_SIZE;
			}
		}
	}
	(*currentDir).rwptr += biteCount;
	free(buffer);
	return biteCount;
}

int sfs_fwrite(int fileID, const char *buf, int length) {
	file_descriptor *currentDir;
	currentDir = &fd_table[fileID];

	inode_t *currentIn;
	currentIn = (*currentDir).inode;

	int rwptr = (*currentDir).rwptr;
	int start_address = rwptr / BLOCK_SIZE;
	int start_index = rwptr % BLOCK_SIZE;
	int endRwptr = rwptr + length;
	int endBlockIndex = endRwptr / BLOCK_SIZE;
	int endIndexInBlock = endRwptr % BLOCK_SIZE;

	//set up the pointers' varaible and condition 
	int biteCount = 0;
	buffer = (void*)malloc(BLOCK_SIZE);
	int addresses[NUM_ADDRESSES_INDIRECT];
	int dirInitFlag = 0;
	int indirectModFlag = 0;

	int fullError = 0;
	for (int i = start_address; i <= endBlockIndex; i++) {
		memset(buffer, 0, BLOCK_SIZE);
		if (i > 11) {// indirect pointers
			if (!dirInitFlag) {
				// reading indirect block, initializing addresses
				if ((*currentIn).indirectPointer == -1) {
					if (get_index() > 1023 || get_index() < 0) {
						fullError = 1;
						break;
					}
					(*currentIn).indirectPointer = get_index();
					force_set_index((*currentIn).indirectPointer);
					int index;
					for (index = 0; index<NUM_ADDRESSES_INDIRECT; index++)
						addresses[index] = -1;
					indirectModFlag = 1;
				}
				else {
					read_blocks((*currentIn).indirectPointer, 1, buffer);
					memcpy(addresses, buffer, BLOCK_SIZE);
					memset(buffer, 0, BLOCK_SIZE);
				}
				dirInitFlag = 1;
			}
			// write data
			if (i-12 >= NUM_ADDRESSES_INDIRECT) {
				fullError = 1;
				break;
			}
			if (addresses[i - 12] == -1) {
				if (get_index() > 1023 || get_index() < 0) {
					fullError = 1;
					break;
				}
				addresses[i - 12] = get_index();
				force_set_index(addresses[i - 12]);
				indirectModFlag = 1;
			}
			read_blocks(addresses[i - 12], 1, buffer);
			if (i == start_address) {
				if (start_address == endBlockIndex) {
					memcpy(buffer + start_index, buf, endIndexInBlock - start_index);
					biteCount += endIndexInBlock - start_index;
				}
				else {
					memcpy(buffer + start_index, buf, BLOCK_SIZE - start_index);
					biteCount += BLOCK_SIZE - start_index;
				}
			}
			else if (i == endBlockIndex) {
				memcpy(buffer, buf + biteCount, endIndexInBlock);
				biteCount += endIndexInBlock;
			}
			else {
				memcpy(buffer, buf + biteCount, BLOCK_SIZE);
				biteCount += BLOCK_SIZE;
			}
			if (addresses[i - 12] < 0 || addresses[i - 12] > 1023) {
				fullError = 1;
				break;
			}
			else {
				write_blocks(addresses[i - 12], 1, buffer);
			}
		}
		else {
			// direct
			if ((*currentIn).data_ptrs[i] == -1) {
				if (get_index() > 1023 || get_index() < 0) {
					fullError = 1;
					break;
				}
				(*currentIn).data_ptrs[i] = get_index();
				force_set_index((*currentIn).data_ptrs[i]);
			}
			read_blocks((*currentIn).data_ptrs[i], 1, buffer);
			if (i == start_address) {
				if (start_address == endBlockIndex) {
					memcpy(buffer + start_index, buf, endIndexInBlock - start_index);
					biteCount += endIndexInBlock - start_index;
				}
				else {
					memcpy(buffer + start_index, buf, BLOCK_SIZE - start_index);
					biteCount += BLOCK_SIZE - start_index;
				}
			}
			else if (i == endBlockIndex) {
				memcpy(buffer, buf + biteCount, endIndexInBlock);
				biteCount += endIndexInBlock;
			}
			else {
				// write BLOCK_SIZE bytes from buf+biteCount to buffer
				memcpy(buffer, buf + biteCount, BLOCK_SIZE);
				biteCount += BLOCK_SIZE;
			}
			if ((*currentIn).data_ptrs[i] < 0 || (*currentIn).data_ptrs[i] > 1023) {
				fullError = 1;
				break;
			}
			else {
				write_blocks((*currentIn).data_ptrs[i], 1, buffer);
			}
		}
	}

	// write indirectblock back to disk
	if (indirectModFlag) {
		memset(buffer, 0, BLOCK_SIZE);
		memcpy(buffer, addresses, BLOCK_SIZE);
		if ((*currentIn).indirectPointer < 0 || (*currentIn).indirectPointer > 1023) {
			fullError = 1;
		}
		else {
			write_blocks((*currentIn).indirectPointer, 1, buffer);
		}
	}

	(*currentDir).rwptr += biteCount;
	if ((*currentIn).size < (*currentDir).rwptr) {
		(*currentIn).size = (*currentDir).rwptr;
	}
	free(buffer);
	writeInTable();
	writeBitMap();

	if (fullError)
		return -1;
	else
		return biteCount;
}

int sfs_fseek(int fileID, int loc) {
	if (fileID <= 0 || fileID > INODE_SIZE-1) {
		return -1;
	}

	if (fd_table[fileID].inodeIndex == -1) {
		return -1;
	}

	if (loc < 0) {
		fd_table[fileID].rwptr = 0;
	}
	else if (loc > in_table[fd_table[fileID].inodeIndex].size) {
		fd_table[fileID].rwptr = in_table[fd_table[fileID].inodeIndex].size;
	}
	else {
		fd_table[fileID].rwptr = loc;
	}

	return 0;
}

int sfs_remove(char *file) {
	int i;
	int iNodeIndex = -1;
	for (i = 0; i<INODE_SIZE-1; i++) {
		if (!strcmp(rootDir[i].name, file)) {
			iNodeIndex = rootDir[i].num;
			rootDir[i].num = -1;
			memset(rootDir[i].name, '\0', sizeof(rootDir[i].name));
			break;
		}
	}

	if (i == INODE_SIZE-1) {
		return -1;
	}

	for (i = 1; i<INODE_SIZE; i++) {
		if (fd_table[i].inodeIndex == iNodeIndex) {
			fd_table[i] = (file_descriptor) { -1, NULL, 0 };
		}
	}

	inode_t *currentIn;
	currentIn = &in_table[iNodeIndex];
	int endBlockIndex = (*currentIn).size / BLOCK_SIZE;
	int addresses[NUM_ADDRESSES_INDIRECT];
	int dirInitFlag = 0;
	int indirectBlockIndex;
	int indirectModFlag = 0;

	buffer = (void*)malloc(BLOCK_SIZE);
	for (i = 0; i <= endBlockIndex; i++) {
		memset(buffer, 0, BLOCK_SIZE);
		if (i>11) { //indrect
			if (!dirInitFlag) {
				if ((*currentIn).indirectPointer == -1) {
					break;
				}
				read_blocks((*currentIn).indirectPointer, 1, buffer);
				memcpy(addresses, buffer, BLOCK_SIZE);
				memset(buffer, 0, BLOCK_SIZE);
				dirInitFlag = 1;
			}

			indirectBlockIndex = i - 12;
			if (addresses[indirectBlockIndex] == -1) {
				break;
			}
			write_blocks(addresses[indirectBlockIndex], 1, buffer);
			rm_index(addresses[indirectBlockIndex]);
			addresses[indirectBlockIndex] = -1;
			indirectModFlag = 1;
		}
		else {//direct
			write_blocks((*currentIn).data_ptrs[i], 1, buffer);
			rm_index((*currentIn).data_ptrs[i]);
			(*currentIn).data_ptrs[i] = -1;
		}
	}

	// write indirectblock
	if (indirectModFlag) {
		memset(buffer, 0, BLOCK_SIZE);
		memcpy(buffer, addresses, BLOCK_SIZE);
		write_blocks((*currentIn).indirectPointer, 1, buffer);
		indirectModFlag = 0;
	}

	if (dirInitFlag) { // indirectPointer flag 1
		rm_index((*currentIn).indirectPointer);
		(*currentIn).indirectPointer = -1;
	}
	(*currentIn).size = -1;
	free(buffer);
	writeInTable();
	writeBitMap();
	writeRootDir();
	return 0;
}