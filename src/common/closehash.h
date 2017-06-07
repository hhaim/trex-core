#ifndef CLOSE_HASH_H
#define CLOSE_HASH_H

/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

                    
#include "dlist.h"
#include <string.h>
#include <stdio.h>

struct CCloseHashRec {
  TCGenDList m_list;
};

enum HASH_STATUS {
    hsOK,
    hsERR_INSERT_FULL       = -1,
    hsERR_INSERT_DUPLICATE  = -2,
    hsERR_REMOVE_NO_KEY     = -3
};


template <class K, class V>
class CHashEntry : public TCDListNode {
public:
    K key;
    V value;
};


class COpenHashCounters {
public:
    uint32_t   m_max_entry_in_row;
    
    uint32_t   m_cmd_find;   
    uint32_t   m_cmd_open;   
    uint32_t   m_cmd_remove; 

    COpenHashCounters(){
        m_max_entry_in_row=0;
        m_cmd_find=0;
        m_cmd_open=0;
        m_cmd_remove=0;
    }

    void UpdateFindEntry(uint32_t iter){
        if ( iter>m_max_entry_in_row ) {
            m_max_entry_in_row=iter;
        }
    }

    void Clear(void){
        memset(this,0,sizeof(COpenHashCounters));
    }
    void Dump(FILE *fd){
        fprintf(fd,"m_max_entry_in_row       : %lu \n",(ulong)m_max_entry_in_row);
        fprintf(fd,"m_cmd_find               : %lu \n",(ulong)m_cmd_find);
        fprintf(fd,"m_cmd_open               : %lu \n",(ulong)m_cmd_open);
        fprintf(fd,"m_cmd_remove             : %lu \n",(ulong)m_cmd_remove);
    }

};


/*
example for hash env 

class HASH_ENV{
  public:
   static uint32_t Hash(const KEY & key);
}
*/

template<class KEY, class VAL,class HASH_ENV>
class CCloseHash {

public:

    typedef CHashEntry<KEY,VAL>  hashEntry_t;

    CCloseHash();

    ~CCloseHash();

    bool Create(uint32_t size);
    
    void Delete();
    
    void reset();
public:
    
    HASH_STATUS insert(const KEY & key,
                       hashEntry_t * & handle);
    
    void  remove(hashEntry_t * lp);

    HASH_STATUS remove(const KEY & key);

    hashEntry_t * find(const KEY & key);


    uint32_t get_hash_size(){
        return m_maxEntries;
    }

    uint32_t get_count(){
        return m_numEntries;
    }


public:
    void Dump(FILE *fd);

private:
    CCloseHashRec *              m_tbl; 
    uint32_t                     m_size;  // size of m_tbl log2
    uint32_t                     m_numEntries; // Number of entries in hash
    uint32_t                     m_maxEntries; // Max number of entries allowed
    COpenHashCounters            m_stats;
};





template<class KEY, class VAL,class HASH_ENV>
CCloseHash< KEY, VAL,HASH_ENV>::CCloseHash(){
    m_size = 0;
    m_tbl=0; 
    m_numEntries=0;
    m_maxEntries=0;
}

template<class KEY, class VAL,class HASH_ENV>
CCloseHash< KEY, VAL,HASH_ENV>::~CCloseHash(){
}


inline  uint32_t hash_FindPower2(uint32_t number){
    uint32_t  powerOf2=1;
    while(powerOf2 < number){
        if(powerOf2==0x80000000)
            return 0;
        powerOf2<<=1;
    }
    return(powerOf2);
}
    
template<class KEY, class VAL,class HASH_ENV>
bool CCloseHash<KEY,VAL,HASH_ENV>::Create(uint32_t size){

    m_size = hash_FindPower2(size);
    m_tbl = new CCloseHashRec[m_size];
    if (!m_tbl) {
        return false;
    }
    int i;
    for (i=0; i<(int)m_size; i++) {
        CCloseHashRec * lp =&m_tbl[i];
        if ( !lp->m_list.Create() ){
            return(false);
        }
    }
    m_maxEntries = m_size;
    reset();
    return true;
}

template<class KEY, class VAL,class HASH_ENV>
void CCloseHash<KEY,VAL,HASH_ENV>::Delete(){
    delete []m_tbl;
    m_tbl=0;
    m_size = 0;
    m_numEntries=0;
    m_maxEntries=0;
}



template<class KEY, class VAL,class HASH_ENV>
HASH_STATUS CCloseHash<KEY,VAL,HASH_ENV>::insert(const KEY & key,
                                                 CCloseHash::hashEntry_t* & handle){

    m_stats.m_cmd_open++;
    uint32_t place = HASH_ENV::Hash(key) & (m_size-1);
    CCloseHashRec * lpRec= &m_tbl[place];
    TCGenDListIterator iter(lpRec->m_list);
    hashEntry_t *lp;
    for ( ;(lp=(hashEntry_t *)iter.node()); iter++){
        if (lp->key==key){
            return(hsERR_INSERT_DUPLICATE);
        }
    }
    lp = new CCloseHash::hashEntry_t();
    if (!lp) {
        handle=0;
        return(hsERR_INSERT_FULL);
    }
    lp->key=key;
    lpRec->m_list.append(lp);
    handle=lp;
    m_numEntries++;
    return(hsOK);
}

template<class KEY, class VAL,class HASH_ENV>
void  CCloseHash<KEY,VAL,HASH_ENV>::remove(CCloseHash::hashEntry_t * lp){
    m_stats.m_cmd_remove++;
    lp->detach();
    delete lp;
    m_numEntries--;
}

template<class KEY, class VAL,class HASH_ENV>
HASH_STATUS CCloseHash<KEY,VAL,HASH_ENV>::remove(const KEY & key){
    m_stats.m_cmd_remove++;
    uint32_t place = HASH_ENV::Hash(key) & (m_size-1);
    CCloseHashRec * lpRec= &m_tbl[place];
    TCGenDListIterator iter(lpRec->m_list);
    hashEntry_t *lp;
    for ( ;(lp=(hashEntry_t *)iter.node()); iter++){
        if ( lp->key == key ){
            lp->detach();
            delete lp;
            m_numEntries--;
            return(hsOK);
        }
    }
    return(hsERR_REMOVE_NO_KEY);
}


template<class KEY, class VAL,class HASH_ENV>
typename CCloseHash<KEY,VAL,HASH_ENV>::hashEntry_t * CCloseHash<KEY,VAL,HASH_ENV>::find(const KEY & key){
    m_stats.m_cmd_find++;
    uint32_t place = HASH_ENV::Hash(key) & (m_size-1);
    CCloseHashRec * lpRec= &m_tbl[place];
    TCGenDListIterator iter(lpRec->m_list);
    hashEntry_t *lp;
    int f_itr=0;
    for ( ;(lp=(hashEntry_t *)iter.node()); iter++){
        if ( lp->key == key ){
            m_stats.UpdateFindEntry(f_itr);
            return (lp);
        }
        f_itr++;
    }
    return((hashEntry_t *)0);
}



template<class KEY, class VAL,class HASH_ENV>
void CCloseHash<KEY,VAL,HASH_ENV>::reset(){
    m_stats.Clear();
    for(int i=0;i<m_size;i++){
        m_tbl[i].m_list.Create();
    }
    m_numEntries=0;
}




template<class KEY, class VAL,class HASH_ENV>
void CCloseHash<KEY,VAL,HASH_ENV>::Dump(FILE *fd){
	fprintf(fd," stats \n");
    fprintf(fd," ---------------------- \n");
    m_stats.Dump(fd);
    fprintf(fd," ---------------------- \n");
    fprintf(fd," size         : %lu \n",(ulong)m_size);
    fprintf(fd," num entries  : %lu \n",(ulong)m_numEntries);
}



#endif

