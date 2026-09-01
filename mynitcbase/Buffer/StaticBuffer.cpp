#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

StaticBuffer::StaticBuffer(){
    for(int i = 0; i < BUFFER_CAPACITY; i++){
        metainfo[i].free = true; 
        metainfo[i].dirty = false;
        metainfo[i].timeStamp = -1;
        metainfo[i].blockNum = -1;
    }
}           

StaticBuffer::~StaticBuffer() {
    for (int i = 0; i < BUFFER_CAPACITY; i++) {
        if (!metainfo[i].free && metainfo[i].dirty) {
            Disk::writeBlock(blocks[i], metainfo[i].blockNum);
        }
    }
}

int StaticBuffer::getFreeBuffer(int blockNum){
    if (blockNum < 0 || blockNum > DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    for(int i = 0; i < BUFFER_CAPACITY; i++){
        if(!metainfo[i].free){
            metainfo[i].timeStamp++;
        }
    }

    int bufferNum;

    for(int i = 0; i < BUFFER_CAPACITY; i++){
        if(metainfo[i].free){
            bufferNum =  i;
        }
    }

    if(bufferNum == -1){
        int maxTimeStamp = -1;
        for(int i = 0; i < BUFFER_CAPACITY; i++){
            if(metainfo[i].timeStamp > maxTimeStamp){
                maxTimeStamp = metainfo[i].timeStamp;
                bufferNum = i;
            }
        }

        if(metainfo[bufferNum].dirty){
            Disk::writeBlock(blocks[bufferNum], metainfo[bufferNum].blockNum);
        }
    }

    metainfo[bufferNum] =  {false, false, blockNum, 0};
    return bufferNum;
}

int StaticBuffer::getBufferNum(int blockNum){
    if (blockNum < 0 || blockNum > DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    for(int i = 0; i < BUFFER_CAPACITY; i++){
        if(metainfo[i].blockNum == blockNum){
            return i;
        }   
    }

    return E_BLOCKNOTINBUFFER;
}

int StaticBuffer::setDirtyBit(int blockNum){
    int bufferNum = getBufferNum(blockNum);

    if(bufferNum == E_BLOCKNOTINBUFFER){
        return E_BLOCKNOTINBUFFER;
    }

    if(bufferNum == E_OUTOFBOUND){
        return E_OUTOFBOUND;
    }

    metainfo[bufferNum].dirty = true;

    return SUCCESS;
}