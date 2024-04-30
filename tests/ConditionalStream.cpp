#include "ConditionalStream.h"

#include <ostream>

#define ENABLE_LOGGING 1

#if !ENABLE_LOGGING
namespace {
    class blackhole_buf final : public std::streambuf {
        char_type target[64];
        std::streamsize xsputn(const char_type *, std::streamsize n) {
            return n;
        }
        int_type overflow(int_type = traits_type::eof()) {
            setp(target, std::end(target));
            return 0;
        }
    };
    blackhole_buf bf;
    std::ostream kNullBufStm{&bf};
} // namespace
std::ostream & LOGGER = kNullBufStm;
#else
std::ostream & LOGGER = std::cout;
#endif