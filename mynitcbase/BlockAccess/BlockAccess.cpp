#include "BlockAccess.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op) {
    // get the previous search index of the relation relId from the relation cache
    // (use RelCacheTable::getSearchIndex() function)
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId, &prevRecId);

    
    // let block and slot denote the record id of the record being currently checked
    int block, slot;

    // if the current search index record is invalid(i.e. both block and slot = -1)
    if (prevRecId.block == -1 && prevRecId.slot == -1)
    {
        RelCatEntry relCatBuf;
        RelCacheTable::getRelCatEntry(relId, &relCatBuf);

        block = relCatBuf.firstBlk;
        slot = 0;
    }
    else
    {
        // (there is a hit from previous search; search should start from
        // the record next to the search index record)

        // block = search index's block
        // slot = search index's slot + 1
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    while (block != -1)
    {
        RecBuffer recBuffer(block);
        // get the record with id (block, slot) using RecBuffer::getRecord()
        RelCatEntry relCatEntry;
        RelCacheTable::getRelCatEntry(relId, &relCatEntry);

        Attribute record[relCatEntry.numAttrs];
        
        // get header of the block using RecBuffer::getHeader() function
        HeadInfo head;
        recBuffer.getHeader(&head);

        if (slot >= head.numSlots) {
            block = head.rblock;
            slot = 0;
            continue;  
        }

        unsigned char slotMap[head.numSlots];
        recBuffer.getSlotMap(slotMap);
        
        if(slot >= head.numSlots)
        {
            block = head.rblock;
            slot = 0;
            continue;  
        }

        if(slotMap[slot] == SLOT_UNOCCUPIED)
        {
            slot++;
            continue;
        }
        recBuffer.getRecord(record, slot);
        // compare record's attribute value to the the given attrVal as below:
        /*
            firstly get the attribute offset for the attrName attribute
            from the attribute cache entry of the relation using
            AttrCacheTable::getAttrCatEntry()
        */
        /* use the attribute offset to get the value of the attribute from
           current record */
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

        int attrOffset = attrCatEntry.offset;
        Attribute attrVal2 = record[attrOffset];

        int cmpVal = compareAttrs(attrVal2, attrVal, attrCatEntry.attrType);

        /* Next task is to check whether this record satisfies the given condition.
           It is determined based on the output of previous comparison and
           the op value received.
           The following code sets the cond variable if the condition is satisfied.
        */
        if (
            (op == NE && cmpVal != 0) ||    // if op is "not equal to"
            (op == LT && cmpVal < 0) ||     // if op is "less than"
            (op == LE && cmpVal <= 0) ||    // if op is "less than or equal to"
            (op == EQ && cmpVal == 0) ||    // if op is "equal to"
            (op == GT && cmpVal > 0) ||     // if op is "greater than"
            (op == GE && cmpVal >= 0)       // if op is "greater than or equal to"
        ) {
            /*
            set the search index in the relation cache as
            the record id of the record that satisfies the given condition
            (use RelCacheTable::setSearchIndex function)
            */
            RecId temp = {block, slot};
            RelCacheTable::setSearchIndex(relId, &temp);
            return RecId{block, slot};
        }

        slot++;
    }

    // no record in the relation with Id relid satisfies the given condition
    return RecId{-1, -1};
}

int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE]){
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute newRelationName;    // set newRelationName with newName
    strcpy(newRelationName.sVal, newName);
    
    RecId recId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, newRelationName, EQ);
    if(recId.block != -1 || recId.slot != -1){
        return E_RELEXIST;
    }

    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute oldRelationName;    // set oldRelationName with oldName
    strcpy(oldRelationName.sVal, oldName);

    recId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, oldRelationName, EQ);
    if(recId.block == -1 && recId.slot == -1){
        return E_RELNOTEXIST;
    }


    /* get the relation catalog record of the relation to rename using a RecBuffer
       on the relation catalog [RELCAT_BLOCK] and RecBuffer.getRecord function
    */
    RecBuffer relCatBuffer(recId.block);

    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBuffer.getRecord(relCatRecord, recId.slot);
    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, newName);
    relCatBuffer.setRecord(relCatRecord, recId.slot);
    /*
    update all the attribute catalog entries in the attribute catalog corresponding
    to the relation with relation name oldName to the relation name newName
    */

    /* reset the searchIndex of the attribute catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    while(true){
        RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, ATTRCAT_ATTR_RELNAME, oldRelationName, EQ);

        if(attrCatRecId.block == -1 && attrCatRecId.slot == -1){break;}

        RecBuffer attrCatBuffer(attrCatRecId.block);

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBuffer.getRecord(attrCatRecord, attrCatRecId.slot);

        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, newName);

        attrCatBuffer.setRecord(attrCatRecord, attrCatRecId.slot);
    }

    return SUCCESS;
}

int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {

    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;    // set relNameAttr to relName
    strcpy(relNameAttr.sVal, relName);

    RecId relRecId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, relNameAttr, EQ);

    if(relRecId.block == -1 && relRecId.slot == -1){
        return E_RELNOTEXIST;
    }
    
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    /* declare variable attrToRenameRecId used to store the attr-cat recId
    of the attribute to rename */
    RecId attrToRenameRecId{-1, -1};
    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

    while (true) {  
        RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);
        if(attrCatRecId.block == -1 && attrCatRecId.slot == -1){break;}

        RecBuffer attrCatBuffer(attrCatRecId.block);
        attrCatBuffer.getRecord(attrCatEntryRecord, attrCatRecId.slot);

        if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0){
            attrToRenameRecId = attrCatRecId;
        }

        if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName) == 0){
            return E_ATTREXIST;
        }
    }

    if (attrToRenameRecId.block == -1 && attrToRenameRecId.slot == -1) {
        return E_ATTRNOTEXIST;
    }   

    // Update the entry corresponding to the attribute in the Attribute Catalog Relation.
    /*   declare a RecBuffer for attrToRenameRecId.block and get the record at
         attrToRenameRecId.slot */
    //   update the AttrName of the record with newName
    //   set back the record with RecBuffer.setRecord
    RecBuffer attrToRenameBuffer(attrToRenameRecId.block);
    Attribute attrCatEntryToRename[ATTRCAT_NO_ATTRS];
    attrToRenameBuffer.getRecord(attrCatEntryToRename, attrToRenameRecId.slot);

    strcpy(attrCatEntryToRename[ATTRCAT_ATTR_NAME_INDEX].sVal, newName);
    attrToRenameBuffer.setRecord(attrCatEntryToRename, attrToRenameRecId.slot);
    return SUCCESS;
}

int BlockAccess::insert(int relId, Attribute *record) {

    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    int blockNum = relCatEntry.firstBlk;

    RecId rec_id = {-1, -1};

    int numOfSlots = relCatEntry.numSlotsPerBlk;
    int numOfAttributes = relCatEntry.numAttrs;

    int prevBlockNum = -1;
  
    while (blockNum != -1) {

        RecBuffer recBuffer(blockNum);

        struct HeadInfo header;
        recBuffer.getHeader(&header);

        unsigned char slotMap[numOfSlots];
        recBuffer.getSlotMap(slotMap);

        for (int slot = 0; slot < numOfSlots; slot++) {
            if (slotMap[slot] == SLOT_UNOCCUPIED) {
                rec_id.block = blockNum;
                rec_id.slot = slot;
                break;
            }
        }

        if(rec_id.slot != -1){break;}

        prevBlockNum = blockNum;
        blockNum= header.rblock;
    }

    if(rec_id.block == -1 || rec_id.slot == -1){

        if (relId == RELCAT_RELID) {
            return E_MAXRELATIONS;
        }

        RecBuffer newRecBuffer;
        int ret = newRecBuffer.getBlockNum();
        if (ret == E_DISKFULL) {
            return E_DISKFULL;
        }

        rec_id.block = ret;
        rec_id.slot = 0;


        struct HeadInfo newHeader;
        newHeader.blockType = REC;
        newHeader.pblock = -1;
        newHeader.lblock = prevBlockNum;
        newHeader.rblock = -1;
        newHeader.numEntries = 0;
        newHeader.numSlots = numOfSlots;
        newHeader.numAttrs = numOfAttributes;
        newRecBuffer.setHeader(&newHeader);

        unsigned char newSlotMap[numOfSlots];
        for (int i = 0; i < numOfSlots; i++){
            newSlotMap[i] = SLOT_UNOCCUPIED;
        }
        newRecBuffer.setSlotMap(newSlotMap);

        if(prevBlockNum != -1)
        {
            RecBuffer prevRecBuffer(prevBlockNum);
            struct HeadInfo prevHeader;
            prevRecBuffer.getHeader(&prevHeader);
            prevHeader.rblock = rec_id.block;
            prevRecBuffer.setHeader(&prevHeader);
        }else
        {
            relCatEntry.firstBlk = rec_id.block;
            RelCacheTable::setRelCatEntry(relId, &relCatEntry);
        }


        relCatEntry.lastBlk = rec_id.block;
        RelCacheTable::setRelCatEntry(relId, &relCatEntry);
    }

    RecBuffer blockBuffer(rec_id.block);

    blockBuffer.setRecord(record, rec_id.slot);

    // mark the slot occupied
    unsigned char slotmap[numOfSlots];
    blockBuffer.getSlotMap(slotmap);
    slotmap[rec_id.slot] = SLOT_OCCUPIED;
    blockBuffer.setSlotMap(slotmap);

    // bump numEntries in the block header
    struct HeadInfo header;
    blockBuffer.getHeader(&header);
    header.numEntries++;
    blockBuffer.setHeader(&header);

    // bump numRecs in the relation cache
    relCatEntry.numRecs++;
    RelCacheTable::setRelCatEntry(relId, &relCatEntry);

    int flag = SUCCESS;

    for (int attrOffset = 0; attrOffset < numOfAttributes; attrOffset++) {

        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(relId, attrOffset, &attrCatEntry);

        int rootBlock = attrCatEntry.rootBlock;

        if (rootBlock != -1) {
            int retVal = BPlusTree::bPlusInsert(relId, attrCatEntry.attrName, record[attrOffset], rec_id);
            if (retVal == E_DISKFULL) {
                // index for this attribute has been destroyed
                flag = E_INDEX_BLOCKS_RELEASED;
            }
        }
    }

    return flag;
}

int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
    RecId recId;
    AttrCatEntry attrcatentry;

    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrcatentry);

    if(ret != SUCCESS){
        return ret;
    }

    int rootblk = attrcatentry.rootBlock;
    if(rootblk == -1){
        recId = linearSearch(relId, attrName, attrVal, op);
    }else{
        recId = BPlusTree::bPlusSearch(relId, attrName, attrVal, op);
    }

    if(recId.block == -1 && recId.slot == -1){
        return E_NOTFOUND;
    }

    RecBuffer recbuffer(recId.block);
    recbuffer.getRecord(record, recId.slot);

    return SUCCESS; 
}

int BlockAccess::deleteRelation(char relName[ATTR_SIZE]) {

    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }

    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);

    char relNameAttrName[ATTR_SIZE];
    strcpy(relNameAttrName, RELCAT_ATTR_RELNAME);

    RecId relCatRecId = BlockAccess::linearSearch(RELCAT_RELID, relNameAttrName, relNameAttr, EQ);
    if (relCatRecId.block == -1 || relCatRecId.slot == -1) {
        return E_RELNOTEXIST;
    }

    Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
    RecBuffer relCatBlock(relCatRecId.block);
    relCatBlock.getRecord(relCatEntryRecord, relCatRecId.slot);

    int firstBlock = (int) relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;

    /*** Release all record blocks of the relation ***/
    int currentBlock = firstBlock;
    while (currentBlock != -1) {
        RecBuffer recBlock(currentBlock);
        HeadInfo header;
        recBlock.getHeader(&header);

        int nextBlock = header.rblock;
        recBlock.releaseBlock();
        currentBlock = nextBlock;
    }

    /*** Delete attrcat entries and index blocks of the relation ***/
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    int numberOfAttributesDeleted = 0;

    while (true) {
        RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, relNameAttrName, relNameAttr, EQ);
        if (attrCatRecId.block == -1 || attrCatRecId.slot == -1) {
            break;
        }

        numberOfAttributesDeleted++;

        RecBuffer attrCatBlock(attrCatRecId.block);
        HeadInfo attrCatHeader;
        attrCatBlock.getHeader(&attrCatHeader);

        // read the record before the slot is freed or the block released
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBlock.getRecord(attrCatRecord, attrCatRecId.slot);
        int rootBlock = (int) attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;

        // mark the slot unoccupied
        unsigned char slotMap[SLOTMAP_SIZE_RELCAT_ATTRCAT];
        attrCatBlock.getSlotMap(slotMap);
        slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
        attrCatBlock.setSlotMap(slotMap);

        attrCatHeader.numEntries--;
        attrCatBlock.setHeader(&attrCatHeader);

        // block became empty: unlink it and release it
        if (attrCatHeader.numEntries == 0) {
            // attrcat is never empty (it holds the catalogs' own entries),
            // so lblock is always valid here
            RecBuffer leftBlock(attrCatHeader.lblock);
            HeadInfo leftHeader;
            leftBlock.getHeader(&leftHeader);
            leftHeader.rblock = attrCatHeader.rblock;
            leftBlock.setHeader(&leftHeader);

            if (attrCatHeader.rblock != -1) {
                RecBuffer rightBlock(attrCatHeader.rblock);
                HeadInfo rightHeader;
                rightBlock.getHeader(&rightHeader);
                rightHeader.lblock = attrCatHeader.lblock;
                rightBlock.setHeader(&rightHeader);
            } else {
                // this was attrcat's last block: update its LastBlock
                RelCatEntry attrCatEntry;
                RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrCatEntry);
                attrCatEntry.lastBlk = attrCatHeader.lblock;
                RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrCatEntry);
            }

            attrCatBlock.releaseBlock();
            RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
        }

        // free the B+ tree of this attribute, if it has one
        if (rootBlock != -1) {
            BPlusTree::bPlusDestroy(rootBlock);
        }
    }

    /*** Delete the relation's entry from relcat ***/
    HeadInfo relCatHeader;
    relCatBlock.getHeader(&relCatHeader);
    relCatHeader.numEntries--;
    relCatBlock.setHeader(&relCatHeader);

    unsigned char relSlotMap[SLOTMAP_SIZE_RELCAT_ATTRCAT];
    relCatBlock.getSlotMap(relSlotMap);
    relSlotMap[relCatRecId.slot] = SLOT_UNOCCUPIED;
    relCatBlock.setSlotMap(relSlotMap);

    /*** Update the relation cache entries of the catalogs ***/
    RelCatEntry relCatCacheEntry;
    RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatCacheEntry);
    relCatCacheEntry.numRecs--;
    RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatCacheEntry);

    RelCatEntry attrCatCacheEntry;
    RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrCatCacheEntry);
    attrCatCacheEntry.numRecs -= numberOfAttributesDeleted;
    RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrCatCacheEntry);

    return SUCCESS;
}   

int BlockAccess::project(int relId, Attribute *record) {
    // Get the previous search index of the relation from the relation cache
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId, &prevRecId);

    // block and slot store the record id of the slot to check
    int block, slot;

    // If search index is {-1, -1}, start searching from the beginning of relation
    if (prevRecId.block == -1 && prevRecId.slot == -1) {
        RelCatEntry relCatBuf;
        RelCacheTable::getRelCatEntry(relId, &relCatBuf);

        block = relCatBuf.firstBlk;
        slot = 0;
    } else {
        // Continue from the record after the previous search index
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    // Iterate through the records to find the next occupied slot
    while (block != -1) {
        RecBuffer recBuffer(block);

        HeadInfo head;
        recBuffer.getHeader(&head);

        // If slot exceeds the number of slots in the block, move to the right block
        if (slot >= head.numSlots) {
            block = head.rblock;
            slot = 0;
            continue;
        }

        unsigned char slotMap[head.numSlots];
        recBuffer.getSlotMap(slotMap);

        // If the slot is empty, check the next slot
        if (slotMap[slot] == SLOT_UNOCCUPIED) {
            slot++;
        } else {
            // Next occupied slot/record found
            break;
        }
    }

    // If block == -1, all records have been exhausted
    if (block == -1) {
        return E_NOTFOUND;
    }

    // Update the search index to the found record id
    RecId nextRecId{block, slot};
    RelCacheTable::setSearchIndex(relId, &nextRecId);

    // Read the record from the block into the caller's record buffer
    RecBuffer recordBuffer(block);
    recordBuffer.getRecord(record, slot);

    return SUCCESS;
}
