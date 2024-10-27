#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

#include "coke/wait.h"
#include "coke/go.h"
#include "coke/tools/option_parser.h"
#include "coke/mysql/mysql_client.h"
#include "coke/mysql/mysql_utils.h"

#include "readline_helper.h"

constexpr const char *first_prompt = "mysql> ";
constexpr const char *other_prompt = "    -> ";

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

int main(int argc, char *argv[]) {
    coke::OptionParser args;
    coke::MySQLClientParams params;
    params.port = 3306;

    args.add_string(params.host, 'h', "host", true)
        .set_description("Mysql server hostname.");
    args.add_integer(params.port, 'P', "port")
        .set_default(3306)
        .set_description("Mysql server port");
    args.add_string(params.username, 'u', "user", true)
        .set_description("Mysql user name");
    args.add_string(params.password, 'p', "password")
        .set_description("Mysql password for user.");
    args.set_help_flag(coke::NULL_SHORT_NAME, "help");
    args.set_extra_prompt("db_name");

    std::string err;
    int ret = args.parse(argc, argv, err);
    if (ret < 0) {
        std::cerr << err << std::endl;
        return 1;
    }
    else if (ret > 0) {
        args.usage(std::cout);
        return 0;
    }

    auto ext = args.get_extra_args();
    if (!ext.empty())
        params.dbname.assign(ext[0]);

    const char *miss = nullptr;
    if (params.host.empty()) miss = "host";
    else if (params.username.empty()) miss = "user";
    else if (params.dbname.empty()) miss = "dbname";

    if (miss) {
        std::cerr << "Missing " << miss << " in command line args" << std::endl;
        args.usage(std::cout);
        return 1;
    }

    readline_init();

    coke::sync_wait(mysql_cli(params));

    readline_deinit();
    return 0;
}
