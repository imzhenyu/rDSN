/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "parser.h"

#include <stdbool.h>
#include <string.h>

using namespace dsn;

int grpc_http1_trace = 0;

static char *buf2str(void *buffer, size_t length) {
  char *out = (char*)dsn_transient_malloc(length + 1);
  memcpy(out, buffer, length);
  out[length] = 0;
  return out;
}

static ::dsn::error_code handle_response_line(grpc_http_parser *parser) {
  uint8_t *beg = parser->cur_line;
  uint8_t *cur = beg;
  uint8_t *end = beg + parser->cur_line_length;

  if (cur == end || *cur++ != 'H')
  {
    derror ("Expected 'H'");
    return ERR_INVALID_DATA;
  }
  if (cur == end || *cur++ != 'T')
  {
    derror ("Expected 'T'");
    return ERR_INVALID_DATA;
  }
  if (cur == end || *cur++ != 'T')
  {
    derror ("Expected 'T'");
    return ERR_INVALID_DATA;
  }
  if (cur == end || *cur++ != 'P')
  {
    derror ("Expected 'P'");
    return ERR_INVALID_DATA;
  }
  if (cur == end || *cur++ != '/')
  {
    derror ("Expected '/'");
    return ERR_INVALID_DATA;
  }
  if (cur == end || *cur++ != '1')
  {
    derror ("Expected '1'");
    return ERR_INVALID_DATA;
  }
  if (cur == end || *cur++ != '.')
  {
    derror ("Expected '.'");
    return ERR_INVALID_DATA;
  }

  if (cur == end || *cur < '0' || *cur++ > '1') 
  {
    derror("Expected HTTP/1.0 or HTTP/1.1");
    return ERR_INVALID_DATA;
  }
  if (cur == end || *cur++ != ' ')
  {
    derror ("Expected ' '");
    return ERR_INVALID_DATA;
  }
  if (cur == end || *cur < '1' || *cur++ > '9')
  {
    derror("Expected status code");
    return ERR_INVALID_DATA;
  }
  if (cur == end || *cur < '0' || *cur++ > '9')
  {
    derror("Expected status code");
    return ERR_INVALID_DATA;
  }
  if (cur == end || *cur < '0' || *cur++ > '9')
  {
    derror("Expected status code");
    return ERR_INVALID_DATA;
  }
  parser->http.response->status =
      (cur[-3] - '0') * 100 + (cur[-2] - '0') * 10 + (cur[-1] - '0');
  if (cur == end || *cur++ != ' ') 
  {
    derror("Expected ' '");
    return ERR_INVALID_DATA;
  }

  /* we don't really care about the status code message */
  return ERR_OK;
}

static ::dsn::error_code handle_request_line(grpc_http_parser *parser) {
  uint8_t *beg = parser->cur_line;
  uint8_t *cur = beg;
  uint8_t *end = beg + parser->cur_line_length;
  uint8_t vers_major = 0;
  uint8_t vers_minor = 0;

  while (cur != end && *cur++ != ' ')
    ;
  if (cur == end)
  {
    derror("No method on HTTP request line");
    return ERR_INVALID_DATA;
  }
  parser->http.request->method = buf2str(beg, (size_t)(cur - beg - 1));

  beg = cur;
  while (cur != end && *cur++ != ' ')
    ;
  if (cur == end) {
    derror ("No path on HTTP request line");
    return ERR_INVALID_DATA;
  }
  parser->http.request->path = buf2str(beg, (size_t)(cur - beg - 1));

  if (cur == end || *cur++ != 'H')
  {
    derror ("Expected 'H'");
    return ERR_INVALID_DATA;
  }
  if (cur == end || *cur++ != 'T')
  {
    derror ("Expected 'T'");
    return ERR_INVALID_DATA;
  }
  if (cur == end || *cur++ != 'T')
  {
    derror ("Expected 'T'");
    return ERR_INVALID_DATA;
  }
  if (cur == end || *cur++ != 'P')
  {
    derror ("Expected 'P'");
    return ERR_INVALID_DATA;
  }
  if (cur == end || *cur++ != '/')
  {
    derror ("Expected '/'");
    return ERR_INVALID_DATA;
  }
  vers_major = (uint8_t)(*cur++ - '1' + 1);
  ++cur;
  if (cur == end)
  {
    derror ("End of line in HTTP version string");
    return ERR_INVALID_DATA;
  }
  vers_minor = (uint8_t)(*cur++ - '1' + 1);

  if (vers_major == 1) {
    if (vers_minor == 0) {
      parser->http.request->version = GRPC_HTTP_HTTP10;
    } else if (vers_minor == 1) {
      parser->http.request->version = GRPC_HTTP_HTTP11;
    } else 
    {
      derror ("Expected one of HTTP/1.0, HTTP/1.1, or HTTP/2.0");
      return ERR_INVALID_DATA;
    }
  } else if (vers_major == 2) {
    if (vers_minor == 0) {
      parser->http.request->version = GRPC_HTTP_HTTP20;
    } else {
      derror ("Expected one of HTTP/1.0, HTTP/1.1, or HTTP/2.0");
      return ERR_INVALID_DATA;
    }
  } else {
    derror ("Expected one of HTTP/1.0, HTTP/1.1, or HTTP/2.0");
      return ERR_INVALID_DATA;
  }

  return ERR_OK;
}

static ::dsn::error_code handle_first_line(grpc_http_parser *parser) {
  switch (parser->type) {
    case GRPC_HTTP_REQUEST:
      return handle_request_line(parser);
    case GRPC_HTTP_RESPONSE:
      return handle_response_line(parser);
  }
  dassert (false, "invalid exec flow");
  return ERR_OK;
}

static ::dsn::error_code add_header(grpc_http_parser *parser) {
  uint8_t *beg = parser->cur_line;
  uint8_t *cur = beg;
  uint8_t *end = beg + parser->cur_line_length;
  size_t *hdr_count = NULL;
  grpc_http_header **hdrs = NULL;
  grpc_http_header hdr = {NULL, NULL};
  ::dsn::error_code error = ERR_OK;

  dassert (cur != end, "cur != end");

  if (*cur == ' ' || *cur == '\t') {
    derror ("Continued header lines not supported yet");
    error = ERR_INVALID_DATA;
    goto done;
  }

  while (cur != end && *cur != ':') {
    cur++;
  }
  if (cur == end) {
    derror ("Didn't find ':' in header string");
    error = ERR_INVALID_DATA;
    goto done;
  }
  dassert (cur >= beg, "");
  hdr.key = buf2str(beg, (size_t)(cur - beg));
  cur++; /* skip : */

  while (cur != end && (*cur == ' ' || *cur == '\t')) {
    cur++;
  }
  dassert ((size_t)(end - cur) >= parser->cur_line_end_length, "");
  hdr.value = buf2str(cur, (size_t)(end - cur) - parser->cur_line_end_length);

  switch (parser->type) {
    case GRPC_HTTP_RESPONSE:
      hdr_count = &parser->http.response->hdr_count;
      hdrs = &parser->http.response->hdrs;
      break;
    case GRPC_HTTP_REQUEST:
      hdr_count = &parser->http.request->hdr_count;
      hdrs = &parser->http.request->hdrs;
      break;
  }

  if (*hdr_count == parser->hdr_capacity) {
    parser->hdr_capacity =
        (std::max)(parser->hdr_capacity + 1, parser->hdr_capacity * 3 / 2);
    
    *hdrs = (grpc_http_header*)dsn_transient_realloc(*hdrs, parser->hdr_capacity * sizeof(**hdrs));
  }
  (*hdrs)[(*hdr_count)++] = hdr;

done:
  if (error != ERR_OK) {
    dsn_transient_free(hdr.key);
    dsn_transient_free(hdr.value);
  }
  return error;
}

static ::dsn::error_code finish_line(grpc_http_parser *parser,
                               bool *found_body_start) {
  ::dsn::error_code err;
  switch (parser->state) {
    case GRPC_HTTP_FIRST_LINE:
      err = handle_first_line(parser);
      if (err != ERR_OK) return err;
      parser->state = GRPC_HTTP_HEADERS;
      break;
    case GRPC_HTTP_HEADERS:
      if (parser->cur_line_length == parser->cur_line_end_length) {
        parser->state = GRPC_HTTP_BODY;
        *found_body_start = true;
        break;
      }
      err = add_header(parser);
      if (err != ERR_OK) {
        return err;
      }
      break;
    case GRPC_HTTP_BODY:
      dassert (false, "invalid exec flow");
      return ERR_OK;
  }

  parser->cur_line_length = 0;
  return ERR_OK;
}

static ::dsn::error_code addbyte_body(grpc_http_parser *parser, uint8_t byte) {
  size_t *body_length = NULL;
  char **body = NULL;

  if (parser->type == GRPC_HTTP_RESPONSE) {
    body_length = &parser->http.response->body_length;
    body = &parser->http.response->body;
  } else if (parser->type == GRPC_HTTP_REQUEST) {
    body_length = &parser->http.request->body_length;
    body = &parser->http.request->body;
  } else {
    dassert (false, "invalid exec flow");
      return ERR_OK;
  }

  if (*body_length == parser->body_capacity) {
    parser->body_capacity = DSN_MAX(8, parser->body_capacity * 3 / 2);
    *body = (char*)dsn_transient_realloc((void *)*body, parser->body_capacity);
  }
  (*body)[*body_length] = (char)byte;
  (*body_length)++;

  return ERR_OK;
}

static bool check_line(grpc_http_parser *parser) {
  if (parser->cur_line_length >= 2 &&
      parser->cur_line[parser->cur_line_length - 2] == '\r' &&
      parser->cur_line[parser->cur_line_length - 1] == '\n') {
    return true;
  }

  // HTTP request with \n\r line termiantors.
  else if (parser->cur_line_length >= 2 &&
           parser->cur_line[parser->cur_line_length - 2] == '\n' &&
           parser->cur_line[parser->cur_line_length - 1] == '\r') {
    return true;
  }

  // HTTP request with only \n line terminators.
  else if (parser->cur_line_length >= 1 &&
           parser->cur_line[parser->cur_line_length - 1] == '\n') {
    parser->cur_line_end_length = 1;
    return true;
  }

  return false;
}

static ::dsn::error_code addbyte(grpc_http_parser *parser, uint8_t byte,
                           bool *found_body_start) {
  switch (parser->state) {
    case GRPC_HTTP_FIRST_LINE:
    case GRPC_HTTP_HEADERS:
      if (parser->cur_line_length >= GRPC_HTTP_PARSER_MAX_HEADER_LENGTH) {
        if (grpc_http1_trace)
          derror("HTTP client max line length (%d) exceeded",
                  GRPC_HTTP_PARSER_MAX_HEADER_LENGTH);
        return ERR_OK;
      }
      parser->cur_line[parser->cur_line_length] = byte;
      parser->cur_line_length++;
      if (check_line(parser)) {
        return finish_line(parser, found_body_start);
      }
      return ERR_OK;
    case GRPC_HTTP_BODY:
      return addbyte_body(parser, byte);
  }

  dassert (false, "invalid exec flow");
  return ERR_OK;
}

void grpc_http_parser_init(grpc_http_parser *parser, grpc_http_type type,
                           void *request_or_response) {
  memset(parser, 0, sizeof(*parser));
  parser->state = GRPC_HTTP_FIRST_LINE;
  parser->type = type;
  parser->http.request_or_response = request_or_response;
  parser->cur_line_end_length = 2;
}

void grpc_http_parser_destroy(grpc_http_parser *parser) {}

void grpc_http_request_destroy(grpc_http_request *request) {
  size_t i;
  dsn_transient_free(request->body);
  for (i = 0; i < request->hdr_count; i++) {
    dsn_transient_free(request->hdrs[i].key);
    dsn_transient_free(request->hdrs[i].value);
  }
  dsn_transient_free(request->hdrs);
  dsn_transient_free(request->method);
  dsn_transient_free(request->path);
}

void grpc_http_response_destroy(grpc_http_response *response) {
  size_t i;
  dsn_transient_free(response->body);
  for (i = 0; i < response->hdr_count; i++) {
    dsn_transient_free(response->hdrs[i].key);
    dsn_transient_free(response->hdrs[i].value);
  }
  dsn_transient_free(response->hdrs);
}

::dsn::error_code grpc_http_parser_parse(grpc_http_parser *parser, const char* ptr, size_t sz,
                                   size_t *start_of_body) {
  for (size_t i = 0; i < sz; i++) 
  {
    bool found_body_start = false;
    ::dsn::error_code err =
        addbyte(parser, ptr[i], &found_body_start);
    if (err != ERR_OK) return err;
    if (found_body_start && start_of_body != NULL) *start_of_body = i + 1;
  }
  return ERR_OK;
}

::dsn::error_code grpc_http_parser_eof(grpc_http_parser *parser) {
  if (parser->state != GRPC_HTTP_BODY) {
    derror ("Did not finish header");
    return ERR_INVALID_DATA;
  }
  return ERR_OK;
}
