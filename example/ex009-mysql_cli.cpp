#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <getopt.h>

#include "coke/coke.h"
#include "coke/mysql_client.h"
#include "coke/mysql_utils.h"

#include "readline_helper.h"

constexpr const char *first_prompt = "mysql> ";
constexpr const char *other_prompt = "    -> ";
const char *optstr = "h:p:P:u:";

/**
 * This example implements a simple mysql-cli, read commands from standard input,
 * send them to the mysql server, and write the results to standard output.
*/

std::string to_string(const coke::MySQLCellView &c) {
    // Only display some simple data types, avoiding unknown data to
    // dirty the console

    std::string str;
    if (c.is_integer() || c.is_float() || c.is_double())
        str.assign(c.raw_view());
    else if (c.is_date() || c.is_time() || c.is_datetime() || c.is_string())
        str.assign(c.raw_view());
    else if (c.is_null())
        str.assign("NULL");
    else
        str.assign("type:").append(std::to_string(c.get_data_type()))
            .append(",len:").append(std::to_string(c.raw_view().size()));

    return str;
}

void show_result_set(coke::MySQLResultSetView &v) {
    std::vector<coke::MySQLCellView> row;
    auto fields = v.get_fields();
    int field_count = v.get_field_count();
    int width = 24;  // Use fixed-length tables for simplicity

    if (field_count == 0)
        return;

    // header line
    std::cout << "|";
    for (int i = 0; i < field_count; i++) {
        const auto &f = fields[i];
        std::cout << " " << std::setw(width) << f.get_name_view() << " |";
    }
    std::cout << "\n";

    // delimiter line
    std::cout << "|";
    for (int i = 0; i < field_count; i++) {
        std::cout << " " << std::string(width, '-') << " |";
    }
    std::cout << "\n";

    // result set data
    while (v.next_row(row)) {
        std::cout << "|";
        for (int i = 0; i < field_count; i++) {
            std::string str = to_string(row[i]);
            std::cout << " " << std::setw(width) << str << " |";
        }
        std::cout << "\n";
    }
}

// Read from stdin, until trailing `;` to indicate end of this sql
bool get_sql(std::string &sql) {
    const char *prompt = first_prompt;
    std::string line;
    sql.clear();

    while (nextline(prompt, line)) {
        std::size_t pos = line.find_last_not_of(" \t\r\n");
        if (pos != std::string::npos)
            sql.append(line, 0, pos+1);

        if (!sql.empty() && sql.back() == ';') {
            add_history(sql);
            return true;
        }

        if (!sql.empty()) {
            prompt = other_prompt;
            sql.push_back('\n');
        }
    }

    return false;
}

coke::Task<> mysql_cli(const coke::MySQLClientParams &params) {
    coke::MySQLClient cli(params);
    coke::MySQLResult res;
    std::string sql;

    while (true) {
        // Read command in another thread
        co_await coke::switch_go_thread();

        if (!get_sql(sql) || sql.starts_with("quit"))
            break;

        res = co_await cli.request(sql);
        const auto &resp = res.resp;

        if (res.state != coke::STATE_SUCCESS) {
            const char *errstr = coke::get_error_string(res.state, res.error);
            std::cout << "ERROR " << errstr << "\n";
            continue;
        }

        // A request may have multiple result sets, traverse to get all results
        for (auto result_view : coke::MySQLResultSetCursor(res.resp)) {
            if (result_view.is_ok()) {
                int rows = result_view.get_affected_rows();
                int warns = result_view.get_warnings();
                auto insert_id = result_view.get_insert_id();
                std::cout << "Query OK, " << rows << " row(s) affected. "
                    << warns << " warning(s). last insert id "
                    << insert_id << ". " << result_view.get_info_view() << std::endl;
            }
            else
                show_result_set(result_view);
        }

        if (resp.get_packet_type() == MYSQL_PACKET_ERROR) {
            std::cout << "ERROR " << resp.get_error_code()
                << ": " << resp.get_error_msg() << std::endl;
        }
    }

    std::cout << "Bye" << std::endl;
}

void usage(const char *arg0) {
    std::cout << "Usage: " << arg0 << " [options]... dbname\n";
    std::cout << R"(
        -h host         Mysql server hostname.
        -P port         Mysql server port, default 3306.
        -u user         Mysql user name.
        -p password     Mysql password for user.

        dbname          Database to use.
    )" << std::endl;
}

int main(int argc, char *argv[]) {
    int copt;
    coke::MySQLClientParams params;
    params.port = 3306;

    while ((copt = getopt(argc, argv, optstr)) != -1) {
        switch (copt) {
        case 'h': params.host.assign(optarg); break;
        case 'p': params.password.assign(optarg); break;
        case 'P': params.port = std::atoi(optarg); break;
        case 'u': params.username.assign(optarg); break;
        default: usage(argv[0]); return 1;
        }
    }

    if (optind < argc)
        params.dbname.assign(argv[optind]);

    const char *miss = nullptr;
    if (params.host.empty()) miss = "host";
    else if (params.username.empty()) miss = "user";
    else if (params.dbname.empty()) miss = "dbname";

    if (miss) {
        std::cout << "Missing " << miss << " in command line args" << std::endl;
        usage(argv[0]);
        return 1;
    }

    readline_init();

    coke::sync_wait(mysql_cli(params));

    readline_deinit();
    return 0;
}
