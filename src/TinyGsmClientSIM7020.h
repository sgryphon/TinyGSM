/**
 * @file       TinyGsmClientSim7020.h
 * @author     Sly Gryphon
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy, 2022 Sly Gryphon
 * @date       Aug 2022
 */

#ifndef SRC_TINYGSMCLIENTSIM7020_H_
#define SRC_TINYGSMCLIENTSIM7020_H_

// #define TINY_GSM_DEBUG Serial
// #define TINY_GSM_USE_HEX

#define TINY_GSM_MUX_COUNT 5
#define TINY_GSM_BUFFER_READ_AND_CHECK_SIZE

#include "TinyGsmClientSIM70xx.h"
#include "TinyGsmTCP.tpp"
#include "TinyGsmSSL.tpp"
#include "TinyGsmHTTP.tpp"

class TinyGsmSim7020 : public TinyGsmSim70xx<TinyGsmSim7020>,
                       public TinyGsmTCP<TinyGsmSim7020, TINY_GSM_MUX_COUNT>,
                       public TinyGsmSSL<TinyGsmSim7020>,
                       public TinyGsmHTTP<TinyGsmSim7020, TINY_GSM_MUX_COUNT> {
  friend class TinyGsmSim70xx<TinyGsmSim7020>;
  friend class TinyGsmTCP<TinyGsmSim7020, TINY_GSM_MUX_COUNT>;
  friend class TinyGsmSSL<TinyGsmSim7020>;
  friend class TinyGsmHTTP<TinyGsmSim7020, TINY_GSM_MUX_COUNT>;

  /*
   * Inner Client
   */
 public:
  class GsmClientSim7020 : public GsmClient {
    friend class TinyGsmSim7020;

   public:
    GsmClientSim7020() {}

    explicit GsmClientSim7020(TinyGsmSim7020& modem, uint8_t mux = 0) {
      init(&modem, mux);
    }

    bool init(TinyGsmSim7020* modem, uint8_t mux = 0) {
      this->at       = modem;
      sock_available = 0;
      prev_check     = 0;
      sock_connected = false;
      got_data       = false;

      if (mux < TINY_GSM_MUX_COUNT) {
        this->mux = mux;
      } else {
        this->mux = (mux % TINY_GSM_MUX_COUNT);
      }
      at->sockets[this->mux] = this;

      return true;
    }

   public:
    virtual int connect(const char* host, uint16_t port, int timeout_s) {
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      sock_connected = at->modemConnect(host, port, mux, false, timeout_s);
      return sock_connected;
    }
    TINY_GSM_CLIENT_CONNECT_OVERRIDES

    void stop(uint32_t maxWaitMs) {
      if (socket_id > -1 && sock_connected) {
        dumpModemBuffer(maxWaitMs);
        at->sendAT(GF("+CSOCL="), socket_id);
        at->waitResponse(3000);
        sock_connected = false;
      }
    }
    void stop() override {
      stop(15000L);
    }

    /*
     * Extended API
     */

    String remoteIP() TINY_GSM_ATTR_NOT_IMPLEMENTED;

    bool is_tls = false;
    int8_t socket_id = -1;
  };

  /*
   * Inner Secure Client
   */

  class GsmClientSecureSIM7020 : public GsmClientSim7020 {
   public:
    GsmClientSecureSIM7020() {}

    explicit GsmClientSecureSIM7020(TinyGsmSim7020& modem, uint8_t mux = 0)
        : GsmClientSim7020(modem, mux) {
      is_tls = true;
    }

   public:
    bool setServerCertificate(const String& certificate) {
      return at->setServerCertificate(certificate, mux);
    }

    virtual int connect(const char* host, uint16_t port,
                        int timeout_s) override {
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      sock_connected = at->modemConnect(host, port, mux, true, timeout_s);
      return sock_connected;
    }
    TINY_GSM_CLIENT_CONNECT_OVERRIDES

    void stop(uint32_t maxWaitMs) {
      if (socket_id > -1 && sock_connected) {
        dumpModemBuffer(maxWaitMs);
        at->sendAT(GF("+CTLSCLOSE="), mux);
        sock_connected = false;
        at->waitResponse(3000);
      }
    }
    void stop() override {
      stop(15000L);
    }
  };

  /*
   * Inner HTTP Client
   */
  class GsmHttpClientSim7020 : public GsmHttpClient {
    friend class TinyGsmSim7020;

   public:
    GsmHttpClientSim7020() {}

    explicit GsmHttpClientSim7020(GsmClientSim7020& client, const char* server_name, uint16_t server_port = 80) {
      init(client.at, server_name, server_port, client.is_tls ? SCHEME_HTTPS : SCHEME_HTTP);
    }

    bool init(TinyGsmSim7020* modem, const char* server_name, uint16_t server_port = 80, UrlScheme scheme = SCHEME_HTTP) {
      this->at = modem;
      this->scheme = scheme;
      this->server_name = server_name;
      this->server_port = server_port;
      this->http_client_id = -1;

      return true;
    }

   protected:
    int startRequestImpl(const char* url_path,
                     const char* http_method,
                     const char* content_type = NULL,
                     int content_length = -1,
                     const byte body[] = NULL) override {
      DBG(GF("### HTTP"), http_method, url_path);

      // Connect if needed
      if (!is_connected) {
        // Create if needed
        if (this->http_client_id == -1) {
          DBG(GF("### HTTP creating"));

          // Create
          if (scheme == SCHEME_HTTP) {
            at->sendAT(GF("+CHTTPCREATE=\""), PREFIX_HTTP, server_name, ':', server_port, "/\"");
          } else if (scheme == SCHEME_HTTPS) {
            // TODO: with cert
          } else {
            return GSM_HTTP_ERROR_API;
          }

          if (at->waitResponse(30000, GF(GSM_NL "+CHTTPCREATE:")) != 1) { 
            at->sendAT(GF("+CHTTPCREATE?"));
            at->waitResponse();
            return GSM_HTTP_ERROR_API;
          }
          int8_t http_client_id = at->streamGetIntBefore('\n');
          DBG(GF("### HTTP client created:"), http_client_id);
          if (at->waitResponse() != 1) { return GSM_HTTP_ERROR_API; }

          // Store the connection
          this->http_client_id = http_client_id;
          at->http_clients[this->http_client_id] = this;
        }

        DBG(GF("### HTTP connecting"), http_client_id);

        // Connect
        at->sendAT(GF("+CHTTPCON="), this->http_client_id);
        if (at->waitResponse(30000) != 1) { 
          return GSM_HTTP_ERROR_CONNECTION_FAILED;
        }

        is_connected = true;
      }

      // Send the request
      if (strcmp(http_method, GSM_HTTP_METHOD_GET) == 0) {
        at->sendAT(GF("+CHTTPSEND="), this->http_client_id, ",0,", url_path);
        if (at->waitResponse(5000) != 1) { return GSM_HTTP_ERROR_INVALID_RESPONSE; }
      // } else if (strcmp(http_method, GSM_HTTP_METHOD_POST) == 0) {

      // } else if (strcmp(http_method, GSM_HTTP_METHOD_PUT) == 0) {

      // } else if (strcmp(http_method, GSM_HTTP_METHOD_DELETE) == 0) {

      } else {
        return GSM_HTTP_ERROR_API;
      }

      return GSM_HTTP_SUCCESS;
    }

    void stopImpl() override {
      if (this->http_client_id > -1) {
        at->sendAT(GF("+CHTTPDISCON="), this->http_client_id);
        at->waitResponse(30000);
        at->sendAT(GF("+CHTTPDESTROY="), this->http_client_id);
        at->waitResponse(30000);
      }
    }

    /*
     * Extended API
     */

  };

  /*
   * Constructor
   */
 public:
  explicit TinyGsmSim7020(Stream& stream)
      : TinyGsmSim70xx<TinyGsmSim7020>(stream),
        certificates() {
    memset(sockets, 0, sizeof(sockets));
  }

  /*
   * Certificate setup
   */
  bool setRootCA(const String& certificate) {
    return setCertificate(0, certificate.c_str());
  }

  bool setClientCA(const String& certificate) {
    return setCertificate(1, certificate.c_str());
  }

  bool setClientPrivateKey(const String& certificate) {
    return setCertificate(2, certificate.c_str());
  }

  /*
   * Basic functions
   */
 protected:
  bool initImpl(const char* pin = NULL) {
    DBG(GF("### TinyGSM Version:"), TINYGSM_VERSION);
    DBG(GF("### TinyGSM Compiled Module:  TinyGsmClientSIM7020"));

    if (!testAT()) { return false; }

    sendAT(GF("E0"));  // Echo Off
    if (waitResponse() != 1) { return false; }

#ifdef TINY_GSM_DEBUG
    sendAT(GF("+CMEE=2"));  // turn on verbose error codes
#else
    sendAT(GF("+CMEE=0"));  // turn off error codes
#endif
    waitResponse();

    DBG(GF("### Modem:"), getModemName());

    // Enable Local Time Stamp for getting network time
    sendAT(GF("+CLTS=1"));
    if (waitResponse(10000L) != 1) { return false; }

    // Enable battery checks
    sendAT(GF("+CBATCHK=1"));
    if (waitResponse() != 1) { return false; }

    // Set IPv6 format
    sendAT(GF("+CGPIAF=1,1,0,1"));
    if (waitResponse() != 1) { return false; }

    SimStatus ret = getSimStatus();
    // if the sim isn't ready and a pin has been provided, try to unlock the sim
    if (ret != SIM_READY && pin != NULL && strlen(pin) > 0) {
      simUnlock(pin);
      return (getSimStatus() == SIM_READY);
    } else {
      // if the sim is ready, or it's locked but no pin has been provided,
      // return true
      return (ret == SIM_READY || ret == SIM_LOCKED);
    }
  }

  void maintainImpl() {
    // Keep listening for modem URC's and proactively iterate through
    // sockets asking if any data is avaiable
    bool check_socks = false;
    for (int mux = 0; mux < TINY_GSM_MUX_COUNT; mux++) {
      GsmClientSim7020* sock = sockets[mux];
      if (sock && sock->got_data) {
        sock->got_data = false;
        check_socks    = true;
      }
    }
    // modemGetAvailable checks all socks, so we only want to do it once
    // modemGetAvailable calls modemGetConnected(), which also checks allf
    if (check_socks) { modemGetAvailable(0); }
    while (stream.available()) { waitResponse(15, NULL, NULL); }
  }

  /*
   * Power functions
   */
 protected:
  // Follows the SIM70xx template

  /*
   * Generic network functions
   */
 protected:
  String getLocalIPImpl() {
    // Returns the IPv4 address first, then the IPv6 suffix
    // Alternative is +IPCONFIG, showing resolved IPv6 addresses
    // Or AT+CGCONTRDP with the display format controlled by AT+CGPIAF? (with IPv6 suffix)
    sendAT(GF("+CGPADDR=1"));
    if (waitResponse(GF(GSM_NL "+CGPADDR:")) != 1) { return ""; }
    streamSkipUntil('\"');
    String res = stream.readStringUntil('\"');
    waitResponse();
    return res;
  }

  /*
   * Secure socket layer functions
   */
 protected:
  bool setServerCertificate(const String& certificate, const uint8_t mux = 0) {
    if (mux >= TINY_GSM_MUX_COUNT) return false;
    certificates[mux] = certificate;
    return true;
  }

  bool setCertificate(int8_t type, const char *certificate, int8_t mux = -1)
  {
    /*  type 0 : Root CA
        type 1 : Client CA
        type 2 : Client Private Key
    */
    if (certificate == NULL) {
        return false;
    }

    int16_t length = strlen_P(certificate);
    int16_t count_escaped = 0;
    for (int16_t i = 0; i < length; i++) {
      if (certificate[i] == '\r' || certificate[i] == '\n') {
        count_escaped++;
      }
    }
    int16_t total_length = length + count_escaped;
    int8_t is_more = 1;
    int16_t index = 0;
    int16_t chunk_end = 0;
    char c = '\0';

    while (index < length) {
      chunk_end += 1000;
      if (chunk_end >= length) {
        chunk_end = length;
        is_more = 0;
      }

      if (mux == -1) {
        streamWrite(GF("AT+CSETCA="));
        streamWrite(type);
        streamWrite(',');
        streamWrite(total_length);
        streamWrite(',');
        streamWrite(is_more);
        streamWrite(",0,\"");
      } else {
        int8_t mux_type = 6 + type;
        streamWrite(GF("AT+CTLSCFG="));
        streamWrite(mux);
        streamWrite(',');
        streamWrite(mux_type);
        streamWrite(',');
        streamWrite(total_length);
        streamWrite(',');
        streamWrite(is_more);
        streamWrite(",\"");
      }
      while (index < chunk_end) {
        c = certificate[index];
        if (c == '\r') {
          streamWrite("\\r");
        } if (c == '\n') {
          streamWrite("\\n");
        } else {
          streamWrite(c);
        }
        index++;
      }
      streamWrite("\"" GSM_NL);
      if (waitResponse() != 1) { return false; }
    }
    return true;
  }

  /*
   * GPRS functions
   */
 protected:
  bool gprsConnectImpl(const char* apn, const char* user = NULL,
                       const char* pwd = NULL) {
    gprsDisconnect();

    // Based on "APN Manual Configuration", from SIM7020 TCPIP Application Note

    sendAT(GF("+CFUN=0"));
    waitResponse();

    // Set Default PSD Connection Settings
    // Set the user name and password
    // AT*MCGDEFCONT=<PDP_type>[,<APN>[,<username>[,<password>]]]
    // <pdp_type> IPV4V6: Dual PDN Stack
    //            IPV6: Internet Protocol Version 6
    //            IP: Internet Protocol Version 4
    //            Non-IP: external packet data network
    bool res = false;
    if (pwd && strlen(pwd) > 0 && user && strlen(user) > 0) {
      sendAT(GF("*MCGDEFCONT=\"IPV4V6\",\""), apn, "\",\"", user, "\",\"", pwd, '"');
      waitResponse();
    } else if (user && strlen(user) > 0) {
      // Set the user name only
      sendAT(GF("*MCGDEFCONT=\"IPV4V6\",\""), apn, "\",\"", user, '"');
      waitResponse();
    } else {
      // Set the APN only
      sendAT(GF("*MCGDEFCONT=\"IPV4V6\",\""), apn, '"');
      waitResponse();
    }

    sendAT(GF("+CFUN=1"));
    res = waitResponse(20000L, GF(GSM_NL "+CPIN: READY"));
    if (res != 1) { return res; }
    waitResponse();

    //sendAT(GF("+CGREG?"));
    //res = waitResponse(20000L, GF(GSM_NL "+CGREG: 0,1"));
    //waitResponse();
    res = waitForNetwork(60000, true);
    delay(100);

    return res;
  }

  bool gprsDisconnectImpl() {
    // Shut down the general application TCP/IP connection
    //sendAT(GF("+CGACT=0,0"));
    //if (waitResponse(60000L) != 1) { return false; }

    sendAT(GF("+CGATT=0"));  // Deactivate the bearer context
    if (waitResponse(60000L) != 1) { return false; }

    return true;
  }

  /*
   * SIM card functions
   */
 protected:
  // Follows the SIM70xx template

  /*
   * Messaging functions
   */
 protected:
  // Follows all messaging functions per template

  /*
   * GPS/GNSS/GLONASS location functions
   */
 protected:
  // Follows the SIM70xx template

  /*
   * Time functions
   */
  // Can follow CCLK as per template

  /*
   * NTP server functions
   */
  // Can sync with server using CNTP as per template

  /*
   * Battery functions
   */
 protected:
  // Follows all battery functions per template

  /*
   * Client related functions
   */
 protected:
  bool modemConnect(const char* host, uint16_t port, uint8_t mux,
                    bool ssl = false, int timeout_s = 75) {
    if (ssl) {
      return tlsModemConnect(host, port, mux, timeout_s);
    } else {
      return insecureModemConnect(host, port, mux, timeout_s);
    }
  }

  int16_t modemSend(const void* buff, size_t len, uint8_t mux) {
    if (!sockets[mux]) { return 0; }
    if (!sockets[mux]->sock_connected) { return 0; }

    if (sockets[mux]->is_tls) {
      return tlsModemSend(buff, len, mux);
    } else {
      return insecureModemSend(buff, len, mux);
    }
  }

  size_t modemRead(size_t size, uint8_t mux) {
    if (!sockets[mux]) { return 0; }
    if (!sockets[mux]->sock_connected) { return 0; }

    if (sockets[mux]->is_tls) {
      return tlsModemRead(size, mux);
    } else {
      return insecureModemRead(size, mux);
    }
  }

  size_t modemGetAvailable(uint8_t mux) {
    // If the socket doesn't exist, just return
    if (!sockets[mux]) { return 0; }
    if (!sockets[mux]->sock_connected) {
      sockets[mux]->sock_available = 0;
    } else {
      // Query data available
      sendAT(GF("+CSORXGET=4,"), sockets[mux]->socket_id);

      streamSkipUntil(',');  // skip the mode
      streamSkipUntil(',');  // skip the response socket id
      int16_t len_confirmed = streamGetIntBefore('\n');
      waitResponse();

      sockets[mux]->sock_available = len_confirmed;
    }
    return sockets[mux]->sock_available;
  }

  bool modemGetConnected(uint8_t mux) {
    sendAT(GF("+CSOSTATUS="), sockets[mux]->socket_id);
    int8_t res = waitResponse(3000, GF("+CSOSTATUS:"));
    if (res == 1) {
      streamSkipUntil(',');  // skip the ID
      int8_t state = streamGetIntBefore('\n');
      sockets[mux]->sock_connected = (state == 2);
    } else {
      sockets[mux]->sock_connected = false;
    } 
    return sockets[mux]->sock_connected;
  }

  /*
   * Utilities
   */
 public:
  // TODO(vshymanskyy): Optimize this!
  int8_t waitResponse(uint32_t timeout_ms, String& data,
                      GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL) {
    /*String r1s(r1); r1s.trim();
    String r2s(r2); r2s.trim();
    String r3s(r3); r3s.trim();
    String r4s(r4); r4s.trim();
    String r5s(r5); r5s.trim();
    DBG("### ..:", r1s, ",", r2s, ",", r3s, ",", r4s, ",", r5s);*/
    data.reserve(64);
    uint8_t  index       = 0;
    uint32_t startMillis = millis();
    do {
      TINY_GSM_YIELD();
      while (stream.available() > 0) {
        TINY_GSM_YIELD();
        int8_t a = stream.read();
        if (a <= 0) continue;  // Skip 0x00 bytes, just in case
        data += static_cast<char>(a);
        if (r1 && data.endsWith(r1)) {
          index = 1;
          goto finish;
        } else if (r2 && data.endsWith(r2)) {
          index = 2;
          goto finish;
        } else if (r3 && data.endsWith(r3)) {
#if defined TINY_GSM_DEBUG
          if (r3 == GFP(GSM_CME_ERROR)) {
            streamSkipUntil('\n');  // Read out the error
          }
#endif
          index = 3;
          goto finish;
        } else if (r4 && data.endsWith(r4)) {
          index = 4;
          goto finish;
        } else if (r5 && data.endsWith(r5)) {
          index = 5;
          goto finish;
          /*
        } else if (data.endsWith(GF("+CARECV:"))) {
          int8_t  mux = streamGetIntBefore(',');
          int16_t len = streamGetIntBefore('\n');
          if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
            sockets[mux]->got_data = true;
            if (len >= 0 && len <= 1024) { sockets[mux]->sock_available = len; }
          }
          data = "";
          DBG("### Got Data:", len, "on", mux);
          */
        } else if (data.endsWith(GF("+CSONMI:"))) {
          int8_t socket_id = streamGetIntBefore(',');
          int16_t data_length = streamGetIntBefore(',');
          int8_t mux = getMuxFromSocketId(socket_id);
          if (mux >= 0) {
            sockets[mux]->got_data = true;
          }
          for (int i = 0; i < data_length; i++) {
            uint32_t startMillis = millis();
            while (!stream.available() &&
                  (millis() - startMillis < sockets[mux]->_timeout)) {
              TINY_GSM_YIELD();
            }
            char c = stream.read();
            if (mux >= 0) {
              sockets[mux]->rx.put(c);
            }
          }
          DBG("### Got Data:", data_length, "on", mux);
          data = "";
        } else if (data.endsWith(GF("+CSOERR:"))) {
          int8_t socket_id = streamGetIntBefore(',');
          int8_t error_code = streamGetIntBefore('\n');
          int8_t mux = getMuxFromSocketId(socket_id);
          sockets[mux]->sock_connected = false;
          // <error code> 4 = disconnected (expected automatic disconnection)
          if (mux >= 0 && error_code != 4) {
            DBG("### Closed: ", mux, "error", error_code);
          }
          data = "";
        } else if (data.endsWith(GF("+CHTTPNMIH:"))) {
          int8_t mux = streamGetIntBefore(',');
          GsmHttpClientSim7020 *http_client = http_clients[mux];
          int16_t response_code = streamGetIntBefore(',');
          http_client->response_status_code = response_code;
          int16_t header_length = streamGetIntBefore(',');
          if (header_length > 0) {
            for (int i = 0; i < header_length; i++) {
              uint32_t startMillis = millis();
              while (!stream.available() &&
                    (millis() - startMillis < 1000)) {
                TINY_GSM_YIELD();
              }
              char c = stream.read();
              if (http_client) {
                http_client->headers[i] = c;
              }
            }
            http_client->headers[header_length] = '\0';
          }
          DBG("### HTTP got response", response_code, "length", header_length, "on", mux);
          data = "";
        } else if (data.endsWith(GF("+CHTTPNMIC:"))) {
          int8_t mux = streamGetIntBefore(',');
          GsmHttpClientSim7020 *http_client = http_clients[mux];
          int16_t more_flag = streamGetIntBefore(',');
          int16_t content_length = streamGetIntBefore(',');
          int16_t package_length = streamGetIntBefore(',');
          if (package_length > 0) {
            int16_t previous_data_length = strlen(http_client->data);
            char hex[3] = { 0, 0, 0 };
            DBG("### HTTP reading hex", previous_data_length, "to", previous_data_length + package_length);
            for (int i = previous_data_length; i < previous_data_length + package_length; i++) {
              uint32_t startMillis = millis();
              while (!stream.available() &&
                    (millis() - startMillis < 1000)) {
                TINY_GSM_YIELD();
              }
              hex[0] = stream.read();
              hex[1] = stream.read();
              if (http_client) {
                http_client->data[i] = strtol(hex, NULL, 16);
              }
            }
            http_client->data[previous_data_length + package_length] = '\0';
          }
          if (more_flag == 0) { 
            http_client->is_completed = true;
          }
          DBG("### HTTP got content", more_flag, "length", package_length, "on", mux);
          data = "";
        } else if (data.endsWith(GF("+CHTTPERR:"))) {
          int8_t mux = streamGetIntBefore(',');
          int8_t error_code = streamGetIntBefore('\n');
          GsmHttpClientSim7020 *http_client = http_clients[mux];
          http_client->is_connected = false;
          if (mux >= 0) {
            if (error_code == -2) {
              // <error code> -2 = closed by remote host (expected automatic disconnection)
              DBG("### HTTP closed: ", mux);
            } else {
              DBG("### HTTP close: ", mux, "error", error_code);
            }
          }
          data = "";
        } else if (data.endsWith(GF("+CLTS:"))) {
          streamSkipUntil('\n');  // Refresh time and time zone by network
          data = "";
          DBG("### Unsolicited local timestamp.");
        } else if (data.endsWith(GF("+CTZV:"))) {
          streamSkipUntil('\n');  // Refresh network time zone by network
          data = "";
          DBG("### Unsolicited time zone updated.");
        } else if (data.endsWith(GF(GSM_NL "SMS Ready" GSM_NL))) {
          data = "";
          DBG("### Unexpected module reset!");
          init();
          data = "";
        }
      }
    } while (millis() - startMillis < timeout_ms);
  finish:
    if (!index) {
      data.trim();
      if (data.length()) { DBG("### Unhandled:", data); }
      data = "";
    }
    // data.replace(GSM_NL, "/");
    // DBG('<', index, '>', data);
    return index;
  }

  int8_t waitResponse(uint32_t timeout_ms, GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL) {
    String data;
    return waitResponse(timeout_ms, data, r1, r2, r3, r4, r5);
  }

  int8_t waitResponse(GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL) {
    return waitResponse(1000, r1, r2, r3, r4, r5);
  }

 protected:
  String certificates[TINY_GSM_MUX_COUNT];
  GsmHttpClientSim7020* http_clients[TINY_GSM_MUX_COUNT];
  GsmClientSim7020* sockets[TINY_GSM_MUX_COUNT];

  int8_t getMuxFromSocketId(int8_t socket_id) {
    for (int muxNo = 0; muxNo < TINY_GSM_MUX_COUNT; muxNo++) {
      if (sockets[muxNo] && sockets[muxNo]->socket_id == socket_id) {
        return muxNo;
      }
    }
    return -1;
  }

  /*
   * Non-TLS implementation
   */

  bool insecureModemConnect(const char* host, uint16_t port, uint8_t mux,
                    int timeout_s = 75) {
    DBG("### Insecure modem connect", host, ":", port);

    uint32_t timeout_ms = ((uint32_t)timeout_s) * 1000;

    IPAddress address;
    if (!address.fromString(host)) {
      sendAT(GF("+CDNSGIP=\""), host, '"');
      if (waitResponse(30000, GF(GSM_NL "+CDNSGIP:")) != 1) { return false; }
      int8_t dns_success = streamGetIntBefore(',');
      if (dns_success != 1) { return false; }
      streamSkipUntil(','); // skip domain name
      String dns_results = stream.readStringUntil('\n');
      int16_t first_quote = dns_results.indexOf('"');
      int16_t second_quote = dns_results.indexOf('"', first_quote + 1);
      String dns_ip = dns_results.substring(first_quote + 1, second_quote - 1);
      DBG("### Resolved DNS IP <", dns_ip, ">");
      if (!address.fromString(dns_ip)) { return false; }
    }

    // Create a TCP/UDP Socket
    // AT+CSOC=<domain>,<type>,<protocol>[,<cid>]
    // <domain> 1: IPv4, 2: IPv6
    // <type> 1: TCP, 2: UDP, 3: RAW
    // <protocol> 1: IP, 2: ICMP
    sendAT(GF("+CSOC=1,1,1"));
    if (waitResponse(GF(GSM_NL "+CSOC:")) != 1) { return false; }
    // returns <socket id> range 0-4
    int8_t socket_id = streamGetIntBefore('\n');
    waitResponse();
    sockets[mux]->socket_id = socket_id;
    sockets[mux]->is_tls = false;

    // Connect Socket to Remote Address and Port
    // AT+CSOCON=<socket_id>,<remote_port>,<remote_address>
    sendAT(GF("+CSOCON="), sockets[mux]->socket_id, ",", port, ",\"", address, '"');
    int8_t res = waitResponse();
    if (res != 1) { return false; }

    // Enable Get Data from Network Manually
    //sendAT(GF("+CSORXGET=1,"), sockets[mux]->socket_id);
    //sendAT(GF("+CSORXGET=1"));
    //if (waitResponse(GF("+CSORXGET:")) != 1) { return 0; }

#if TINY_GSM_USE_HEX
    sendAT(GF("+CSORCVFLAG=0"));
#else
    sendAT(GF("+CSORCVFLAG=1"));
#endif
    res = waitResponse();

    return res == 1;
  }

  int16_t insecureModemSend(const void* buffer, size_t length, uint8_t mux) {
#if TINY_GSM_USE_HEX
    // TODO: hex send
#else
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer);
    int16_t count_escaped = 0;
    for (int16_t i = 0; i < length; i++) {
      if (data[i] == '\r' || data[i] == '\n') {
        count_escaped++;
      }
    }
    int16_t total_length = length + count_escaped;

    // Send Data to Remote Via Socket With Data Mode
    sendAT(GF("+CSODSEND="), sockets[mux]->socket_id, ',', total_length);
    if (waitResponse(GF(">")) != 1) { return 0; }

    char c = 0;
    for (int16_t i = 0; i < length; i++) {
      c = data[i];
      if (c == '\r') {
        stream.write("\\r");
      } if (c == '\n') {
        stream.write("\\n");
      } else {
        stream.write(c);
      }
    }
    stream.flush();

    // DATA ACCEPT after posting data      
    if (waitResponse(1000, GF(GSM_NL "DATA ACCEPT:")) != 1) { return 0; }
    int8_t accepted = streamGetIntBefore('\n');
    if (accepted != total_length) {
      DBG("### Mismatch accepted data", accepted, "total length", total_length);
    }
    waitResponse();
#endif
    return length;
  }

  size_t insecureModemRead(size_t size, uint8_t mux) {
    if (!sockets[mux]) { return 0; }
    if (!sockets[mux]->sock_connected) { return 0; }

    int16_t len_confirmed = 0;
    if (size > 1460) {
      size = 1460;
    }
    // Get Data
    sendAT(GF("+CSORXGET=2,"), sockets[mux]->socket_id, ',', (uint16_t)size);
    if (waitResponse(GF("+CSORXGET:")) != 1) { return 0; }

    streamSkipUntil(',');  // skip the mode
    streamSkipUntil(',');  // skip the response socket id
    streamSkipUntil(',');  // skip the response requested length
    len_confirmed = streamGetIntBefore(',');

    if (len_confirmed <= 0) {
      sockets[mux]->sock_available = modemGetAvailable(mux);
      return 0;
    }

#if TINY_GSM_USE_HEX
      // TODO: hex receive
#else
    for (int i = 0; i < len_confirmed; i++) {
      uint32_t startMillis = millis();
      while (!stream.available() &&
            (millis() - startMillis < sockets[mux]->_timeout)) {
        TINY_GSM_YIELD();
      }
      char c = stream.read();
      sockets[mux]->rx.put(c);
    }
    // make sure the sock available number is accurate again
    sockets[mux]->sock_available = modemGetAvailable(mux);
    return len_confirmed;
#endif
  }

  /*
   * TLS implementation
   */

  bool tlsModemConnect(const char* host, uint16_t port, uint8_t mux,
                    int timeout_s = 75) {
    DBG("### TLS modem connect", host, ":", port);
    uint32_t timeout_ms = ((uint32_t)timeout_s) * 1000;

    // Configure TLS Parameters
    // AT+CTLSCFG=<tid>,<type>,<value>[,<type>,<value>...
    // <tid> TLS identifier (1-6)
    // <type> 1: Server name
    //        2: Port (default 443)
    //        3: Socket type (must be 0 = tcp)
    //        4: Auth_mode (default 2)
    //        5: Debug level
    //        6: Server CA (<size>,<more>,<certficiate>)
    //        7: Client certificate
    //        8: Client private key
    sendAT(GF("+CTLSCFG="), mux, ",1,\"", host, "\",2,", port, ",3,0,4,2,5,2");
    if (waitResponse(5000L) != 1) return false;

    if (certificates[mux] && certificates[mux] != "") {
      if (!setCertificate(0, certificates[mux].c_str(), mux)) { return false; }
    }

    sockets[mux]->socket_id = -1;
    sockets[mux]->is_tls = true;

    sendAT(GF("+CTLSCONN="), mux, ",1");
    // Sends OK
    if (waitResponse() != 1) { return false; }
    // Then the connection status
    if (waitResponse(120000L, GF(GSM_NL "+CTLSCONN:")) != 1) { return false; }
    streamSkipUntil(',');
    int32_t res = stream.parseInt();
    streamSkipUntil('\n');
    if (res != 1) {
      DBG("### TLS connection error", res);
    }
    return res == 1;
  }

  int16_t tlsModemSend(const void* buff, size_t len, uint8_t mux) {
    // Send Data
    sendAT(GF("+CTLSSEND="), sockets[mux]->socket_id, ',', (uint16_t)len, ",\"", (const char*)buff, '"');
    if (waitResponse(GF(GSM_NL "+CTLSSEND:")) != 1) { return 0; }
    waitResponse();
    return len;
  }

  size_t tlsModemRead(size_t size, uint8_t mux) {
    if (!sockets[mux]) { return 0; }
    if (!sockets[mux]->sock_connected) { return 0; }
    int16_t len_confirmed = 0;

    if (size > 1024) {
      size = 1024;
    }
    // Send Data
    sendAT(GF("+CTLSRECV="), mux, ',', (uint16_t)size);
    if (waitResponse(GF(GSM_NL "+CTLSRECV:")) != 1) { return 0; }

    streamSkipUntil(',');  // skip the response mux
    len_confirmed = streamGetIntBefore(',');
    streamSkipUntil('"');  // skip the open quote

    if (len_confirmed <= 0) {
      sockets[mux]->sock_available = modemGetAvailable(mux);
      return 0;
    }

    for (int i = 0; i < len_confirmed; i++) {
      uint32_t startMillis = millis();
      while (!stream.available() &&
            (millis() - startMillis < sockets[mux]->_timeout)) {
        TINY_GSM_YIELD();
      }
      char c = stream.read();
      sockets[mux]->rx.put(c);
    }
    // make sure the sock available number is accurate again
    sockets[mux]->sock_available = modemGetAvailable(mux);
    return len_confirmed;
  }
};

#endif  // SRC_TINYGSMCLIENTSIM7020_H_
