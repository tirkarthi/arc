#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <cstring>

#include <glibmm.h>

#include <arc/FileUtils.h>
#include <arc/Logger.h>
#include <arc/StringConv.h>

#include "uid.h"

#include "FileRecordSQLite.h"

namespace ARex {

  #define FR_DB_NAME "list"

  bool FileRecordSQLite::dberr(const char* s, int err) {
    if(err == SQLITE_OK) return true;
    error_num_ = err;
    error_str_ = std::string(s)+": "+sqlite3_errstr(err);
    return false;
  }

  FileRecordSQLite::FileRecordSQLite(const std::string& base, bool create):
      FileRecord(base, create),
      db_(NULL) {
    valid_ = open(create);
  }

  bool FileRecordSQLite::verify(void) {
    // Performing various kinds of verifications
/*
    std::string dbpath = basepath_ + G_DIR_SEPARATOR_S + FR_DB_NAME;
    {
      Db db_test(NULL,DB_CXX_NO_EXCEPTIONS);
      if(!dberr("Error verifying databases",
                db_test.verify(dbpath.c_str(),NULL,NULL,DB_NOORDERCHK))) {
        if(error_num_ != ENOENT) return false;
      };
    };
    {
      Db db_test(NULL,DB_CXX_NO_EXCEPTIONS);
      if(!dberr("Error verifying database 'meta'",
                db_test.verify(dbpath.c_str(),"meta",NULL,DB_ORDERCHKONLY))) {
        if(error_num_ != ENOENT) return false;
      };
    };
    // Skip 'link' - it is not of btree kind
    // Skip 'lock' - for unknown reason it returns DB_NOTFOUND
    // Skip 'locked' - for unknown reason it returns DB_NOTFOUND
*/
    return true;
  }

  FileRecordSQLite::~FileRecordSQLite(void) {
    close();
  }

  bool FileRecordSQLite::open(bool create) {
    std::string dbpath = basepath_ + G_DIR_SEPARATOR_S + FR_DB_NAME;
    if(db_ != NULL) return true;

    if(!dberr("Error opening database", sqlite3_open(dbpath.c_str(), &db_))) {
      db_ = NULL;
      return false;
    };
    if(!dberr("Error creating table", sqlite3_exec(db_, "CREATE TABLE IF NOT EXISTS rec(id, owner, uid, meta, UNIQUE(id, owner), UNIQUE(uid))", NULL, NULL, NULL))) {
      (void)sqlite3_close(db_); // todo: handle error
      db_ = NULL;
      return false;
    };
    if(!dberr("Error creating table", sqlite3_exec(db_, "CREATE TABLE IF NOT EXISTS lock(lockid, uid)", NULL, NULL, NULL))) {
      (void)sqlite3_close(db_); // todo: handle error
      db_ = NULL;
      return false;
    };
    if(!dberr("Error creating table", sqlite3_exec(db_, "CREATE INDEX IF NOT EXISTS ON lock (lockid)", NULL, NULL, NULL))) {
      (void)sqlite3_close(db_); // todo: handle error
      db_ = NULL;
      return false;
    };
    if(!dberr("Error creating table", sqlite3_exec(db_, "CREATE INDEX IF NOT EXISTS ON lock (uid)", NULL, NULL, NULL))) {
      (void)sqlite3_close(db_); // todo: handle error
      db_ = NULL;
      return false;
    };
  }

  void FileRecordSQLite::close(void) {
    valid_ = false;
    if(db_) {
      (void)sqlite3_close(db_); // todo: handle error
      db_ = NULL;
    };
  }

  static const std::string sql_special_chars("'#\r\n\b\0",6);
  static const char sql_escape_char('%');
  static const Arc::escape_type sql_escape_type(Arc::escape_hex);

  inline static std::string sql_escape(const std::string& str) {
    return Arc::escape_chars(str, sql_special_chars, sql_escape_char, false, sql_escape_type);
  }

  inline static std::string sql_unescape(const std::string& str) {
    return Arc::unescape_chars(str, sql_escape_char,sql_escape_type);
  }

  void store_strings(const std::list<std::string>& strs, std::string& buf) {
    if(!strs.empty()) {
      for(std::list<std::string>::const_iterator str = strs.begin(); ; ++str) {
        buf += sql_escape(*str);
        if (str == strs.end()) break;
        buf += '#';
      };
    };
  }

  static void parse_strings(std::list<std::string>& strs, const char* buf) {
    if(!buf || (*buf == '\0')) return;
    const char* sep = std::strchr(buf, '#');
    while(sep) {
      strs.push_back(sql_unescape(std::string(buf,sep-buf)));
      buf = sep+1;
      sep = std::strchr(buf, '#');
    };
  }

  bool FileRecordSQLite::Recover(void) {
    Glib::Mutex::Lock lock(lock_);
    // Real recovery not implemented yet.
    close();
    error_num_ = -1;
    error_str_ = "Recovery not implemented yet.";
    return false;
  }

  struct FindCallbackUidMetaArg {
    std::string& uid;
    std::list<std::string>& meta;
    FindCallbackUidMetaArg(std::string& uid, std::list<std::string>& meta): uid(uid), meta(meta) {};
  };

  static int FindCallbackUidMeta(void* arg, int colnum, char** texts, char** names) {
    for(int n = 0; n < colnum; ++n) {
      if(names[n] && texts[n]) {
        if(strcmp(names[n], "uid") == 0) {
          ((FindCallbackUidMetaArg*)arg)->uid = texts[n];
        } else if(strcmp(names[n], "meta") == 0) {
          parse_strings(((FindCallbackUidMetaArg*)arg)->meta, texts[n]);
        };
      };
    };
    return 0;
  }

  struct FindCallbackUidArg {
    std::string& uid;
    FindCallbackUidArg(std::string& uid): uid(uid) {};
  };

  static int FindCallbackUid(void* arg, int colnum, char** texts, char** names) {
    for(int n = 0; n < colnum; ++n) {
      if(names[n] && texts[n]) {
        if(strcmp(names[n], "uid") == 0) {
          ((FindCallbackUidMetaArg*)arg)->uid = texts[n];
        };
      };
    };
  }

  struct FindCallbackCountArg {
    int count;
    FindCallbackCountArg():count(0) {};
  };

  static int FindCallbackCount(void* arg, int colnum, char** texts, char** names) {
    ((FindCallbackCountArg*)arg)->count += 1;
  }

  struct FindCallbackIdOwnerArg {
    std::list< std::pair<std::string,std::string> >& records;
    FindCallbackIdOwnerArg(std::list< std::pair<std::string,std::string> >& recs): records(recs) {};
  };

  static int FindCallbackIdOwner(void* arg, int colnum, char** texts, char** names) {
    std::pair<std::string,std::string> rec;
    for(int n = 0; n < colnum; ++n) {
      if(names[n] && texts[n]) {
        if(strcmp(names[n], "id") == 0) {
          rec.first = sql_unescape(texts[n]);
        } else if(strcmp(names[n], "owner") == 0) {
          rec.second = sql_unescape(texts[n]);
        };
      };
    };
    if(!rec.first.empty()) ((FindCallbackIdOwnerArg*)arg)->records.push_back(rec);
  }


  std::string FileRecordSQLite::Add(std::string& id, const std::string& owner, const std::list<std::string>& meta) {
    if(!valid_) return "";
    Glib::Mutex::Lock lock(lock_);
    // todo: retries for unique uid?
    std::string uid = rand_uid64().substr(4);
    std::string metas;
    store_strings(meta, metas);
    if(id.empty()) id = uid;
    std::string sqlcmd = "INSERT INTO rec(id, owner, uid, meta) VALUES ('"+
                             sql_escape(id)+"', '"+sql_escape(owner)+"', '"+uid+"', '"+metas+"')";
    if(!dberr("Failed to add record to database", sqlite3_exec(db_, sqlcmd.c_str(), NULL, NULL, NULL))) {
      return "";
    };
    if(sqlite3_changes(db_) != 1) {
      error_str_ = "Failed to add record to database";
      return "";
    };
    return uid_to_path(uid);
  }

  std::string FileRecordSQLite::Find(const std::string& id, const std::string& owner, std::list<std::string>& meta) {
    if(!valid_) return "";
    Glib::Mutex::Lock lock(lock_);
    std::string sqlcmd = "SELECT uid, meta FROM rec WHERE ((id = '"+sql_escape(id)+"') AND (owner = '"+sql_escape(owner)+"'))";
    std::string uid;
    FindCallbackUidMetaArg arg(uid, meta);
    if(!dberr("Failed to retrieve record from database",sqlite3_exec(db_, sqlcmd.c_str(), &FindCallbackUidMeta, &arg, NULL))) {
      return "";
    };
    if(uid.empty()) {
      error_str_ = "Failed to retrieve record from database";
      return "";
    };
    return uid_to_path(uid);
  }

  bool FileRecordSQLite::Modify(const std::string& id, const std::string& owner, const std::list<std::string>& meta) {
    if(!valid_) return false;
    Glib::Mutex::Lock lock(lock_);
    std::string metas;
    store_strings(meta, metas);
    std::string sqlcmd = "UPDATE rec SET meta = '"+metas+"' WHERE ((id = '"+sql_escape(id)+"') AND (owner = '"+sql_escape(owner)+"'))";
    if(!dberr("Failed to update record in database",sqlite3_exec(db_, sqlcmd.c_str(), NULL, NULL, NULL))) {
      return false;
    };
    if(sqlite3_changes(db_) < 1) {
      error_str_ = "Failed to find record in database";
      return false;
    };
    return true;
  }

  bool FileRecordSQLite::Remove(const std::string& id, const std::string& owner) {
    if(!valid_) return false;
    Glib::Mutex::Lock lock(lock_);
    std::string uid;
    {
      std::string sqlcmd = "SELECT uid FROM rec WHERE ((id = '"+sql_escape(id)+"') AND (owner = '"+sql_escape(owner)+"'))";
      FindCallbackUidArg arg(uid);
      if(!dberr("Failed to retrieve record from database",sqlite3_exec(db_, sqlcmd.c_str(), &FindCallbackUid, &arg, NULL))) {
        return false; // No such record?
      };
    };
    if(uid.empty()) {
      error_str_ = "Record not found";
      return false; // No such record
    };
    {
      std::string sqlcmd = "SELECT FROM lock WHERE (uid = '"+uid+"')";
      FindCallbackCountArg arg;
      if(!dberr("Failed to find locks in database",sqlite3_exec(db_, sqlcmd.c_str(), &FindCallbackCount, &arg, NULL))) {
        return false;
      };
      if(arg.count > 0) {
        error_str_ = "Record has active locks";
        return false; // have locks
      };
    };
    ::unlink(uid_to_path(uid).c_str()); // TODO: handle error
    {
      std::string sqlcmd = "DELETE FROM rec WHERE (uid = '"+uid+"')";
      if(!dberr("Failed to delete record in database",sqlite3_exec(db_, sqlcmd.c_str(), NULL, NULL, NULL))) {
        return false;
      };
      if(sqlite3_changes(db_) < 1) {
        error_str_ = "Failed to delete record in database";
        return false; // no such record
      };
    };
    return true;
  }

  bool FileRecordSQLite::AddLock(const std::string& lock_id, const std::list<std::string>& ids, const std::string& owner) {
    if(!valid_) return false;
    Glib::Mutex::Lock lock(lock_);
    for(std::list<std::string>::const_iterator id = ids.begin(); id != ids.end(); ++id) {
      std::string uid;
      {
        std::string sqlcmd = "SELECT uid FROM rec WHERE ((id = '"+sql_escape(*id)+"') AND (owner = '"+sql_escape(owner)+"'))";
        FindCallbackUidArg arg(uid);
        if(!dberr("Failed to retrieve record from database",sqlite3_exec(db_, sqlcmd.c_str(), &FindCallbackUid, &arg, NULL))) {
          return false; // No such record?
        };
      };
      if(uid.empty()) {
        // No such record
        continue;
      };
      std::string sqlcmd = "INSERT INTO lock(lockid, uid) VALUES ('"+sql_escape(lock_id)+"','"+uid+"')";
      if(!dberr("addlock:put",sqlite3_exec(db_, sqlcmd.c_str(), NULL, NULL, NULL))) {
        return false;
      };
    };
    return true;
  }

  bool FileRecordSQLite::RemoveLock(const std::string& lock_id) {
    std::list<std::pair<std::string,std::string> > ids;
    return RemoveLock(lock_id,ids);
  }

  bool FileRecordSQLite::RemoveLock(const std::string& lock_id, std::list<std::pair<std::string,std::string> >& ids) {
    if(!valid_) return false;
    Glib::Mutex::Lock lock(lock_);
    // map lock to id,owner 
    {
      std::string sqlcmd = "SELECT id,owner FROM rec WHERE uid IN SELECT uid FROM lock WHERE (lockid = '"+sql_escape(lock_id)+"')";
      FindCallbackIdOwnerArg arg(ids);
      if(!dberr("removelock:get",sqlite3_exec(db_, sqlcmd.c_str(), &FindCallbackIdOwner, &arg, NULL))) {
        //return false;
      };
    };
    {
      std::string sqlcmd = "DELETE FROM lock WHERE (lockid = '"+sql_escape(lock_id)+"')";
      if(!dberr("removelock:del",sqlite3_exec(db_, sqlcmd.c_str(), NULL, NULL, NULL))) {
        return false;
      };
      if(sqlite3_changes(db_) < 1) {
        error_str_ = "";
        return false;
      };
    };
    return true;
  }


  bool FileRecordSQLite::ListLocked(const std::string& lock_id, std::list<std::pair<std::string,std::string> >& ids) {
    if(!valid_) return false;
    Glib::Mutex::Lock lock(lock_);
/*
    Dbc* cur = NULL;
    if(!dberr("listlocked:cursor",db_lock_->cursor(NULL,&cur,0))) return false;
    Dbt key;
    Dbt data;
    make_string(lock_id,key);
    void* pkey = key.get_data();
    if(!dberr("listlocked:get1",cur->get(&key,&data,DB_SET))) { // TODO: handle errors
      ::free(pkey);
      cur->close(); return false;
    };
    for(;;) {
      std::string id;
      std::string owner;
      uint32_t size = data.get_size();
      void* buf = data.get_data();
      buf = parse_string(id,buf,size); //  lock_id - skip
      buf = parse_string(id,buf,size);
      buf = parse_string(owner,buf,size);
      ids.push_back(std::pair<std::string,std::string>(id,owner));
      if(cur->get(&key,&data,DB_NEXT_DUP) != 0) break;
    };
    ::free(pkey);
    cur->close();
*/
    return true;
  }

  bool FileRecordSQLite::ListLocks(std::list<std::string>& locks) {
    if(!valid_) return false;
    Glib::Mutex::Lock lock(lock_);
/*
    Dbc* cur = NULL;
    if(db_lock_->cursor(NULL,&cur,0)) return false;
    for(;;) {
      Dbt key;
      Dbt data;
      if(cur->get(&key,&data,DB_NEXT_NODUP) != 0) break; // TODO: handle errors
      std::string str;
      uint32_t size = key.get_size();
      parse_string(str,key.get_data(),size);
      locks.push_back(str);
    };
    cur->close();
*/
    return true;
  }

  bool FileRecordSQLite::ListLocks(const std::string& id, const std::string& owner, std::list<std::string>& locks) {
    // Not implemented yet
    return false;
  }

  FileRecordSQLite::Iterator::Iterator(FileRecordSQLite& frec):FileRecord::Iterator(frec) {
/*
    Glib::Mutex::Lock lock(frec_.lock_);
    if(!frec_.dberr("Iterator:cursor",frec_.db_rec_->cursor(NULL,&cur_,0))) {
      if(cur_) {
        cur_->close(); cur_=NULL;
      };
      return;
    };
    Dbt key;
    Dbt data;
    if(!frec_.dberr("Iterator:first",cur_->get(&key,&data,DB_FIRST))) {
      cur_->close(); cur_=NULL;
      return;
    };
    parse_record(uid_,id_,owner_,meta_,key,data);
*/
  }

  FileRecordSQLite::Iterator::~Iterator(void) {
/*
    Glib::Mutex::Lock lock(frec_.lock_);
    if(cur_) {
      cur_->close(); cur_=NULL;
    };
*/
  }

  FileRecordSQLite::Iterator& FileRecordSQLite::Iterator::operator++(void) {
/*
    if(!cur_) return *this;
    Glib::Mutex::Lock lock(frec_.lock_);
    Dbt key;
    Dbt data;
    if(!frec_.dberr("Iterator:first",cur_->get(&key,&data,DB_NEXT))) {
      cur_->close(); cur_=NULL;
      return *this;
    };
    parse_record(uid_,id_,owner_,meta_,key,data);
*/
    return *this;
  }

  FileRecordSQLite::Iterator& FileRecordSQLite::Iterator::operator--(void) {
/*
    if(!cur_) return *this;
    Glib::Mutex::Lock lock(frec_.lock_);
    Dbt key;
    Dbt data;
    if(!frec_.dberr("Iterator:first",cur_->get(&key,&data,DB_PREV))) {
      cur_->close(); cur_=NULL;
      return *this;
    };
    parse_record(uid_,id_,owner_,meta_,key,data);
*/
    return *this;
  }

  void FileRecordSQLite::Iterator::suspend(void) {
/*
    Glib::Mutex::Lock lock(frec_.lock_);
    if(cur_) {
      cur_->close(); cur_=NULL;
    }
*/
  }

  bool FileRecordSQLite::Iterator::resume(void) {
/*
    Glib::Mutex::Lock lock(frec_.lock_);
    if(!cur_) {
      if(id_.empty()) return false;
      if(!frec_.dberr("Iterator:cursor",frec_.db_rec_->cursor(NULL,&cur_,0))) {
        if(cur_) {
          cur_->close(); cur_=NULL;
        };
        return false;
      };
      Dbt key;
      Dbt data;
      make_key(id_,owner_,key);
      void* pkey = key.get_data();
      if(!frec_.dberr("Iterator:first",cur_->get(&key,&data,DB_SET))) {
        ::free(pkey);
        cur_->close(); cur_=NULL;
        return false;
      };
      parse_record(uid_,id_,owner_,meta_,key,data);
      ::free(pkey);
    };
*/
    return true;
  }

} // namespace ARex

