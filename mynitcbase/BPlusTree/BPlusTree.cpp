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