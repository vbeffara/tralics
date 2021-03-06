// Tralics, a LaTeX to XML translator.
// Copyright INRIA/apics/marelle (Jose' Grimm) 2006-2011

// This software is governed by the CeCILL license under French law and
// abiding by the rules of distribution of free software.  You can  use,
// modify and/ or redistribute the software under the terms of the CeCILL
// license as circulated by CEA, CNRS and INRIA at the following URL
// "http://www.cecill.info".
// (See the file COPYING in the main directory for details)

// This file contains ltclass.dtx

#include "txclasses.h"
#include "txinline.h"
#include "txparser.h"

Buffer file_list;

namespace {
    Buffer      local_buf;
    ClassesData the_class_data;
    // global variable so that txparser.h does not need to know OptionList
    OptionList cur_opt_list;
} // namespace

namespace classes_ns {
    auto parse_version(const std::string &s) -> int;
    auto is_in_vector(const OptionList &V, const std::string &s, bool X) -> std::optional<size_t>;
    auto is_raw_option(const OptionList &V, String s) -> bool;
    auto is_in_option(const OptionList &V, const KeyAndVal &s) -> bool;
    auto make_options(TokenList &L) -> OptionList;
    auto compare_options(const OptionList &A, const OptionList &B) -> bool;
    void dump_options(const OptionList &A, String x);
    auto cur_options(bool star, TokenList &spec, bool normal) -> TokenList;
    auto make_keyval(TokenList &L) -> KeyAndVal;
    void register_key(const std::string &Key);
    void unknown_optionX(TokenList &cur, TokenList &action);
    void unknown_option(KeyAndVal &cur_keyval, TokenList &res, TokenList &spec, int X);
    void add_to_filelist(const std::string &s, const std::string &date);
    void add_sharp(TokenList &L);
} // namespace classes_ns

using namespace classes_ns;
using namespace token_ns;

// ---------------------------------------------------------
// Functions dealing with option lists

// Splits key=val in two parts
auto classes_ns::make_keyval(TokenList &key_val) -> KeyAndVal {
    TokenList key;
    Token     equals      = Token(other_t_offset, '=');
    bool      have_equals = split_at(equals, key_val, key);
    remove_first_last_space(key_val);
    remove_first_last_space(key);
    if (have_equals) key_val.push_front(equals);
    std::string key_name = the_parser.list_to_string_c(key, "Invalid option");
    std::string key_full = key_name;
    if (have_equals) {
        Buffer &B = local_buf;
        B << bf_reset << key_name << key_val;
        key_full = B.to_string();
    }
    return KeyAndVal(key_name, key_val, key_full);
}

// Reverse function
auto KeyAndVal::to_list() const -> TokenList {
    TokenList u = string_to_list(name, false);
    if (val.empty()) return u;
    TokenList aux = val;
    u.splice(u.end(), aux);
    return u;
}

// Constructs an option list from a comma separated string.
auto classes_ns::make_options(TokenList &L) -> OptionList {
    OptionList res;
    TokenList  key;
    Token      comma = the_parser.hash_table.comma_token;
    while (!L.empty()) {
        token_ns::split_at(comma, L, key);
        token_ns::remove_first_last_space(key);
        if (key.empty()) continue;
        res.push_back(make_keyval(key));
    }
    return res;
}

// Prints the options list on log and tty
void classes_ns::dump_options(const OptionList &A, String x) {
    Buffer &B = local_buf;
    B.reset();
    auto n = A.size();
    for (size_t i = 0; i < n; i++) {
        if (i > 0) B << ",";
        A[i].dump(B);
    }
    log_and_tty << x << B << ".\n";
}

// Adds a copy of L to the option list of the package.
void LatexPackage::add_options(const OptionList &L) {
    auto n = L.size();
    for (size_t j = 0; j < n; j++) Uoptions.push_back(L[j]);
}

// Returns true if S is in the option list (for check_builtin_class)
auto classes_ns::is_raw_option(const OptionList &V, String s) -> bool {
    auto n = V.size();
    for (size_t i = 0; i < n; i++)
        if (V[i].has_name(s)) return true;
    return false;
}

// Returns true if slot is in the vector V with the same value
auto classes_ns::is_in_option(const OptionList &V, const KeyAndVal &slot) -> bool {
    const std::string &s = slot.name;
    auto               n = V.size();
    for (size_t i = 0; i < n; i++)
        if (V[i].has_name(s)) return compare(slot.val, V[i].val);
    return false;
}

// Returns true if each element of B is in A, i.e. A contains B
auto classes_ns::compare_options(const OptionList &A, const OptionList &B) -> bool {
    auto n = B.size();
    for (size_t i = 0; i < n; i++)
        if (!is_in_option(A, B[i])) return false;
    return true;
}

// Like is_in_option, but returns a position
// If X true, checks the keyname
auto classes_ns::is_in_vector(const OptionList &V, const std::string &s, bool X) -> std::optional<size_t> {
    for (size_t i = 0; i < V.size(); i++)
        if (X ? V[i].has_name(s) : V[i].has_full_name(s)) return i;
    return {};
}

// ------------------------------------------------------------

// Find the index of a package in the list. Returns 0 in case of
// failure (slot 0 does not hold a valid package)
// Creates if creat is true.
auto ClassesData::find_package(const std::string &name, bool type, bool creat) -> size_t {
    std::string full_name = (type ? "C" : "P") + name;
    auto        n         = packages.size();
    for (size_t i = 1; i < n; i++)
        if (packages[i]->has_name(full_name)) return i;
    if (!creat) return 0;
    packages.push_back(new LatexPackage(full_name));
    return n;
}

// Implements \@ifpackageloaded \@ifclassloaded.
// True if the date field is set (missing date in a package gives 0000/00/00)
void Parser::T_if_package_loaded(bool type) // true for class
{
    std::string name = sE_arg_nopar();
    auto        i    = the_class_data.find_package(name, type, false);
    bool        res  = false;
    if ((i != 0) && !the_class_data.packages[i]->date.empty()) res = true;
    one_of_two(res);
}

// Implements \@ifpackagelater \@ifclasslater.
// \@ifpackagelater{name}{YYYY/MM/DD}
void Parser::T_if_package_later(bool c) // true for class
{
    std::string name  = sE_arg_nopar();
    std::string date  = sE_arg_nopar();
    int         idate = parse_version(date);
    int         pdate = 0;
    auto        i     = the_class_data.find_package(name, c, false);
    if (i != 0) pdate = parse_version(the_class_data.packages[i]->date);
    one_of_two(pdate >= idate);
}

// Implements \@ifpackagewith \@ifclasswith.
// \@ifpackagewith{name}{option-list}
void Parser::T_if_package_with(bool c) // true for class
{
    std::string name    = sE_arg_nopar();
    TokenList   options = read_arg();
    OptionList  A;
    OptionList  B = make_options(options);
    auto        p = the_class_data.find_package(name, c, false);
    if (p != 0) A = the_class_data.packages[p]->Uoptions;
    bool res = compare_options(A, B);
    one_of_two(res);
}

// Class data ctor
ClassesData::ClassesData() { packages.push_back(new LatexPackage("Fdummy file")); }

LatexPackage::LatexPackage(std::string A) : name(std::move(A)), has_a_default(false), seen_process(false), checked(false) {}

// Returns data for current class or package
// Hack for InputClass, where N is negative
auto ClassesData::cur_pack() -> LatexPackage * {
    auto n = the_parser.get_cur_file_pos();
    if (n < 0) n = -n;
    return packages[to_unsigned(n)];
}

// Date is something like 2004/12/03 converted to 20041203
// Hack: we read at most 8 digits, ignore everything else
auto classes_ns::parse_version(const std::string &s) -> int {
    auto n = s.size();
    int  r = 0;
    int  k = 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (is_digit(c)) {
            r = 10 * r + (c - '0');
            k++;
            if (k == 8) break;
        }
    }
    return r;
}

// Use the option, meaning execute the code associated to it.
// We put a copy of the value at the end of A, rather than putting it directly
// in the input stream
void KeyAndVal::use(TokenList &A) const {
    TokenList val_copy = val;
    A.splice(A.end(), val_copy);
}

// ------------------------------------------------------------

// Insert the \AtEndOfClass hook, and other stuff
// This is called by pop_input_stack when the end of a file is seen.
// The integer n is the value of cur_file_type
void Parser::insert_hook(long n) {
    auto k = the_class_data.packages.size();
    if (n <= 0 || n >= to_signed(k)) return;
    LatexPackage *C = the_class_data.packages[to_unsigned(n)];
    if (!C->seen_process && !C->Uoptions.empty())
        log_and_tty << "Warning: " << C->pack_or_class() << C->real_name() << " has no \\ProcessOptions\n";
    back_input(C->hook);
    if (parse_version(C->date) < parse_version(C->req_date)) {
        log_and_tty << "Warning: You have requested, on line " << get_cur_line() << ", version\n`" << C->req_date << "' of "
                    << C->pack_or_class() << C->real_name() << ",\n"
                    << "but only version\n`" << C->date << "' is available\n";
    }
}

// Store the file name (without dir) and date in the file_list buffer
void classes_ns::add_to_filelist(const std::string &s, const std::string &date) {
    auto n = s.size();
    long k = -1;
    for (size_t i = 0; i < n; i++)
        if (s[i] == '/') k = to_signed(i);
    String S  = s.c_str() + k + 1;
    auto   nn = 12 - to_signed(strlen(S));
    while (nn > 0) {
        file_list << ' ';
        --nn;
    }
    file_list << S << "   " << date << "\n";
}

// This implements \ProvidesPackage, \ProvidesClass (synonym)
// Also \ProvidesFile
void Parser::T_provides_package(bool c) // True for a file
{
    std::string name = sE_arg_nopar();
    std::string date = sE_optarg_nopar();
    add_to_filelist(get_cur_filename(), date);
    the_log << lg_start;
    if (c) {
        the_log << "File: " << name << " " << date << "\n";
        return;
    }
    LatexPackage *cur = the_class_data.cur_pack();
    if (!date.empty()) cur->date = date;
    String S = cur->real_name();
    if (name != S && !the_class_data.using_default_class) {
        log_and_tty << "Warning: " << cur->pack_or_class() << S << " claims to be " << name << ".\n";
    }
    Buffer &b = local_buf;
    b << bf_reset << (cur->is_class() ? "Document class: " : "Package: ") << name << " " << date << "\n";
    if (cur->is_class())
        log_and_tty << b;
    else
        the_log << b;
}

// This implements \PassOptionsToPackage, \PassOptionsToClass
// Latex says:
// % If the package has been loaded, we check that it was first loaded with
// % the options.  Otherwise we add the option list to that of the package.
// But the test seems to be missing

void Parser::T_pass_options(bool c) // true if a class
{
    TokenList   Lopt = read_arg();
    std::string name = sE_arg_nopar();
    OptionList  L    = make_options(Lopt);
    auto        p    = the_class_data.find_package(name, c, true);
    the_class_data.packages[p]->add_options(L);
}

// This implements \DeclareOption, \DeclareOption*
void Parser::T_declare_options() {
    bool star = remove_initial_star();
    if (star)
        T_declare_option_star();
    else {
        std::string   name = sE_arg_nopar();
        TokenList     L    = read_arg();
        LatexPackage *C    = the_class_data.cur_pack();
        C->Poptions.push_back(KeyAndVal(name, L, name));
    }
}

// The xkeyval package uses also this function

void classes_ns::register_key(const std::string &Key) {
    TokenList     L; // empty
    LatexPackage *C = the_class_data.cur_pack();
    C->Poptions.push_back(KeyAndVal(Key, L, Key));
}

// \DeclareOption*{foo}
// Same code for xkeyval and kvoption packages
void Parser::T_declare_option_star() {
    TokenList     L    = read_arg();
    LatexPackage *C    = the_class_data.cur_pack();
    C->has_a_default   = true;
    C->default_handler = L;
}

// This implements \OptionNotUsed
// This is the default option handler. In the case of a package, ignore,
// in the case of a class, remove the used flag
void Parser::T_option_not_used() {
    if (!the_class_data.cur_pack()->is_class()) return;
    OptionList &GO = the_class_data.global_options;
    expand_no_arg("CurrentOption");
    TokenList L = read_arg();
    KeyAndVal s = make_keyval(L);
    auto      j = is_in_vector(GO, s.full_name, true);
    if (j) GO[*j].mark_un_used();
}

// We execute key=val (from U), with code in this
// If X is false, val is unused, we insert code in L
// Otherwise, we insert key=val
// We mark this as used.
void KeyAndVal::use_and_kill(TokenList &L, KeyAndVal &U, bool X) {
    used = true;
    if (!X) {
        L.splice(L.end(), val); // insert action code
        return;
    }
    TokenList W = U.to_list();
    if (!L.empty()) L.push_back(Token(other_t_offset, ','));
    L.splice(L.end(), W); // insert key=val
}

// Case \ProcessOptions* or  \ProcessOptionsX*
// in a package; we execute global options in order
void LatexPackage::check_global_options(TokenList &action, bool X) {
    OptionList &GO = the_class_data.global_options; // options of the class
    OptionList &DO = Poptions;                      // Known options
    auto        n  = GO.size();
    for (size_t i = 0; i < n; i++) {
        std::string nname = X ? GO[i].name : GO[i].full_name;
        auto        j     = find_option(nname);
        if (j <= 0) continue;
        if (DO[to_unsigned(j)].is_used()) continue; // should not happen
        GO[i].mark_used();
        local_buf << bf_comma << nname;
        DO[to_unsigned(j)].use_and_kill(action, GO[i], X);
    }
}

// Case \ProcessOptions or \ProcessOptionsX
// (no star) in class a package; we execute global options in order
void LatexPackage::check_local_options(TokenList &res, bool X) {
    OptionList &DO = Poptions;                      // Known options
    OptionList &GO = the_class_data.global_options; // options of the class
    OptionList &CO = Uoptions;                      // arg of \usepackage
    auto        n  = DO.size();
    for (size_t i = 0; i < n; i++) {
        const std::string &nname = DO[i].name;
        if (DO[i].is_used()) continue; // should to happen
        auto j = is_in_vector(CO, nname, false);
        if (j) {
            ClassesData::remove_from_unused(nname);
            DO[i].use_and_kill(res, CO[*j], X);
        } else if (is_class())
            continue;
        else {
            j = is_in_vector(GO, nname, X);
            if (j) {
                DO[i].use_and_kill(res, GO[*j], X);
                GO[*j].mark_used();
            } else
                continue;
        }
        local_buf << bf_comma << nname;
    }
}

// This is for \ProcessOptionX when an option is unknown,
//  but there is as default value.
// This is done at the end of ProcessOptionsX
void classes_ns::unknown_optionX(TokenList &cur, TokenList &action) {
    LatexPackage *C = the_class_data.cur_pack();
    if (!C->has_a_default) return; // should not happen
    KeyAndVal W = make_keyval(cur);
    TokenList unused;
    unknown_option(W, unused, action, 1); // insert default code
}

// General case.
void classes_ns::unknown_option(KeyAndVal &cur, TokenList &res, TokenList &spec, int X) {
    LatexPackage *     C    = the_class_data.cur_pack();
    const std::string &name = cur.full_name;
    if (!C->has_a_default) {
        if (C->is_class()) {
        } else
            log_and_tty << "Unknown option `" << name << "' for package `" << C->real_name() << "'\n";
    } else {
        TokenList u = cur.to_list();
        if (X == 1) {
            TokenList w;
            w = string_to_list(cur.name, true);
            spec.push_back(the_parser.hash_table.def_token);
            spec.push_back(the_parser.hash_table.CurrentOptionKey_token);
            spec.splice(spec.end(), w);

            w = cur.val;
            if (w.empty())
                w.push_back(the_parser.hash_table.relax_token);
            else
                w.pop_front();
            the_parser.brace_me(w);
            spec.push_back(the_parser.hash_table.def_token);
            spec.push_back(the_parser.hash_table.CurrentOptionValue_token);
            spec.splice(spec.end(), w);
            the_parser.brace_me(u);
            spec.push_back(the_parser.hash_table.def_token);
            spec.push_back(the_parser.hash_table.CurrentOption_token);
            spec.splice(spec.end(), u);
            u = C->default_handler;
            spec.splice(spec.end(), u);
        } else if (X == 2) {
            if (!res.empty()) res.push_back(Token(other_t_offset, ','));
            res.splice(res.end(), u);
        } else {
            the_parser.brace_me(u);
            res.push_back(the_parser.hash_table.def_token);
            res.push_back(the_parser.hash_table.CurrentOption_token);
            res.splice(res.end(), u);
            u = C->default_handler;
            res.splice(res.end(), u);
        }
    }
}

//
void LatexPackage::check_all_options(TokenList &action, TokenList &spec, int X) {
    OptionList &CO = Uoptions; // arg of \usepackage
    OptionList &DO = Poptions; // Known options
    auto        n  = CO.size();
    for (size_t i = 0; i < n; i++) {
        std::string nname = CO[i].name;
        auto        j     = find_option(X != 0 ? nname : CO[i].full_name);
        if (j == -1) {
            unknown_option(CO[i], action, spec, X);
        } else {
            ClassesData::remove_from_unused(nname);
            if (DO[to_unsigned(j)].is_used()) continue;
            local_buf << bf_comma << DO[to_unsigned(j)].name;
            DO[to_unsigned(j)].use_and_kill(action, CO[i], X != 0);
        }
    }
    // clear memory
    n = DO.size();
    for (size_t i = 0; i < n; i++) DO[i].kill();
}

// This implements \ProcessOptions, \ProcessOptions*
void Parser::T_process_options() {
    bool          star     = remove_initial_star();
    LatexPackage *C        = the_class_data.cur_pack();
    bool          in_class = C->is_class();
    C->seen_process        = true;
    Buffer &b              = local_buf;
    b.reset();
    TokenList action; // token list to evaluate
    if (star) {
        if (!in_class) C->check_global_options(action, false);
    } else
        C->check_local_options(action, false);
    TokenList unused;
    C->check_all_options(action, unused, 0);
    T_process_options_aux(action);
}

void Parser::T_execute_options() {
    TokenList     opt  = read_arg();
    LatexPackage *C    = the_class_data.cur_pack();
    OptionList &  pack = C->Poptions;
    TokenList     action;
    Buffer &      b = local_buf;
    b.reset();
    OptionList L = make_options(opt);
    auto       n = L.size();
    for (size_t i = 0; i < n; i++) {
        const std::string &option = L[i].full_name;
        auto               k      = C->find_option(option);
        if (k >= 0) {
            b << bf_comma << pack[to_unsigned(k)].name;
            pack[to_unsigned(k)].use(action);
        }
    }
    T_process_options_aux(action);
}

// Common code;
void Parser::T_process_options_aux(TokenList &action) {
    the_log << lg_startbrace << "Options to execute->" << local_buf << lg_endbrace;
    if (tracing_commands()) the_log << lg_startbrace << "Options code to execute->" << action << lg_endbrace;
    back_input(action);
}

// This is used by \ProcessOptionsX and \ProcessKeyvalOptions
// It gets the list of all keyval pairs.
auto classes_ns::cur_options(bool star, TokenList &spec, bool normal) -> TokenList {
    LatexPackage *C        = the_class_data.cur_pack();
    bool          in_class = C->is_class();
    C->seen_process        = true;
    Buffer &b              = local_buf;
    b.reset();
    TokenList action; // token list to evaluate
    if (star) {
        if (!in_class) C->check_global_options(action, true);
    } else
        C->check_local_options(action, true);
    C->check_all_options(action, spec, normal ? 1 : 2);
    the_log << lg_startbrace << "Options to execute->" << local_buf << lg_endbrace;
    return action;
}

void Parser::T_inputclass() {
    std::string name = sE_arg_nopar();
    bool        res  = tralics_ns::find_in_confdir(name + ".clt", true);
    if (!res) {
        parse_error(err_tok, "Cannot input " + name + ".clt", "");
    } else {
        auto k = cur_file_pos;
        open_tex_file(true);
        if (k > 0) k = -k;
        set_cur_file_pos(k);
    }
}

// Implements \LoadClassWithOptions and \RequirePackageWithOptions
void Parser::T_load_with_options(bool c) // c is true for a class
{
    std::string name                        = sE_arg_nopar();
    the_class_data.cur_pack()->seen_process = true; // someone else processes
    std::string date                        = sE_optarg_nopar();
    cur_opt_list                            = the_class_data.cur_pack()->Uoptions;
    bool b                                  = check_builtin_pack(name);
    use_a_package(name, c, date, b);
}

// This implements \documentclass, and \LoadClass
// bad is true in not vertical mode of after \begin{document}
void Parser::T_documentclass(bool bad) {
    int       c = cur_cmd_chr.chr;
    Token     T = cur_tok;
    TokenList Loptions;
    read_optarg_nopar(Loptions);
    std::string name = sE_arg_nopar();
    std::string date = sE_optarg_nopar();
    cur_opt_list     = make_options(Loptions);
    if (c == 0) { // else is LoadClass
        cur_tok = T;
        if (bad || the_class_data.seen_document_class) wrong_mode("Bad \\documentclass");
        the_class_data.seen_document_class = true;
    }
    check_builtin_class(); // handles builtin classes here
    if (c == 0) the_class_data.global_options = cur_opt_list;
    use_a_package(name, true, date, false);
}

// This implements \usepackage
void Parser::T_usepackage() {
    TokenList Loptions;
    read_optarg_nopar(Loptions);
    std::string name = sE_arg_nopar(); // can be a list of names
    std::string date = sE_optarg_nopar();
    cur_opt_list     = make_options(Loptions);
    Splitter S(name);
    for (;;) {
        if (S.at_end()) return;
        std::string pack = S.get_next();
        bool        b    = check_builtin_pack(pack);
        use_a_package(pack, false, date, b);
    }
}

// This is done after the first \usepackage; may warn
void LatexPackage::reload() {
    if (compare_options(Uoptions, cur_opt_list)) // true if A contains opt
        return;
    log_and_tty << "Option clash in \\usepackage " << real_name() << "\n";
    dump_options(Uoptions, "Old options: ");
    dump_options(cur_opt_list, "New options: ");
}

// This implements \@onefilewithoptions. Arguments are
// name of class/package,  class indicator, optional date, options.
void Parser::use_a_package(const std::string &name, bool type, const std::string &date, bool builtin) {
    auto          p   = the_class_data.find_package(name, type, true);
    LatexPackage *cur = the_class_data.packages[p];
    if (cur->checked) {
        cur->reload();
        return;
    }
    cur->checked = true;
    cur->add_options(cur_opt_list);
    cur->req_date                      = date;
    String T                           = type ? "Class" : "Package";
    bool   res                         = tralics_ns::find_in_confdir(name + (type ? ".clt" : ".plt"), true);
    the_class_data.using_default_class = false;
    if (!res) {
        std::string D = the_main->default_class;
        if (type && !D.empty()) {
            res = tralics_ns::find_in_confdir(D + ".clt", true);
            if (res) {
                log_and_tty << "Using default class " << D << "\n";
                the_class_data.using_default_class = true;
            }
        }
    }
    if (!res) {
        if (builtin) cur->date = "2006/01/01";
        the_log << lg_start << T << " " << name << (builtin ? " builtin" : " unknown") << lg_end;
        return;
    }
    cur->date = "0000/00/00";
    open_tex_file(true);
    set_cur_file_pos(to_signed(p));
    Buffer &b = local_buf;
    b << bf_reset << name;
    TokenList cc    = b.str_toks11(false);
    Token     cctok = hash_table.locate("CurrentClass");
    new_macro(cc, cctok);
}

// Built-in package handler
auto Parser::check_builtin_pack(const std::string &pack) -> bool {
    if (pack == "calc") {
        calc_loaded = true;
        return false;
    }
    if (pack == "fp") {
        boot_fp();
        return true;
    }
    if (pack == "french" || pack == "frenchle") {
        set_default_language(1);
        return true;
    }
    if (pack == "german") {
        set_default_language(2);
        return true;
    }
    if (pack == "babel") {
        check_language();
        return true;
    }
    if (pack == "fancyhdr") {
        hash_table.boot_fancyhdr();
        return true;
    }
    return false;
}

// Built-in class handler. Class name unused
void Parser::check_builtin_class() {
    Xid doc_att(1);
    if (is_raw_option(cur_opt_list, "useallsizes")) the_main->use_all_sizes = true;
    if (is_raw_option(cur_opt_list, "french")) set_default_language(1);
    if (is_raw_option(cur_opt_list, "english")) set_default_language(0);
}

// Handles the language option in \usepackage[xx]{babel}
void Parser::check_language() {
    int  lang = -1;
    auto n    = cur_opt_list.size();
    for (size_t i = 0; i < n; i++) {
        const std::string &s = cur_opt_list[i].name;
        if (s == "french" || s == "francais" || s == "frenchb" || s == "acadian" || s == "canadien") {
            if (lang == -1) the_log << "babel options: ";
            lang = 1;
            the_log << "french ";
        }
        if (s == "english" || s == "american" || s == "british" || s == "canadian" || s == "UKenglish" || s == "USenglish") {
            if (lang == -1) the_log << "babel options: ";
            lang = 0;
            the_log << "english ";
        }
        if (s == "austrian" || s == "german" || s == "germanb" || s == "naustrian" || s == "ngerman") {
            if (lang == -1) the_log << "babel options: ";
            lang = 2;
            the_log << "german ";
        }
    }
    if (lang == -1) return;
    the_log << "\n";
    set_default_language(lang);
}

// This adds the language attribute to the main XML element
void Parser::add_language_att() {
    name_positions b = cst_empty;
    int            D = get_def_language_num();
    if (D == 0)
        b = np_english;
    else if (D == 1)
        b = np_french;
    else if (D == 2)
        b = np_german;
    Xid doc_att(1);
    if ((b != 0U) && !the_names[np_language].null()) doc_att.get_att().push_back(np_language, b);
}

auto LatexPackage::find_option(const std::string &nname) -> long {
    auto n = Poptions.size();
    for (size_t i = 0; i < n; i++)
        if (Poptions[i].has_name(nname)) return to_signed(i);
    return -1;
}

void ClassesData::remove_from_unused(const std::string &name) {
    OptionList &GO = the_class_data.global_options;
    auto        j  = is_in_vector(GO, name, true);
    if (j) GO[*j].mark_used();
}

void show_unused_options() { ClassesData::show_unused(); }

void ClassesData::show_unused() {
    OptionList &GO = the_class_data.global_options;
    auto        n  = GO.size();
    Buffer &    B  = local_buf;
    B.reset();
    int k = 0;
    for (size_t i = 0; i < n; i++) {
        if (GO[i].is_used()) continue;
        if (GO[i].has_name("useallsizes")) continue;
        k++;
        if (!B.empty()) B << ',';
        GO[i].dump(B);
    }
    if (k == 0) return;
    log_and_tty << "Tralics Warning: Unused global option" << (k == 1 ? "" : "s") << "\n   " << B << ".\n";
}

// Implements \AtEndOfPackage \AtEndOfClass
void Parser::T_at_end_of_class() {
    TokenList L = read_arg();
    the_class_data.cur_pack()->add_to_hook(L);
}

// Implements \ClassError etc
void Parser::T_class_error(subtypes c) {
    if (c == messagebreak_code) return;
    static Token message_break_token = hash_table.locate("MessageBreak");
    std::string  prefix, prea;
    msg_type     what    = mt_none;
    bool         on_line = true;
    bool         std     = true;
    bool         simple  = false;
    int          skip    = 0;
    int          n       = 0;
    switch (c) {
    case packageerror_code:
        n    = 14;
        what = mt_error;
        prea = "Package";
        break;
    case packagewarning_code:
        n    = 16;
        what = mt_warning;
        prea = "Package";
        break;
    case packagewarningnoline_code:
        n       = 16;
        what    = mt_warning;
        prea    = "Package";
        on_line = false;
        break;
    case packageinfo_code:
        n    = 13;
        what = mt_info;
        prea = "Package";
        break;
    case classerror_code:
        n    = 12;
        what = mt_error;
        prea = "Class";
        break;
    case classwarning_code:
        n    = 14;
        what = mt_warning;
        prea = "Class";
        break;
    case classwarningnoline_code:
        n       = 14;
        what    = mt_warning;
        prea    = "Class";
        on_line = false;
        break;
    case classinfo_code:
        n    = 11;
        what = mt_info;
        prea = "Class";
        break;
    case latexerror_code:
        n      = 15;
        what   = mt_error;
        prea   = "Tralics";
        simple = true;
        break;
    case latexwarning_code:
        n      = 15;
        what   = mt_warning;
        prea   = "Tralics";
        simple = true;
        break;
    case latexwarningnoline_code:
        n       = 15;
        what    = mt_warning;
        prea    = "Tralics";
        simple  = true;
        on_line = false;
        break;
    case latexinfo_code:
        n    = 12;
        what = mt_info;
        prea = "Tralics";
        break;
    case latexinfonoline_code:
        n       = 12;
        what    = mt_info;
        prea    = "Tralics";
        on_line = false;
        break;
    case genericerror_code:
        std  = false;
        what = mt_error;
        skip = 2;
        break;
    case genericinfo_code:
        std  = false;
        what = mt_info;
        break;
    case genericwarning_code:
        std  = false;
        what = mt_warning;
        break;
    default:;
    }
    if (!simple) prefix = string_to_write(write18_slot + 1);
    Buffer &B = local_buf;
    B.reset();
    if (std) {
        std::string name = prefix;
        if (!simple) B << "(" << prefix << ")";
        while (n > 0) {
            --n;
            B << ' ';
        }
        prefix = B.to_string();
        B.reset();
        B << prea;
        if (!simple) B << " " << name;
        name_positions posta = np_Info;
        if (what == mt_error)
            posta = np_Error;
        else if (what == mt_warning)
            posta = np_Warning;
        B << " " << Istring(posta) << ": ";
    }
    TokenList L = scan_general_text();
    L.push_back(hash_table.relax_token);
    read_toks_edef(L);
    if (!L.empty()) {
        if (L.back() == hash_table.relax_token)
            L.pop_back();
        else
            on_line = false;
    }
    auto C = L.begin();
    auto E = L.end();
    while (C != E) {
        if (*C == message_break_token)
            B << "\n" << prefix;
        else
            B.insert_token(*C, false);
        ++C;
    }
    if (on_line && what != mt_error) {
        B << " at line " << get_cur_line();
        std::string f = get_cur_filename();
        if (!f.empty()) B << " of file " << f;
    }
    if (what != mt_error) B << ".\n";
    if (std && what == mt_error) skip = 1;
    if (skip != 0) ignore_arg();
    if (skip == 2) ignore_arg();
    out_warning(B, what);
}

void Parser::out_warning(Buffer &B, msg_type what) {
    name_positions w = np_Info;
    if (what == mt_error)
        w = np_Error;
    else if (what == mt_warning)
        w = np_Warning;
    if (!Istring(np_warning).empty()) {
        flush_buffer();
        Xml *res = new Xml(np_warning, new Xml(Istring(B)));
        res->id.add_attribute(np_letter_c, w);
        res->id.add_attribute(np_letter_l, cur_line_to_istring());
        the_stack.add_last(res);
    }
    if (what == mt_none) return;
    String res = B.convert_to_log_encoding();
    if (what == mt_error)
        parse_error(err_tok, res, "uerror");
    else if (what == mt_warning)
        log_and_tty << lg_start << res;
    else
        the_log << res;
}

void Parser::T_change_element_name() {
    flush_buffer();
    bool        star  = remove_initial_star();
    std::string name  = special_next_arg();
    std::string value = sE_arg_nopar();
    bool        res;
    if (star) {
        res = config_ns::assign_att(name.c_str(), value.c_str());
    } else
        res = config_ns::assign_name(name.c_str(), value.c_str());
    if (res) the_log << lg_start << "Changed " << (star ? "att_" : "xml_") << name << " to " << value << lg_end;
}

// -------------------------------------------------------------------
// Code for kvoptions

// Common function for all kv macros
void Parser::kvo_family(subtypes k) {
    switch (k) {
    case kvo_fam_set_code:
    case kvo_fam_get_code:
    case kvo_pre_set_code:
    case kvo_pre_get_code: kvo_family_etc(k); return;
    case kvo_bool_opt_code: kvo_bool_opt(); return;
    case kvo_comp_opt_code: kvo_comp_opt(); return;
    case kvo_boolkey_code: kvo_bool_key(); return;
    case kvo_voidkey_code: kvo_void_key(); return;
    case kvo_string_opt_code: kvo_string_opt(); return;
    case kvo_void_opt_code: kvo_void_opt(); return;
    case kvo_decdef_code: return;
    case kvo_process_code: kvo_process(); return;
    default: return;
    }
}

// Helper functions for kvo etc
// Insert {#1} in L
void classes_ns::add_sharp(TokenList &L) {
    L.push_front(make_char_token('}', 2));
    L.push_front(Token(other_t_offset, '1'));
    L.push_front(make_char_token('#', 6));
    L.push_front(make_char_token('{', 1));
}

// Puts \define@key{fam}{arg} in front of L
void Parser::call_define_key(TokenList &L, Token cmd, const std::string &arg, const std::string &fam) {
    TokenList aux = string_to_list(arg, true);
    L.splice(L.begin(), aux);
    aux = string_to_list(fam, true);
    L.splice(L.begin(), aux);
    L.push_front(hash_table.locate("define@key"));
    if (tracing_commands()) the_log << lg_start << cmd << "->" << L << "\n";
    back_input(L);
}

// Generates
// \define@key{Fam}{arg}[true]{\KVO@boolkey{Pfoo}{fam}{arg}{##1}}
void Parser::finish_kvo_bool(Token T, const std::string &fam, const std::string &arg) {
    TokenList L, aux;
    classes_ns::register_key(arg);
    add_sharp(L);
    aux = string_to_list(arg, true);
    L.splice(L.begin(), aux);
    aux = string_to_list(fam, true);
    L.splice(L.begin(), aux);
    std::string s = the_class_data.cur_pack()->full_name();
    aux           = string_to_list(s, true);
    L.splice(L.begin(), aux);
    L.push_front(hash_table.locate("KVO@boolkey"));
    brace_me(L);
    aux = string_to_list("[true]", false);
    L.splice(L.begin(), aux);
    call_define_key(L, T, arg, fam);
}

// \KVO@boolkey{Pfoo}{fam}{arg}{val}
// checks that val is true/false and calls \fam@argval
void Parser::kvo_bool_key() {
    std::string A = sE_arg_nopar(); // package
    std::string C = sE_arg_nopar(); // prefix
    std::string D = sE_arg_nopar(); // key
    std::string d = sE_arg_nopar(); // val
    if (!(d == "true" || d == "false")) {
        Buffer &B = local_buf;
        B << bf_reset << "Illegal boolean value " << d << " ignored";
        parse_error(err_tok, B.c_str(), "bad bool");
        log_and_tty << "Value  should be true or false in " << (A[0] == 'P' ? "package " : "class ") << (A.c_str() + 1) << ".\n";
        return;
    }
    local_buf << bf_reset << C << '@' << D << d;
    back_input(hash_table.locate(local_buf));
}

// \DeclareStringOption[a]{b}[c]
// is \define@key{P}{b}[c]{\def\p@b{##1}}

void Parser::kvo_string_opt() {
    Token     cmd = cur_tok;
    TokenList init, defval;
    read_optarg(init);
    std::string arg         = sE_arg_nopar();
    bool        has_default = read_optarg(defval);
    classes_ns::register_key(arg);
    std::string fam = kvo_getfam();
    Buffer &    B   = local_buf;
    B << bf_reset << fam << "@" << arg;
    Token T = hash_table.locate(B);
    if (!hash_table.eqtb[T.eqtb_loc()].is_undef_or_relax()) {
        parse_error(err_tok, "Cannot redefine ", T, "", "bad redef");
        return;
    }
    new_macro(init, T);
    TokenList L, aux;
    add_sharp(L);
    L.push_front(T);
    L.push_front(hash_table.locate("def"));
    brace_me(L);
    if (has_default) {
        brace_me(defval);
        defval.push_front(Token(other_t_offset, '['));
        defval.push_back(Token(other_t_offset, ']'));
        L.splice(L.begin(), defval);
    }
    call_define_key(L, cmd, arg, fam);
}

// Signals an error if the option is not @VOID@
void Parser::kvo_void_key() {
    read_arg();                     // package (should appear in the error message)
    std::string C = sE_arg_nopar(); // current option
    std::string d = sE_arg_nopar(); // option value
    if (d == "@VOID@") return;
    parse_error(err_tok, "Option " + C + " takes no argument", "bad opt");
}

void Parser::kvo_process() {
    bool        ok  = remove_initial_star();
    std::string fam = ok ? kvo_getfam() : sE_arg_nopar();
    TokenList   spec;
    TokenList   L = classes_ns::cur_options(true, spec, true);
    brace_me(L);
    back_input(L);
    TokenList aux = string_to_list(fam, true);
    back_input(aux);
    back_input(hash_table.locate("setkeys"));
    back_input(spec);
}

void Parser::kvo_void_opt() {
    Token       cmd = cur_tok;
    std::string arg = sE_arg_nopar();
    std::string fam = kvo_getfam();
    Buffer &    B   = local_buf;
    classes_ns::register_key(arg);
    B << bf_reset << fam << "@" << arg;
    Token T = hash_table.locate(B);
    if (!hash_table.eqtb[T.eqtb_loc()].is_undef_or_relax()) {
        parse_error(err_tok, "Cannot redefine ", T, "", "bad redef");
        return;
    }
    back_input(T);
    M_def(false, false, user_cmd, rd_always);
    TokenList L, aux;
    add_sharp(L);
    L.push_back(T);
    aux = string_to_list(arg, true);
    L.splice(L.begin(), aux);
    std::string s = the_class_data.cur_pack()->full_name();
    aux           = string_to_list(s, true);
    L.splice(L.begin(), aux);
    L.push_front(hash_table.locate("KVO@voidkey"));
    brace_me(L);
    aux = string_to_list("[@VOID@]", false);
    L.splice(L.begin(), aux);
    call_define_key(L, cmd, arg, fam);
}

// Implements \DeclareBoolOption[def]{name}
// defined a boolean foo
void Parser::kvo_bool_opt() {
    Token       T   = cur_tok;
    std::string df  = sE_optarg_nopar();
    std::string arg = sE_arg_nopar();
    // Optional argument must be true or false
    if (!(df.empty() || df == "false" || df == "true")) { log_and_tty << "Bad option " << df << " of " << arg << " replaced by false\n"; }
    subtypes    v   = df == "true" ? if_true_code : if_false_code;
    std::string fam = kvo_getfam();
    std::string s   = fam + '@' + arg;
    if (!check_if_redef(s)) return;
    // This is \newif
    Token W = cur_tok;
    eq_define(W.eqtb_loc(), CmdChr(if_test_cmd, v), false);
    M_newif_aux(W, s, true);
    M_newif_aux(W, s, false);
    finish_kvo_bool(T, fam, arg);
}

// \DeclareComplementaryOption{new}{old}
void Parser::kvo_comp_opt() {
    Token       cmd  = cur_tok;
    std::string arg  = sE_arg_nopar();
    std::string comp = sE_arg_nopar();
    std::string fam  = kvo_getfam();
    Buffer &    B    = local_buf;
    B << bf_reset << "if" << fam << '@' << comp;
    Token T = hash_table.locate(B);
    if (hash_table.eqtb[T.eqtb_loc()].is_undef()) {
        B << bf_reset << "Cannot generate code for `" << arg << "', no parent " << comp;
        parse_error(err_tok, B.c_str(), "bad redef");
        return;
    }
    // make boolean old inverse of foo
    B << bf_reset << fam << '@' << comp << "true";
    Token T1 = hash_table.locate(B);
    B << bf_reset << fam << '@' << arg << "false";
    Token T2 = hash_table.locate(B);
    B << bf_reset << fam << '@' << comp << "false";
    Token T3 = hash_table.locate(B);
    B << bf_reset << fam << '@' << arg << "true";
    Token T4 = hash_table.locate(B);
    if (!hash_table.eqtb[T2.eqtb_loc()].is_undef_or_relax()) { parse_error(err_tok, "Cannot redefine ", T2, "", "bad redef"); }
    if (!hash_table.eqtb[T4.eqtb_loc()].is_undef_or_relax()) { parse_error(err_tok, "Cannot redefine ", T4, "", "bad redef"); }
    M_let_fast(T2, T1, true);
    M_let_fast(T4, T3, true);
    finish_kvo_bool(cmd, fam, arg);
}

// Get/set for family and prefix
void Parser::kvo_family_etc(subtypes k) {
    std::string s = the_class_data.cur_pack()->full_name();
    Buffer &    B = local_buf;
    B << bf_reset << "KVO@";
    if (k == kvo_fam_set_code || k == kvo_fam_get_code)
        B << "family@";
    else
        B << "prefix@";
    B << s;
    Token T = hash_table.locate(B);
    if (k == kvo_fam_set_code || k == kvo_pre_set_code) {
        TokenList L = read_arg();
        new_macro(L, T);
    } else if (hash_table.eqtb[T.eqtb_loc()].is_undef()) {
        B << bf_reset << s.c_str() + 1;
        if (k == kvo_pre_get_code) B << "@";
        TokenList res = B.str_toks11(false);
        back_input(res);
    } else {
        back_input(T);
        expand_when_ok(true);
    }
}

// This gets prefix and family
auto Parser::kvo_getfam() -> std::string {
    back_input(hash_table.CB_token);
    kvo_family_etc(kvo_fam_get_code);
    back_input(hash_table.OB_token);
    return sE_arg_nopar();
}

// If arg is foo checks that \iffoo \footrue \foofalse
// are undefined . Puts \iffo in cur_tok
auto Parser::check_if_redef(const std::string &s) -> bool {
    Buffer &B = local_buf;
    B << bf_reset << s << "true";
    Token T2 = hash_table.locate(B);
    if (!hash_table.eqtb[T2.eqtb_loc()].is_undef_or_relax()) {
        parse_error(err_tok, "Cannot redefine ", T2, "", "bad redef");
        return false;
    }
    B << bf_reset << s << "false";
    Token T3 = hash_table.locate(B);
    if (!hash_table.eqtb[T3.eqtb_loc()].is_undef_or_relax()) {
        parse_error(err_tok, "Cannot redefine ", T3, "", "bad redef");
        return false;
    }
    B << bf_reset << "if" << s;
    Token T1 = hash_table.locate(B);
    if (!hash_table.eqtb[T1.eqtb_loc()].is_undef_or_relax()) {
        parse_error(err_tok, "Cannot redefine ", T1, "", "bad redef");
        return false;
    }
    cur_tok = T1;
    return true;
}

// --------------------------------------------------
// extensions for Xkeyval

// You can use <foo.sty> as optional argument
auto Parser::XKV_parse_filename() -> TokenList {
    skip_initial_space();
    if (cur_tok.is_valid()) back_input();
    if (cur_tok == Token(other_t_offset, '<')) {
        get_token();
        return read_until_nopar(Token(other_t_offset, '>'));
    }
    LatexPackage *C = the_class_data.cur_pack();
    local_buf << bf_reset << C->real_name();
    local_buf << (C->is_class() ? ".cls" : ".sty");
    return local_buf.str_toks11(false);
}
