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

class TinyGsmSim7020 : public TinyGsmSim70xx<TinyGsmSim7020>,
                       public TinyGsmTCP<TinyGsmSim7020, TINY_GSM_MUX_COUNT>,
                       public TinyGsmSSL<TinyGsmSim7020> {
  friend class TinyGsmSim70xx<TinyGsmSim7020>;
  friend class TinyGsmTCP<TinyGsmSim7020, TINY_GSM_MUX_COUNT>;
  friend class TinyGsmSSL<TinyGsmSim7020>;

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
      dumpModemBuffer(maxWaitMs);
      at->sendAT(GF("+CSOCL="), socket_id);
      sock_connected = false;
      at->waitResponse(3000);
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
    bool setCertificate(const String& certificateName) {
      return at->setCertificate(certificateName, mux);
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
      dumpModemBuffer(maxWaitMs);
      at->sendAT(GF("+CTLSCLOSE="), mux);
      sock_connected = false;
      at->waitResponse(3000);
    }
    void stop() override {
      stop(15000L);
    }
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
  bool setCertificate(const String& certificateName, const uint8_t mux = 0) {
    if (mux >= TINY_GSM_MUX_COUNT) return false;
    certificates[mux] = certificateName;
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

    sendAT(GF("+CGREG?"));
    res = waitResponse(20000L, GF(GSM_NL "+CREG: 0,1"));

    return res;
  }

  bool gprsDisconnectImpl() {
    // Shut down the general application TCP/IP connection
    sendAT(GF("+CGACT=0,0"));
    if (waitResponse(60000L) != 1) { return false; }

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
    uint32_t timeout_ms = ((uint32_t)timeout_s) * 1000;

    if (ssl) {
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
      sendAT(GF("+CTLSCFG="), mux, ",1,", host, ",2,", port);
      if (waitResponse(5000L) != 1) return false;

      if (certificates[mux] != "") {
        sendAT(GF("+CTLSCFG="), mux, ",6,", certificates[mux].length(),",\"", certificates[mux].c_str(),
               "\"");
        if (waitResponse(5000L) != 1) return false;
      }

      sockets[mux]->socket_id = -1;
      sockets[mux]->is_tls = true;

      sendAT(GF("+CTLSCONN="), mux, ",1");
      if (waitResponse(GF(GSM_NL "+CTLSCONN:")) != 1) { return false; }
      streamSkipUntil(',');
      int8_t res = stream.parseInt();
      waitResponse();
      return res == 1;
    } else {
      // Create a TCP/UDP Socket
      // AT+CSOC=<domain>,<type>,<protocol>[,<cid>]
      // <domain> 1: IPv4, 2: IPv6
      // <type> 1: TCP, 2: UDP, 3: RAW
      // <protocol> 1: IP, 2: ICMP
      sendAT(GF("+CSOC=1,1,1"));
      if (waitResponse(GF(GSM_NL "+CSOC:")) != 1) { return false; }
      // returns <socket id> range 0-4
      int8_t socket_id = stream.parseInt();
      waitResponse();
      sockets[mux]->socket_id = socket_id;
      sockets[mux]->is_tls = false;

      // Connect Socket to Remote Address and Port
      // AT+CSOCON=<socket_id>,<remote_port>,<remote_address>
      sendAT(GF("+CSOCON="), sockets[mux]->socket_id, ",", port, ",", host);
      int8_t res = waitResponse();
      return res == 1;
    }
  }

  int16_t modemSend(const void* buff, size_t len, uint8_t mux) {
    if (sockets[mux]->is_tls) {
      // Send Data
      sendAT(GF("+CTLSSEND="), sockets[mux]->socket_id, ',', (uint16_t)len, (const char*)buff);
      if (waitResponse(GF(GSM_NL "+CTLSSEND:")) != 1) { return false; }
      waitResponse();
      return len;
    } else {
      // Send Data to Remote Via Socket With Data Mode
      sendAT(GF("+CSODSEND="), sockets[mux]->socket_id, ',', (uint16_t)len);
      if (waitResponse(GF(">")) != 1) { return 0; }

      stream.write(reinterpret_cast<const uint8_t*>(buff), len);
      stream.flush();

      // OK after posting data
      if (waitResponse() != 1) { return 0; }

      return len;
    }
  }

  size_t modemRead(size_t size, uint8_t mux) {
    if (!sockets[mux]) { return 0; }

    int16_t len_confirmed = 0;
    if (sockets[mux]->is_tls) {
      if (size > 1024) {
        size = 1024;
      }
      // Send Data
      sendAT(GF("+CTLSRECV="), mux, ',', (uint16_t)size);
      if (waitResponse(GF(GSM_NL "+CTLSRECV:")) != 1) { return 0; }

      streamSkipUntil(',');  // skip the response mux
      len_confirmed = stream.parseInt();
      streamSkipUntil(',');  // skip the comma
      streamSkipUntil('"');  // skip the open quote
    } else {
      if (size > 1460) {
        size = 1460;
      }

      // Enable Get Data from Network Manually
      sendAT(GF("+CSORXGET=1,"), sockets[mux]->socket_id);
      if (waitResponse(GF("+CSORXGET:")) != 1) { return 0; }
      waitResponse();

      // Get Data
      sendAT(GF("+CSORXGET=2,"), sockets[mux]->socket_id, ',', (uint16_t)size);
      if (waitResponse(GF("+CSORXGET:")) != 1) { return 0; }

      streamSkipUntil(',');  // skip the mode
      streamSkipUntil(',');  // skip the response socket id
      streamSkipUntil(',');  // skip the response requested length
      len_confirmed = stream.parseInt();
      streamSkipUntil(',');  // skip the comma
    }

    if (len_confirmed <= 0) {
      waitResponse();
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
    waitResponse();
    // make sure the sock available number is accurate again
    sockets[mux]->sock_available = modemGetAvailable(mux);
    return len_confirmed;    
  }

  size_t modemGetAvailable(uint8_t mux) {
    // If the socket doesn't exist, just return
    if (!sockets[mux]) { return 0; }
    // Query data available
    sendAT(GF("+CSORXGET=4,"), sockets[mux]->socket_id);

    streamSkipUntil(',');  // skip the mode
    streamSkipUntil(',');  // skip the response socket id
    int16_t len_confirmed = stream.parseInt();
    waitResponse();

    sockets[mux]->sock_available = len_confirmed;

    modemGetConnected(mux);
    if (!sockets[mux]) { return 0; }
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
          int16_t len = streamGetIntBefore(',');
          int8_t mux = getMuxFromSocketId(socket_id);
          if (mux >= 0) {
            sockets[mux]->got_data = true;
            sockets[mux]->sock_available = len;
            DBG("### Got Data:", len, "on", mux);
          }
          data = "";
        } else if (data.endsWith(GF("+CSOERR:"))) {
          int8_t socket_id = streamGetIntBefore(',');
          int8_t error_code = streamGetIntBefore('\n');
          int8_t mux = getMuxFromSocketId(socket_id);
          if (mux >= 0) {
            sockets[mux]->sock_connected = false;
            DBG("### Closed: ", mux, "error", error_code);
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
  GsmClientSim7020* sockets[TINY_GSM_MUX_COUNT];
  String            certificates[TINY_GSM_MUX_COUNT];

  int8_t getMuxFromSocketId(int8_t socket_id) {
    for (int muxNo = 0; muxNo < TINY_GSM_MUX_COUNT; muxNo++) {
      if (sockets[muxNo] && sockets[muxNo]->socket_id == socket_id) {
        return muxNo;
      }
    }
    return -1;
  }
};

#endif  // SRC_TINYGSMCLIENTSIM7020_H_
