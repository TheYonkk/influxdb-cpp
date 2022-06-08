/*
  influxdb-cpp -- 💜 C++ client for InfluxDB.

  Copyright (c) 2010-2018 <http://ez8.co> <orca.zhang@yahoo.com>
  This library is released under the MIT License.

  Please see LICENSE file or visit https://github.com/orca-zhang/influxdb-cpp for details.
 */

#ifndef INFLUXDB_CPP_HPP
#define INFLUXDB_CPP_HPP

#include <sstream>
#include <cstring>
#include <cstdio>
#include <cstdlib>

// for testing
#include <iostream>

#define DEFAULT_PRECISION 5

#ifdef _WIN32
    #define NOMINMAX
    #include <windows.h>
    #include <algorithm>
    #pragma comment(lib, "ws2_32")
    typedef struct iovec { void* iov_base; size_t iov_len; } iovec;
    inline __int64 writev(int sock, struct iovec* iov, int cnt) {
        __int64 r = send(sock, (const char*)iov->iov_base, iov->iov_len, 0);
        return (r < 0 || cnt == 1) ? r : r + writev(sock, iov + 1, cnt - 1);
    }
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/uio.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #define closesocket close
#endif

namespace influxdb_cpp {
    struct server_info {
        std::string host_;
        int port_;
        std::string org_;
        std::string bkt_;
        std::string tkn_;
        struct addrinfo hints_, *res_=NULL;
        server_info(const std::string& host, int port, const std::string& org, const std::string& token, const std::string& bucket = "") {
            port_ = port;
            org_  = org;
            tkn_  = token;
            bkt_  = bucket;

            // please reference the IBM documentation for IPv4/IPv6 with questions
            // https://www.ibm.com/docs/en/i/7.2?topic=clients-example-ipv4-ipv6-client
            int resp = 0;

            struct in6_addr serveraddr;
            memset(&hints_, 0x00, sizeof(hints_));
            hints_.ai_flags    = AI_NUMERICSERV;
            hints_.ai_family   = AF_UNSPEC;
            hints_.ai_socktype = SOCK_STREAM;

            // check to see if the address is a valid IPv4 address
            resp = inet_pton(AF_INET, host.c_str(), &serveraddr);
            if (resp == 1){
                hints_.ai_family = AF_INET; // IPv4
                hints_.ai_flags |= AI_NUMERICHOST;
            
            // not a valid IPv4 -> check to see if address is a valid IPv6 address
            } else {
                resp = inet_pton(AF_INET6, host.c_str(), &serveraddr);
                if (resp == 1) {
                    hints_.ai_family = AF_INET6;  // IPv6
                    hints_.ai_flags |= AI_NUMERICHOST;

                }
            }

            resp = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints_, &res_);
            if (resp != 0) {
                std::cerr << "Host not found --> " << gai_strerror(resp) << std::endl;
                if (resp == EAI_SYSTEM)
                    std::cerr << "getaddrinfo() failed" << std::endl;
                exit(1);
            }

        }
    };
    namespace detail {
        struct meas_caller;
        struct tag_caller;
        struct field_caller;
        struct ts_caller;
        struct inner {
            static int http_request(const char*, const char*, const std::string&, const std::string&, const server_info&, std::string*);
            static inline unsigned char to_hex(unsigned char x) { return  x > 9 ? x + 55 : x + 48; }
            static void url_encode(std::string& out, const std::string& src);
        };
    }

    inline int flux_query(std::string& resp, const std::string& query, const server_info& si) {

        // query JSON body
        std::stringstream body;
        body << "{\"query\": \"";
        body << query;
        body << "\", \"type\": \"flux\" }";

        return detail::inner::http_request("POST", "query", "", body.str(), si, &resp);
    }

    struct builder {
        detail::tag_caller& meas(const std::string& m) {
            lines_.imbue(std::locale("C"));
            lines_.clear();
            return _m(m);
        }
    protected:
        detail::tag_caller& _m(const std::string& m) {
            _escape(m, ", ");
            return (detail::tag_caller&)*this;
        }
        detail::tag_caller& _t(const std::string& k, const std::string& v) {
            lines_ << ',';
            _escape(k, ",= ");
            lines_ << '=';
            _escape(v, ",= ");
            return (detail::tag_caller&)*this;
        }
        detail::field_caller& _f_s(char delim, const std::string& k, const std::string& v) {
#ifdef INFLUXCPP2_DEBUG
            std::cout << "KV: " << k << " " << v << std::endl;
#endif
            lines_ << delim;
            lines_ << std::fixed;
            _escape(k, ",= ");
            lines_ << "=\"";
            _escape(v, "\"");
            lines_ << '\"';
            return (detail::field_caller&)*this;
        }
        detail::field_caller& _f_i(char delim, const std::string& k, long long v) {
            lines_ << delim;
            lines_ << std::fixed;
            _escape(k, ",= ");
            lines_ << '=';
            lines_ << v << 'i';
            return (detail::field_caller&)*this;
        }
        detail::field_caller& _f_f(char delim, const std::string& k, double v, int prec) {
            lines_ << delim;
            _escape(k, ",= ");
            lines_ << std::fixed;
            lines_.precision(prec);
            lines_ << '=' << v;
            return (detail::field_caller&)*this;
        }
        detail::field_caller& _f_b(char delim, const std::string& k, bool v) {
            lines_ << delim;
            _escape(k, ",= ");
            lines_ << std::fixed;
            lines_ << '=' << (v ? 't' : 'f');
            return (detail::field_caller&)*this;
        }
        detail::ts_caller& _ts(long long ts) {
            lines_ << ' ' << ts;
            return (detail::ts_caller&)*this;
        }
        int _post_http(const server_info& si, std::string* resp) {
            return detail::inner::http_request("POST", "write", "", lines_.str(), si, resp);
        }
        void _escape(const std::string& src, const char* escape_seq) {
            size_t pos = 0, start = 0;
            while((pos = src.find_first_of(escape_seq, start)) != std::string::npos) {
                lines_.write(src.c_str() + start, pos - start);
                lines_ << '\\' << src[pos];
                start = ++pos;
            }
            lines_.write(src.c_str() + start, src.length() - start);
        }

        std::stringstream lines_;
    };

    namespace detail {
        struct tag_caller : public builder {
            detail::tag_caller& tag(const std::string& k, const std::string& v)       { return _t(k, v); }
            detail::field_caller& field(const std::string& k, const std::string& v)   { return _f_s(' ', k, v); }
            detail::field_caller& field(const std::string& k, bool v)                 { return _f_b(' ', k, v); }
            detail::field_caller& field(const std::string& k, short v)                { return _f_i(' ', k, v); }
            detail::field_caller& field(const std::string& k, int v)                  { return _f_i(' ', k, v); }
            detail::field_caller& field(const std::string& k, long v)                 { return _f_i(' ', k, v); }
            detail::field_caller& field(const std::string& k, long long v)            { return _f_i(' ', k, v); }
            detail::field_caller& field(const std::string& k, double v, int prec = DEFAULT_PRECISION) { return _f_f(' ', k, v, prec); }
        private:
            detail::tag_caller& meas(const std::string& m);
        };
        struct ts_caller : public builder {
            detail::tag_caller& meas(const std::string& m)                            { lines_ << '\n'; return _m(m); }
            int post_http(const server_info& si, std::string* resp = NULL)            { return _post_http(si, resp); }
        };
        struct field_caller : public ts_caller {
            detail::field_caller& field(const std::string& k, const std::string& v)   { return _f_s(',', k, v); }
            detail::field_caller& field(const std::string& k, bool v)                 { return _f_b(',', k, v); }
            detail::field_caller& field(const std::string& k, short v)                { return _f_i(',', k, v); }
            detail::field_caller& field(const std::string& k, int v)                  { return _f_i(',', k, v); }
            detail::field_caller& field(const std::string& k, long v)                 { return _f_i(',', k, v); }
            detail::field_caller& field(const std::string& k, long long v)            { return _f_i(',', k, v); }
            detail::field_caller& field(const std::string& k, double v, int prec = 2) { return _f_f(',', k, v, prec); }
            detail::ts_caller& timestamp(unsigned long long ts)                       { return _ts(ts); }
        };
        inline void inner::url_encode(std::string& out, const std::string& src) {
            size_t pos = 0, start = 0;
            while((pos = src.find_first_not_of("abcdefghijklmnopqrstuvwxyqABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~", start)) != std::string::npos) {
                out.append(src.c_str() + start, pos - start);
                if(src[pos] == ' ')
                    out += "+";
                else {
                    out += '%';
                    out += to_hex((unsigned char)src[pos] >> 4);
                    out += to_hex((unsigned char)src[pos] & 0xF);
                }
                start = ++pos;
            }
            out.append(src.c_str() + start, src.length() - start);
        }
        inline int inner::http_request(const char* method, const char* uri,
            const std::string& querystring, const std::string& body, const server_info& si, std::string* resp) {
            std::string header;
            struct iovec iv[2];
            int sock, ret_code = 0, content_length = 0, len = 0;
            char ch;
            unsigned char chunked = 0;


            // open the socket
            sock = socket(si.res_->ai_family, si.res_->ai_socktype, si.res_->ai_protocol);
            if (sock < 0) {
                std::cerr << "socket() failed" << std::endl;
                closesocket(sock);
                return(1);
            }



            // connect to the server
            ret_code = connect(sock, si.res_->ai_addr, si.res_->ai_addrlen);
            if (ret_code < 0)
            {
                std::cerr << "connect() failed" << std::endl;
                closesocket(sock);
                exit(1);
            }



            header.resize(len = 0x100);

            for(;;) {
                iv[0].iov_len = snprintf(&header[0], len,
                    "%s /api/v2/%s?org=%s&bucket=%s%s HTTP/1.1\r\nHost: %s\r\nAuthorization:Token %s\r\nContent-Length: %d\r\n\r\n",
                    method, uri, si.org_.c_str(), si.bkt_.c_str(),
                    querystring.c_str(), si.host_.c_str(), si.tkn_.c_str(), (int)body.length());

#ifdef INFLUXCPP2_DEBUG
                    std::cout << printf(&header[0], len,
                    "%s /api/v2/%s?org=%s&bucket=%s%s HTTP/1.1\r\nHost: %s\r\nAuthorization:Token %s\r\nContent-Length: %d\r\n\r\n",
                    method, uri, si.org_.c_str(), si.bkt_.c_str(),
                    querystring.c_str(), si.host_.c_str(), si.tkn_.c_str(), (int)body.length()) << std::endl;
#endif
                if((int)iv[0].iov_len >= len)
                    header.resize(len *= 2);
                else
                    break;
            }
            iv[0].iov_base = &header[0];
            iv[1].iov_base = (void*)&body[0];
            iv[1].iov_len = body.length();

            if(writev(sock, iv, 2) < (int)(iv[0].iov_len + iv[1].iov_len)) {
                ret_code = -6;
                goto END;
            }

            iv[0].iov_len = len;

#define _NO_MORE() (len >= (int)iv[0].iov_len && \
    (iv[0].iov_len = recv(sock, &header[0], header.length(), len = 0)) == size_t(-1))
#define _GET_NEXT_CHAR() (ch = _NO_MORE() ? 0 : header[len++])
#define _LOOP_NEXT(statement) for(;;) { if(!(_GET_NEXT_CHAR())) { ret_code = -7; goto END; } statement }
#define _UNTIL(c) _LOOP_NEXT( if(ch == c) break; )
#define _GET_NUMBER(n) _LOOP_NEXT( if(ch >= '0' && ch <= '9') n = n * 10 + (ch - '0'); else break; )
#define _GET_CHUNKED_LEN(n, c) _LOOP_NEXT( if(ch >= '0' && ch <= '9') n = n * 16 + (ch - '0'); \
            else if(ch >= 'A' && ch <= 'F') n = n * 16 + (ch - 'A') + 10; \
            else if(ch >= 'a' && ch <= 'f') n = n * 16 + (ch - 'a') + 10; else {if(ch != c) { ret_code = -8; goto END; } break;} )
#define _(c) if((_GET_NEXT_CHAR()) != c) break;
#define __(c) if((_GET_NEXT_CHAR()) != c) { ret_code = -9; goto END; }

            if(resp) resp->clear();

            _UNTIL(' ')_GET_NUMBER(ret_code)
            for(;;) {
                _UNTIL('\n')
                switch(_GET_NEXT_CHAR()) {
                    case 'C':_('o')_('n')_('t')_('e')_('n')_('t')_('-')
                        _('L')_('e')_('n')_('g')_('t')_('h')_(':')_(' ')
                        _GET_NUMBER(content_length)
                        break;
                    case 'T':_('r')_('a')_('n')_('s')_('f')_('e')_('r')_('-')
                        _('E')_('n')_('c')_('o')_('d')_('i')_('n')_('g')_(':')
                        _(' ')_('c')_('h')_('u')_('n')_('k')_('e')_('d')
                        chunked = 1;
                        break;
                    case '\r':__('\n')
                        switch(chunked) {
                            do {__('\r')__('\n')
                            case 1:
                                _GET_CHUNKED_LEN(content_length, '\r')__('\n')
                                if(!content_length) {
                                    __('\r')__('\n')
                                    goto END;
                                }
                            case 0:
                                while(content_length > 0 && !_NO_MORE()) {
                                    content_length -= (iv[1].iov_len = std::min(content_length, (int)iv[0].iov_len - len));
                                    if(resp) resp->append(&header[len], iv[1].iov_len);
                                    len += iv[1].iov_len;
                                }
                            } while(chunked);
                        }
                        goto END;
                }
                if(!ch) {
                    ret_code = -10;
                    goto END;
                }
            }
            ret_code = -11;
        END:
            closesocket(sock);
            return ret_code / 100 == 2 ? 0 : ret_code;
#undef _NO_MORE
#undef _GET_NEXT_CHAR
#undef _LOOP_NEXT
#undef _UNTIL
#undef _GET_NUMBER
#undef _GET_CHUNKED_LEN
#undef _
#undef __
        }
    }
}

#endif // INFLUXDB_CPP_HPP
