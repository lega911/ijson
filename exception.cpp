
#include "exception.h"


Exception error::not_implemented(const char *msg) {return Exception(Exception::NOT_IMPLEMENTED, msg);}
