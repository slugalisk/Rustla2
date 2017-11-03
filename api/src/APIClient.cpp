#include "APIClient.h"

#include <cxxabi.h>
#include <folly/Format.h>

#include "JSON.h"
#include "Status.h"

namespace rustla2 {

Status APIResult::SetData(const char* data, size_t length) {
  Status status;
  data_ = json::Parse(data, length, GetSchema(), &status);

  if (status.Ok()) return status;

  int demangle_error;
  char* name = abi::__cxa_demangle(typeid(this).name(), 0, 0, &demangle_error);

  return Status(status.GetCode(),
                folly::sformat("{}: {}", demangle_error ? "unknown" : name,
                               status.GetErrorMessage()),
                status.GetErrorDetails());
}

}  // namespace rustla2
