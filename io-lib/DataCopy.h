#ifndef _DATA_COPY_H_
#define _DATA_COPY_H_

#include "InputStream.h"
#include "OutputStream.h"

#include "util/inttypes.h"

class DataCopy : public InputStream, public OutputStream
{
public:
  DataCopy();

  size_t read(void* buffer, size_t len) throw(IOException);
  size_t write(const void* buffer, size_t len) throw(IOException);
  size_t available();

private:
  std::vector<UINT8> buf;
};
#endif

