#include "BPlusTree.h"

#include <cstring>

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op) {

    IndexId searchIndex;

    AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);

    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    int block, index;

    if (searchIndex.block == -1 && searchIndex.index == -1) {

        block = attrCatEntry.rootBlock;
        index = 0;

        if (block == -1) {
            return RecId{-1, -1};
        }

    } else {
        /*a valid searchIndex points to an entry in the leaf index of the attribute's
        B+ Tree which had previously satisfied the op for the given attrVal.*/

        block = searchIndex.block;
        index = searchIndex.index + 1;  // search is resumed from the next index.

        IndLeaf leaf(block);
        HeadInfo leafHead;
        leaf.getHeader(&leafHead);

        if (index >= leafHead.numEntries) {

            // update block to rblock of current block and index to 0.
            block = leafHead.rblock;
            index = 0;

            if (block == -1) {
                return RecId{-1, -1};
            }
        }
    }

    int blocktype = StaticBuffer::getStaticBlockType(block);

    while(blocktype == IND_INTERNAL) {  

        IndInternal internalBlk(block);

        HeadInfo intHead;
        internalBlk.getHeader(&intHead);

        InternalEntry intEntry;

        if (op == NE || op == LT || op == LE) {

            internalBlk.getEntry(&intEntry, 0);
            block = intEntry.lChild;
        } else {
            
            int idx;
            for(idx = 0; idx < intHead.numEntries; idx++){
                internalBlk.getEntry(&intEntry, idx);
                int cmp = compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType);
                
                if ((op == EQ || op == GE) && cmp >= 0) break;
                if (op == GT && cmp > 0) break;
            }


            if (idx < intHead.numEntries) {
                block =  intEntry.lChild;
            } else {
                internalBlk.getEntry(&intEntry, intHead.numEntries - 1);
                block =  intEntry.rChild;
            }
        }
        blocktype = StaticBuffer::getStaticBlockType(block);
    }


    while (block != -1) {
        IndLeaf leafBlk(block);
        HeadInfo leafHead;

        leafBlk.getHeader(&leafHead);
        Index leafEntry;

        while (index < leafHead.numEntries) {

            leafBlk.getEntry(&leafEntry, index);


            int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType);

            if(
                (op == EQ && cmpVal == 0) || (op == LE && cmpVal <= 0) || (op == LT && cmpVal < 0) ||
                (op == GT && cmpVal > 0) || (op == GE && cmpVal >= 0) || (op == NE && cmpVal != 0)
            ){
                searchIndex = {block, index};
                AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);
                return RecId{leafEntry.block, leafEntry.slot};
            } else if ((op == EQ || op == LE || op == LT) && cmpVal > 0) {
                return RecId {-1, -1};
            }

            ++index;
        }

      
        if (op != NE) {
            break;
        }

        // block = next block in the linked list, i.e., the rblock in leafHead.
        // update index to 0.
        block = leafHead.rblock;
        index = 0;
    }

    return {-1,-1};
}

int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]) {

    if(relId == RELCAT_RELID || relId == ATTRCAT_RELID){
        return E_NOTPERMITTED;
    }

    AttrCatEntry attrcatentry;
    int retVal = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrcatentry);

    if(retVal != SUCCESS){
        return retVal;
    }

    if (attrcatentry.rootBlock != -1) {
        return SUCCESS;
    }

    /******Creating a new B+ Tree ******/

    IndLeaf rootBlockBuf;


    int rootBlock = rootBlockBuf.getBlockNum();

    if (rootBlock == E_DISKFULL) {
        return E_DISKFULL;
    }

    attrcatentry.rootBlock = rootBlock;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrcatentry);

    RelCatEntry relCatEntry;

    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    int block = relCatEntry.firstBlk;

    /***** Traverse all the blocks in the relation and insert them one
           by one into the B+ Tree *****/
    while (block != -1) {

        RecBuffer blockbuffer(block);
        unsigned char slotMap[relCatEntry.numSlotsPerBlk];

        blockbuffer.getSlotMap(slotMap);
        
        for(int slot  = 0; slot < relCatEntry.numSlotsPerBlk; slot++)
        {   
            if(slotMap[slot] == SLOT_OCCUPIED){
                Attribute record[relCatEntry.numAttrs];
                blockbuffer.getRecord(record, slot);

                RecId recId = RecId{block, slot};

                retVal = bPlusInsert(relId, attrName, record[attrcatentry.offset], recId);

                if(retVal == E_DISKFULL){
                    return E_DISKFULL;
                }
            }
        }

        HeadInfo head;
        blockbuffer.getHeader(&head);

        block = head.rblock;
    }

    return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum) {
    if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    int type = StaticBuffer::getStaticBlockType(rootBlockNum);

    if (type == IND_LEAF) {
        IndLeaf leaf(rootBlockNum);
        leaf.releaseBlock();
        return SUCCESS;

    } else if (type == IND_INTERNAL) {
        IndInternal internalBlk(rootBlockNum);

        HeadInfo head;
        internalBlk.getHeader(&head);

        InternalEntry entry;

        // first entry: destroy lChild, then its rChild
        internalBlk.getEntry(&entry, 0);
        bPlusDestroy(entry.lChild);

        // every entry: destroy rChild only (rChild of i == lChild of i+1)
        for (int i = 0; i < head.numEntries; i++) {
            internalBlk.getEntry(&entry, i);
            bPlusDestroy(entry.rChild);
        }

        internalBlk.releaseBlock();
        return SUCCESS;

    } else {
        return E_INVALIDBLOCK;
    }
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId) {

    AttrCatEntry attrcatbuffer;
    int retVal = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrcatbuffer);

    if(retVal != SUCCESS){
        return retVal;
    }

    int rootblk = attrcatbuffer.rootBlock;

    if (rootblk == -1) {
        return E_NOINDEX;
    }

    int leafBlkNum = findLeafToInsert(rootblk, attrVal, attrcatbuffer.attrType);

    struct Index entry;
    memcpy(&entry.attrVal, &attrVal, sizeof(Attribute));
    entry.block = recId.block;
    entry.slot = recId.slot;

    if (insertIntoLeaf(relId, attrName, leafBlkNum, entry) == E_DISKFULL) {

        AttrCacheTable::getAttrCatEntry(relId, attrName, &attrcatbuffer);
        BPlusTree::bPlusDestroy(attrcatbuffer.rootBlock);

        attrcatbuffer.rootBlock = -1;
        AttrCacheTable::setAttrCatEntry(relId, attrName, &attrcatbuffer);

        return E_DISKFULL;
    }

    return SUCCESS;
}

int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType) {
    int blockNum = rootBlock;

    int blocktype = StaticBuffer::getStaticBlockType(blockNum);

    while (blocktype != IND_LEAF){  

        IndInternal internalBlock(blockNum);

        HeadInfo blockHeader;
        internalBlock.getHeader(&blockHeader);

        int index = 0;
        while (index < blockHeader.numEntries)
        {
            InternalEntry entry;
            internalBlock.getEntry(&entry, index);

            if (compareAttrs(attrVal, entry.attrVal, attrType) <= 0)
                break;

            index++;
        }

        if (index == blockHeader.numEntries) {
            InternalEntry entry;
            internalBlock.getEntry(&entry, blockHeader.numEntries-1);

            blockNum = entry.rChild;

        } else {
            InternalEntry entry;
            internalBlock.getEntry(&entry, index);

            blockNum = entry.lChild;
        }

        blocktype = StaticBuffer::getStaticBlockType(blockNum);
    }

    return blockNum;
}

int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry) {

    AttrCatEntry attrCatEntry;
    int retVal = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (retVal != SUCCESS) {
        return retVal;
    }

    IndLeaf leaf(blockNum);

    HeadInfo blockHeader;
    leaf.getHeader(&blockHeader);

    Index indices[blockHeader.numEntries + 1];

    // merge existing entries + new entry in ascending order
    int k = 0;
    bool placed = false;
    for (int i = 0; i < blockHeader.numEntries; i++) {
        Index existing;
        leaf.getEntry(&existing, i);

        if (!placed && compareAttrs(existing.attrVal, indexEntry.attrVal, attrCatEntry.attrType) > 0) {
            indices[k++] = indexEntry;
            placed = true;
        }
        indices[k++] = existing;
    }
    if (!placed) {
        indices[k++] = indexEntry;
    }

    if (blockHeader.numEntries != MAX_KEYS_LEAF) {
        blockHeader.numEntries++;
        leaf.setHeader(&blockHeader);

        for (int i = 0; i < blockHeader.numEntries; i++) {
            leaf.setEntry(&indices[i], i);
        }
        return SUCCESS;
    }

    // leaf is full: split
    int newRightBlk = splitLeaf(blockNum, indices);
    if (newRightBlk == E_DISKFULL) {
        return E_DISKFULL;
    }

    if (blockHeader.pblock != -1) {
        InternalEntry internalEntry;
        internalEntry.attrVal = indices[MIDDLE_INDEX_LEAF].attrVal;
        internalEntry.lChild = blockNum;
        internalEntry.rChild = newRightBlk;

        retVal = insertIntoInternal(relId, attrName, blockHeader.pblock, internalEntry);
    } else {
        retVal = createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal,
                               blockNum, newRightBlk);
    }

    if (retVal == E_DISKFULL) {
        BPlusTree::bPlusDestroy(newRightBlk);
        return E_DISKFULL;
    }
    return SUCCESS;
}

int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]) {

    IndLeaf rightBlk;                 // constructor 1: allocates a new leaf block
    IndLeaf leftBlk(leafBlockNum);    // constructor 2: existing leaf block

    int rightBlkNum = rightBlk.getBlockNum();
    int leftBlkNum = leftBlk.getBlockNum();

    if (rightBlkNum == E_DISKFULL) {
        return E_DISKFULL;
    }

    HeadInfo leftBlkHeader, rightBlkHeader;
    leftBlk.getHeader(&leftBlkHeader);
    rightBlk.getHeader(&rightBlkHeader);

    rightBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    rightBlkHeader.pblock = leftBlkHeader.pblock;
    rightBlkHeader.lblock = leftBlkNum;
    rightBlkHeader.rblock = leftBlkHeader.rblock;   // old right neighbour
    rightBlk.setHeader(&rightBlkHeader);

    if (rightBlkHeader.rblock != -1) {                                                                                                                                                                       
        IndLeaf oldRightBlk(rightBlkHeader.rblock);                                                                                                                                                          
        HeadInfo oldRightBlkHeader;                                                                                                                                                                          
        oldRightBlk.getHeader(&oldRightBlkHeader);                                                                                                                                                           
        oldRightBlkHeader.lblock = rightBlkNum;                                                                                                                                                              
        oldRightBlk.setHeader(&oldRightBlkHeader);                                                                                                                                                           
    } 

    leftBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    leftBlkHeader.rblock = rightBlkNum;
    leftBlk.setHeader(&leftBlkHeader);

    int half = (MAX_KEYS_LEAF + 1) / 2;
    for (int i = 0; i < half; i++) {
        leftBlk.setEntry(&indices[i], i);
        rightBlk.setEntry(&indices[half + i], i);
    }

    return rightBlkNum;
}

int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int intBlockNum, InternalEntry intEntry) {

    AttrCatEntry attrCatEntry;
    int retVal = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (retVal != SUCCESS) {
        return retVal;
    }

    IndInternal intBlk(intBlockNum);

    HeadInfo blockHeader;
    intBlk.getHeader(&blockHeader);

    InternalEntry internalEntries[blockHeader.numEntries + 1];

    // find where the new entry goes (first existing key greater than it)
    int pos = blockHeader.numEntries;
    for (int i = 0; i < blockHeader.numEntries; i++) {
        InternalEntry tmp;
        intBlk.getEntry(&tmp, i);
        if (compareAttrs(tmp.attrVal, intEntry.attrVal, attrCatEntry.attrType) > 0) {
            pos = i;
            break;
        }
    }

    // entries before pos, then the new entry, then the rest shifted by one
    for (int i = 0; i < pos; i++) {
        intBlk.getEntry(&internalEntries[i], i);
    }
    internalEntries[pos] = intEntry;
    for (int i = pos; i < blockHeader.numEntries; i++) {
        intBlk.getEntry(&internalEntries[i + 1], i);
    }

    // entry after the new one must start at the new entry's rChild
    if (pos < blockHeader.numEntries) {
        internalEntries[pos + 1].lChild = intEntry.rChild;
    }

    if (blockHeader.numEntries != MAX_KEYS_INTERNAL) {
        blockHeader.numEntries++;
        intBlk.setHeader(&blockHeader);

        for (int i = 0; i < blockHeader.numEntries; i++) {
            intBlk.setEntry(&internalEntries[i], i);
        }
        return SUCCESS;
    }

    // block is full: split
    int newRightBlk = splitInternal(intBlockNum, internalEntries);

    if (newRightBlk == E_DISKFULL) {
        // free the not-yet-attached subtree built by the split below
        BPlusTree::bPlusDestroy(intEntry.rChild);
        return E_DISKFULL;
    }

    if (blockHeader.pblock != -1) {
        InternalEntry parentEntry;
        parentEntry.attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;
        parentEntry.lChild = intBlockNum;
        parentEntry.rChild = newRightBlk;

        retVal = insertIntoInternal(relId, attrName, blockHeader.pblock, parentEntry);
        if (retVal == E_DISKFULL) {
            BPlusTree::bPlusDestroy(newRightBlk);
            return E_DISKFULL;
        }
    } else {
        retVal = createNewRoot(relId, attrName, internalEntries[MIDDLE_INDEX_INTERNAL].attrVal, intBlockNum, newRightBlk);
    }

    if (retVal == E_DISKFULL) {
        return E_DISKFULL;
    }
    return SUCCESS;
}

int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[]) {

    IndInternal rightBlk;                  // constructor 1: allocates a new internal block
    IndInternal leftBlk(intBlockNum);      // constructor 2: existing block

    int rightBlkNum = rightBlk.getBlockNum();
    int leftBlkNum = leftBlk.getBlockNum();

    if (rightBlkNum == E_DISKFULL) {
        return E_DISKFULL;
    }

    HeadInfo leftBlkHeader, rightBlkHeader;
    leftBlk.getHeader(&leftBlkHeader);
    rightBlk.getHeader(&rightBlkHeader);

    rightBlkHeader.numEntries = MAX_KEYS_INTERNAL / 2;
    rightBlkHeader.pblock = leftBlkHeader.pblock;
    rightBlk.setHeader(&rightBlkHeader);

    leftBlkHeader.numEntries = MAX_KEYS_INTERNAL / 2;
    leftBlk.setHeader(&leftBlkHeader);

    int half = MAX_KEYS_INTERNAL / 2;   // 50

    // left gets entries 0..49, right gets entries 51..100 (50 moves up)
    for (int i = 0; i < half; i++) {
        leftBlk.setEntry(&internalEntries[i], i);
        rightBlk.setEntry(&internalEntries[half + 1 + i], i);
    }

    // children of the right block now have a new parent
    int type = StaticBuffer::getStaticBlockType(internalEntries[0].lChild);

    // child pointers of the right block: lChild of each entry, plus rChild of the last
    for (int i = 0; i <= half; i++) {
        int childBlkNum = (i < half) ? internalEntries[half + 1 + i].lChild
                                     : internalEntries[MAX_KEYS_INTERNAL].rChild;

        BlockBuffer child(childBlkNum);
        HeadInfo childHeader;
        child.getHeader(&childHeader);
        childHeader.pblock = rightBlkNum;
        child.setHeader(&childHeader);
    }

    return rightBlkNum;
}

int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild) {

    AttrCatEntry attrCatEntry;
    int retVal = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (retVal != SUCCESS) {
        return retVal;
    }

    IndInternal newRootBlk;   // constructor 1: allocates a new internal block

    int newRootBlkNum = newRootBlk.getBlockNum();

    if (newRootBlkNum == E_DISKFULL) {
        // right subtree was built but never attached to the tree
        BPlusTree::bPlusDestroy(rChild);
        return E_DISKFULL;
    }

    HeadInfo newRootHeader;
    newRootBlk.getHeader(&newRootHeader);
    newRootHeader.numEntries = 1;
    newRootBlk.setHeader(&newRootHeader);

    InternalEntry rootEntry;
    rootEntry.lChild = lChild;
    rootEntry.attrVal = attrVal;
    rootEntry.rChild = rChild;
    newRootBlk.setEntry(&rootEntry, 0);

    BlockBuffer leftChild(lChild);
    HeadInfo leftHeader;
    leftChild.getHeader(&leftHeader);
    leftHeader.pblock = newRootBlkNum;
    leftChild.setHeader(&leftHeader);

    BlockBuffer rightChild(rChild);
    HeadInfo rightHeader;
    rightChild.getHeader(&rightHeader);
    rightHeader.pblock = newRootBlkNum;
    rightChild.setHeader(&rightHeader);

    attrCatEntry.rootBlock = newRootBlkNum;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

    return SUCCESS;
}