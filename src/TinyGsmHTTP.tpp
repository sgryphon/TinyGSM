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

#ifndef TINY_GSM_HTTP_HEADER_BUFFER
#define TINY_GSM_HTTP_HEADER_BUFFER 500
#endif

#ifndef TINY_GSM_HTTP_DATA_BUFFER
#define TINY_GSM_HTTP_DATA_BUFFER 2000
#endif

#ifndef GSM_HTTP_RESPONSE_WAIT
#define GSM_HTTP_RESPONSE_WAIT 500
#endif

#ifndef GSM_HTTP_RESPONSE_TIMEOUT
#define GSM_HTTP_RESPONSE_TIMEOUT 30000
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
      return startRequest(url_path, GSM_HTTP_METHOD_GET);
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
      resetState();
      return startRequestImpl(url_path, http_method, content_type, content_length, body);
    }

    virtual void stop() {
      stopImpl();
    }

   protected:
    virtual void resetState() {
      this->response_status_code = 0;
      this->headers[0] = '\0';
      this->data[0] = '\0';
      this->is_completed = false;
    }

    virtual String responseBodyImpl() {
      unsigned long timeout_end = millis() + GSM_HTTP_RESPONSE_TIMEOUT;
      while (!is_completed) {

        if (millis() > timeout_end) { return String((const char*)NULL); }
        at->waitResponse(GSM_HTTP_RESPONSE_WAIT);
      }
      return String(this->data);
    }

    virtual int responseStatusCodeImpl() {
      unsigned long timeout_end = millis() + GSM_HTTP_RESPONSE_TIMEOUT;
      while (response_status_code == 0) {
        if (millis() > timeout_end) { return GSM_HTTP_ERROR_TIMED_OUT; }
        at->waitResponse(GSM_HTTP_RESPONSE_WAIT);
      }
      return response_status_code;
    }

    virtual int startRequestImpl(const char* url_path,
                     const char* http_method,
                     const char* content_type = NULL,
                     int content_length = -1,
                     const byte body[] = NULL) TINY_GSM_ATTR_NOT_IMPLEMENTED;

    virtual void stopImpl() {}

    modemType* at = nullptr;
    char data[TINY_GSM_HTTP_DATA_BUFFER] = { 0 };
    char headers[TINY_GSM_HTTP_HEADER_BUFFER] = { 0 };
    bool is_completed = false;
    bool is_connected = false;
    int8_t http_client_id = -1;
    int16_t response_status_code = 0;
    UrlScheme scheme = SCHEME_UNKNOWN;
    const char *server_name = { 0 }; 
    uint16_t server_port = 0;
  };
};

#endif  // SRC_TINYGSMHTTP_H_
