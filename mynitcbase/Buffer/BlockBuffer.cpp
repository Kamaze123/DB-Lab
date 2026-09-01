#include "BlockBuffer.h"
#include <cstdlib>
#include <cstring>

BlockBuffer::BlockBuffer(int blockNum) {
    // initialise this.blockNum with the argument
    this->blockNum = blockNum;
}

// calls the parent class constructor
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

// load the block header into the argument pointer
int BlockBuffer::getHeader(struct HeadInfo *head){
    
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS){
        return ret;
    }

    // populate the numEntries, numAttrs and numSlots fields in *head
    memcpy(&head->numSlots, bufferPtr + 24, 4);
    memcpy(&head->numEntries, bufferPtr + 16, 4);
    memcpy(&head->numAttrs, bufferPtr + 20, 4);
    memcpy(&head->rblock, bufferPtr + 12, 4);
    memcpy(&head->lblock, bufferPtr + 8, 4);

    return SUCCESS; 
}

int RecBuffer::getSlotMap(unsigned char *slotMap) {
    unsigned char *bufferPtr;

    // get the starting address of the buffer containing the block using loadBlockAndGetBufferPtr().
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) {
        return ret;
    }

    struct HeadInfo head;
    // get the header of the block using getHeader() function
    ret = getHeader(&head);
    if(ret != SUCCESS){
        return ret;
    }

    int slotCount = head.numSlots;

    // get a pointer to the beginning of the slotmap in memory by offsetting HEADER_SIZE
    unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

    // copy the values from `slotMapInBuffer` to `slotMap` (size is `slotCount`)
    memcpy(slotMap, slotMapInBuffer, slotCount);
    return SUCCESS;
}


// load the record at slotNum into the argument pointer
int RecBuffer::getRecord(union Attribute *rec, int slotNum) {
    struct HeadInfo head;

    int ret = this->getHeader(&head);
    if(ret != SUCCESS){
        return ret;
    }

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    unsigned char *bufferPtr;
    ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS){
        return ret;
    }

    int recordSize = attrCount * ATTR_SIZE;

    unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);

    memcpy(rec, slotPointer, recordSize);

    return SUCCESS; 
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr){
    int bufferNum =  StaticBuffer::getBufferNum(this->blockNum);

    if(bufferNum != E_BLOCKNOTINBUFFER){
        for(int i = 0; i < BUFFER_CAPACITY; i++){
            if(!StaticBuffer::metainfo[i].free){
                StaticBuffer::metainfo[i].timeStamp++;
            }
        }
        StaticBuffer::metainfo[bufferNum].timeStamp = 0;
    }else{
        int freeBufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

        if(freeBufferNum == E_OUTOFBOUND){
            return E_OUTOFBOUND;
        }

        Disk::readBlock(StaticBuffer::blocks[freeBufferNum], this->blockNum);

        bufferNum = freeBufferNum;
    }

    *buffPtr = StaticBuffer::blocks[bufferNum];

    return SUCCESS;
}

int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType) {
    double diff;
    if(attrType == STRING){
        diff = strcmp(attr1.sVal, attr2.sVal);
    }
    else{
        diff = attr1.nVal - attr2.nVal;
    }

    if(diff > 0){return 1;}
    else if(diff < 0){return -1;}
    else{return 0;}
}

int RecBuffer::setRecord(union Attribute *rec, int slotNum) {
    unsigned char *bufferPtr;
    int retVal = loadBlockAndGetBufferPtr(&bufferPtr);
    if(retVal != SUCCESS){
        return retVal;
    }

    struct HeadInfo head;
    this->getHeader(&head);

    int attrcount = head.numAttrs;
    int slotcount = head.numSlots;

    if(slotNum >= slotcount){
        return E_OUTOFBOUND;
    }
    
    int recordSize = attrcount * ATTR_SIZE;
    unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotcount + (slotNum * recordSize);

    memcpy(slotPointer, rec, recordSize);

    StaticBuffer::setDirtyBit(this->blockNum);
    return SUCCESS;
}