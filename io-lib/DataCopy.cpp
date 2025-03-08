#include "DataCopy.h"

DataCopy::DataCopy() {}

size_t DataCopy::write(const void* buffer, size_t len)
{
  buf.insert(buf.end(), (UINT8*)buffer, (UINT8*)buffer + len);
  return len;
}

size_t DataCopy::read(void* buffer, size_t len)
{
  size_t have = buf.size();
  if (have < len) {
    len = have;
  }
  std::copy(buf.begin(), buf.begin() + len, (UINT8*)buffer);
  std::copy(buf.begin() + len, buf.end(), buf.begin());
  buf.resize(have - len);
  return len;
}

size_t DataCopy::available()
{
  return buf.size();
}
