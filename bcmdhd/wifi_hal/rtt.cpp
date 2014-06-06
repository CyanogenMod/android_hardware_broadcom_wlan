#include <stdint.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <linux/rtnetlink.h>
#include <netpacket/packet.h>
#include <linux/filter.h>
#include <linux/errqueue.h>

#include <linux/pkt_sched.h>
#include <netlink/object-api.h>
#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <netlink-types.h>

#include "nl80211_copy.h"

#include "sync.h"

#define LOG_TAG  "WifiHAL"

#include <utils/Log.h>

#include "wifi_hal.h"
#include "common.h"
#include "cpp_bindings.h"

typedef enum {

    RTT_SUBCMD_SET_CONFIG = ANDROID_NL80211_SUBCMD_RTT_RANGE_START

} RTT_SUB_COMMAND;

typedef enum {
    RTT_ATTRIBUTE_TARGET_CNT,
    RTT_ATTRIBUTE_TARGET_INFO,
    RTT_ATTRIBUTE_TARGET_MAC,
    RTT_ATTRIBUTE_TARGET_TYPE,
    RTT_ATTRIBUTE_TARGET_PEER,
    RTT_ATTRIBUTE_TARGET_CHAN,
    RTT_ATTRIBUTE_TARGET_MODE,
    RTT_ATTRIBUTE_TARGET_INTERVAL,
    RTT_ATTRIBUTE_TARGET_NUM_MEASUREMENT,
    RTT_ATTRIBUTE_TARGET_NUM_PKT,
    RTT_ATTRIBUTE_TARGET_NUM_RETRY,
    RTT_ATTRIBUTE_FLUSH_CFG
} GSCAN_ATTRIBUTE;


class RttCommand : public WifiCommand
{
    unsigned numRttParams;
    static const int MAX_RESULTS = 64;
    wifi_rtt_result rttResults[MAX_RESULTS];
    wifi_rtt_config *rttParams;
    wifi_rtt_event_handler rttHandler;
public:
    RttCommand(wifi_interface_handle iface, int id, unsigned num_rtt_config,
                wifi_rtt_config rtt_config[], wifi_rtt_event_handler handler)
        : WifiCommand(iface, id), numRttParams(num_rtt_config), rttParams(rtt_config),
         rttHandler(handler)
    { }


    int createSetupRequest(WifiRequest& request) {
        int result = request.create(GOOGLE_OUI, RTT_SUBCMD_SET_CONFIG);
        if (result < 0) {
            return result;
        }

        nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);
        result = request.put_u8(RTT_ATTRIBUTE_TARGET_CNT, numRttParams);
        if (result < 0) {
            return result;
        }
        nlattr *rtt_config = request.attr_start(RTT_ATTRIBUTE_TARGET_INFO);
        for (unsigned i = 0; i < numRttParams; i++) {

            nlattr *attr2 = request.attr_start(i);
            if (attr2 == NULL) {
                return WIFI_ERROR_OUT_OF_MEMORY;
            }

            result = request.put_addr(RTT_ATTRIBUTE_TARGET_MAC, rttParams[i].addr);
            if (result < 0) {
                return result;
            }
            result = request.put_u8(RTT_ATTRIBUTE_TARGET_TYPE, rttParams[i].type);
            if (result < 0) {
                return result;
            }
            result = request.put_u8(RTT_ATTRIBUTE_TARGET_PEER, rttParams[i].peer);
            if (result < 0) {
                return result;
            }
            result = request.put(RTT_ATTRIBUTE_TARGET_CHAN, &rttParams[i].channel,
                     sizeof(wifi_channel_info));
            if (result < 0) {
                return result;
            }
            result = request.put_u8(RTT_ATTRIBUTE_TARGET_MODE, rttParams[i].continuous);
            if (result < 0) {
                return result;
            }
            result = request.put_u32(RTT_ATTRIBUTE_TARGET_INTERVAL, rttParams[i].interval);
            if (result < 0) {
                return result;
            }
            result = request.put_u32(RTT_ATTRIBUTE_TARGET_NUM_MEASUREMENT,
                     rttParams[i].num_measurements);
            if (result < 0) {
                return result;
            }
            result = request.put_u32(RTT_ATTRIBUTE_TARGET_NUM_PKT,
                     rttParams[i].num_samples_per_measurement);
            if (result < 0) {
                return result;
            }
            result = request.put_u32(RTT_ATTRIBUTE_TARGET_NUM_RETRY,
                     rttParams[i].num_retries_per_measurement);
            if (result < 0) {
                return result;
            }
            request.attr_end(attr2);
        }

        request.attr_end(rtt_config);
        request.attr_end(data);
        return WIFI_SUCCESS;
    }

    int createTeardownRequest(WifiRequest& request, unsigned num_devices, mac_addr addr[]) {
        int result = request.create(GOOGLE_OUI, RTT_SUBCMD_SET_CONFIG);
        if (result < 0) {
            return result;
        }

        nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);
        struct nlattr * attr = request.attr_start(RTT_ATTRIBUTE_FLUSH_CFG);
        for(unsigned i = 0; i < num_devices; i++) {
            result = request.put_addr(RTT_ATTRIBUTE_TARGET_MAC, addr[i]);
            if (result < 0) {
                return result;
            }
        }

        request.attr_end(attr);
        request.attr_end(data);
        return result;
    }
    int start() {
        ALOGD("Setting configuration");
        WifiRequest request(familyId(), ifaceId());
        int result = createSetupRequest(request);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to create setup request; result = %d", result);
            return result;
        }

        result = requestResponse(request);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to configure setup; result = %d", result);
            return result;
        }

        registerVendorHandler(GOOGLE_OUI, RTT_EVENT_COMPLETE);

        result = requestResponse(request);
        if (result < 0) {
            unregisterVendorHandler(GOOGLE_OUI, GSCAN_EVENT_HOTLIST_RESULTS);
            return result;
        }

        ALOGI("successfully restarted the scan");
        return result;
    }

    virtual int cancel() {
        ALOGD("Stopping scan");

        WifiRequest request(familyId(), ifaceId());
        int result = createTeardownRequest(request, 0, NULL);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to create stop request; result = %d", result);
        } else {
            result = requestResponse(request);
            if (result != WIFI_SUCCESS) {
                ALOGE("failed to stop scan; result = %d", result);
            }
        }

        unregisterVendorHandler(GOOGLE_OUI, GSCAN_EVENT_SCAN_RESULTS_AVAILABLE);
        return WIFI_SUCCESS;
    }

    int cancel_specific(unsigned num_devices, mac_addr addr[]) {
        ALOGD("Stopping scan");

        WifiRequest request(familyId(), ifaceId());
        int result = createTeardownRequest(request, num_devices, addr);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to create stop request; result = %d", result);
        } else {
            result = requestResponse(request);
            if (result != WIFI_SUCCESS) {
                ALOGE("failed to stop scan; result = %d", result);
            }
        }

        unregisterVendorHandler(GOOGLE_OUI, GSCAN_EVENT_SCAN_RESULTS_AVAILABLE);
        return WIFI_SUCCESS;
    }

    virtual int handleResponse(WifiEvent& reply) {
        /* Nothing to do on response! */
        return NL_SKIP;
    }

    virtual int handleEvent(WifiEvent& event) {
        ALOGI("Got a scan results event");

        // event.log();

        nlattr *vendor_data = event.get_attribute(NL80211_ATTR_VENDOR_DATA);
        int len = event.get_vendor_data_len();

        if (vendor_data == NULL || len == 0) {
            ALOGI("No rtt results found");
            return NL_SKIP;
        }

        memset(rttResults, 0, sizeof(wifi_rtt_result) * MAX_RESULTS);

        int num = len / sizeof(wifi_rtt_result);
        num = min(MAX_RESULTS, num);
        memcpy(rttResults, event.get_vendor_data(), num * sizeof(wifi_rtt_result));
        ALOGI("Retrieved %d rtt results", num);

        (*rttHandler.on_rtt_results)(id(), num, rttResults);
        return NL_SKIP;
    }
};


/* API to request RTT measurement */
wifi_error wifi_rtt_range_request(wifi_request_id id, wifi_interface_handle iface,
        unsigned num_rtt_config, wifi_rtt_config rtt_config[], wifi_rtt_event_handler handler)
{
    wifi_handle handle = getWifiHandle(iface);

    RttCommand *cmd = new RttCommand(iface, id, num_rtt_config, rtt_config, handler);
    wifi_register_cmd(handle, id, cmd);
    return (wifi_error)cmd->start();
}

/* API to cancel RTT measurements */
wifi_error wifi_rtt_range_cancel(wifi_request_id id,  wifi_interface_handle iface,
        unsigned num_devices, mac_addr addr[])
{
    wifi_handle handle = getWifiHandle(iface);

    RttCommand *cmd = (RttCommand *)wifi_get_cmd(handle, id);
    if (cmd) {
        cmd->cancel_specific(num_devices, addr);
        return WIFI_SUCCESS;
    }

    return WIFI_ERROR_INVALID_ARGS;
}



