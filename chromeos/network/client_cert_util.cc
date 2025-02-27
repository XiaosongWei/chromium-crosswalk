// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/client_cert_util.h"

#include <cert.h>
#include <pk11pub.h>
#include <stddef.h>

#include <list>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/network/network_event_log.h"
#include "components/onc/onc_constants.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace client_cert {

namespace {

const char kDefaultTPMPin[] = "111111";

std::string GetStringFromDictionary(const base::DictionaryValue& dict,
                                    const std::string& key) {
  std::string s;
  dict.GetStringWithoutPathExpansion(key, &s);
  return s;
}

void GetClientCertTypeAndPattern(
    const base::DictionaryValue& dict_with_client_cert,
    ClientCertConfig* cert_config) {
  using namespace ::onc::client_cert;
  dict_with_client_cert.GetStringWithoutPathExpansion(
      kClientCertType, &cert_config->client_cert_type);

  if (cert_config->client_cert_type == kPattern) {
    const base::DictionaryValue* pattern = NULL;
    dict_with_client_cert.GetDictionaryWithoutPathExpansion(kClientCertPattern,
                                                            &pattern);
    if (pattern) {
      bool success = cert_config->pattern.ReadFromONCDictionary(*pattern);
      DCHECK(success);
    }
  }
}

}  // namespace

// Returns true only if any fields set in this pattern match exactly with
// similar fields in the principal.  If organization_ or organizational_unit_
// are set, then at least one of the organizations or units in the principal
// must match.
bool CertPrincipalMatches(const IssuerSubjectPattern& pattern,
                          const net::CertPrincipal& principal) {
  if (!pattern.common_name().empty() &&
      pattern.common_name() != principal.common_name) {
    return false;
  }

  if (!pattern.locality().empty() &&
      pattern.locality() != principal.locality_name) {
    return false;
  }

  if (!pattern.organization().empty()) {
    if (std::find(principal.organization_names.begin(),
                  principal.organization_names.end(),
                  pattern.organization()) ==
        principal.organization_names.end()) {
      return false;
    }
  }

  if (!pattern.organizational_unit().empty()) {
    if (std::find(principal.organization_unit_names.begin(),
                  principal.organization_unit_names.end(),
                  pattern.organizational_unit()) ==
        principal.organization_unit_names.end()) {
      return false;
    }
  }

  return true;
}

std::string GetPkcs11AndSlotIdFromEapCertId(const std::string& cert_id,
                                            int* slot_id) {
  *slot_id = -1;
  if (cert_id.empty())
    return std::string();

  size_t delimiter_pos = cert_id.find(':');
  if (delimiter_pos == std::string::npos) {
    // No delimiter found, so |cert_id| only contains the PKCS11 id.
    return cert_id;
  }
  if (delimiter_pos + 1 >= cert_id.size()) {
    LOG(ERROR) << "Empty PKCS11 id in cert id.";
    return std::string();
  }
  int parsed_slot_id;
  if (base::StringToInt(cert_id.substr(0, delimiter_pos), &parsed_slot_id))
    *slot_id = parsed_slot_id;
  else
    LOG(ERROR) << "Slot ID is not an integer. Cert ID is: " << cert_id << ".";
  return cert_id.substr(delimiter_pos + 1);
}

void GetClientCertFromShillProperties(
    const base::DictionaryValue& shill_properties,
    ConfigType* cert_config_type,
    int* tpm_slot,
    std::string* pkcs11_id) {
  *cert_config_type = CONFIG_TYPE_NONE;
  *tpm_slot = -1;
  pkcs11_id->clear();

  // Look for VPN specific client certificate properties.
  //
  // VPN Provider values are read from the "Provider" dictionary, not the
  // "Provider.Type", etc keys (which are used only to set the values).
  const base::DictionaryValue* provider_properties = NULL;
  if (shill_properties.GetDictionaryWithoutPathExpansion(
          shill::kProviderProperty, &provider_properties)) {
    // Look for OpenVPN specific properties.
    if (provider_properties->GetStringWithoutPathExpansion(
            shill::kOpenVPNClientCertIdProperty, pkcs11_id)) {
      *cert_config_type = CONFIG_TYPE_OPENVPN;
      return;
    }
    // Look for L2TP-IPsec specific properties.
    if (provider_properties->GetStringWithoutPathExpansion(
                   shill::kL2tpIpsecClientCertIdProperty, pkcs11_id)) {
      std::string cert_slot;
      provider_properties->GetStringWithoutPathExpansion(
          shill::kL2tpIpsecClientCertSlotProperty, &cert_slot);
      if (!cert_slot.empty() && !base::StringToInt(cert_slot, tpm_slot)) {
        LOG(ERROR) << "Cert slot is not an integer: " << cert_slot << ".";
        return;
      }

      *cert_config_type = CONFIG_TYPE_IPSEC;
    }
    return;
  }

  // Look for EAP specific client certificate properties, which can either be
  // part of a WiFi or EthernetEAP configuration.
  std::string cert_id;
  if (shill_properties.GetStringWithoutPathExpansion(shill::kEapCertIdProperty,
                                                     &cert_id)) {
    // Shill requires both CertID and KeyID for TLS connections, despite the
    // fact that by convention they are the same ID, because one identifies
    // the certificate and the other the private key.
    std::string key_id;
    shill_properties.GetStringWithoutPathExpansion(shill::kEapKeyIdProperty,
                                                   &key_id);
    // Assume the configuration to be invalid, if the two IDs are not identical.
    if (cert_id != key_id) {
      LOG(ERROR) << "EAP CertID differs from KeyID";
      return;
    }
    *pkcs11_id = GetPkcs11AndSlotIdFromEapCertId(cert_id, tpm_slot);
    *cert_config_type = CONFIG_TYPE_EAP;
  }
}

void SetShillProperties(const ConfigType cert_config_type,
                        const int tpm_slot,
                        const std::string& pkcs11_id,
                        base::DictionaryValue* properties) {
  switch (cert_config_type) {
    case CONFIG_TYPE_NONE: {
      return;
    }
    case CONFIG_TYPE_OPENVPN: {
      properties->SetStringWithoutPathExpansion(shill::kOpenVPNPinProperty,
                                                kDefaultTPMPin);
      properties->SetStringWithoutPathExpansion(
          shill::kOpenVPNClientCertIdProperty, pkcs11_id);
      break;
    }
    case CONFIG_TYPE_IPSEC: {
      properties->SetStringWithoutPathExpansion(shill::kL2tpIpsecPinProperty,
                                                kDefaultTPMPin);
      properties->SetStringWithoutPathExpansion(
          shill::kL2tpIpsecClientCertSlotProperty, base::IntToString(tpm_slot));
      properties->SetStringWithoutPathExpansion(
          shill::kL2tpIpsecClientCertIdProperty, pkcs11_id);
      break;
    }
    case CONFIG_TYPE_EAP: {
      properties->SetStringWithoutPathExpansion(shill::kEapPinProperty,
                                                kDefaultTPMPin);
      std::string key_id =
          base::StringPrintf("%i:%s", tpm_slot, pkcs11_id.c_str());

      // Shill requires both CertID and KeyID for TLS connections, despite the
      // fact that by convention they are the same ID, because one identifies
      // the certificate and the other the private key.
      properties->SetStringWithoutPathExpansion(shill::kEapCertIdProperty,
                                                key_id);
      properties->SetStringWithoutPathExpansion(shill::kEapKeyIdProperty,
                                                key_id);
      break;
    }
  }
}

void SetEmptyShillProperties(const ConfigType cert_config_type,
                             base::DictionaryValue* properties) {
  switch (cert_config_type) {
    case CONFIG_TYPE_NONE: {
      return;
    }
    case CONFIG_TYPE_OPENVPN: {
      properties->SetStringWithoutPathExpansion(shill::kOpenVPNPinProperty,
                                                std::string());
      properties->SetStringWithoutPathExpansion(
          shill::kOpenVPNClientCertIdProperty, std::string());
      break;
    }
    case CONFIG_TYPE_IPSEC: {
      properties->SetStringWithoutPathExpansion(shill::kL2tpIpsecPinProperty,
                                                std::string());
      properties->SetStringWithoutPathExpansion(
          shill::kL2tpIpsecClientCertSlotProperty, std::string());
      properties->SetStringWithoutPathExpansion(
          shill::kL2tpIpsecClientCertIdProperty, std::string());
      break;
    }
    case CONFIG_TYPE_EAP: {
      properties->SetStringWithoutPathExpansion(shill::kEapPinProperty,
                                                std::string());
      // Shill requires both CertID and KeyID for TLS connections, despite the
      // fact that by convention they are the same ID, because one identifies
      // the certificate and the other the private key.
      properties->SetStringWithoutPathExpansion(shill::kEapCertIdProperty,
                                                std::string());
      properties->SetStringWithoutPathExpansion(shill::kEapKeyIdProperty,
                                                std::string());
      break;
    }
  }
}

ClientCertConfig::ClientCertConfig()
    : location(CONFIG_TYPE_NONE),
      client_cert_type(onc::client_cert::kClientCertTypeNone) {
}

void OncToClientCertConfig(const base::DictionaryValue& network_config,
                           ClientCertConfig* cert_config) {
  using namespace ::onc;

  *cert_config = ClientCertConfig();

  const base::DictionaryValue* dict_with_client_cert = NULL;

  const base::DictionaryValue* wifi = NULL;
  network_config.GetDictionaryWithoutPathExpansion(network_config::kWiFi,
                                                   &wifi);
  if (wifi) {
    const base::DictionaryValue* eap = NULL;
    wifi->GetDictionaryWithoutPathExpansion(wifi::kEAP, &eap);
    if (!eap)
      return;

    dict_with_client_cert = eap;
    cert_config->location = CONFIG_TYPE_EAP;
  }

  const base::DictionaryValue* vpn = NULL;
  network_config.GetDictionaryWithoutPathExpansion(network_config::kVPN, &vpn);
  if (vpn) {
    const base::DictionaryValue* openvpn = NULL;
    vpn->GetDictionaryWithoutPathExpansion(vpn::kOpenVPN, &openvpn);
    const base::DictionaryValue* ipsec = NULL;
    vpn->GetDictionaryWithoutPathExpansion(vpn::kIPsec, &ipsec);
    if (openvpn) {
      dict_with_client_cert = openvpn;
      cert_config->location = CONFIG_TYPE_OPENVPN;
    } else if (ipsec) {
      dict_with_client_cert = ipsec;
      cert_config->location = CONFIG_TYPE_IPSEC;
    } else {
      return;
    }
  }

  const base::DictionaryValue* ethernet = NULL;
  network_config.GetDictionaryWithoutPathExpansion(network_config::kEthernet,
                                                   &ethernet);
  if (ethernet) {
    const base::DictionaryValue* eap = NULL;
    ethernet->GetDictionaryWithoutPathExpansion(wifi::kEAP, &eap);
    if (!eap)
      return;
    dict_with_client_cert = eap;
    cert_config->location = CONFIG_TYPE_EAP;
  }

  if (dict_with_client_cert)
    GetClientCertTypeAndPattern(*dict_with_client_cert, cert_config);
}

bool IsCertificateConfigured(const ConfigType cert_config_type,
                             const base::DictionaryValue& service_properties) {
  // VPN certificate properties are read from the Provider dictionary.
  const base::DictionaryValue* provider_properties = NULL;
  service_properties.GetDictionaryWithoutPathExpansion(
      shill::kProviderProperty, &provider_properties);
  switch (cert_config_type) {
    case CONFIG_TYPE_NONE:
      return true;
    case CONFIG_TYPE_OPENVPN:
      // OpenVPN generally requires a passphrase and we don't know whether or
      // not one is required, so always return false here.
      return false;
    case CONFIG_TYPE_IPSEC: {
      if (!provider_properties)
        return false;

      std::string client_cert_id;
      provider_properties->GetStringWithoutPathExpansion(
          shill::kL2tpIpsecClientCertIdProperty, &client_cert_id);
      return !client_cert_id.empty();
    }
    case CONFIG_TYPE_EAP: {
      std::string cert_id = GetStringFromDictionary(
          service_properties, shill::kEapCertIdProperty);
      std::string key_id = GetStringFromDictionary(
          service_properties, shill::kEapKeyIdProperty);
      std::string identity = GetStringFromDictionary(
          service_properties, shill::kEapIdentityProperty);
      return !cert_id.empty() && !key_id.empty() && !identity.empty();
    }
  }
  NOTREACHED();
  return false;
}

}  // namespace client_cert

}  // namespace chromeos
