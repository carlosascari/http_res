/**!
* MIT License
*
* Copyright (c) 2019 Carlos Ascari Gutierrez Hermosillo Carlos.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "http_res.h"

/**
* Parse status line i.e "HTTP/2.0 200 OK"
* @param source - Http response data
* @param size - Size of http response data
* @param status_size - Pointer to integer that will hold size of the status line
* @return Pointer to start of status line
*/
char* http_res_parse_status_line(char* source, int size, int* status_size) {
  *status_size = 0;

  char* end = source + size;
  char* status_start = source;
  char* status_end = NULL;

  for (char* p = status_start; p != end; p++) {
    if (!strncmp(p, "\r\n", 2)) {
      status_end = p;
      *status_size = status_end - status_start;
      return status_start;
    }
  }

  return NULL;
}

/**
* Parse headers
* @param source - Http response data
* @param size - Size of http response data
* @param headers_size - Pointer to integer that will hold size of headers
* @return Pointer to the start of all the headers in the http response
*/
char* http_res_parse_headers(char* source, int size, int* headers_size) {
  *headers_size = 0;

  int tmp;
  char* end = source + size;
  char* status_start = NULL;
  char* headers_start = NULL;
  char* headers_end = NULL;

  status_start = http_res_parse_status_line(source, size, &tmp);

  if (!status_start) return NULL;

  headers_start = status_start + 2;

  for (char* p = headers_start; p != end; p++) {
    if (!strncmp(p, "\r\n\r\n", 4)) {
      headers_end = p;
      *headers_size = headers_end - headers_start;
      return headers_start;
    }
  }

  return NULL;
}

/**
* Parse a header value
* @param source - Http response data
* @param size - Size of http response data
* @param field - Null-terminated string of header field whose value will be returned
* @param value_size - Pointer to integer that will hold size of the header value
* @return Pointer to the start of the header value
*/
char* http_res_parse_header(char* source, int size, char* field, int* value_size) {
  *value_size = 0;

  int tmp;
  char* end = source + size;
  char* headers_start = NULL;
  char* headers_end = NULL;
  int headers_size;

  headers_start = http_res_parse_headers(source, size, &headers_size);

  if (!headers_start) return NULL;

  headers_end = headers_start + headers_size;

  int field_length = strlen(field);
  char* header_start = NULL;
  char* header_end = NULL;
  char* value_start = NULL;
  char* value_end = NULL;

  for (char* p = headers_start; p != headers_end; p++) {
    if (!strncasecmp(p, field, field_length)) {
      header_start = p;
      p += field_length;
      if (*p == ':') {
        p++;
        while(*p == ' ') p++;
        value_start = p;
        for (; p != headers_end; p++) {
          if (!strncmp(p, "\r\n", 2)) {
            header_end = p;
            value_end = p;
            *value_size = value_end - value_start;
            return value_start;
          }
        }        
      }
    }
  }

  return NULL;
}

/**
* Parse full http body
* @note If `transfer-encoding` is *chunked*, this body will include all
* chunks including the headers for each chunk, the last empty chunk and the
* optional trailer. 
* @tip Use `http_res_copy_body` for chunked encoding.
* @param source - Http response data
* @param size - Size of http response data
* @param body_size - Pointer to integer that will hold size of body
* @return Pointer to the start of body in the http response
*/
char* http_res_parse_full_body(char* source, int size, int* body_size) {
  *body_size = 0;

  int tmp;
  char* end = source + size;
  char* headers_start = NULL;
  char* body_start = NULL;
  char* body_end = end;

  headers_start = http_res_parse_headers(source, size, &tmp);

  if (!headers_start) return NULL;

  body_start = headers_start + tmp + 4;

  *body_size = body_end - body_start;

  return body_start;
}

/**
* Parse full http body
* @note This function will return the same value as http_res_parse_full_body, unless
* a `content-length` is available with a size different to the full body
* @param source - Http response data
* @param size - Size of http response data
* @param body_size - Pointer to integer that will hold size of body
* @return Pointer to the start of body in the http response
*/
char* http_res_parse_body(char* source, int size, int *body_size) {
  *body_size = 0;

  int tmp;
  char* end = source + size;
  char* status_start = NULL;
  char* status_end = NULL;
  char* headers_start = NULL;
  char* headers_end = NULL;
  char* body_start = NULL;
  char* body_end = end;
  char* chunk_start = NULL;
  char* chunk_end = NULL;
  char* content_length;

  headers_start = http_res_parse_headers(source, size, &tmp);

  if (!headers_start) return NULL;

  headers_end = headers_start + tmp;

  body_start = http_res_parse_full_body(source, size, &tmp);

  if (!body_start) return NULL;

  body_end = body_start + tmp;

  content_length = http_res_parse_header(source, size, "content-length", &tmp);

  if (!content_length) {
    *body_size = body_end - body_start;
    return body_start;
  } else {
    tmp = (int) strtol(content_length, (char**) content_length + tmp, 10);
    if (tmp <= body_end - body_start) {
      *body_size = tmp;
      return body_start;
    } else {
      return NULL;
    }
  }

  return NULL;
}

/**
* Copy the http response body to a buffer, works with chunked and non-chunked encoded data.
* Will only copy chunk data (no size info or trailers).
* @param dest - Buffer where response body will be copied to.
* @param source - Http response data
* @param size - Size of http response data
* @return Size of copied response body
*/
int http_res_copy_body(char* dest, char* source, int size) {
  int tmp;
  char* end = source + size;
  char* status_start = NULL;
  char* status_end = NULL;
  char* headers_start = NULL;
  char* headers_end = NULL;
  char* full_body_start = NULL;
  char* full_body_end = end;
  char* chunk_start = NULL;
  char* chunk_end = NULL;
  int chunk_size;
  char* transfer_encoding_start = NULL;
  char* transfer_encoding_end = NULL;
  int total_size = 0;

  headers_start = http_res_parse_headers(source, size, &tmp);

  if (!headers_start) return 0;

  headers_end = headers_start + tmp;

  full_body_start = http_res_parse_body(source, size, &tmp);

  if (!full_body_start) return 0;

  full_body_end = full_body_start + tmp;

  transfer_encoding_start = http_res_parse_header(source, size, "transfer-encoding", &tmp);

  if (!transfer_encoding_start) {
    total_size = full_body_end - full_body_start;
    memcpy(dest, full_body_start, total_size);
    return total_size;
  } else {
    transfer_encoding_end = transfer_encoding_start + tmp;

    for (char* q = transfer_encoding_start; q != transfer_encoding_end; q++) {
      if (!strncasecmp(q, "chunked", 7)) {
        for (char* p = full_body_start; p != full_body_end; p++) {
          
          chunk_size = (int) strtol(p, NULL, 16);

          if (!chunk_size) {
            // @note: Last chunk, we are done. Ignore the trailer
            // The trailer would look like: `\r\n<optional-trailer>\r\n`
            return total_size;
          } else {
            for (; p != full_body_end; p++) {
              if (!strncmp(p, "\r\n", 2)) {
                chunk_start = p + 2;
                chunk_end = chunk_start + chunk_size;
                memcpy(dest + total_size, chunk_start, chunk_size);
                total_size += chunk_size;
                p = chunk_end;
                break;
              }
            }
          }
        }
        return -1;
      }
    }
    total_size = full_body_end - full_body_start;
    memcpy(dest, full_body_start, total_size);
    return total_size;
  }
}