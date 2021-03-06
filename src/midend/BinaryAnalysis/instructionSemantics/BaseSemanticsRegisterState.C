#include <rosePublicConfig.h>
#ifdef ROSE_BUILD_BINARY_ANALYSIS_SUPPORT
#include <sage3basic.h>
#include <BaseSemanticsRegisterState.h>

#include <BaseSemanticsFormatter.h>

namespace Rose {
namespace BinaryAnalysis {
namespace InstructionSemantics2 {
namespace BaseSemantics {

std::ostream&
operator<<(std::ostream &o, const RegisterState &x) {
    x.print(o);
    return o;
}

std::ostream&
operator<<(std::ostream &o, const RegisterState::WithFormatter &x) {
    x.print(o);
    return o;
}

void
RegisterState::print(std::ostream &stream, const std::string prefix) const {
    Formatter fmt;
    fmt.set_line_prefix(prefix);
    print(stream, fmt);
}

RegisterState::WithFormatter
RegisterState::operator+(const std::string &linePrefix) {
    static Formatter fmt;
    fmt.set_line_prefix(linePrefix);
    return with_format(fmt);
}
    
} // namespace
} // namespace
} // namespace
} // namespace

#endif
