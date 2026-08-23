#include "OpenRelTable.h"
#include "../BlockAccess/BlockAccess.h"
#include <cstring>
#include <cstdlib> 
#include <iostream>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

int OpenRelTable::getFreeOpenRelTableEntry() {

  for(int i = 0; i < MAX_OPEN; i++){
    if(tableMetaInfo[i].free == true){
      return i;
    }
  }

  return E_CACHEFULL;
}

int OpenRelTable::getRelId(char relName[ATTR_SIZE]){
  
  for(int i = 0; i < MAX_OPEN; i++){
    if(!tableMetaInfo[i].free && strcmp(tableMetaInfo[i].relName,relName) == 0){
      return i;
    }
  }

  return E_RELNOTOPEN;
}

OpenRelTable::OpenRelTable() {

  for (int i = 0; i < MAX_OPEN; ++i) {
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
    tableMetaInfo[i].free = true;
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
  relCacheEntry.searchIndex = {-1,-1};

  RelCacheTable::relCache[RELCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;

  // --- ATTRCAT's entry ---
  Attribute attrCatRelRecord[RELCAT_NO_ATTRS];
  relCatBlock.getRecord(attrCatRelRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);

  struct RelCacheEntry attrCatRelCacheEntry;
  RelCacheTable::recordToRelCatEntry(attrCatRelRecord, &attrCatRelCacheEntry.relCatEntry);
  attrCatRelCacheEntry.recId.block = RELCAT_BLOCK;
  attrCatRelCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;
  attrCatRelCacheEntry.searchIndex = {-1,-1};

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

  tableMetaInfo[RELCAT_RELID].free = false;
  strcpy(tableMetaInfo[RELCAT_RELID].relName, RELCAT_RELNAME);

  tableMetaInfo[ATTRCAT_RELID].free = false;
  strcpy(tableMetaInfo[ATTRCAT_RELID].relName, ATTRCAT_RELNAME);
}

OpenRelTable::~OpenRelTable() {

  for (int i = 2; i < MAX_OPEN; ++i) {
    if (!tableMetaInfo[i].free) {
      OpenRelTable::closeRel(i); // we will implement this function later
    }
  }

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
  while (entry != nullptr){
    AttrCacheEntry* temp = entry;
    entry = entry->next;
    free(temp);
  }
  AttrCacheTable::attrCache[relId] = nullptr;
  tableMetaInfo[relId].free = true;

  return SUCCESS;
}

int OpenRelTable::openRel( char relName[ATTR_SIZE]) {

  int relId = OpenRelTable::getRelId(relName);
  if(relId != E_RELNOTOPEN){
    return relId;
  }

  int freeSlot = OpenRelTable::getFreeOpenRelTableEntry();
  if (freeSlot == E_CACHEFULL){
    return E_CACHEFULL;
  }

  relId = freeSlot;

  /****** Setting up Relation Cache entry for the relation ******/

  /* search for the entry with relation name, relName, in the Relation Catalog using
      BlockAccess::linearSearch().
      Care should be taken to reset the searchIndex of the relation RELCAT_RELID
      before calling linearSearch().*/
  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  Attribute attrVal;
  strcpy(attrVal.sVal, relName);

  RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, attrVal, EQ);

  if (relcatRecId.block == -1 && relcatRecId.slot == -1) {
    return E_RELNOTEXIST;
  }

  /* read the record entry corresponding to relcatRecId and create a relCacheEntry
      on it using RecBuffer::getRecord() and RelCacheTable::recordToRelCatEntry().
      update the recId field of this Relation Cache entry to relcatRecId.
      use the Relation Cache entry to set the relId-th entry of the RelCacheTable.
    NOTE: make sure to allocate memory for the RelCacheEntry using malloc()
  */
  Attribute relCatRecord[RELCAT_NO_ATTRS];
  RecBuffer relCatBuffer(relcatRecId.block);
  relCatBuffer.getRecord(relCatRecord, relcatRecId.slot);

  RelCacheEntry* relCacheEntry = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry->relCatEntry);
  relCacheEntry->recId = relcatRecId;
  relCacheEntry->searchIndex = {-1,-1};

  RelCacheTable::relCache[relId] = relCacheEntry;
  /****** Setting up Attribute Cache entry for the relation ******/

  // let listHead be used to hold the head of the linked list of attrCache entries.
  AttrCacheEntry* listHead = nullptr;
  AttrCacheEntry* listTail = nullptr;

  /*iterate over all the entries in the Attribute Catalog corresponding to each
  attribute of the relation relName by multiple calls of BlockAccess::linearSearch()
  care should be taken to reset the searchIndex of the relation, ATTRCAT_RELID,
  corresponding to Attribute Catalog before the first call to linearSearch().*/
  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

  Attribute attrCatAttrVal;
  strcpy(attrCatAttrVal.sVal, relName);

  while(true){
      /* let attrcatRecId store a valid record id an entry of the relation, relName,
      in the Attribute Catalog.*/
      RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, ATTRCAT_ATTR_RELNAME, attrCatAttrVal, EQ);

      if(attrcatRecId.block == -1 && attrcatRecId.slot == -1){break;}

      /* read the record entry corresponding to attrcatRecId and create an
      Attribute Cache entry on it using RecBuffer::getRecord() and
      AttrCacheTable::recordToAttrCatEntry().
      update the recId field of this Attribute Cache entry to attrcatRecId.
      add the Attribute Cache entry to the linked list of listHead .*/
      // NOTE: make sure to allocate memory for the AttrCacheEntry using malloc()

      Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
      RecBuffer attrCatBuffer(attrcatRecId.block);
      attrCatBuffer.getRecord(attrCatRecord, attrcatRecId.slot);
      
      AttrCacheEntry* attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
      AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
      attrCacheEntry->recId = attrcatRecId;
      attrCacheEntry->next = nullptr;

      if(listHead == nullptr){
        listHead = attrCacheEntry;
        listTail = attrCacheEntry;
      }else{
        listTail->next = attrCacheEntry;
        listTail = attrCacheEntry;
      }
  }

  AttrCacheTable::attrCache[relId] = listHead;

  // set the relIdth entry of the AttrCacheTable to listHead.

  /****** Setting up metadata in the Open Relation Table for the relation******/

  // update the relIdth entry of the tableMetaInfo with free as false and
  // relName as the input.
  tableMetaInfo[relId].free = false;
  strcpy(tableMetaInfo[relId].relName, relName);

  return relId;
}
