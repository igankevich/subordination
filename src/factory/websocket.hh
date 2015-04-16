/*
 * WebSocket lib with support for "wss://" encryption.
 * Copyright 2010 Joel Martin
 * Licensed under LGPL version 3 (see docs/LICENSE.LGPL-3)
 *
 * You can make a cert/key with openssl using:
 * openssl req -new -x509 -days 365 -nodes -out self.pem -keyout self.pem
 * as taken from http://docs.python.org/dev/library/ssl.html#certificates
 */

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/md5.h> /* md5 hash */
#include <openssl/sha.h> /* sha1 hash */

namespace factory {

	struct Autoinitialize_openssl_library {
		Autoinitialize_openssl_library() {
			::SSL_library_init();
			::OpenSSL_add_all_algorithms();
			::SSL_load_error_strings();
		}
	} _autoinitialize_openssl_library;


	const size_t BUFSIZE = 65536;
	const size_t DBUFSIZE = (BUFSIZE * 3) / 4 - 20;

#define SERVER_HANDSHAKE_HIXIE "HTTP/1.1 101 Web Socket Protocol Handshake\r\n\
Upgrade: WebSocket\r\n\
Connection: Upgrade\r\n\
%sWebSocket-Origin: %s\r\n\
%sWebSocket-Location: %s://%s%s\r\n\
%sWebSocket-Protocol: %s\r\n\
\r\n%s"

#define SERVER_HANDSHAKE_HYBI "HTTP/1.1 101 Switching Protocols\r\n\
Upgrade: websocket\r\n\
Connection: Upgrade\r\n\
Sec-WebSocket-Accept: %s\r\n\
Sec-WebSocket-Protocol: %s\r\n\
\r\n"

#define POLICY_RESPONSE "<cross-domain-policy><allow-access-from domain=\"*\" to-ports=\"*\" /></cross-domain-policy>\n"

enum Subprotocol {
	BINARY = 0,
	BASE64 = 1
};

const char* SUBPROTOCOL_NAMES[] = {
	"binary",
	"base64"
};

typedef struct {
    char path[1024+1];
    char host[1024+1];
    char origin[1024+1];
    char version[1024+1];
    char connection[1024+1];
    char protocols[1024+1];
    char key1[1024+1];
    char key2[1024+1];
    char key3[8+1];
} headers_t;

typedef struct {
    int        sockfd;
    ::SSL_CTX   *ssl_ctx;
    ::SSL       *ssl;
    int        hixie;
    int        hybi;
	Subprotocol subproto;
    headers_t *headers;
    char      *cin_buf;
    char      *cout_buf;
    char      *tin_buf;
    char      *tout_buf;
} ws_ctx_t;

typedef struct {
    int verbose;
    char listen_host[256];
    int listen_port;
    void (*handler)(ws_ctx_t*);
    int handler_id;
    char *cert;
    char *key;
    int ssl_only;
    int daemon;
    int run_once;
} settings_t;

int b64_ntop(const unsigned char* src, size_t srclength, char *target, size_t targsize) {
	size_t encoded_size = factory::base64_encoded_size(srclength);
	if (encoded_size > targsize) return -1;
	factory::base64_encode(src, src + srclength, target);
	return encoded_size;
}

int b64_pton(const char *src, unsigned char* target, size_t targsize, size_t srclength) {
	size_t max_size = factory::base64_max_decoded_size(srclength);
	if (max_size > targsize) return -1;
	return factory::base64_decode(src, src + srclength, target);
}


/*
 * Global state
 *
 *   Warning: not thread safe
 */
settings_t settings;




/*
 * SSL Wrapper Code
 */

ssize_t ws_recv(ws_ctx_t *ctx, void *buf, size_t len) {
    if (ctx->ssl) {
        //Logger(Level::WEBSOCKET) << "SSL recv" << std::endl;
        return ::SSL_read(ctx->ssl, buf, len);
    } else {
        return recv(ctx->sockfd, buf, len, 0);
    }
}

ssize_t ws_send(ws_ctx_t *ctx, const void *buf, size_t len) {
    if (ctx->ssl) {
        //Logger(Level::WEBSOCKET) << "SSL send" << std::endl;
        return ::SSL_write(ctx->ssl, buf, len);
    } else {
        return send(ctx->sockfd, buf, len, 0);
    }
}

ws_ctx_t *alloc_ws_ctx() {
    ws_ctx_t *ctx;
	ctx = new ws_ctx_t;
	ctx->cin_buf = new char[BUFSIZE];
	ctx->cout_buf = new char[BUFSIZE];
	ctx->tin_buf = new char[BUFSIZE];
	ctx->tout_buf = new char[BUFSIZE];
    ctx->headers = new headers_t;
    ctx->ssl = NULL;
    ctx->ssl_ctx = NULL;
    return ctx;
}

void free_ws_ctx(ws_ctx_t *ctx) {
    free(ctx->cin_buf);
    free(ctx->cout_buf);
    free(ctx->tin_buf);
    free(ctx->tout_buf);
    free(ctx);
}

void ws_socket(ws_ctx_t *ctx, int socket) {
    ctx->sockfd = socket;
}

ws_ctx_t *ws_socket_ssl(ws_ctx_t *ctx, int socket, char * certfile, char * keyfile) {

    int ret;
    char* use_keyfile;
    ws_socket(ctx, socket);

    if (keyfile && (keyfile[0] != '\0')) {
        // Separate key file
        use_keyfile = keyfile;
    } else {
        // Combined key and cert file
        use_keyfile = certfile;
    }

    ctx->ssl_ctx = ::SSL_CTX_new(TLSv1_server_method());
    if (ctx->ssl_ctx == NULL) {
        ::ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to configure SSL context");
    }

    if (::SSL_CTX_use_PrivateKey_file(ctx->ssl_ctx, use_keyfile,
                                    SSL_FILETYPE_PEM) <= 0) {
		std::stringstream msg;
        msg << "Unable to load private key file '" << use_keyfile << "'." << std::endl;
        throw std::runtime_error(msg.str());
    }

    if (::SSL_CTX_use_certificate_file(ctx->ssl_ctx, certfile,
                                     SSL_FILETYPE_PEM) <= 0) {
		std::stringstream msg;
        msg << "Unable to load certificate file '" << certfile << "'." << std::endl;
        throw std::runtime_error(msg.str());
    }

//    if (SSL_CTX_set_cipher_list(ctx->ssl_ctx, "DEFAULT") != 1) {
//        sprintf(msg, "Unable to set cipher" << std::endl;
//        throw std::runtime_error(msg);
//    }

    // Associate socket and ssl object
    ctx->ssl = ::SSL_new(ctx->ssl_ctx);
    ::SSL_set_fd(ctx->ssl, socket);

    ret = ::SSL_accept(ctx->ssl);
    if (ret < 0) {
        ::ERR_print_errors_fp(stderr);
        return NULL;
    }

    return ctx;
}

void ws_socket_free(ws_ctx_t *ctx) {
    if (ctx->ssl) {
        ::SSL_free(ctx->ssl);
        ctx->ssl = NULL;
    }
    if (ctx->ssl_ctx) {
        ::SSL_CTX_free(ctx->ssl_ctx);
        ctx->ssl_ctx = NULL;
    }
    if (ctx->sockfd) {
        shutdown(ctx->sockfd, SHUT_RDWR);
        close(ctx->sockfd);
        ctx->sockfd = 0;
    }
}

/* ------------------------------------------------------- */


int encode_hixie(u_char const *src, size_t srclength,
                 char *target, size_t targsize) {
    int sz = 0, len = 0;
    target[sz++] = '\x00';
    len = b64_ntop(src, srclength, target+sz, targsize-sz);
    if (len < 0) {
        return len;
    }
    sz += len;
    target[sz++] = '\xff';
    return sz;
}

int decode_hixie(char *src, size_t srclength,
                 u_char *target, size_t targsize,
                 unsigned int *opcode, unsigned int *left) {
    char *start, *end, cntstr[4];
    int len, framecount = 0, retlen = 0;
//    unsigned char chr;
    if ((src[0] != '\x00') || (src[srclength-1] != '\xff')) {
        Logger(Level::WEBSOCKET) << "WebSocket framing error" << std::endl;
        return -1;
    }
    *left = srclength;

    if (srclength == 2 &&
        (src[0] == '\xff') && 
        (src[1] == '\x00')) {
        // client sent orderly close frame
        *opcode = 0x8; // Close frame
        return 0;
    }
    *opcode = 0x1; // Text frame

    start = src+1; // Skip '\x00' start
    do {
        /* We may have more than one frame */
//        end = (char *)memchr(start, '\xff', srclength);
		// TODO: check this
        end = std::find(start, src + srclength, '\xff');
        *end = '\x00';
		// TODO: check this
        len = b64_pton(start, target+retlen, targsize-retlen, end-start);
        if (len < 0) {
            return len;
        }
        retlen += len;
        start = end + 2; // Skip '\xff' end and '\x00' start 
        framecount++;
    } while (end < (src+srclength-1));
    if (framecount > 1) {
        snprintf(cntstr, 3, "%d", framecount);
    }
    *left = 0;
    return retlen;
}

int encode_hybi(u_char const *src, size_t srclength,
                char *target, size_t targsize, unsigned int opcode)
{
    unsigned long long b64_sz, payload_offset = 2;
	int len = 0;
    
    if ((int)srclength <= 0)
    {
        return 0;
    }

    b64_sz = ((srclength - 1) / 3) * 4 + 4;

    target[0] = (char)((opcode & 0x0F) | 0x80);

    if (b64_sz <= 125) {
        target[1] = (char) b64_sz;
        payload_offset = 2;
    } else if ((b64_sz > 125) && (b64_sz < 65536)) {
        target[1] = (char) 126;
        *(u_short*)&(target[2]) = htons(b64_sz);
        payload_offset = 4;
    } else {
        Logger(Level::WEBSOCKET) << "Sending frames larger than 65535 bytes not supported" << std::endl;
        return -1;
        //target[1] = (char) 127;
        //*(u_long*)&(target[2]) = htonl(b64_sz);
        //payload_offset = 10;
    }

    len = b64_ntop(src, srclength, target+payload_offset, targsize-payload_offset);
    
    if (len < 0) {
        return len;
    }

    return len + payload_offset;
}

int decode_hybi(unsigned char *src, size_t srclength,
                u_char *target, size_t,
                unsigned int *opcode, unsigned int *left)
{
    unsigned char *frame, *mask, *payload, save_char;
    char cntstr[4];
    int masked = 0;
    int len, framecount = 0;
	unsigned int i = 0;
    size_t remaining = 0;
    unsigned int target_offset = 0, hdr_length = 0, payload_length = 0;
    
    *left = srclength;
    frame = src;

    //printf("Deocde new frame" << std::endl;
    while (1) {
        // Need at least two bytes of the header
        // Find beginning of next frame. First time hdr_length, masked and
        // payload_length are zero
        frame += hdr_length + 4*masked + payload_length;
        //printf("frame[0..3]: 0x%x 0x%x 0x%x 0x%x (tot: %d)\n",
        //       (unsigned char) frame[0],
        //       (unsigned char) frame[1],
        //       (unsigned char) frame[2],
        //       (unsigned char) frame[3], srclength);

        if (frame > src + srclength) {
            //printf("Truncated frame from client, need %d more bytes\n", frame - (src + srclength) );
            break;
        }
        remaining = (src + srclength) - frame;
        if (remaining < 2) {
            //printf("Truncated frame header from client" << std::endl;
            break;
        }
        framecount ++;

        *opcode = frame[0] & 0x0f;
        masked = (frame[1] & 0x80) >> 7;

        if (*opcode == 0x8) {
            // client sent orderly close frame
            break;
        }

        payload_length = frame[1] & 0x7f;
        if (payload_length < 126) {
            hdr_length = 2;
            //frame += 2 * sizeof(char);
        } else if (payload_length == 126) {
            payload_length = (frame[2] << 8) + frame[3];
            hdr_length = 4;
        } else {
            Logger(Level::WEBSOCKET) << "Receiving frames larger than 65535 bytes not supported" << std::endl;
            return -1;
        }
        if ((hdr_length + 4*masked + payload_length) > remaining) {
            continue;
        }
        //printf("    payload_length: %u, raw remaining: %u\n", payload_length, remaining);
        payload = frame + hdr_length + 4*masked;

        if (*opcode != 1 && *opcode != 2) {
            Logger(Level::WEBSOCKET) << "Ignoring non-data frame, opcode 0x"
				<< std::hex << *opcode << std::dec << std::endl;
            continue;
        }

        if (payload_length == 0) {
            Logger(Level::WEBSOCKET) << "Ignoring empty frame" << std::endl;
            continue;
        }

        if ((payload_length > 0) && (!masked)) {
            Logger(Level::WEBSOCKET) << "Received unmasked payload from client" << std::endl;
            return -1;
        }

        // Terminate with a null for base64 decode
        save_char = payload[payload_length];
        payload[payload_length] = '\0';

        // unmask the data
        mask = payload - 4;
        for (i = 0; i < payload_length; i++) {
            payload[i] ^= mask[i%4];
        }

		std::cout << "payload size = " << payload_length << std::endl;
		std::cout << "payload: ";
		for (unsigned int i=0; i<payload_length; ++i) {
			std::cout << (int)payload[i] << ' ';
		}
		std::cout << std::endl;
		std::copy(payload, payload + payload_length, target);

        // base64 decode the data
		len = payload_length;
//        len = b64_pton((const char*)payload, target+target_offset, targsize);

        // Restore the first character of the next frame
        payload[payload_length] = save_char;
//        if (len < 0) {
//            Logger(Level::WEBSOCKET) << "Base64 decode error code %d", len);
//            return len;
//        }
        target_offset += len;

        //printf("    len %d, raw %s\n", len, frame);
    }

    if (framecount > 1) {
        snprintf(cntstr, 3, "%d", framecount);
    }
    
    *left = remaining;
    return target_offset;
}



int parse_handshake(ws_ctx_t *ws_ctx, char *handshake) {
    char *start, *end;
    headers_t *headers = ws_ctx->headers;

    headers->key1[0] = '\0';
    headers->key2[0] = '\0';
    headers->key3[0] = '\0';
    
    if ((strlen(handshake) < 92) || (bcmp(handshake, "GET ", 4) != 0)) {
        return 0;
    }
    start = handshake+4;
    end = strstr(start, " HTTP/1.1");
    if (!end) { return 0; }
    strncpy(headers->path, start, end-start);
    headers->path[end-start] = '\0';

    start = strstr(handshake, "\r\nHost: ");
    if (!start) { return 0; }
    start += 8;
    end = strstr(start, "\r\n");
    strncpy(headers->host, start, end-start);
    headers->host[end-start] = '\0';

    headers->origin[0] = '\0';
    start = strstr(handshake, "\r\nOrigin: ");
    if (start) {
        start += 10;
    } else {
        start = strstr(handshake, "\r\nSec-WebSocket-Origin: ");
        if (!start) { return 0; }
        start += 24;
    }
    end = strstr(start, "\r\n");
    strncpy(headers->origin, start, end-start);
    headers->origin[end-start] = '\0';
   
    start = strstr(handshake, "\r\nSec-WebSocket-Version: ");
    if (start) {
        // HyBi/RFC 6455
        start += 25;
        end = strstr(start, "\r\n");
        strncpy(headers->version, start, end-start);
        headers->version[end-start] = '\0';
        ws_ctx->hixie = 0;
        ws_ctx->hybi = strtol(headers->version, NULL, 10);

        start = strstr(handshake, "\r\nSec-WebSocket-Key: ");
        if (!start) { return 0; }
        start += 21;
        end = strstr(start, "\r\n");
        strncpy(headers->key1, start, end-start);
        headers->key1[end-start] = '\0';
   
        start = strstr(handshake, "\r\nConnection: ");
        if (!start) { return 0; }
        start += 14;
        end = strstr(start, "\r\n");
        strncpy(headers->connection, start, end-start);
        headers->connection[end-start] = '\0';
   
        start = strstr(handshake, "\r\nSec-WebSocket-Protocol: ");
        if (!start) { return 0; }
        start += 26;
        end = strstr(start, "\r\n");
        strncpy(headers->protocols, start, end-start);
        headers->protocols[end-start] = '\0';
    } else {
        // Hixie 75 or 76
        ws_ctx->hybi = 0;

        start = strstr(handshake, "\r\n\r\n");
        if (!start) { return 0; }
        start += 4;
        if (strlen(start) == 8) {
            ws_ctx->hixie = 76;
            strncpy(headers->key3, start, 8);
            headers->key3[8] = '\0';

            start = strstr(handshake, "\r\nSec-WebSocket-Key1: ");
            if (!start) { return 0; }
            start += 22;
            end = strstr(start, "\r\n");
            strncpy(headers->key1, start, end-start);
            headers->key1[end-start] = '\0';
        
            start = strstr(handshake, "\r\nSec-WebSocket-Key2: ");
            if (!start) { return 0; }
            start += 22;
            end = strstr(start, "\r\n");
            strncpy(headers->key2, start, end-start);
            headers->key2[end-start] = '\0';
        } else {
            ws_ctx->hixie = 75;
        }

    }

    return 1;
}

unsigned long parse_hixie76_key(const char * key) {
    unsigned long i, spaces = 0, num = 0;
    for (i=0; i < strlen(key); i++) {
        if (key[i] == ' ') {
            spaces += 1;
        }
        if ((key[i] >= 48) && (key[i] <= 57)) {
            num = num * 10 + (key[i] - 48);
        }
    }
    return num / spaces;
}

int gen_md5(const headers_t *headers, char *target) {

	static const size_t HIXIE_MD5_DIGEST_LENGTH = 16;

    unsigned long key1_long = parse_hixie76_key(headers->key1);
    unsigned long key2_long = parse_hixie76_key(headers->key2);

//	union Bytes {
//		Bytes(unsigned long ll): l(ll) {}
//		char operator[](int i) const { return bytes[i]; }
//	private:
//		unsigned long l;
//		char bytes[4];
//	};

	using factory::Bytes;
	Bytes<unsigned long> key1 = key1_long;
	Bytes<unsigned long> key2 = key2_long;

    const char *key3 = headers->key3;

    ::MD5_CTX c;
    char in[HIXIE_MD5_DIGEST_LENGTH] = {
// TODO: check this refactoring
//        key1 >> 24, key1 >> 16, key1 >> 8, key1,
//        key2 >> 24, key2 >> 16, key2 >> 8, key2,
        key1[0], key1[1], key1[2], key1[3],
        key2[0], key2[1], key2[2], key2[3],
        key3[0], key3[1], key3[2], key3[3],
        key3[4], key3[5], key3[6], key3[7]
    };

    ::MD5_Init(&c);
    ::MD5_Update(&c, (void *)in, sizeof in);
    ::MD5_Final((unsigned char *)target, &c);

    target[HIXIE_MD5_DIGEST_LENGTH] = '\0';

    return 1;
}

static void gen_sha1(headers_t *headers, char *target) {

	static const size_t HYBI10_ACCEPTHDRLEN = 29;
	static const char HYBI_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    ::SHA_CTX c;
    unsigned char hash[SHA_DIGEST_LENGTH];

    ::SHA1_Init(&c);
    ::SHA1_Update(&c, headers->key1, strlen(headers->key1));
    ::SHA1_Update(&c, HYBI_GUID, 36);
    ::SHA1_Final(hash, &c);

    b64_ntop(hash, sizeof hash, target, HYBI10_ACCEPTHDRLEN);
    //assert(r == HYBI10_ACCEPTHDRLEN - 1);
}


ws_ctx_t *do_handshake(int sock) {
    char handshake[4096], response[4096], sha1[29], trailer[17];
    std::string scheme, pre;
    headers_t *headers;
    int len, i, offset;
    ws_ctx_t * ws_ctx;

    // Peek, but don't read the data
    len = recv(sock, handshake, 1024, MSG_PEEK);
    handshake[len] = 0;
    if (len == 0) {
        Logger(Level::WEBSOCKET) << "ignoring empty handshake" << std::endl;
        return NULL;
    } else if (bcmp(handshake, "<policy-file-request/>", 22) == 0) {
        len = recv(sock, handshake, 1024, 0);
        handshake[len] = 0;
        Logger(Level::WEBSOCKET) << "sending flash policy response" << std::endl;
		// TODO: this line sends NULL character
        send(sock, POLICY_RESPONSE, sizeof(POLICY_RESPONSE), 0);
        return NULL;
    } else if ((bcmp(handshake, "\x16", 1) == 0) ||
               (bcmp(handshake, "\x80", 1) == 0)) {
        // SSL
        if (!settings.cert) {
            Logger(Level::WEBSOCKET) << "SSL connection but no cert specified" << std::endl;
            return NULL;
        } else if (access(settings.cert, R_OK) != 0) {
            Logger(Level::WEBSOCKET) << "SSL connection but '"
				<< settings.cert << "' not found" << std::endl;
            return NULL;
        }
        ws_ctx = alloc_ws_ctx();
        ws_socket_ssl(ws_ctx, sock, settings.cert, settings.key);
        if (! ws_ctx) { return NULL; }
        scheme = "wss";
        Logger(Level::WEBSOCKET) << "using SSL socket" << std::endl;
    } else if (settings.ssl_only) {
        Logger(Level::WEBSOCKET) << "non-SSL connection disallowed" << std::endl;
        return NULL;
    } else {
        ws_ctx = alloc_ws_ctx();
        ws_socket(ws_ctx, sock);
        if (! ws_ctx) { return NULL; }
        scheme = "ws";
        Logger(Level::WEBSOCKET) << "using plain (not SSL) socket" << std::endl;
    }
    offset = 0;
    for (i = 0; i < 10; i++) {
        len = ws_recv(ws_ctx, handshake+offset, 4096);
        if (len == 0) {
            Logger(Level::WEBSOCKET) << "Client closed during handshake" << std::endl;
            return NULL;
        }
        offset += len;
        handshake[offset] = 0;
        if (strstr(handshake, "\r\n\r\n")) {
            break;
        }
        usleep(10);
    }

    Logger(Level::WEBSOCKET) << "handshake: " <<  handshake << std::endl;
    if (!parse_handshake(ws_ctx, handshake)) {
        Logger(Level::WEBSOCKET) << "Invalid WS request" << std::endl;
        return NULL;
    }

	ws_ctx->subproto = BINARY;

    headers = ws_ctx->headers;
    if (ws_ctx->hybi > 0) {
        Logger(Level::WEBSOCKET) << "using protocol HyBi/IETF 6455 " << ws_ctx->hybi << std::endl;
        gen_sha1(headers, sha1);
        sprintf(response, SERVER_HANDSHAKE_HYBI, sha1, SUBPROTOCOL_NAMES[ws_ctx->subproto]);
    } else {
        if (ws_ctx->hixie == 76) {
            Logger(Level::WEBSOCKET) << "using protocol Hixie 76" << std::endl;
            gen_md5(headers, trailer);
            pre = "Sec-";
        } else {
            Logger(Level::WEBSOCKET) << "using protocol Hixie 75" << std::endl;
            trailer[0] = '\0';
            pre = "";
        }
        sprintf(response, SERVER_HANDSHAKE_HIXIE, pre.c_str(), headers->origin, pre.c_str(), scheme.c_str(),
                headers->host, headers->path, pre.c_str(), SUBPROTOCOL_NAMES[ws_ctx->subproto], trailer);
    }
    
    //Logger(Level::WEBSOCKET) << "response: %s\n", response);
    ws_send(ws_ctx, response, strlen(response));

    return ws_ctx;
}

	struct Web_socket {

		explicit Web_socket(ws_ctx_t* context): _context(context) {}

		ssize_t read(char*, size_t) {
			return 0;
		}

		ssize_t write(const char* buf, size_t size) {
			unsigned int encoded_size;
            if (_context->hybi) {
                encoded_size = encode_hybi((unsigned char*)buf, size, _context->cout_buf, BUFSIZE, 1);
            } else {
                encoded_size = encode_hixie((unsigned char*)buf, size, _context->cout_buf, BUFSIZE);
            }
			return ws_send(_context, _context->cout_buf, encoded_size);
		}

		friend std::ostream& operator<<(std::ostream& out, const Web_socket& rhs) {
			return out << rhs._context->sockfd;
		}

	private:
		ws_ctx_t* _context;
	};

}
