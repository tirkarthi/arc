#ifndef __ARC_PAYLOADRAW_H__
#define __ARC_PAYLOADRAW_H__

#include <vector>

#include "Message.h"

namespace Arc {

/// Random Access Payload for Message objects
/** This class is a virtual interface for managing Message payload with 
  arbitrarily accessible content. Inheriting classes are supposed to implement
  memory-resident or memory-mapped content made of optionally multiple chunks/buffers.
  Every buffer has own size and offset.
  This class is purely virtual. */
class PayloadRawInterface: virtual public MessagePayload {
 public:
  // Avoid defining size of int - just use biggest possible
  typedef signed long long int Size_t;
  PayloadRawInterface(void) { };
  virtual ~PayloadRawInterface(void) { };
  /** Returns content of byte at specified position. Specified position 'pos' 
    is treated as global one and goes through all buffers placed one 
    after another. */  
  virtual char operator[](Size_t pos) const = 0;
  /** Get pointer to buffer content at global position 'pos'. By default to
    beginning of main buffer whatever that means. */
  virtual char* Content(Size_t pos = -1) = 0;
  /** Returns logical size of whole structure. */
  virtual Size_t Size(void) const = 0;
  /**  Create new buffer at global position 'pos' of size 'size'. */
  virtual char* Insert(Size_t pos = 0,Size_t size = 0) = 0;
  /** Create new buffer at global position 'pos' of size 'size'.
    Created buffer is filled with content of memory at 's'.
    If 'size' is negative content at 's' is expected to be null-terminated. */
  virtual char* Insert(const char* s,Size_t pos = 0,Size_t size = -1) = 0;
  /** Returns pointer to num'th buffer */
  virtual char* Buffer(unsigned int num) = 0;
  /** Returns length of num'th buffer */
  virtual Size_t BufferSize(unsigned int num) const = 0;
  /** Returns position of num'th buffer */
  virtual Size_t BufferPos(unsigned int num) const = 0;
  /** Change size of stored information.
    If size exceeds end of allocated buffer, buffers are not 
    re-allocated, only logical size is extended.
    Buffers with location behind new size are deallocated. */
  virtual bool Truncate(Size_t size) = 0;
};

/* Buffer type for PayloadRaw */
typedef struct {
    char* data;     /** pointer to buffer in memory */
    int size;       /** size of allocated memory */
    int length;     /** size of used memory - size of buffer */
    bool allocated; /** true if memory has to free by destructor */
} PayloadRawBuf;

/// Raw byte multi-buffer. 
/** This is implementation of PayloadRawInterface. Buffers are memory 
  blocks logically placed one after another. */ 
class PayloadRaw: virtual public PayloadRawInterface {
 protected:
  Size_t offset_;
  Size_t size_;
  std::vector<PayloadRawBuf> buf_; /** List of handled buffers. */
 public:
  /** Constructor. Created object contains no buffers. */
  PayloadRaw(void):offset_(0),size_(0) { };
  /** Destructor. Frees allocated buffers. */
  virtual ~PayloadRaw(void);
  virtual char operator[](Size_t pos) const;
  virtual char* Content(Size_t pos = -1);
  virtual Size_t Size(void) const;
  virtual char* Insert(Size_t pos = 0,Size_t size = 0);
  virtual char* Insert(const char* s,Size_t pos = 0,Size_t size = -1);
  virtual char* Buffer(unsigned int num = 0);
  virtual Size_t BufferSize(unsigned int num = 0) const;
  virtual Size_t BufferPos(unsigned int num = 0) const;
  virtual bool Truncate(Size_t size);
};

/// Returns pointer to main memory chunk of Message payload. 
/** If no buffer is present or if payload is not of PayloadRawInterface type NULL is returned. */ 
const char* ContentFromPayload(const MessagePayload& payload);

} // namespace Arc

#endif /* __ARC_PAYLOADRAW_H__ */
