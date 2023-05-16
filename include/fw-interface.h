
#ifndef __FW_INTERFACE_H__
#define __FW_INTERFACE_H__


#include <sqlite3.h> 
#include <glib.h>
#include <glib/gtypes.h>
#include <json-glib/json-glib.h>
#include "hawkbit-client.h"
#include <curl/curl.h>
#include "config-file.h"




typedef struct { 
    int major;
    int minor;
}  version_t;





typedef struct { 
    gchar *name; 
    version_t hw;
    version_t fw;
    version_t latest_fw;
    version_t fallback_fw;
    int id;
} RCE_DEVICE;





int get_devices();

gboolean  add_devices_to_config(GHashTable *hash);
gboolean rauc_complete_cb(gpointer ptr);
gboolean parse_fw(JsonArray *json_chunks, gchar *feedback_url_tmp, gboolean forced);

#endif // _FW_INTERFACE_H__