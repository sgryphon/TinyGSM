/**
 * @file       TinyGsmBattery.tpp
 * @author     Volodymyr Shymanskyy
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       Nov 2016
 */

#ifndef SRC_TINYGSMHTTP_H_
#define SRC_TINYGSMHTTP_H_

#include "TinyGsmCommon.h"

#define TINY_GSM_MODEM_HAS_HTTP

#ifndef HTTP_AUTHORITY_BUFFER
#define HTTP_AUTHORITY_BUFFER 255
#endif

#define GSM_HTTP_METHOD_GET "GET"
#define GSM_HTTP_METHOD_POST   "POST"
#define GSM_HTTP_METHOD_PUT    "PUT"
#define GSM_HTTP_METHOD_PATCH  "PATCH"
#define GSM_HTTP_METHOD_DELETE "DELETE"

static const int GSM_HTTP_SUCCESS = 0;
static const int GSM_HTTP_ERROR_CONNECTION_FAILED = -1;
static const int GSM_HTTP_ERROR_API = -2;
static const int GSM_HTTP_ERROR_TIMED_OUT = -3;
static const int GSM_HTTP_ERROR_INVALID_RESPONSE = -4;

enum UrlScheme {
  SCHEME_UNKNOWN = 0,
  SCHEME_HTTP = 1,
  SCHEME_HTTPS = 2,
};

const char* PREFIX_HTTP = "http://";
const int8_t PREFIX_HTTP_LENGTH = 7;
const char* PREFIX_HTTPS = "https://";
const int8_t PREFIX_HTTPS_LENGTH = 8;

template <class modemType, uint8_t muxCount>
class TinyGsmHTTP {
 public:
  /*
   * CRTP Helper
   */
 protected:
  inline const modemType& thisModem() const {
    return static_cast<const modemType&>(*this);
  }
  inline modemType& thisModem() {
    return static_cast<modemType&>(*this);
  }

  /*
   * Inner Client
   */
 public:
  class GsmHttpClient {
    // Make all classes created from the modem template friends
    friend class TinyGsmHTTP<modemType, muxCount>;

   /*
    * HTTP functions compatible with ArduinoHttpClient
    */
   public:
    /** Connect to the server and start to send a GET request.
      @param url_path     Url to request
      @return 0 if successful, else error
    */
    int get(const char* url_path) {
      return startRequest(GSM_HTTP_METHOD_GET, url_path);
    }

    /** Return the response body as a String
      Also skips response headers if they have not been read already
      MUST be called after responseStatusCode()
      @return response body of request as a String
    */
    String responseBody() {
      return responseBodyImpl();
    }
    
    /** Get the HTTP status code contained in the response.
      For example, 200 for successful request, 404 for file not found, etc.
    */
    int responseStatusCode() {
      return responseStatusCodeImpl();
    }

    /** Connect to the server and start to send the request.
        If a body is provided, the entire request (including headers and body) will be sent
      @param url_path        Url to request
      @param http_method     Type of HTTP request to make, e.g. "GET", "POST", etc.
      @param content_type    Content type of request body (optional)
      @param content_length  Length of request body (optional)
      @param body           Body of request (optional)
      @return 0 if successful, else error
    */
    int startRequest(const char* url_path,
                     const char* http_method,
                     const char* content_type = NULL,
                     int content_length = -1,
                     const byte body[] = NULL) {
      return startRequestImpl(url_path, http_method, content_type, content_length, body);
    }

   protected:
    String responseBodyImpl() TINY_GSM_ATTR_NOT_IMPLEMENTED;
    int responseStatusCodeImpl() TINY_GSM_ATTR_NOT_IMPLEMENTED;
    int startRequestImpl(const char* url_path,
                     const char* http_method,
                     const char* content_type = NULL,
                     int content_length = -1,
                     const byte body[] = NULL) TINY_GSM_ATTR_NOT_IMPLEMENTED;

    modemType* at = nullptr;
    const char *server_name = { 0 }; 
    bool is_connected = false;
    uint8_t mux = -1;
    UrlScheme scheme = SCHEME_UNKNOWN;
    uint16_t server_port = 0;
  };
};

#endif  // SRC_TINYGSMHTTP_H_
