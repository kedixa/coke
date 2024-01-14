#ifndef COKE_MYSQL_UTILS_H
#define COKE_MYSQL_UTILS_H

#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include "coke/compatible/to_numeric.h"

#include "workflow/MySQLMessage.h"
#include "workflow/mysql_types.h"

namespace coke {

using protocol::MySQLMessage;
using protocol::MySQLRequest;
using protocol::MySQLResponse;

class MySQLCellView {
public:
    MySQLCellView() : data_type(MYSQL_TYPE_NULL) { }
    MySQLCellView(int data_type, std::string_view data)
        : data_type(data_type), data(data)
    { }

    MySQLCellView(const MySQLCellView &) = default;
    MySQLCellView &operator= (const MySQLCellView &) = default;

    int get_data_type() const { return data_type; }

    bool is_null() const { return check_type(MYSQL_TYPE_NULL); }
    bool is_integer() const {
        return check_type(MYSQL_TYPE_TINY, MYSQL_TYPE_SHORT, MYSQL_TYPE_INT24, MYSQL_TYPE_LONG, MYSQL_TYPE_LONGLONG);
    }
    bool is_float() const { return check_type(MYSQL_TYPE_FLOAT); }
    bool is_double() const { return check_type(MYSQL_TYPE_DOUBLE); }
    bool is_decimal() const { return check_type(MYSQL_TYPE_DECIMAL, MYSQL_TYPE_NEWDECIMAL); }
    bool is_string() const { return check_type(MYSQL_TYPE_VARCHAR, MYSQL_TYPE_VAR_STRING, MYSQL_TYPE_STRING); }
    bool is_blob() const { return check_type(MYSQL_TYPE_TINY_BLOB, MYSQL_TYPE_MEDIUM_BLOB, MYSQL_TYPE_LONG_BLOB, MYSQL_TYPE_BLOB); }
    bool is_date() const { return check_type(MYSQL_TYPE_DATE); }
    bool is_time() const { return check_type(MYSQL_TYPE_TIME); }
    bool is_datetime() const { return check_type(MYSQL_TYPE_DATETIME); }

    std::errc to_longlong(long long &value) const {
        return is_integer() ? to_numeric(value) : std::errc::invalid_argument;
    }

    std::errc to_ulonglong(unsigned long long &value) const {
        return is_integer() ? to_numeric(value) : std::errc::invalid_argument;
    }

    std::errc to_float(float &value) const {
        return is_float() ? to_numeric(value) : std::errc::invalid_argument;
    }

    std::errc to_double(double &value) const {
        return is_double() ? to_numeric(value) : std::errc::invalid_argument;
    }

    long long as_longlong() const { return as_numeric<long long>(0); }
    unsigned long long as_ulonglong() const { return as_numeric<unsigned long long>(0); }
    float as_float() const { return as_numeric<float>(std::nanf("")); }
    double as_double() const { return as_numeric<double>(std::nan("")); }

    std::string as_string() const { return std::string(data); }
    std::string_view as_string_view() const { return data; }

    std::string_view raw_view() const { return data; }

    void reset(int data_type, std::string_view data) {
        this->data_type = data_type;
        this->data = data;
    }

protected:
    template<typename... ARGS>
    bool check_type(ARGS&&... args) const {
        return ((data_type == args) || ...);
    }

    template<typename T>
    std::errc to_numeric(T &value) const noexcept {
        return comp::to_numeric(data, value);
    }

    template<typename T>
    T as_numeric(T err) const noexcept {
        T t;
        auto ec = to_numeric<T>(t);
        return ec == std::errc() ? t : err;
    }

private:
    int data_type;
    std::string_view data;
};

class MySQLFieldView {
public:
    MySQLFieldView() : length(0), flags(0), decimals(0), charsetnr(0), data_type(MYSQL_TYPE_NULL) { }
    MySQLFieldView(const char *buf, mysql_field_t *field) { reset(buf, field); }
    MySQLFieldView(const MySQLFieldView &) = default;
    MySQLFieldView& operator= (const MySQLFieldView &) = default;

    std::string_view get_name_view() const      { return name; }
    std::string_view get_org_name_view() const  { return org_name; }
    std::string_view get_table_view() const     { return table; }
    std::string_view get_org_table_view() const { return org_table; }
    std::string_view get_db_view() const        { return db; }
    std::string_view get_catalog_view() const   { return catalog; }
    std::string_view get_def_view() const       { return def; }

    int get_length() const      { return length; }
    int get_flags() const       { return flags; }
    int get_decimals() const    { return decimals; }
    int get_charsetnr() const   { return charsetnr; }
    int get_data_type() const   { return data_type; }

    void reset(const char *buf, mysql_field_t *field);

private:
    std::string_view name;
    std::string_view org_name;
    std::string_view table;
    std::string_view org_table;
    std::string_view db;
    std::string_view catalog;
    std::string_view def;

    int length;
    int flags;
    int decimals;
    int charsetnr;
    int data_type;
};

class MySQLResultSetView {
    using result_set_t = __mysql_result_set;

public:
    MySQLResultSetView() : MySQLResultSetView(nullptr, nullptr) { }
    MySQLResultSetView(const result_set_t *result_set, const char *buf)
        : result_set(result_set), buf(buf)
    { rewind(); }

    bool is_ok() const { return result_set && result_set->type == MYSQL_PACKET_OK; }
    bool is_result_set() const { return result_set && result_set->type == MYSQL_PACKET_GET_RESULT; }
    bool is_error() const { return result_set && result_set->type == MYSQL_PACKET_ERROR; }

    int get_server_status() const { return result_set->server_status; }

    // for is_ok
    std::string get_info() const { return std::string(get_info_view()); }
    std::string_view get_info_view() const {
        const char *info = buf + result_set->info_offset;
        return std::string_view(info, result_set->info_len);
    }

    int get_affected_rows() const { return result_set->affected_rows; }
    int get_warnings() const { return result_set->warning_count; }
    unsigned long long get_insert_id() const { return result_set->insert_id; }

    // for is_result_set
    int get_field_count() const { return result_set->field_count; }
    int get_row_count() const { return result_set->row_count; }
    bool get_fields(std::vector<MySQLFieldView> &fields) const;

    std::vector<MySQLFieldView> get_fields() const {
        std::vector<MySQLFieldView> fields;
        get_fields(fields);
        return fields;
    }

    bool next_row(std::vector<MySQLCellView> &cells);
    void rewind() noexcept;

private:
    const result_set_t *result_set;
    const char *buf;
    const unsigned char *off_begin;
    const unsigned char *off_end;
    const unsigned char *off_cur;
};

class MySQLResultSetCursor {
    struct iterator {
        iterator() {
            cursor.head = nullptr;
            cursor.current = nullptr;
            result_set = nullptr;
            buf = nullptr;
        }

        iterator(const MySQLResponse &resp, const char *buf) {
            mysql_result_set_cursor_init(&cursor, resp.get_parser());
            this->result_set = nullptr;
            this->buf = buf;

            next();
        }

        ~iterator() {
            mysql_result_set_cursor_deinit(&cursor);
        }

        bool operator==(const iterator &o) const {
            return cursor.current == o.cursor.current;
        }

        bool operator!=(const iterator &o) const {
            return cursor.current != o.cursor.current;
        }

        iterator &operator++() {
            next();
            return *this;
        }

        MySQLResultSetView operator*() const {
            return MySQLResultSetView(result_set, buf);
        }

    private:
        bool next() noexcept {
            if (!cursor.current)
                return false;

            if (mysql_result_set_cursor_next(&result_set, &cursor) != 0) {
                cursor.head = nullptr;
                cursor.current = nullptr;
                result_set = nullptr;
            }

            return true;
        }

    private:
        mysql_result_set_cursor_t cursor;
        struct __mysql_result_set *result_set;
        const char *buf;
    };

public:
    MySQLResultSetCursor(const MySQLResponse &resp) : resp(resp) { }

    iterator begin() const {
        const char *buf = static_cast<const char *>(resp.get_parser()->buf);
        return iterator(resp, buf);
    }
    iterator end() const { return iterator(); }

    iterator cbegin() const { return begin(); }
    iterator cend() const { return end(); }

private:
    const MySQLResponse &resp;
};

constexpr const char *mysql_datatype_to_str(int data_type) {
    #define _COKE_MYSQL_TYPE(type) case MYSQL_TYPE_##type: return #type
    switch (data_type) {
        _COKE_MYSQL_TYPE(DECIMAL);
        _COKE_MYSQL_TYPE(TINY);
        _COKE_MYSQL_TYPE(SHORT);
        _COKE_MYSQL_TYPE(LONG);
        _COKE_MYSQL_TYPE(FLOAT);
        _COKE_MYSQL_TYPE(DOUBLE);
        _COKE_MYSQL_TYPE(NULL);
        _COKE_MYSQL_TYPE(TIMESTAMP);
        _COKE_MYSQL_TYPE(LONGLONG);
        _COKE_MYSQL_TYPE(INT24);
        _COKE_MYSQL_TYPE(DATE);
        _COKE_MYSQL_TYPE(TIME);
        _COKE_MYSQL_TYPE(DATETIME);
        _COKE_MYSQL_TYPE(YEAR);
        _COKE_MYSQL_TYPE(NEWDATE);
        _COKE_MYSQL_TYPE(VARCHAR);
        _COKE_MYSQL_TYPE(BIT);
        _COKE_MYSQL_TYPE(TIMESTAMP2);
        _COKE_MYSQL_TYPE(DATETIME2);
        _COKE_MYSQL_TYPE(TIME2);
        _COKE_MYSQL_TYPE(TYPED_ARRAY);
        _COKE_MYSQL_TYPE(JSON);
        _COKE_MYSQL_TYPE(NEWDECIMAL);
        _COKE_MYSQL_TYPE(ENUM);
        _COKE_MYSQL_TYPE(SET);
        _COKE_MYSQL_TYPE(TINY_BLOB);
        _COKE_MYSQL_TYPE(MEDIUM_BLOB);
        _COKE_MYSQL_TYPE(LONG_BLOB);
        _COKE_MYSQL_TYPE(BLOB);
        _COKE_MYSQL_TYPE(VAR_STRING);
        _COKE_MYSQL_TYPE(STRING);
        _COKE_MYSQL_TYPE(GEOMETRY);
    default:
        return "UNKNOWN";
    }
    #undef _COKE_MYSQL_TYPE
}

} // namespace coke

#endif // COKE_MYSQL_UTILS_H
