#include <iomanip>

#include "coke/dag.h"

namespace coke::detail {

bool dag_check(const std::vector<std::vector<dag_index_t>> &outs,
               const std::vector<std::vector<dag_index_t>> &weak_outs,
               std::vector<dag_index_t> &counts,
               std::vector<bool> &weak_flags)
{
    std::size_t n = outs.size();
    if (n == 0)
        return false;

    std::vector<dag_index_t> cnts, wcnts;
    cnts.resize(n, 0);
    wcnts.resize(n, 0);

    for (dag_index_t from = 0; from < n; from++) {
        for (auto to : outs[from])
            cnts[to]++;
        for (auto to : weak_outs[from])
            wcnts[to]++;
    }

    // check dag
    if (cnts[0] != 0 || wcnts[0] != 0)
        return false;

    std::vector<dag_index_t> v;
    std::vector<dag_index_t> cs = cnts;
    std::vector<dag_index_t> ws = wcnts;
    std::vector<bool> used(n, false);
    v.push_back(0);
    used[0] = true;

    while (!v.empty()) {
        dag_index_t from = v.back();
        v.pop_back();

        for (auto to : outs[from]) {
            if (used[to])
                return false;

            --cs[to];
            if (cs[to] == 0 && ws[to] == 0) {
                v.push_back(to);
                used[to] = true;
            }
        }

        for (auto to : weak_outs[from]) {
            if (used[to])
                return false;

            --ws[to];
            if (cs[to] == 0 && ws[to] == 0) {
                v.push_back(to);
                used[to] = true;
            }
        }
    }

    for (std::size_t i = 0; i < n; i++)
        if (!used[i])
            return false;

    counts = cnts;
    weak_flags.resize(n, false);
    for (std::size_t i = 0; i < n; i++)
        weak_flags[i] = (wcnts[i] > 0);
    return true;
}

struct GetLabel {
    dag_index_t id;
    const std::string &name;

    GetLabel(dag_index_t id, const std::string &name)
        : id(id), name(name)
    { }
};

struct GetName {
    dag_index_t id;

    GetName(dag_index_t id) : id(id) { }
};

std::ostream &operator<< (std::ostream &os, const GetLabel &x) {
    if (x.name.empty())
        os << "<Node<sub>" << x.id << "</sub>>";
    else
        os << std::quoted(x.name);
    return os;
}

std::ostream &operator<< (std::ostream &os, const GetName &x) {
    os << "N" << x.id;
    return os;
}

void dag_dump(std::ostream &os,
              const std::vector<std::vector<dag_index_t>> &outs,
              const std::vector<std::vector<dag_index_t>> &weak_outs,
              const std::vector<std::string> &names)
{
    dag_index_t n = (dag_index_t)outs.size();
    bool first;

    os << "digraph {\n";

    for (dag_index_t i = 0; i < n; i++) {
        os << "    " << GetName(i)
           << " [label=" << GetLabel(i, names[i]) << "];\n";
    }

    os << "\n";

    for (dag_index_t i = 0; i < n; i++) {
        if (!outs[i].empty()) {
            os << "    " << GetName(i) << " -> {";

            first = true;
            for (auto to : outs[i]) {
                if (!first)
                    os << ", ";

                first = false;
                os << GetName(to);
            }
            os << "};\n";
        }

        if (!weak_outs[i].empty()) {
            os << "    " << GetName(i) << " -> {";
            first = true;
            for (auto to : weak_outs[i]) {
                if (!first)
                    os << ", ";

                first = false;
                os << GetName(to);
            }
            os << "} [style=dashed];\n";
        }
    }

    os << "}\n";
}

} // namespace coke::detail
