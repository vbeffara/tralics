#include "txinline.h"

Istring::Istring(name_positions N) : value(the_names[N].value) {}

// True if the string holds only white space
auto Istring::only_space() const -> bool {
    const std::string &s = the_main->SH[value];
    size_t             i = 0, l = s.length();
    while (i < l) {
        if (is_space(s[i]))
            i++;
        else if (s[i] == static_cast<char>(194) && i + 1 < l && s[i + 1] == static_cast<char>(160))
            i += 2;
        else
            return false;
    }
    return true;
}

// True if the string holds only white space or &nbsp;
auto Istring::only_space_spec() const -> bool {
    int    i = 0;
    String s = c_str();
    while (s[i] != 0) {
        if (is_space(s[i]))
            i++;
        else if ((strncmp(s + i, "&nbsp;", 6) == 0) || (strncmp(s + i, "&#xA0;", 6) == 0) || (strncmp(s + i, "\302\240", 2) == 0))
            i += 6;
        else
            return false;
    }
    return true;
}
