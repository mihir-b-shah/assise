
#ifndef _BCACHE_H_
#define _BCACHE_H_

#include <cstdlib>
#include <cstdio>

template<typename T>
class CNode {
private:
  static const uint8_t kNone = 0;
  static const uint8_t kEmpty = 1 << 0;
  static const uint8_t kTombstone = 1 << 1;

  uint32_t num_;
  T val_;
  CNode<T>* prev_;
  CNode<T>* next_;
  uint8_t flags_;

private:
  CNode(uint8_t flags) : num_(0), val_(T()), prev_(nullptr), next_(nullptr), flags_(flags) {}

  bool TestFlag(uint8_t flag){ return (flags_ & flag) != 0; }

public:
  CNode() : CNode(kEmpty) {}
  CNode(uint32_t num, T val) : num_(num), val_(val), prev_(nullptr), next_(nullptr), flags_(kNone) {}

  static CNode<T> Empty(){ return CNode(kEmpty); }
  static CNode<T> Tombstone(){ return CNode(kTombstone); }

  uint32_t GetNum() { return num_; }
  T GetVal() { return val_; }
  bool IsEmpty() { return TestFlag(kEmpty); }
  bool IsTombstone() { return TestFlag(kTombstone); }

  CNode<T>*& Prev() { return prev_; }
  CNode<T>*& Next() { return next_; }
};

// Hash table for uint32_t tags impl
template<typename T, size_t Cap>
class HashTable {
private:
  CNode<T> table_[Cap];
  size_t size_;
  
  template<bool CheckTomb>
  CNode<T>* Get(uint32_t key)
  {
    if(size_ >= Cap){ 
      std::perror("Hash table too small for cache.\n");
      std::exit(0);
    }

    uint32_t pos = key % Cap;
    uint32_t ctr = 0;
    while (ctr < Cap && !table_[pos].IsEmpty() && !(CheckTomb && table_[pos].IsTombstone())
                    && (table_[pos].IsTombstone() || table_[pos].GetNum() != key)) {
      pos = (pos+1) % Cap;
      ++ctr;
    }
    return &table_[pos];
  }

public:
  HashTable() : table_(), size_(0) {}

  CNode<T>* Get(uint32_t key)
  {
    CNode<T>* ptr = Get<false>(key);
    return ptr->GetNum() == key ? ptr : nullptr;
  }

  CNode<T>* Insert(uint32_t key, T val)
  {
    CNode<T>* ptr = Get<true>(key);
    *ptr = CNode<T>(key, val);
    ++size_;
    return ptr;
  }

  CNode<T>* Erase(uint32_t key)
  {
    CNode<T>* ptr = Get<false>(key);
    *ptr = CNode<T>::Tombstone();
    --size_;
    return ptr;
  }

  size_t Size() { return size_; }
};

template<typename T>
class LinkList {
private:
  CNode<T>* head_;
  CNode<T>* tail_;

public:
  LinkList() : head_(nullptr), tail_(nullptr) {}

  void Remove(CNode<T>* node)
  {
    if(node == head_ && node == tail_){
      head_ = nullptr;
      tail_ = nullptr;
    } else if(node == head_){
      node->Next()->Prev() = nullptr;
      head_ = node->Next();
    } else if(node == tail_){
      node->Prev()->Next() = nullptr;
      tail_ = node->Prev();
    } else {
      node->Next()->Prev() = node->Prev();
      node->Prev()->Next() = node->Next();
    }
  }

  void InsertHead(CNode<T>* node)
  {
    node->Prev() = nullptr;
    node->Next() = head_;
    if(head_ == nullptr && tail_ == nullptr){
      tail_ = node;
    } else {
      head_->Prev() = node;
    }
    head_ = node;
  }
    
  CNode<T>* head() { return head_; }
  CNode<T>* tail() { return tail_; }
};

// Cache impl
template<typename T, size_t Size>
class Cache {
private:
  LinkList<T> ll_;
  HashTable<T, 2*Size> imap_;

public:
  Cache(){}

  void Put(uint32_t id, T buf)
  {
    CNode<T>* node = imap_.Insert(id, buf);
    ll_.InsertHead(node);
    if(imap_.Size() > Size){
      CNode<T>* tail = ll_.tail();
      ll_.Remove(tail);
      imap_.Erase(tail->GetNum());
    }
  }

  T Get(uint32_t id)
  {
    CNode<T>* node = imap_.Get(id);
    if(node == nullptr){
      return T();
    }
    ll_.Remove(node);
    ll_.InsertHead(node);
    return node->GetVal();
  }
};

#endif
