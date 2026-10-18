#ifndef M8_SHARED_LIBRARY_H
#define M8_SHARED_LIBRARY_H

#if defined(__APPLE__)
#define M8_SHARED_LIBRARY_NAME(stem) stem ".dylib"
#else
#define M8_SHARED_LIBRARY_NAME(stem) stem ".so"
#endif

#endif
