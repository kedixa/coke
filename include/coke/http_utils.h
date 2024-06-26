/**
 * Copyright 2024 Coke Project (https://github.com/kedixa/coke)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Authors: kedixa (https://github.com/kedixa)
*/

#ifndef COKE_HTTP_UTILS_H
#define COKE_HTTP_UTILS_H

#include <string_view>

#include "workflow/HttpMessage.h"

namespace coke {

using HttpMessage = protocol::HttpMessage;

inline std::string_view http_body_view(const HttpMessage &message) {
    const void *body;
    std::size_t blen;

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

        iterator(const char *body, std::size_t blen, bool chunked) {
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
