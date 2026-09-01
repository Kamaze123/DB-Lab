#include "BlockAccess.h"

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
        recBuffer.getRecord(record, slot);
        // get header of the block using RecBuffer::getHeader() function
        HeadInfo head;
        recBuffer.getHeader(&head);
        // get slot map of the block using RecBuffer::getSlotMap() function
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
