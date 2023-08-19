#include <atomic>

#include "coke/mysql_client.h"
#include "coke/mysql_utils.h"

#include "workflow/StringUtil.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/mysql_byteorder.h"

namespace coke {

MySQLClient::MySQLClient(const MySQLClientParams &p, bool use_transaction, std::size_t transaction_id)
    : use_transaction(use_transaction), transaction_id(transaction_id), params(p)
{
    url.assign(params.use_ssl ? "mysqls://" : "mysql://");

    params.username = StringUtil::url_encode_component(p.username);
    params.password = StringUtil::url_encode_component(p.password);
    params.dbname = StringUtil::url_encode_component(p.dbname);

    // disable retry when use transaction
    if (use_transaction)
        params.retry_max = 0;

    if (!params.username.empty() || !params.password.empty())
        url.append(params.username).append(":")
           .append(params.password).append("@");

    url.append(params.host).append(":")
       .append(std::to_string(params.port)).append("/")
       .append(params.dbname);

    std::size_t pos = url.size();
    if (!params.character_set.empty())
        url.append("&character_set=").append(params.character_set);
    if (!params.character_set_results.empty())
        url.append("&character_set_results=").append(params.character_set_results);

    if (use_transaction)
        url.append("&transaction=coke_mysql_transaction_id_")
           .append(std::to_string(transaction_id));

    if (url.size() > pos)
        url[pos] = '?';

    URIParser::parse(url, uri);
}

MySQLClient::AwaiterType MySQLClient::request(const std::string &query) {
    WFMySQLTask *task;

    task = WFTaskFactory::create_mysql_task(uri, params.retry_max, nullptr);
    task->set_send_timeout(params.send_timeout);
    task->set_receive_timeout(params.receive_timeout);
    task->set_keep_alive(params.keep_alive_timeout);
    task->get_req()->set_query(query);

    return AwaiterType(task);
}

std::size_t MySQLConnection::get_unique_id() {
    static std::atomic<std::size_t> uid{0};
    return uid.fetch_add(1, std::memory_order_relaxed);
}

MySQLConnection::AwaiterType MySQLConnection::disconnect() {
    WFMySQLTask *task;

    task = WFTaskFactory::create_mysql_task(uri, params.retry_max, nullptr);
    task->set_send_timeout(params.send_timeout);
    task->set_receive_timeout(params.receive_timeout);
    task->set_keep_alive(0);
    task->get_req()->set_query("");

    return AwaiterType(task);
}

void MySQLFieldView::reset(const char *buf, mysql_field_t *field) {
    name        = std::string_view(buf + field->name_offset, field->name_length);
    org_name    = std::string_view(buf + field->org_name_offset, field->org_name_length);
    table       = std::string_view(buf + field->table_offset, field->table_length);
    org_table   = std::string_view(buf + field->org_table_offset, field->org_table_length);
    db          = std::string_view(buf + field->db_offset, field->db_length);
    catalog     = std::string_view(buf + field->catalog_offset, field->catalog_length);

    if (field->def_offset == std::size_t(-1) && field->def_length == 0)
        def = std::string_view();
    else
        def = std::string_view(buf + field->def_offset, field->def_length);

    flags = field->flags;
    length = field->length;
    decimals = field->decimals;
    charsetnr = field->charsetnr;
    data_type = field->data_type;
}

bool MySQLResultSetView::get_fields(std::vector<MySQLFieldView> &fields) const {
    if (!is_result_set())
        return false;

    int field_count = get_field_count();
    fields.clear();
    fields.reserve(field_count);

    for (int i = 0; i < field_count; i++) {
        fields.emplace_back(buf, result_set->fields[i]);
    }

    return true;
}

bool MySQLResultSetView::next_row(std::vector<MySQLCellView> &cells) {
    unsigned long long len;
    const unsigned char *data, *p;
    int dtype;

    if (off_cur == off_end)
        return false;

    p = off_cur;
    cells.clear();
    cells.reserve(result_set->field_count);

    for (int i = 0; i < result_set->field_count; i++) {
        dtype = result_set->fields[i]->data_type;

        if (*p == MYSQL_PACKET_HEADER_NULL) {
            p++;
            cells.emplace_back(MYSQL_TYPE_NULL, std::string_view());
        }
        else if (decode_string(&data, &len, &p, off_end) > 0) {
            const char *ptr = reinterpret_cast<const char *>(data);
            cells.emplace_back(dtype, std::string_view(ptr, len));
        }
        else {
            // error
            cells.clear();
            return false;
        }
    }

    off_cur = p;
    return true;
}

void MySQLResultSetView::rewind() noexcept {
    if (is_result_set()) {
        const auto *p = reinterpret_cast<const unsigned char *>(buf);
        off_begin = p + result_set->rows_begin_offset;
        off_end = p + result_set->rows_end_offset;
        off_cur = off_begin;
    }
    else {
        off_begin = off_end = off_cur = nullptr;
    }
}

} // namespace coke
