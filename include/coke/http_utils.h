#ifndef COKE_HTTP_UTILS_H
#define COKE_HTTP_UTILS_H

#include <string_view>

#include "workflow/HttpMessage.h"

namespace coke {

using protocol::HttpMessage;

inline std::string_view http_body_view(const HttpMessage &message) {
    const void *body;
    size_t blen;

    if (message.get_parsed_body(&body, &blen))
        return std::string_view(static_cast<const char *>(body), blen);
    else
        return std::string_view{};
}


struct HttpHeaderView {
    std::string_view name;
    std::string_view value;
};


class HttpHeaderCursor {
    struct iterator {
        iterator() {
            cursor.head = nullptr;
            cursor.next = nullptr;
        }

        iterator(const HttpMessage &message) {
            http_header_cursor_init(&cursor, message.get_parser());
            next();
        }

        ~iterator() {
            http_header_cursor_deinit(&cursor);
        }

        bool operator==(const iterator &o) const {
            return cursor.next == o.cursor.next;
        }

        bool operator!=(const iterator &o) const {
            return cursor.next != o.cursor.next;
        }

        iterator &operator++() {
            next();
            return *this;
        }

        HttpHeaderView operator*() {
            return data;
        }

    private:
        bool next() noexcept;

    private:
        http_header_cursor_t cursor;
        HttpHeaderView data;
    };

public:
    HttpHeaderCursor(const HttpMessage &message) : message(message) { }

    iterator begin() const { return iterator(message); }
    iterator end() const { return iterator(); }

    iterator cbegin() const { return begin(); }
    iterator cend() const { return end(); }

private:
    const protocol::HttpMessage &message;
};


class HttpChunkCursor {
    struct iterator {
        iterator() { cur = end = nullptr; }

        iterator(const char *body, size_t blen, bool chunked) {
            this->cur = body;
            this->end = body + blen;
            this->chunked = chunked;

            next();
        }

        bool operator==(const iterator &o) const {
            return cur == o.cur && end == o.end;
        }

        bool operator!=(const iterator &o) const {
            return cur != o.cur || end != o.end;
        }

        iterator &operator++() {
            next();
            return *this;
        }

        std::string_view operator*() { return chunk; }

    private:
        bool next() noexcept;

    private:
        std::string_view chunk;
        const char *cur;
        const char *end;
        bool chunked;
    };

public:
    HttpChunkCursor(const HttpMessage &message) : message(message) { }

    iterator begin() const {
        std::string_view body = http_body_view(message);
        return iterator(body.data(), body.size(), message.is_chunked());
    }
    iterator end() const { return iterator(); }

    iterator cbegin() const { return begin(); }
    iterator cend() const { return end(); }

private:
    const HttpMessage &message;
};

} // namespace coke

#endif // COKE_HTTP_UTILS_H
