#include "ConditionalStream.h"

#include <ostream>

#define ENABLE_LOGGING 0

#if !ENABLE_LOGGING
namespace {
    class blackhole_buf final : public std::streambuf {
        char_type _target[64] = {};
        std::streamsize xsputn(const char_type *, std::streamsize n) override {
            return n;
        }
        int_type overflow(int_type = traits_type::eof()) override {
            setp(_target, std::end(_target));
            return 0;
        }
    };
    blackhole_buf BF;
    std::ostream K_NULL_BUF_STM{&BF};
} // namespace
std::ostream & LOGGER = K_NULL_BUF_STM;
#else
std::ostream & LOGGER = std::cout;
#endif