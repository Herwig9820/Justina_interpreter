/************************************************************************************************************
*    Justina interpreter library for Arduino boards with 32 bit SAMD microconrollers                        *
*                                                                                                           *
*    Tested with Nano 33 IoT and Arduino RP2040                                                             *
*                                                                                                           *
*    Version:    v1.01 - 12/07/2023                                                                         *
*    Author:     Herwig Taveirne, 2021-2023                                                                 *
*                                                                                                           *
*    Justina is an interpreter which does NOT require you to use an IDE to write and compile programs.      *
*    Programs are written on the PC using any text processor and transferred to the Arduino using any       *
*    Serial or TCP Terminal program capable of sending files.                                               *
*    Justina can store and retrieve programs and other data on an SD card as well.                          *
*                                                                                                           *
*    See GitHub for more information and documentation: https://github.com/Herwig9820/Justina_interpreter   *
*                                                                                                           *
*    This program is free software: you can redistribute it and/or modify                                   *
*    it under the terms of the GNU General Public License as published by                                   *
*    the Free Software Foundation, either version 3 of the License, or                                      *
*    (at your option) any later version.                                                                    *
*                                                                                                           *
*    This program is distributed in the hope that it will be useful,                                        *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of                                         *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                                           *
*    GNU General Public License for more details.                                                           *
*                                                                                                           *
*    You should have received a copy of the GNU General Public License                                      *
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.                                  *
************************************************************************************************************/


#include "Justina.h"


#define PRINT_LLIST_OBJ_CREA_DEL 0


// *****************************************************************
// ***            class LinkedList - implementation              ***
// *****************************************************************

// ---------------------------------------------
// *   initialisation of static class member   *
// ---------------------------------------------

int LinkedList::_listIDcounter = 0;
long LinkedList::_createdListObjectCounter = 0;
Stream* defaultStream = &Serial;
Stream** LinkedList::_ppDebugOutStream = &defaultStream;

// -------------------
// *   constructor   *
// -------------------

LinkedList::LinkedList() {
    _listID = _listIDcounter;
    _listIDcounter++;                                                                               // static variable: number of linked lists created
    _listElementCount = 0;
    _pFirstElement = nullptr;
    _pLastElement = nullptr;
}


// ------------------
// *   destructor   *
// ------------------

LinkedList::~LinkedList() {
    _listIDcounter--;                                                                               // static variable
}


// ----------------------------
// *   set debug out stream   *
// ----------------------------

// NOTE: it's caller's responsibility that stream pointer this pointer is pointing to is valid (e.g. file is open)

void LinkedList::setDebugOutStream(Stream** ppDebugOutStream) {
    _ppDebugOutStream = ppDebugOutStream;
}


// --------------------------------------------------
// *   append a list element to the end of a list   *
// --------------------------------------------------

char* LinkedList::appendListElement(int size) {
    ListElemHead* p = (ListElemHead*)(new char[sizeof(ListElemHead) + size]);                       // create list object with payload of specified size in bytes

    if (_pFirstElement == nullptr) {                                                                // not yet any elements
        _pFirstElement = p;
        p->pPrev = nullptr;                                                                         // is first element in list: no previous element
    }
    else {
        _pLastElement->pNext = p;
        p->pPrev = _pLastElement;
    }
    _pLastElement = p;
    p->pNext = nullptr;                                                                             // because p is now last element
    _listElementCount++;
    _createdListObjectCounter++;

#if PRINT_LLIST_OBJ_CREA_DEL
    (*_ppDebugOutStream)->print("(LIST) Create elem # "); (*_ppDebugOutStream)->print(_listElementCount);
    (*_ppDebugOutStream)->print(", list ID "); (*_ppDebugOutStream)->print(_listID);
    (*_ppDebugOutStream)->print(", stack: "); (*_ppDebugOutStream)->print(_listName);
    if (p == nullptr) { (*_ppDebugOutStream)->println(", list elem adres: nullptr"); }
    else {
        (*_ppDebugOutStream)->print(", list elem address: "); (*_ppDebugOutStream)->println((uint32_t)p, HEX);
    }
#endif
    return (char*)(p + 1);                                                                          // move pointer 1 list element header length: point to payload of newly created element
}


// -----------------------------------------------------
// *   delete a heap object and remove it from list    *
// -----------------------------------------------------

char* LinkedList::deleteListElement(void* pPayload) {                                               // input: pointer to payload of a list element

    ListElemHead* pElem = (ListElemHead*)pPayload;                                                  // still points to payload: check if nullptr
    if (pElem == nullptr) { pElem = _pLastElement; }                                                // nullptr: delete last element in list (if it exists)
    else { pElem = pElem - 1; }                                                                     // pointer to list element header

    if (pElem == nullptr) { return nullptr; }                                                       // still nullptr: return (list is empty)

    ListElemHead* p = pElem->pNext;                                                                 // remember return value

#if PRINT_LLIST_OBJ_CREA_DEL
    // determine list element # by counting from list start
    ListElemHead* q = _pFirstElement;
    int i{};
    for (i = 1; i <= _listElementCount; ++i) {
        if (q == pElem) { break; }            // always a match
        q = q->pNext;
    }

    (*_ppDebugOutStream)->print("(LIST) Delete elem # "); (*_ppDebugOutStream)->print(i); (*_ppDebugOutStream)->print(" (new # "); (*_ppDebugOutStream)->print(_listElementCount - 1);
    (*_ppDebugOutStream)->print("), list ID "); (*_ppDebugOutStream)->print(_listID);
    (*_ppDebugOutStream)->print(", stack: "); (*_ppDebugOutStream)->print(_listName);
    (*_ppDebugOutStream)->print(", list elem address: "); (*_ppDebugOutStream)->println((uint32_t)pElem, HEX);
#endif

    // before deleting object, remove from list:
    // change pointers from previous element (or _pFirstPointer, if no previous element) and next element (or _pLastPointer, if no next element)
    ((pElem->pPrev == nullptr) ? _pFirstElement : pElem->pPrev->pNext) = pElem->pNext;
    ((pElem->pNext == nullptr) ? _pLastElement : pElem->pNext->pPrev) = pElem->pPrev;

    _listElementCount--;
    delete[]pElem;

    if (p == nullptr) { return nullptr; }
    // move pointer 1 list element header length: point to payload of next element in list (or nullptr if last element deleted)
    else { return (char*)(p + 1); }                                                                 
}


// ------------------------------------------
// *   delete all list elements in a list   *
// ------------------------------------------

void LinkedList::deleteList() {
    if (_pFirstElement == nullptr) return;

    ListElemHead* pHead = _pFirstElement;
    while (true) {
        char* pNextPayload = deleteListElement((char*)(pHead + 1));
        if (pNextPayload == nullptr) { return; }
        pHead = ((ListElemHead*)pNextPayload) - 1;                                                  // points to list element header 
    }
}


// ----------------------------------------------------
// *   get a pointer to the first element in a list   *
// ----------------------------------------------------

char* LinkedList::getFirstListElement() {
    return (char*)(_pFirstElement + (_pFirstElement == nullptr ? 0 : 1));                           // add one header length
}


//----------------------------------------------------
// *   get a pointer to the last element in a list   *
//----------------------------------------------------

char* LinkedList::getLastListElement() {

    return (char*)(_pLastElement + (_pLastElement == nullptr ? 0 : 1));                             // add one header length
}


// -------------------------------------------------------
// *   get a pointer to the previous element in a list   *
// -------------------------------------------------------

char* LinkedList::getPrevListElement(void* pPayload) {                                              // input: pointer to payload of a list element  
    if (pPayload == nullptr) { return nullptr; }                                                    // nullptr: return
    ListElemHead* pElem = ((ListElemHead*)pPayload) - 1;                                            // points to list element header
    if (pElem->pPrev == nullptr) { return nullptr; }
    return (char*)(pElem->pPrev + 1);                                                               // points to payload of previous element
}


//----------------------------------------------------
// *   get a pointer to the next element in a list   *
//----------------------------------------------------

char* LinkedList::getNextListElement(void* pPayload) {
    if (pPayload == nullptr) { return nullptr; }                                                    // nullptr: return
    ListElemHead* pElem = ((ListElemHead*)pPayload) - 1;                                            // points to list element header
    if (pElem->pNext == nullptr) { return nullptr; }
    return (char*)(pElem->pNext + 1);                                                               // points to payload of next element
}


//-------------------------------------------------------------
// *   get the list ID (depends on the order of creation !)   *
//-------------------------------------------------------------

int LinkedList::getListID() {
    return _listID;
}


//--------------------------
// *   set the list name   *
//--------------------------

void LinkedList::setListName(char* listName) {
    strncpy(_listName, listName, listNameSize - 1);
    _listName[listNameSize - 1] = '\0';
    return;
}


//--------------------------
// *   get the list name   *
//--------------------------

char* LinkedList::getListName() {
    return _listName;
}


//-------------------------------
// *   get list element count   *
//-------------------------------

int LinkedList::getElementCount() {
    return _listElementCount;
}


//--------------------------------------------------
// *   get count of created objects across lists   *
//--------------------------------------------------

long LinkedList::getCreatedObjectCount() {
    return _createdListObjectCounter;                                                               // across lists
}


