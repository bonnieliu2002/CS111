# NAME: Bonnie Liu,Janice Tsai
# EMAIL: bonnieliu2002@g.ucla.edu,janicetsai@g.ucla.edu
# ID: 005300989,705318599

import sys,csv,math

# Define classes
class superblock:
    def __init__(self,elem):
        self.blocks_count = int(elem[1])
        self.inodes_count = int(elem[2])
        self.block_size = int(elem[3])
        self.inode_size = int(elem[4])
        self.blocks_per_group = int(elem[5])
        self.inodes_per_group = int(elem[6])
        self.first_nr_inode = int(elem[7])

class group:
    def __init__(self,elem):
        self.group_num = int(elem[1])
        self.total_blocks = int(elem[2])
        self.total_inodes = int(elem[3])
        self.num_free_blocks = int(elem[4])
        self.num_free_inodes = int(elem[5])
        self.block_num_block_bitmap = int(elem[6])
        self.block_num_inode_bitmap = int(elem[7])
        self.block_num_fbi = int(elem[8])

class inode:
    def __init__(self,elem):
        self.inode_num = int(elem[1])
        self.file_type = elem[2]
        self.mode = int(elem[3])
        self.owner = int(elem[4])
        self.group = int(elem[5])
        self.link_count = int(elem[6])
        self.last_change = elem[7]
        self.mod_time = elem[8]
        self.access_time = elem[9]
        self.file_size = int(elem[10])
        self.num_blocks = int(elem[11])
        self.blocks_to_levels = {}
        if (self.file_type == "f" or self.file_type == "d"):
            for i in range(12,27):
                if (i < 24):
                    self.blocks_to_levels[int(elem[i])] = ["BLOCK", i - 12]
                elif (i == 24):
                    self.blocks_to_levels[int(elem[i])] = ["INDIRECT BLOCK", i - 12]
                elif (i == 25):
                    self.blocks_to_levels[int(elem[i])] = ["DOUBLE INDIRECT BLOCK", 268]
                elif (i == 26):
                    self.blocks_to_levels[int(elem[i])] = ["TRIPLE INDIRECT BLOCK", 65804]
                else:
                    print_err("too many blocks")
    def process(self):
        global inconsistency_found
        if (self.mode != 0 and self.inode_num in ifree_set):
            print("ALLOCATED INODE", self.inode_num, "ON FREELIST")
            inconsistency_found = 1

class dir:
    def __init__(self,elem):
        self.parent_inode_num = int(elem[1])
        self.offset = int(elem[2])
        self.inode_num = int(elem[3])
        self.entry_len = int(elem[4])
        self.name_len = int(elem[5])
        self.name = elem[6]

class indir:
    def __init__(self,elem):
        self.inode_num = int(elem[1])
        self.level_ind = int(elem[2])
        self.offset = int(elem[3])
        self.block_num_indirect = int(elem[4])
        self.block_num_referenced = int(elem[5])

# Print error message and exit
def print_err(msg):
    sys.stderr.write(msg)
    sys.exit(1)

# Inode Allocation Audits
def perform_inode_allocation_audits(first_nr_inode_pos, inodes_count):
    global inconsistency_found
    # Check ALLOCATED
    for ino in inode_ent:
        ino.process()

    # Check UNALLOCATED
    for ino in range(first_nr_inode_pos,inodes_count):
        if ino not in allocated_ino and ino not in ifree_set:
            print("UNALLOCATED INODE", ino, "NOT ON FREELIST")
            inconsistency_found = 1

# Block Consistency Audits
def perform_block_consistency_audits(num_blocks, first_legal_block_num):
    global inconsistency_found
    # Pseudocode taken from TA slides
    # maps block number to [number of occurrences, level of indirection, inode number, and offset]
    seen={}
    for ino in inode_ent:
        # Inode is not used
        if ino.inode_num == 0:
            continue
        # Check every block in inode
        for block in ino.blocks_to_levels:
            if block == 0:
                continue
            # Check if invalid
            if block < 0 or block > num_blocks:
                print("INVALID", ino.blocks_to_levels[block][0], str(block), "IN INODE", ino.inode_num, "AT OFFSET", ino.blocks_to_levels[block][1])
                inconsistency_found = 1
            # Check if block is supposed to be reserved for stuff like superblock, group summary, free block list, etc.
            if block > 0 and block < first_legal_block_num:
                print("RESERVED", ino.blocks_to_levels[block][0], str(block), "IN INODE", ino.inode_num, "AT OFFSET", ino.blocks_to_levels[block][1])
                block_states[block] = "reserved"
                inconsistency_found = 1
            # Check if block is in free block list when it's not supposed to be
            if block in bfree_set:
                print("ALLOCATED BLOCK", str(block), "ON FREELIST")
                inconsistency_found = 1
            # If block has been seen before, print eror message and update values accordingly
            if block in seen:
                print("DUPLICATE", ino.blocks_to_levels[block][0], str(block), "IN INODE", ino.inode_num, "AT OFFSET", ino.blocks_to_levels[block][1])
                block_states[block] = "duplicate"
                seen[block][0] += 1
                inconsistency_found = 1
            # Otherwise, add block to seen
            else:
                seen[block] = [1, ino.blocks_to_levels[block][0], ino.inode_num, ino.blocks_to_levels[block][1]]
    # Traverse seen one more time and print out blocks that have duplicates
    for block in seen:
        if seen[block][0] > 1:
            print("DUPLICATE", seen[block][1], str(block), "IN INODE", str(seen[block][2]), "AT OFFSET", seen[block][3])
            inconsistency_found = 1
    # Traverse through legal blocks and if block_state is empty string, then it's an UNREFERENCED BLOCK
    for i in range(first_legal_block_num, num_blocks):
        if len(block_states[i]) == 0:
            print("UNREFERENCED BLOCK", str(i))

# Directory Consistency Audits
def perform_directory_consistency_audits(first_nr_inode_pos, inodes_count):
    global inconsistency_found
    ino_links = {} # Dict that maps inode number to number of entries pointing to inode
    ino_to_parent = {} # Dict that maps inode number to inode's parent number
    # Initialize ino_links to map all keys to 0
    for i in range(first_nr_inode_pos, first_nr_inode_pos + inodes_count + 1):
        ino_to_parent[i] = 0
    # TODO: Double check
    ino_to_parent[2]=2
    for dir in dir_ent:
        ino_links[dir.inode_num] = 0
        # Map inode to parent's inode num if directory name isn't . or ..
        if dir.name != "'.'" and dir.name != "'..'":
            ino_to_parent[dir.inode_num] = dir.parent_inode_num
    for dir in dir_ent:
        # Check for unallocated or invalid. Else increment by 1
        if dir.inode_num < 1 or dir.inode_num > inodes_count:
            print("DIRECTORY INODE", dir.parent_inode_num, "NAME", dir.name, "INVALID INODE", dir.inode_num)
            inconsistency_found = 1
        elif dir.inode_num not in allocated_ino:
            print("DIRECTORY INODE", dir.parent_inode_num, "NAME", dir.name, "UNALLOCATED INODE", dir.inode_num)
            inconsistency_found = 1
        else:
            ino_links[dir.inode_num] = ino_links[dir.inode_num] + 1
    # Incorrect inode link count
    for ino in inode_ent:
        if ino.inode_num in ino_links:
            if ino.link_count != ino_links[ino.inode_num]:
                print("INODE", ino.inode_num, "HAS", ino_links[ino.inode_num], "LINKS BUT LINKCOUNT IS", ino.link_count) 
                inconsistency_found = 1
        elif ino.inode_num not in ino_links and ino.link_count > 0:
            print("INODE", ino.inode_num, "HAS 0 LINKS BUT LINKCOUNT IS", ino.link_count)
            inconsistency_found = 1
    # Check if . is not self or if .. is not parent
    for dir in dir_ent:
        if dir.name == "'.'" and dir.parent_inode_num != dir.inode_num:
            print("DIRECTORY INODE", dir.parent_inode_num, "NAME '.' LINK TO INODE", dir.inode_num, "SHOULD BE", dir.parent_inode_num)
            inconsistency_found = 1
        if dir.name == "'..'" and ino_to_parent[dir.parent_inode_num] != dir.inode_num:
            print("DIRECTORY INODE", dir.parent_inode_num, "NAME '..' LINK TO INODE", dir.inode_num, "SHOULD BE", dir.parent_inode_num)
            inconsistency_found = 1

# Global variables
sb=None
gr=None
# original_bm=bitmap()
dir_ent=[]
indir_ent=[]
inode_ent=[]
# Use set for faster lookup time
bfree_set=set()
ifree_set=set()
allocated_ino=set()
# Keep track of state for each block. States can either be free, used, referenced, duplicate.
block_states = {}

def main():
    global inconsistency_found
    inconsistency_found = 0
    # Check arguments
    if len(sys.argv) != 2:
        print_err("Error: Invalid input. Correct usage: ./lab3b <file>\n");
    
    # Attempt to open csv file
    file_name = sys.argv[1]
    try:
        file = open(file_name, 'r')
    except:
        print_err("Error: Invalid file\n")

    # Loop through each line in csv
    lines = csv.reader(file)
    for row in lines:
        first_val = row[0]
        if first_val == "SUPERBLOCK":
            sb = superblock(row)
        elif first_val == "GROUP":
            gr = group(row)
        elif first_val == "BFREE":
            bfree_set.add(int(row[1]))
        elif first_val == "IFREE":
            ifree_set.add(int(row[1]))
        elif first_val == "INODE":
            inode_ent.append(inode(row))
        elif first_val == "DIRENT":
            dir_ent.append(dir(row))
        elif first_val == "INDIRECT":
            indir_ent.append(indir(row))
        else:
            print_err("Error: CSV file could be opened but has invalid content\n")

    # Create set of alloated inodes
    for all_ino in inode_ent:
        if all_ino.inode_num != 0:
            allocated_ino.add(all_ino.inode_num)

    # As specified in spec, calculate total num of blocks and first legal block number
    TOTAL_NUM_BLOCKS = sb.blocks_count
    FIRST_LEGAL_BLOCK_NUM = int(gr.block_num_fbi + gr.total_inodes * sb.inode_size / sb.block_size) # Confirmed this with TA so we good

    # Fill in block states for UNREFERENCED for block inconsistency audits.
    # Set them all initially to empty string
    for i in range(FIRST_LEGAL_BLOCK_NUM, sb.blocks_count):
        block_states[i] = ""
    # Traverse through all inodes. For each inode, traverse through their blocks.
    for entry in inode_ent:
        for block in entry.blocks_to_levels:
            block_states[block] = "used"
    # Traverse through indirect entries.
    for entry in indir_ent:
        block_states[entry.block_num_referenced] = "used"
    # Traverse through bitmap free block list.
    for block in bfree_set:
        block_states[block] = "free"

    # Inode allocation audits
    perform_inode_allocation_audits(sb.first_nr_inode, sb.inodes_count)

    # Block consistency audits
    perform_block_consistency_audits(TOTAL_NUM_BLOCKS, FIRST_LEGAL_BLOCK_NUM)

    # Directory consistency audits
    perform_directory_consistency_audits(sb.first_nr_inode, sb.inodes_count)

    # Exit 0 if no inconsistencies found. Exit 2 if inconsistencies found
    if inconsistency_found == 1:
        sys.exit(2)
    elif inconsistency_found == 0:
        sys.exit(0)

if __name__ == "__main__":
    main()
