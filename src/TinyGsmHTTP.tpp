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

template <class modemType>
class TinyGsmHTTP {
 public:
  /*
   * HTTP functions compatible with ArduinoHttpClient
   */

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
    friend class TinyGsmHTTP<modemType>;

   protected:
    modemType* at;
  };
};

#endif  // SRC_TINYGSMHTTP_H_
