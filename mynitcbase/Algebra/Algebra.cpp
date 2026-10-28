#include "Algebra.h"
#include "../BPlusTree/BPlusTree.h"
#include <cstring>
#include <cstdlib>
#include <iostream>

bool isNumber(char *str);

int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]) {
  int srcRelId = OpenRelTable::getRelId(srcRel);
  if (srcRelId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
  if (ret != SUCCESS) {
    return E_ATTRNOTEXIST;
  }

  /*** Convert strVal (string) to an attribute of data type NUMBER or STRING ***/
  int type = attrCatEntry.attrType;
  Attribute attrVal;
  if (type == NUMBER) {
    if (isNumber(strVal)) {
      attrVal.nVal = atof(strVal);
    } else {
      return E_ATTRTYPEMISMATCH;
    }
  } else if (type == STRING) {
    strcpy(attrVal.sVal, strVal);
  }

  /*** Creating and opening the target relation ***/
  RelCatEntry srcRelCatEntry;
  RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
  int src_nAttrs = srcRelCatEntry.numAttrs;

  // Collect attribute names and types of the source relation
  char attr_names[src_nAttrs][ATTR_SIZE];
  int attr_types[src_nAttrs];

  for (int i = 0; i < src_nAttrs; i++) {
    AttrCatEntry entry;
    AttrCacheTable::getAttrCatEntry(srcRelId, i, &entry);
    strcpy(attr_names[i], entry.attrName);
    attr_types[i] = entry.attrType;
  }

  // Create the target relation with the same schema
  ret = Schema::createRel(targetRel, src_nAttrs, attr_names, attr_types);
  if (ret != SUCCESS) {
    return ret;
  }

  // Open the newly created target relation
  int targetRelId = OpenRelTable::openRel(targetRel);
  if (targetRelId < 0) {
    Schema::deleteRel(targetRel);
    return targetRelId;
  }

  /*** Selecting records from source relation and inserting into target relation ***/
  Attribute record[src_nAttrs];
  RelCacheTable::resetSearchIndex(srcRelId);
  AttrCacheTable::resetSearchIndex(srcRelId, attr);

  numComparisons = 0;

  // Search each record matching the condition and insert it into the target relation
  while (BlockAccess::search(srcRelId, record, attr, attrVal, op) == SUCCESS) {
    ret = BlockAccess::insert(targetRelId, record);
    if (ret != SUCCESS) {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }

  printf("Number of comparisons: %d\n", numComparisons);

  // Close target relation after insertion
  Schema::closeRel(targetRel);
  return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]) {
  int srcRelId = OpenRelTable::getRelId(srcRel);
  if (srcRelId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  RelCatEntry srcRelCatEntry;
  RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
  int numAttrs = srcRelCatEntry.numAttrs;

  // Retrieve attribute names and types of the source relation
  char attrNames[numAttrs][ATTR_SIZE];
  int attrTypes[numAttrs];

  for (int i = 0; i < numAttrs; i++) {
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
    strcpy(attrNames[i], attrCatEntry.attrName);
    attrTypes[i] = attrCatEntry.attrType;
  }

  /*** Creating and opening the target relation ***/
  int ret = Schema::createRel(targetRel, numAttrs, attrNames, attrTypes);
  if (ret != SUCCESS) {
    return ret;
  }

  int targetRelId = OpenRelTable::openRel(targetRel);
  if (targetRelId < 0) {
    Schema::deleteRel(targetRel);
    return targetRelId;
  }

  /*** Inserting copied records into the target relation ***/
  RelCacheTable::resetSearchIndex(srcRelId);

  Attribute record[numAttrs];
  while (BlockAccess::project(srcRelId, record) == SUCCESS) {
    ret = BlockAccess::insert(targetRelId, record);
    if (ret != SUCCESS) {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }

  // Close the target relation
  Schema::closeRel(targetRel);
  return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]) {
  int srcRelId = OpenRelTable::getRelId(srcRel);
  if (srcRelId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  RelCatEntry srcRelCatEntry;
  RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
  int src_nAttrs = srcRelCatEntry.numAttrs;

  int attr_offset[tar_nAttrs];
  int attr_types[tar_nAttrs];

  /*** Verify each target attribute exists in source relation and record offset/type ***/
  for (int i = 0; i < tar_nAttrs; i++) {
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[i], &attrCatEntry);
    if (ret != SUCCESS) {
      return E_ATTRNOTEXIST;
    }
    attr_offset[i] = attrCatEntry.offset;
    attr_types[i] = attrCatEntry.attrType;
  }

  /*** Creating and opening the target relation ***/
  int ret = Schema::createRel(targetRel, tar_nAttrs, tar_Attrs, attr_types);
  if (ret != SUCCESS) {
    return ret;
  }

  int targetRelId = OpenRelTable::openRel(targetRel);
  if (targetRelId < 0) {
    Schema::deleteRel(targetRel);
    return targetRelId;
  }

  /*** Inserting projected records into the target relation ***/
  RelCacheTable::resetSearchIndex(srcRelId);

  Attribute record[src_nAttrs];
  while (BlockAccess::project(srcRelId, record) == SUCCESS) {
    Attribute proj_record[tar_nAttrs];
    for (int i = 0; i < tar_nAttrs; i++) {
      proj_record[i] = record[attr_offset[i]];
    }

    ret = BlockAccess::insert(targetRelId, proj_record);
    if (ret != SUCCESS) {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }

  // Close the target relation
  Schema::closeRel(targetRel);
  return SUCCESS;
}


// will return if a string can be parsed as a floating point number
bool isNumber(char *str) {
  int len;
  float ignore;
  /*
    sscanf returns the number of elements read, so if there is no float matching
    the first %f, ret will be 0, else it'll be 1

    %n gets the number of characters read. this scanf sequence will read the
    first float ignoring all the whitespace before and after. and the number of
    characters read that far will be stored in len. if len == strlen(str), then
    the string only contains a float with/without whitespace. else, there's other
    characters.
  */
  int ret = sscanf(str, "%f %n", &ignore, &len);
  return ret == 1 && len == strlen(str);
}

int Algebra::insert(char relName[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE]){

  if(strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0){
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relName);

  if(relId == E_RELNOTOPEN){
    return E_RELNOTOPEN;
  }

  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(relId, &relCatEntry);

  if(relCatEntry.numAttrs != nAttrs){
    return E_NATTRMISMATCH;
  }

  union Attribute recordValues[nAttrs];

  for(int i = 0; i < nAttrs; i++)
  {
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);

    if (attrCatEntry.attrType == NUMBER){
      if(isNumber(record[i])){
        recordValues[i].nVal = atof(record[i]);
      }
      else{
        return E_ATTRTYPEMISMATCH;
      }
    }
    else if (attrCatEntry.attrType == STRING){
      strcpy((char *)&(recordValues[i].sVal), record[i]);            
    }
  }

  int retVal = BlockAccess::insert(relId, recordValues);
  return retVal;
}

int Algebra::join(char srcRelOne[ATTR_SIZE], char srcRelTwo[ATTR_SIZE], char targetRel[ATTR_SIZE],
                  char attrOne[ATTR_SIZE], char attrTwo[ATTR_SIZE]) {
  // Get relation IDs for both source relations
  int srcRelId1 = OpenRelTable::getRelId(srcRelOne);
  int srcRelId2 = OpenRelTable::getRelId(srcRelTwo);

  if (srcRelId1 == E_RELNOTOPEN || srcRelId2 == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  // Fetch attribute catalog entries for the join attributes
  AttrCatEntry attrCatEntry1, attrCatEntry2;
  int ret1 = AttrCacheTable::getAttrCatEntry(srcRelId1, attrOne, &attrCatEntry1);
  int ret2 = AttrCacheTable::getAttrCatEntry(srcRelId2, attrTwo, &attrCatEntry2);

  if (ret1 != SUCCESS || ret2 != SUCCESS) {
    return E_ATTRNOTEXIST;
  }

  if (attrCatEntry1.attrType != attrCatEntry2.attrType) {
    return E_ATTRTYPEMISMATCH;
  }

  // Fetch relation catalog entries to get attribute counts
  RelCatEntry relCatEntry1, relCatEntry2;
  RelCacheTable::getRelCatEntry(srcRelId1, &relCatEntry1);
  RelCacheTable::getRelCatEntry(srcRelId2, &relCatEntry2);

  int numOfAttributes1 = relCatEntry1.numAttrs;
  int numOfAttributes2 = relCatEntry2.numAttrs;

  // Check for duplicate attribute names between the two relations, excluding the join attributes
  for (int i = 0; i < numOfAttributes1; i++) {
    AttrCatEntry a1;
    AttrCacheTable::getAttrCatEntry(srcRelId1, i, &a1);
    for (int j = 0; j < numOfAttributes2; j++) {
      AttrCatEntry a2;
      AttrCacheTable::getAttrCatEntry(srcRelId2, j, &a2);
      if (strcmp(a1.attrName, a2.attrName) == 0) {
        if (strcmp(a1.attrName, attrOne) == 0 && strcmp(a2.attrName, attrTwo) == 0) {
          continue;
        }
        return E_DUPLICATEATTR;
      }
    }
  }

  // If rel2 does not have an index on attr2, create it
  if (attrCatEntry2.rootBlock == -1) {
    int ret = BPlusTree::bPlusCreate(srcRelId2, attrTwo);
    if (ret != SUCCESS) {
      return ret;
    }
  }

  // Define schema for the target relation (columns from rel1, then rel2 excluding attrTwo)
  int numOfAttributesInTarget = numOfAttributes1 + numOfAttributes2 - 1;
  char targetRelAttrNames[numOfAttributesInTarget][ATTR_SIZE];
  int targetRelAttrTypes[numOfAttributesInTarget];

  for (int i = 0; i < numOfAttributes1; i++) {
    AttrCatEntry entry;
    AttrCacheTable::getAttrCatEntry(srcRelId1, i, &entry);
    strcpy(targetRelAttrNames[i], entry.attrName);
    targetRelAttrTypes[i] = entry.attrType;
  }

  int targetIdx = numOfAttributes1;
  for (int j = 0; j < numOfAttributes2; j++) {
    AttrCatEntry entry;
    AttrCacheTable::getAttrCatEntry(srcRelId2, j, &entry);
    if (strcmp(entry.attrName, attrTwo) == 0) {
      continue;
    }
    strcpy(targetRelAttrNames[targetIdx], entry.attrName);
    targetRelAttrTypes[targetIdx] = entry.attrType;
    targetIdx++;
  }

  // Create target relation
  int ret = Schema::createRel(targetRel, numOfAttributesInTarget, targetRelAttrNames, targetRelAttrTypes);
  if (ret != SUCCESS) {
    return ret;
  }

  // Open the newly created target relation
  int targetRelId = OpenRelTable::openRel(targetRel);
  if (targetRelId < 0) {
    Schema::deleteRel(targetRel);
    return targetRelId;
  }

  // Scan srcRel1 and search matching records in srcRel2
  Attribute record1[numOfAttributes1];
  Attribute record2[numOfAttributes2];
  Attribute targetRecord[numOfAttributesInTarget];

  RelCacheTable::resetSearchIndex(srcRelId1);

  while (BlockAccess::project(srcRelId1, record1) == SUCCESS) {
    RelCacheTable::resetSearchIndex(srcRelId2);
    AttrCacheTable::resetSearchIndex(srcRelId2, attrTwo);

    while (BlockAccess::search(srcRelId2, record2, attrTwo, record1[attrCatEntry1.offset], EQ) == SUCCESS) {
      // Copy attributes of record1
      for (int i = 0; i < numOfAttributes1; i++) {
        targetRecord[i] = record1[i];
      }

      // Copy attributes of record2 excluding attrTwo
      int tIdx = numOfAttributes1;
      for (int j = 0; j < numOfAttributes2; j++) {
        if (j == attrCatEntry2.offset) {
          continue;
        }
        targetRecord[tIdx] = record2[j];
        tIdx++;
      }

      int insertRet = BlockAccess::insert(targetRelId, targetRecord);
      if (insertRet != SUCCESS) {
        OpenRelTable::closeRel(targetRelId);
        Schema::deleteRel(targetRel);
        return E_DISKFULL;
      }
    }
  }

  OpenRelTable::closeRel(targetRelId);
  return SUCCESS;
}