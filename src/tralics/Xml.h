#pragma once
#include "../txstring.h"

class XmlAction;

class Xml {
public:
    Xid                id{0}; // id of the objet
    Istring            name;  // name of the element
    std::vector<Xml *> tree;  // the aux field

    Xml(Istring n = {}) : name(n) {}
    Xml(const Buffer &n) : name(Istring(n.c_str())) {}
    Xml(StrHash &X) : name(Istring(X.hash_find())) {}
    Xml(Istring N, Xml *z);
    Xml(name_positions N, Xml *z);
    Xml(name_positions x, Xid n) : id(n), name(Istring(x)) {}

    [[nodiscard]] auto empty() const -> bool;
    [[nodiscard]] auto get_cell_span() const -> long;
    [[nodiscard]] auto has_name(Istring s) const -> bool { return name == s; }
    [[nodiscard]] auto has_name(name_positions s) const -> bool { return name == the_names[s]; }
    [[nodiscard]] auto is_anchor() const -> bool { return !is_xmlc() && name == the_names[np_anchor]; }
    [[nodiscard]] auto is_empty() const -> bool;
    [[nodiscard]] auto is_empty_p() const -> bool;
    [[nodiscard]] auto is_empty_spec() const -> bool;
    [[nodiscard]] auto is_xmlc() const -> bool { return id.value <= 0; }
    [[nodiscard]] auto last_addr() const -> Xml *;
    [[nodiscard]] auto last_is(name_positions) const -> bool;
    [[nodiscard]] auto last_is_string() const -> bool;
    [[nodiscard]] auto only_hi() const -> bool;
    [[nodiscard]] auto only_recur_hi() const -> bool;
    [[nodiscard]] auto only_text() const -> bool;
    [[nodiscard]] auto real_size() const -> int;
    [[nodiscard]] auto single_non_empty() const -> Xml *;
    [[nodiscard]] auto single_son() const -> Xml *;
    [[nodiscard]] auto size() const { return tree.size(); }
    [[nodiscard]] auto tail_is_anchor() const -> bool;

    auto back() -> Xml * { return tree.empty() ? nullptr : tree.back(); }
    auto contains_env(Istring name) -> bool;
    auto convert_to_string() -> std::string;
    auto delete_one_env0(Istring name) -> Xid;
    auto deep_copy() -> Xml *;
    auto father(Xml *X, int &) -> Xml *;
    auto figline(int &ctr, Xml *junk) -> Xml *;
    auto find_on_tree(Xml *check, Xml **res) const -> bool;
    auto first_lower(Istring src) -> Xml *;
    auto get_first_env(name_positions name) -> Xml *;
    auto how_many_env(Istring match) -> long;
    auto insert_at_ck(int n, Xml *v) -> bool;

    void add_att(Istring a, Istring b) { id.add_attribute(a, b); }
    void add_att(name_positions a, name_positions b) { id.add_attribute(a, b); }
    void add_first(Xml *x);
    void add_ref(std::string s);
    void add_tmp(Xml *x);
    void add_last_nl(Xml *x);
    void add_last_string(const Buffer &B);
    void add_nl();
    void add_non_empty_to(Xml *res);
    void change_name(name_positions s) { name = the_names[s]; }
    void compo_special();
    void convert_to_string(Buffer &B);
    void insert_at(size_t pos, Xml *x);
    void insert_bib(Xml *bib, Xml *match);
    auto is_child(Xml *x) const -> bool;
    void kill_name() { name = Istring(); }
    auto last_box() -> Xml *;
    void last_to_SH();
    void make_hole(size_t pos);
    void move(Istring match, Xml *res);
    void one_fig_tab(bool is_fig);
    auto par_is_empty() -> bool;
    void pop_back() { tree.pop_back(); }
    void postprocess_fig_table(bool is_fig);
    auto prev_sibling(Xml *x) -> Xml *;
    auto put_at(long n, Xml *x) -> bool;
    void put_in_buffer(Buffer &b);
    void push_back(Buffer &B) { push_back(new Xml(B)); }
    void push_back(Xml *x);
    void push_back_list(Xml *x);
    void recurse(XmlAction &X);
    void recurse0(XmlAction &X);
    void remove_empty_par();
    auto remove_at(long n) -> bool;
    auto remove_last() -> Xml *;
    void remove_last_empty_hi();
    void remove_last_space();
    void remove_par_bal_if_ok();
    void rename(Istring old_name, Istring new_name);
    void reset();
    void sans_titre();
    auto sans_titre(Xml *) -> String;
    void set_id(long i) { id = i; }
    void subst_env0(Istring match, Xml *vl);
    void swap_x(Xml *x);
    void to_buffer(Buffer &b) const;
    auto total_span(long &res) const -> bool;
    auto try_cline(bool action) -> bool;
    auto try_cline_again(bool action) -> bool;
    void unbox(Xml *x);
    auto value_at(long n) -> Xml *;
    void word_stats(const std::string &match);
    void word_stats_i();
    auto spec_copy() -> Xml *;
    void replace_first(Xml *x) {
        if (!tree.empty()) tree[0] = x;
    }
    void bordermatrix();
};

struct XmlAndType {
    Xml *      value;
    math_types type;

    XmlAndType(Xml *X, math_types t = mt_flag_small) : value(X), type(t) {}
};

auto read_xml(const std::string &s) -> Xml *;
