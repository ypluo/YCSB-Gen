//
//  core_workload.h
//  YCSB-C
//
//  Created by Jinglei Ren on 12/9/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//  
//  Modified by Yongping Luo on 3/4/22
//  Copyright (c) 2022 Yongping Luo <ypluo18@qq.com>
//

#ifndef YCSB_C_CORE_WORKLOAD_H_
#define YCSB_C_CORE_WORKLOAD_H_

#include <vector>
#include <string>
#include "generator.h"
#include "discrete_generator.h"
#include "counter_generator.h"
#include "properties.h"
#include "utils.h"
#include "basic_db.h"

namespace ycsbc {

using _key_t = uint64_t;

struct StringKey {
  const int KEY_SIZE = 19;
  std::string str_;

  void operator=(std::string s) {
    str_ = s;
  } 

  void operator=(uint64_t u) {
    char buf[KEY_SIZE + 1];
    sprintf(buf, "%0*lu", KEY_SIZE, u);
    str_ = std::string(buf, KEY_SIZE);
  }

  void operator=(int64_t i) {
    *this = (uint64_t) i;
  }

  void operator=(double d) {
    char buf[KEY_SIZE + 1];
    sprintf(buf, "%0*lf", KEY_SIZE, d);
    str_ = std::string(buf, KEY_SIZE);
  }

  void operator=(float f) {
    *this = (double) f;
  }
};

const uint64_t KEYSET_SCALE_DEFAULT = (uint64_t) 128 * 1024 * 1024;

enum Operation {
  INSERT,
  READ,
  UPDATE,
  SCAN,
  READMODIFYWRITE
};

class CoreWorkload {
 public:
   /// 
  /// The name of the property for the proportion of read transactions.
  ///
  static const std::string READ_PROPORTION_PROPERTY;
  static const std::string READ_PROPORTION_DEFAULT;
  
  /// 
  /// The name of the property for the proportion of update transactions.
  ///
  static const std::string UPDATE_PROPORTION_PROPERTY;
  static const std::string UPDATE_PROPORTION_DEFAULT;
  
  /// 
  /// The name of the property for the proportion of insert transactions.
  ///
  static const std::string INSERT_PROPORTION_PROPERTY;
  static const std::string INSERT_PROPORTION_DEFAULT;
  
  /// 
  /// The name of the property for the proportion of scan transactions.
  ///
  static const std::string SCAN_PROPORTION_PROPERTY;
  static const std::string SCAN_PROPORTION_DEFAULT;
  
  ///
  /// The name of the property for the proportion of
  /// read-modify-write transactions.
  ///
  static const std::string READMODIFYWRITE_PROPORTION_PROPERTY;
  static const std::string READMODIFYWRITE_PROPORTION_DEFAULT;
  
  /// 
  /// The name of the property for the the distribution of request keys.
  /// Options are "uniform", "zipfian" and "latest".
  ///
  static const std::string REQUEST_DISTRIBUTION_PROPERTY;
  static const std::string REQUEST_DISTRIBUTION_DEFAULT;

  static const std::string ZIPFIAN_SKEWNESS_PROPERTY;
  static const std::string ZIPFIAN_SKEWNESS_DEFAULT;

  /// 
  /// The name of the property for the max scan length (number of records).
  ///
  static const std::string MAX_SCAN_LENGTH_PROPERTY;
  static const std::string MAX_SCAN_LENGTH_DEFAULT;
  
  /// 
  /// The name of the property for the scan length distribution.
  /// Options are "uniform" and "zipfian" (favoring short scans).
  ///
  static const std::string SCAN_LENGTH_DISTRIBUTION_PROPERTY;
  static const std::string SCAN_LENGTH_DISTRIBUTION_DEFAULT;
  
  static const std::string RECORD_COUNT_PROPERTY;    // the number of data records that are populated already 
  static const std::string OPERATION_COUNT_PROPERTY; // the operation number of transactions to query against db

  static const std::string INSERT_START_PROPERTY;
  static const std::string INSERT_START_DEFAULT;

  ///
  /// Initialize the scenario.
  /// Called once, in the main client thread, before any operations are started.
  ///
  virtual void Init(const utils::Properties &p);
  
  virtual std::string NextSequenceKey(bool query); /// Used for loading data
  virtual std::string NextTransactionKey(); /// Used for transactions
  virtual Operation NextOperation() { return op_chooser_.Next(); }
  virtual size_t NextScanLength() { return scan_len_chooser_->Next(); }

  CoreWorkload(std::string filename, uint64_t max_key_num = KEYSET_SCALE_DEFAULT) :
    key_generator_(NULL), key_chooser_(NULL), 
    scan_len_chooser_(NULL), insert_key_sequence_(3),
    record_count_(0) {
      if(filename == "") {
        from_file_ = false;
      } else {
        if(utils::file_exist(filename.c_str())) {
          //fprintf(stderr, "using keyset file %s\n", filename.c_str());
          from_file_ = true;
          // read keys from file
          keys_.reserve(max_key_num);
          std::ifstream infile_load(filename.c_str());
          max_seq_id_ = 0;
          _key_t key;
          while (true) {
            infile_load >> key;
            if(!infile_load.good() || max_seq_id_ >= max_key_num) break;
            max_seq_id_++;
            keys_.push_back(key);
          }
          infile_load.close();
        } else {
          fprintf(stderr, "keyset file %s not found\n", filename.c_str());
          from_file_ = false;
        }
      }
  }
  
  virtual ~CoreWorkload() {
    if (key_generator_) delete key_generator_;
    if (key_chooser_) delete key_chooser_;
    if (scan_len_chooser_) delete scan_len_chooser_;
  }
  
 protected:
  Generator<uint64_t> *key_generator_;
  DiscreteGenerator<Operation> op_chooser_;
  Generator<uint64_t> *key_chooser_;
  Generator<uint64_t> *scan_len_chooser_;
  CounterGenerator insert_key_sequence_;
  size_t record_count_;
  bool from_file_;
  uint64_t max_seq_id_;
  std::vector<_key_t> keys_;
};

inline std::string CoreWorkload::NextSequenceKey(bool query = false) {
  uint64_t seq_id_ = key_generator_->Next();
  if(query == true) insert_key_sequence_.Next();
  StringKey key;

  if(from_file_) {
    assert(seq_id_ < max_seq_id_);
    key = keys_[seq_id_];       // a key from given dataset
  } else {
    #ifdef DEBUG
      key = myseq_id_;
    #else
      key = utils::Hash(seq_id_); // a random distributed key
    #endif
  }

  return key.str_;
}

inline std::string CoreWorkload::NextTransactionKey() {
  uint64_t seq_id_;
  do {
    seq_id_ = key_chooser_->Next();
  } while (seq_id_ > key_generator_->Last());
  
  StringKey key;
  if(from_file_) {
    assert(seq_id_ < max_seq_id_);
    key = keys_[seq_id_];        // a key from given dataset
  } else {
    #ifdef DEBUG
      key = seq_id_;
    #else
      key = utils::Hash(seq_id_); // a random distributed key
    #endif
  }

  return key.str_;
}
  
} // ycsbc

#endif // YCSB_C_CORE_WORKLOAD_H_
