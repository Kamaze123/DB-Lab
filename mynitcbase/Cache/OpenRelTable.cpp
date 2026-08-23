#include "OpenRelTable.h"
#include "../BlockAccess/BlockAccess.h"
#include <cstring>
#include <cstdlib> 
#include <iostream>

OpenRelTable::OpenRelTable() {

  for (int i = 0; i < MAX_OPEN; ++i) {
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
  }

  /************ Setting up Relation Cache entries ************/

  RecBuffer relCatBlock(RELCAT_BLOCK);

  // --- RELCAT's own entry ---
  Attribute relCatRecord[RELCAT_NO_ATTRS];
  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);

  struct RelCacheEntry relCacheEntry;
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

  RelCacheTable::relCache[RELCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;

  // --- ATTRCAT's entry ---
  Attribute attrCatRelRecord[RELCAT_NO_ATTRS];
  relCatBlock.getRecord(attrCatRelRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);

  struct RelCacheEntry attrCatRelCacheEntry;
  RelCacheTable::recordToRelCatEntry(attrCatRelRecord, &attrCatRelCacheEntry.relCatEntry);
  attrCatRelCacheEntry.recId.block = RELCAT_BLOCK;
  attrCatRelCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

  RelCacheTable::relCache[ATTRCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[ATTRCAT_RELID]) = attrCatRelCacheEntry;

  /************ Setting up Attribute cache entries ************/

  RecBuffer attrCatBlock(ATTRCAT_BLOCK);   // <-- THIS must come before it's used below
  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

  // --- RELCAT's attributes (slots 0-5) ---
  AttrCacheEntry* head = nullptr;
  AttrCacheEntry* tail = nullptr;

  for (int i = 0; i < RELCAT_NO_ATTRS; i++) {
      attrCatBlock.getRecord(attrCatRecord, i);

      AttrCacheEntry* attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
      AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
      attrCacheEntry->recId.block = ATTRCAT_BLOCK;
      attrCacheEntry->recId.slot = i;
      attrCacheEntry->next = nullptr;

      if (head == nullptr) {
          head = attrCacheEntry;
          tail = attrCacheEntry;
      } else {
          tail->next = attrCacheEntry;
          tail = attrCacheEntry;
      }
  }
  AttrCacheTable::attrCache[RELCAT_RELID] = head;

  // --- ATTRCAT's attributes (slots 6-11) ---
  AttrCacheEntry* head2 = nullptr;
  AttrCacheEntry* tail2 = nullptr;

  for (int i = RELCAT_NO_ATTRS; i < RELCAT_NO_ATTRS + ATTRCAT_NO_ATTRS; i++) {
      attrCatBlock.getRecord(attrCatRecord, i);

      AttrCacheEntry* attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
      AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
      attrCacheEntry->recId.block = ATTRCAT_BLOCK;
      attrCacheEntry->recId.slot = i;
      attrCacheEntry->next = nullptr;

      if (head2 == nullptr) {
          head2 = attrCacheEntry;
          tail2 = attrCacheEntry;
      } else {
          tail2->next = attrCacheEntry;
          tail2 = attrCacheEntry;
      }
  }
  AttrCacheTable::attrCache[ATTRCAT_RELID] = head2;

  RecBuffer studentsRelCatBlock(4);
Attribute studentsRelCatRecord[RELCAT_NO_ATTRS];
studentsRelCatBlock.getRecord(studentsRelCatRecord, 2);

struct RelCacheEntry studentsRelCacheEntry;
RelCacheTable::recordToRelCatEntry(studentsRelCatRecord, &studentsRelCacheEntry.relCatEntry);
studentsRelCacheEntry.recId.block = 4;
studentsRelCacheEntry.recId.slot = 2;
studentsRelCacheEntry.searchIndex = {-1, -1};

RelCacheTable::relCache[STUDENTS_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
*(RelCacheTable::relCache[STUDENTS_RELID]) = studentsRelCacheEntry;

/************ Setting up Students' Attribute Cache entries ************/
AttrCacheEntry* studentsHead = nullptr;
AttrCacheEntry* studentsTail = nullptr;

int studentsAttrSlots[] = {12, 13, 14, 15};

for (int i = 0; i < 4; i++) {
    RecBuffer attrBlock(5);
    Attribute attrRecord[ATTRCAT_NO_ATTRS];
    attrBlock.getRecord(attrRecord, studentsAttrSlots[i]);

    AttrCacheEntry* entry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
    AttrCacheTable::recordToAttrCatEntry(attrRecord, &entry->attrCatEntry);
    entry->recId.block = 5;
    entry->recId.slot = studentsAttrSlots[i];
    entry->next = nullptr;

    if (studentsHead == nullptr) { studentsHead = entry; studentsTail = entry; }
    else { studentsTail->next = entry; studentsTail = entry; }
}
AttrCacheTable::attrCache[STUDENTS_RELID] = studentsHead;

}

OpenRelTable::~OpenRelTable() {
  for (int relId = 0; relId < 2; relId++) {  // RELCAT_RELID and ATTRCAT_RELID
    // free the RelCacheEntry allocated for this relId
    free(RelCacheTable::relCache[relId]);

    // free the linked list of AttrCacheEntry nodes for this relId
    AttrCacheEntry* entry = AttrCacheTable::attrCache[relId];
    while (entry != nullptr) {
      AttrCacheEntry* temp = entry;
      entry = entry->next;
      free(temp);
    }
  }
}

int OpenRelTable::closeRel(int relId) {
  if (relId < 0 || relId >= MAX_OPEN || relId == RELCAT_RELID || relId == ATTRCAT_RELID) {
    return E_OUTOFBOUND;   // can't close RELCAT/ATTRCAT — they stay open always
  }

  if (RelCacheTable::relCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  free(RelCacheTable::relCache[relId]);
  RelCacheTable::relCache[relId] = nullptr;

  AttrCacheEntry* entry = AttrCacheTable::attrCache[relId];
  while (entry != nullptr) {
    AttrCacheEntry* temp = entry;
    entry = entry->next;
    free(temp);
  }
  AttrCacheTable::attrCache[relId] = nullptr;

  return SUCCESS;
}

int OpenRelTable::openRel(char relName[ATTR_SIZE]){
  int relId = -1;
  for (int i = 2; i < MAX_OPEN; i++) {   // start at 2, since 0=RELCAT, 1=ATTRCAT
    if (RelCacheTable::relCache[i] == nullptr) {
      relId = i;
      break;
    }
  }

  if (relId == -1) {
    return E_CACHEFULL;   // no free relId slots — check your Errors.h for exact name
  }

  RelCatEntry relCatEntry;
  RecId relCatRecId = {-1, -1};

  int relCatBlockNum = RELCAT_BLOCK;

  while (relCatBlockNum != -1) {
    RecBuffer relCatBlock(relCatBlockNum);

    HeadInfo head;
    relCatBlock.getHeader(&head);

    Attribute record[RELCAT_NO_ATTRS];
    for (int slot = 0; slot < head.numSlots; slot++) {
      relCatBlock.getRecord(record, slot);
      if (strcmp(record[RELCAT_REL_NAME_INDEX].sVal, relName) == 0) {
        RelCacheTable::recordToRelCatEntry(record, &relCatEntry);
        relCatRecId.block = relCatBlockNum;
        relCatRecId.slot = slot;
        break;
      }
    }

    if (relCatRecId.block != -1) break;   // found it, stop scanning
    relCatBlockNum = head.rblock;          // move to next block in the chain
  }

  if (relCatRecId.block == -1) {
    return E_RELNOTEXIST;   // scanned all of RELCAT, no match found
  }

  RelCacheEntry* relCacheEntry = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  relCacheEntry->relCatEntry = relCatEntry;
  relCacheEntry->recId = relCatRecId;
  RelCacheTable::relCache[relId] = relCacheEntry;

  AttrCacheEntry* head_ = nullptr;
  AttrCacheEntry* tail_ = nullptr;

  int attrCatBlockNum = ATTRCAT_BLOCK;

  while (attrCatBlockNum != -1) {
    RecBuffer attrCatBlock(attrCatBlockNum);

    HeadInfo head;
    attrCatBlock.getHeader(&head);

    Attribute record[ATTRCAT_NO_ATTRS];
    for (int slot = 0; slot < head.numSlots; slot++) {
      attrCatBlock.getRecord(record, slot);
      if (strcmp(record[ATTRCAT_REL_NAME_INDEX].sVal, relName) == 0) {
        AttrCacheEntry* attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(record, &attrCacheEntry->attrCatEntry);
        attrCacheEntry->recId.block = attrCatBlockNum;
        attrCacheEntry->recId.slot = slot;
        attrCacheEntry->next = nullptr;

        if (head_ == nullptr) {
          head_ = attrCacheEntry;
          tail_ = attrCacheEntry;
        } else {
          tail_->next = attrCacheEntry;
          tail_ = attrCacheEntry;
        }
      }
    }

    attrCatBlockNum = head.rblock;
  }

  AttrCacheTable::attrCache[relId] = head_;
                         
  return relId;
}

int OpenRelTable::getRelId(char relName[ATTR_SIZE]){
  
  if(strcmp(relName, RELCAT_RELNAME) == 0){
    return RELCAT_RELID;
  }
  
  if(strcmp(relName, ATTRCAT_RELNAME) == 0){
    return ATTRCAT_RELID;
  }

  if(strcmp(relName, "Students") == 0){
    return STUDENTS_RELID;
  }

  return E_RELNOTOPEN;
}

