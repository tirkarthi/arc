#include <stdint.h>
#include <string.h>

namespace Arc {

class CheckSum {
 public:
  CheckSum(void) { };
  virtual ~CheckSum(void) { };
  virtual void start(void) { };
  virtual void add(void* buf,unsigned long long int len) { };
  virtual void end(void) { };
  virtual void result(unsigned char*& res,unsigned int& len) const { len=0; };
  virtual int print(char* buf,int len) const { if(len>0) buf[0]=0; return 0; };
  virtual void scan(const char* buf) { };
  virtual operator bool(void) const { return false; };
  virtual bool operator!(void) const { return true; };
};


class CRC32Sum: public CheckSum {
 private:
  uint32_t r;
  unsigned long long count;
  bool computed;
 public:
  CRC32Sum(void);
  virtual ~CRC32Sum(void) { };
  virtual void start(void);
  virtual void add(void* buf,unsigned long long int len);
  virtual void end(void);
  virtual void result(unsigned char*& res,unsigned int& len) const { res=(unsigned char*)&r; len=4; };
  virtual int print(char* buf,int len) const;
  virtual void scan(const char* buf);
  virtual operator bool(void) const { return computed; };
  virtual bool operator!(void) const { return !computed; };
  uint32_t crc(void) const { return r; };
};

class MD5Sum: public CheckSum {
 private:
  bool computed;
  uint32_t A;
  uint32_t B;
  uint32_t C;
  uint32_t D;
  uint64_t count;
  uint32_t X[16];
  unsigned int Xlen;
  // uint32_t T[64];
 public:
  MD5Sum(void);
  virtual void start(void);
  virtual void add(void* buf,unsigned long long int len);
  virtual void end(void);
  virtual void result(unsigned char*& res,unsigned int& len) const { res=(unsigned char*)&A; len=16; };
  virtual int print(char* buf,int len) const;
  virtual void scan(const char* buf);
  virtual operator bool(void) const { return computed; };
  virtual bool operator!(void) const { return !computed; };
};

class CheckSumAny: public CheckSum {
 public:
  typedef enum {
    none,
    unknown,
    undefined,
    cksum,
    md5
  } type;
 private:
  CheckSum* cs;
  type tp;
 public:
  CheckSumAny(CheckSum* c = NULL):cs(c),tp(none) { };
  CheckSumAny(type type);
  CheckSumAny(const char* type);
  virtual ~CheckSumAny(void) { if(cs) delete cs; };
  virtual void start(void) { if(cs) cs->start(); };
  virtual void add(void* buf,unsigned long long int len) {
    if(cs) cs->add(buf,len);
  };
  virtual void end(void) { if(cs) cs->end(); };
  virtual void result(unsigned char*& res,unsigned int& len) const {
    if(cs) { cs->result(res,len); return; }; len=0;
  };
  virtual int print(char* buf,int len) const {
    if(cs) return cs->print(buf,len);
    if(len>0) buf[0]=0; return 0;
  };
  virtual void scan(const char* buf) { 
    if(cs) cs->scan(buf);
  };
  virtual operator bool(void) const { if(!cs) return false; return *cs; };
  virtual bool operator!(void) const { if(!cs) return true; return !(*cs); };
  bool active(void) { return (cs!=NULL); };
  static type Type(const char* crc);
  type Type(void) { return tp; };
  void operator=(const char* type);
  bool operator==(const char* s);
  bool operator==(const CheckSumAny& ck);
};

} // namespace Arc
