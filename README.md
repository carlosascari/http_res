# HTTP Response Parser 

> A memory efficient http parser written in c. No memory allocation or buffers. 
> Pure functional programming. 

- Handles chunked transfer-encoding
- Zero memory allocation or temporary buffers
- Only depends on: `string.h` and `stdlib.h`
- Naturaly fast
- Very simple; easy for you to snatch

This is a naive, no thrills, no fluff, implementation of a http response parser.

Assumptions are made: 

 - You are not feeding it NULL as a source
 - The source buffer may or may not be complete
 - The response data is standards compliant; there is no error detection.


## Usage

Say you have a response data buffer and its size available, for all these examples.

*The buffer may or may not have the completed response.*

```
int response_size;
char* response = GET_THAT_HTTP_RESPONSE_DATA(&response_size);
```

**Get the status line**

```
int status_line_size;

char* status_line = http_res_parse_status_line(response, response_size, &status_line_size);

if (status_line == NULL) {
  printf("Buffer not ready; doesn't have the status line available");
  return -1;
}

for (char* p = status_line; p != status_line + status_line_size; p++) {
   printf("%c", *p);
}
```

**Get the headers**

```
int headers_size;

char* headers = http_res_parse_headers(response, response_size, &headers_size);

if (headers == NULL) {
  printf("Buffer not ready; doesn't have headers available");
  return -2;
}

for (char* p = headers; p != headers + headers_size; p++) {
   printf("%c", *p);
}
```

**Get a particular header value**

Header field is case-insensitive

```
int header_value_size;

char* header_value = http_res_parse_header(response, response_size, "content-length", &header_value_size);

if (header_value == NULL) {
  printf("Header does not exist or data is not complete");
  return -3;
}

for (char* p = header_value; p != header_value + header_value_size; p++) {
   printf("%c", *p);
}
```

**Get response body**

**Note** that if the response has a *chunked* `transfer-encoding`, the body will include chunk size information for each chunk, the last empty chunk and trailer information. This function works best when transfer-encoding is not chunked.

```
int body_size;

char* body = http_res_parse_body(response, response_size, &body_size);

if (body == NULL) {
  printf("Buffer not ready and/or body data is not available");
  return -4;
}

for (char* p = body; p != body + body_size; p++) {
   printf("%c", *p);
}
```

**Get a chunked encoded response body**

Will work with chunked encoding responses and non-chunk encoded responses.

We have to use a destination buffer in order to merge all the chunks together, while at the same time removing their header information.

```
char body = malloc(response_size); // Will be smaller, unless response has not completed

int body_size = http_res_copy_body(body, response, response_size);

if (body_size == 0) {
  printf("buffer not ready; body data is not available");
  return -5;
}

if (body_size == -1) {
  printf("buffer not ready; chunks are missing!");
  return -6;
}

for (char* p = body; p != body + body_size; p++) {
   printf("%c", *p);
}
```

## How to compile

Its plain c, nothing fancy.

Just include the header

```
#include "http_res.h"
```

## License

MIT