#include "fw-interface.h"
#include <json-glib/json-glib.h>
#include "hawkbit-client.h"
#include <stdio.h>
#include <glib/gtypes.h>
#include "json-helper.h"
#include <stdbool.h>
#include <glib-object.h>
#include<unistd.h>
#include <glib/gstdio.h>

extern  Config *hawkbit_config;
extern const char *HTTPMethod_STRING[];
extern const gint resumable_codes[];
extern GThread *thread_download;
extern GSourceFunc software_ready_cb;
extern struct HawkbitAction *active_action;



/**
 * @brief Parse version string to version struct
 *
 * @param[in] version string representation of version
 * @return  struct representation of version
 */

version_t parse_version(const gchar *version)
{ 
    version_t ret; 
    
    gchar *first, *second = NULL;
    
    first = strtok_r(version, "." ,&version);

    ret.major = atoi(first);
    ret.minor = atoi(version);

    return ret;
}


/**
 * @brief Callback function for query from database
 *
 * @param[in] images pointer to Glist
 * @param[in] argc amount of data
 * @param[in] argv  list of data 
 * @param[in] azColName column name 
 * @return  struct representation of version
 */

int callback(void *images, int argc, char **argv, 
                    char **azColName) {
    

    GList **images_list = (GList **) images;
    gchar *tmp_version =NULL;

    RCE_DEVICE *device =  malloc(sizeof(RCE_DEVICE));
    device-> id  = atoi(argv[0]);
    device->name = malloc(strlen(argv[1]) + 1);
    strcpy ( device->name, argv[1]);
    

    tmp_version = malloc(strlen(argv[2]) + 1 );
    strcpy ( tmp_version, argv[2]);
    device->fw = parse_version(tmp_version);
    g_free(tmp_version);

    tmp_version = malloc(strlen(argv[3]) + 1);
    strcpy ( tmp_version, argv[3]);
    device->latest_fw = parse_version(tmp_version);
    g_free(tmp_version);    

    tmp_version = malloc(strlen(argv[4]) + 1);
    strcpy ( tmp_version, argv[3]);
    device->fallback_fw = parse_version(tmp_version);
    g_free(tmp_version);    

    tmp_version = malloc(strlen(argv[5]) + 1);
    strcpy ( tmp_version, argv[3]);
    device->hw = parse_version(tmp_version);
    g_free(tmp_version);    

    *images_list = g_list_append(*images_list, device);

    if (images_list == NULL){
        g_debug("No devices found in database");
    }

    return 0;
}


/**
 * @brief g_free data in device
 *
 * @param[in] data pointer to data
 * @return 
 */

void free_image(gpointer data)
{ 
    RCE_DEVICE *image = (RCE_DEVICE*) data;
    if (!image)
		return;

	g_free(image->name);
	g_free(image);
}

/**
 * @brief Debug function to print artifact
 *
 * @param[in] data pointer to Artifact
 * @return 
 */
void print_Artifact(Artifact *data)
{ 
    if(!data)
        return;
    g_debug("%s\n",data->name);
    g_debug("%s\n",data->version);
    g_debug("%s\n",data->download_url);
    g_debug("%s\n",data->feedback_url);
    
}


void print_version(version_t version)
{ 
    g_debug("MAJOR_VERSION: %d\n", version.major);
    g_debug("MINOR_VERSION: %d\n", version.minor);

}


/**
 * @brief check if version is newer than previous
 *
 * @param[in] instelled version of installed fw
 * @param[in] server version of server fw
 * @return  True if Success, False otherwise
 */
gboolean compare_version(version_t installed, version_t server)
{   


    if( server.major > installed.major )
    {
        return true;
    }
    else if (server.major == installed.major)
    { 
        if (server.minor > installed.minor)
            return true;
    }

    return false;
}

void print_image(RCE_DEVICE *data)
{ 

    if(!data)
        return;
    g_debug("id : %d\n", data->id);
    g_debug("NAME : %s\n",data->name);
    g_debug("HW : %d.%d\n",data->hw.major,data->hw.minor);
    g_debug("FW : %d.%d\n",data->fw.major,data->hw.minor);
}




/**
 * @brief construct path to which store the rauc update
 *
 * @param[in] name string representing name of fw
 * @return  new alloaceted pointer
 */

gchar* get_fw_path(gchar *name)

{ 
    gchar *location_fw = NULL;

    location_fw = malloc(strlen("/tmp/") + strlen(name) + strlen(".raucb") +1);
    if (location_fw == NULL)
        g_debug("NULL");
    strcpy(location_fw,"/tmp/");
    strcat(location_fw,name);
    strcat(location_fw,".raucb");

    return location_fw ;

}


/**
 * @brief construct path to which store the rauc update
 *
 * @param[in] name string representing name of fw
 * @return  new alloaceted pointer
 */
gint find_name(RCE_DEVICE *data, gconstpointer name)
{ 

    if(!data)
        return -1;

    if (strcmp(data->name, name) != 0)
    {
       return -1;
    }
    return 0;

}


GList * get_current_devices()
{ 
    sqlite3 *db;
    char *err_msg = NULL;
    GList *images = NULL;
    int rc = sqlite3_open(hawkbit_config->database_location,&db);
    if (rc != SQLITE_OK) {
        
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        
        return NULL;
    }
    
    gchar *sql = "SELECT ID,NAME,FW,FW_LATEST,FW_FALLBACK,HW FROM DEVICES";

    rc = sqlite3_exec(db, sql, callback, &images, &err_msg);
    
    sqlite3_close(db);

    return images;
} 



/**
 * @brief Sends query to database
 *
 * @param[in] query string to query into database
 * @return  True if Success, False otherwise
 */
bool update_dabase(gchar *query)
{ 
    sqlite3 *db;
    char *err_msg = NULL;
    int rc = sqlite3_open(hawkbit_config->database_location,&db);
    if (rc != SQLITE_OK) {
        
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        
        return false;
    }
    g_debug("Updating database with this %s",query);
    rc = sqlite3_exec(db, query, NULL, NULL, &err_msg);
    sqlite3_close(db);
    g_debug("RETURN VALUE OF QUERY %d",rc);
    return rc;

}



/**
 * @brief Callback called when rauc finishes installing bundle
 *
 * @param[in] ptr pointer to data
 * @return  G_SOURCE_REMOVE
 */

gboolean rauc_complete_cb(gpointer ptr)
{
        gboolean res = FALSE;
        g_autoptr(GError) error = NULL;
        struct on_install_complete_userdata *result = ptr;
        g_autofree gchar *feedback_url = NULL;

        g_return_val_if_fail(ptr, FALSE);
        g_debug("Installing done");
        g_mutex_lock(&active_action->mutex);

        active_action->state = result->install_success ? ACTION_STATE_PROCESSING : ACTION_STATE_ERROR;
        feedback_url = build_api_url("deploymentBase/%s/feedback", active_action->id);
        res = feedback(
                feedback_url, active_action->id,
                result->install_success ? "Software bundle installed successfully."
                : "Failed to install software bundle.",
                result->install_success ? "success" : "failure",
                "proceeding", &error);

        if (!res)
                g_warning("%s", error->message);

        g_mutex_unlock(&active_action->mutex);
        g_debug("callback done");
    
    return G_SOURCE_REMOVE;

} 


/**
 * @brief Thread to download given Artifact, verfiy its checksum, send hawkBit
 * feedback and call software_ready_cb() callback on success.
 *
 * @param[in] data Artifact* to process
 * @return gpointer being 1 (TRUE) if download succeeded, 0 (FALSE) otherwise. The return value is
 *         meant to be used with the GPOINTER_TO_INT() macro only.
 *         Note that if the download thread waited for installation to finish ('run_once' mode),
 *         TRUE means both installation and download succeeded.
 */
static gpointer download_thread_fw(Artifact *artifact)
{

        g_autoptr(GError) error = NULL, feedback_error = NULL;
        g_autofree gchar *msg = NULL, *sha1sum = NULL;
        gchar *location_fw = NULL;
        gboolean test;
        curl_off_t speed;
        
        g_debug("DOWNLOAD_THREAD_STARTED");
        g_return_val_if_fail(artifact, NULL);
        g_assert_nonnull(hawkbit_config->bundle_download_location);

        g_mutex_lock(&active_action->mutex);
        if (active_action->state == ACTION_STATE_CANCEL_REQUESTED)
               goto cancel;

        active_action->state = ACTION_STATE_DOWNLOADING;
        g_mutex_unlock(&active_action->mutex);

        msg = g_strdup_printf("Starting download of %s", artifact->name);
        if (!feedback_progress(artifact->feedback_url, active_action->id, msg, &error)) {
                g_warning("%s", error->message);
                g_clear_error(&error);
        } 


        g_debug("Start downloading: %s", artifact->download_url);
        while (1) {
                gboolean resumable = FALSE;
                GStatBuf bundle_stat;
                curl_off_t resume_from = 0;

                g_clear_pointer(&sha1sum, g_free);                
                location_fw = get_fw_path(artifact->name);
                
                // Download software bundle (artifact)
                if (g_stat(location_fw, &bundle_stat) == 0)
                        resume_from = (curl_off_t) bundle_stat.st_size;

                if (get_binary(artifact->download_url,location_fw,
                               resume_from, &sha1sum, &speed, &error))

                        break;

                for (const gint *code = &resumable_codes[0]; *code; code++)
                        resumable |= g_error_matches(error, RHU_HAWKBIT_CLIENT_CURL_ERROR, *code);

                if (!hawkbit_config->resume_downloads || !resumable) {
                        g_prefix_error(&error, "Download failed: ");
                        goto report_err;
                }
                g_debug("%s, resuming download..", curl_easy_strerror(error->code));

                g_mutex_lock(&active_action->mutex);
                if (active_action->state == ACTION_STATE_CANCEL_REQUESTED)
                        goto cancel;
                g_mutex_unlock(&active_action->mutex);

                g_clear_error(&error);

                // sleep 0.5 s before attempting to resume download
            g_usleep(500000);
        }
        g_free(location_fw);
        // notify hawkbit that download is complete
        g_debug("Download of %s complete. %.2f MB/s", artifact->name, (double)speed/(1024*1024));
        msg = g_strdup_printf("Download of %s complete. %.2f MB/s", artifact->name,
                              (double)speed/(1024*1024));

        g_mutex_lock(&active_action->mutex);

        if (!feedback_progress(artifact->feedback_url, active_action->id, msg, &error)) {
                g_warning("%s", error->message);
                g_clear_error(&error);
        }
        
        g_mutex_unlock(&active_action->mutex);

        // validate checksum
        if (g_strcmp0(artifact->sha1, sha1sum)) {
                g_set_error(&error, RHU_HAWKBIT_CLIENT_ERROR, RHU_HAWKBIT_CLIENT_ERROR_DOWNLOAD,
                            "Software: %s V%s. Invalid checksum: %s expected %s", artifact->name,
                            artifact->version, sha1sum, artifact->sha1);
                goto report_err;
        }
    
        g_mutex_lock(&active_action->mutex);
        msg = g_strdup_printf("File checksum of %s OK", artifact->name);
        if (!feedback_progress(artifact->feedback_url, active_action->id, "File checksum OK.",
                               &error)) {
                g_warning("%s", error->message);
                g_clear_error(&error);
        }
        g_mutex_unlock(&active_action->mutex);
    
        // last chance to cancel installation

        g_mutex_lock(&active_action->mutex);
        if (active_action->state == ACTION_STATE_CANCEL_REQUESTED)
                goto cancel;

        // skip installation if hawkBit asked us to do so
        if (!artifact->do_install) {
                active_action->state = ACTION_STATE_NONE;
                g_mutex_unlock(&active_action->mutex);

                return GINT_TO_POINTER(true);
        }
    
        g_mutex_unlock(&active_action->mutex);

        return true;

report_err:
    
        g_mutex_lock(&active_action->mutex);
        if (!feedback(artifact->feedback_url, active_action->id, error->message, "failure",
                      "closed", &feedback_error))
                g_warning("%s", feedback_error->message);
        
        active_action->state = ACTION_STATE_ERROR;

cancel:
        if (active_action->state == ACTION_STATE_CANCEL_REQUESTED)
                active_action->state = ACTION_STATE_CANCELED;

        g_cond_signal(&active_action->cond);
        g_mutex_unlock(&active_action->mutex);

        return GINT_TO_POINTER(FALSE);
}



gboolean install(Artifact *artifact)
{ 


    struct on_new_software_userdata userdata = {
        .install_progress_callback = (GSourceFunc) hawkbit_progress,
        .install_complete_callback = rauc_complete_cb,
        .file = get_fw_path(artifact->name),
        .auth_header = NULL,
        .ssl_verify = hawkbit_config->ssl_verify,
        .install_success = FALSE,
        };


    g_debug("Installing %s",artifact->name);
    
    software_ready_cb(&userdata);

    g_free(userdata.file);

    return userdata.install_success;
}

/**
 * @brief Install succesfully downloaded artifact
 *
 * @param[in] artifact pointer to artifact struct
 * @return  True if succcess, False otherwise
 */

gboolean can_install(Artifact *artifact)
{
    g_autoptr(GError) error = NULL, feedback_error = NULL;
    g_autofree gchar *msg = NULL; 
    GList *rce_devices_list= get_current_devices(); 

    if(rce_devices_list == NULL)
        return false;

    int ret = 0;
    g_debug("CAN_INSTALL: LOOPING THROUGH DEVICES %d",g_list_length(rce_devices_list));
    while(rce_devices_list)
    {
        GList *next = rce_devices_list->next;
        RCE_DEVICE *rce_device = (RCE_DEVICE *) rce_devices_list->data;
        if (strcmp(rce_device->name, artifact->name) != 0)
        {
            g_debug("%s : %s", rce_device->name, artifact->name);
            rce_devices_list = next;

            continue;
        }
    
        g_autofree gchar *path  = g_strdup_printf("/data/fw/%s/Active/firmware.hex", rce_device->name);
        g_autofree gchar *bootloader_call = g_strdup_printf("/app/BootloaderCmd -start %d %s", rce_device->id, path);
        g_debug("Calling %s",bootloader_call);
        
        ret  = system(bootloader_call);
        

        if( ret == -1)
        { 
            g_debug("Couldnt call properly BootloaderCmd");
            return false;
        }


        
        if ( WEXITSTATUS(ret) == 0)
        {
            msg = g_strdup_printf("Successfully installed new firmware on %s ", artifact->name);
            if (!feedback_progress(artifact->feedback_url, active_action->id, msg, &error)) {
                g_warning("%s", error->message);
                g_clear_error(&error);
                }
        } 

        if ( WEXITSTATUS(ret) == 4)
        { 
            msg = g_strdup_printf("Couldnt install %s , error in communication with bootloader", artifact->name);
            if (!feedback_progress(artifact->feedback_url, active_action->id, msg, &error)) {
                g_warning("%s", error->message);
                g_clear_error(&error);
            }
        }

        if ( WEXITSTATUS(ret) == 5)
        {
            msg = g_strdup_printf("Couldnt install %s , device is not online", artifact->name);
            if (!feedback_progress(artifact->feedback_url, active_action->id, msg, &error)) {
                g_warning("%s", error->message);
                g_clear_error(&error);
                }
        }

        rce_devices_list = next;

    }

    return true;
}


gboolean can_install_list(GList *list)
{ 

    while(list)
    { 
        GList *next = list->next;
        Artifact *testptr = list->data;
        if(testptr->config_install)
        {
            break;
        }
        if (testptr->install_can)
            can_install(testptr);
        list = next; 
    }

    return true;

}



/**
 * @brief Calls installfunction for all artifacts, after success it updates database
 *
 * @param[in] list list of artifacts
 * @return  True if Success, False otherwise
 */
gboolean install_fw(GList *list)

{   
    while(list)
    {  
        GList *next = list->next;
        Artifact *testptr = list->data;
        if (!install(testptr))
            return false;
        
        if(!testptr->config_install)
        {
            gchar *query = g_strdup_printf("Update DEVICES set FW_LATEST = \"%s\", FW_FALLBACK = (SELECT DISTINCT FW_LATEST FROM DEVICES WHERE NAME=\"%s\" )"
            "WHERE NAME=\"%s\"", testptr->version, testptr->name, testptr->name);
            if(update_dabase(query))
            {
                g_free(query);
                return false;
            }
            g_free(query);
        }
        list = next;
    }

    return true;
}

/**
 * @brief Calls download for all artifacts in list
 *
 * @param[in] list list of artifacts
 * @return  True if Success, False otherwise
 */
gboolean download_fw(GList *list)
{ 

    while(list)
    {  
        GList *next = list->next;
        g_debug("FIRST ARTIFACT");
        Artifact *testptr = list->data;
        g_debug("TESTPOINTERURL %s", testptr->download_url);
        g_debug("%s",active_action->id); 
        
        download_thread_fw(testptr);
        
        
        list = next;
    }

    return true;
}


/**
 * @brief Calls download and install functions for all artifacts in list
 *
 * @param[in] dat pointer to list of data
 * @return  G_SOUCE_REMOVE
 */
gboolean download_and_install(gpointer data)
{

    GList *list = (GList *) data;
    gboolean ret = false;
    GError *error = NULL;
    g_autofree gchar *feedback_url = NULL;

    download_fw(list);

    g_mutex_lock(&active_action->mutex);
    active_action->state = ACTION_STATE_INSTALLING;
    g_cond_signal(&active_action->cond);
    g_mutex_unlock(&active_action->mutex);

    ret = install_fw(list);
    if(ret)
    {
        can_install_list(list);
    }

    g_mutex_lock(&active_action->mutex);
    feedback_url = build_api_url("deploymentBase/%s/feedback", active_action->id);
    active_action->state = ret ? ACTION_STATE_SUCCESS : ACTION_STATE_ERROR;
    feedback(feedback_url, active_action->id,
             ret ? "Software bundle installed completely."
                 : "Failed to install software bundle.",
             ret ? "success" : "failure",
             "closed", error);
    g_mutex_unlock(&active_action->mutex);
    process_deployment_cleanup();

    return G_SOURCE_REMOVE; 
}

/**
 * @brief Adds info about devices to attributes which are send to hawkbit
 *
 * @param[in] hash GhashTable which consists atributes to send to server
 * @return  True if Success, False otherwise
 */
gboolean  add_devices_to_config(GHashTable *hash)
{ 
    gchar *hash_value  = NULL;
    GList *rce_devices_list= get_current_devices();

    if(rce_devices_list == NULL)
        return false;
    g_autofree gchar *devices = g_strdup_printf("%s","Devices");


    while(rce_devices_list)
    { 
        GList *next = rce_devices_list->next;
        RCE_DEVICE *rce_device = rce_devices_list->data;
        gchar *tmp = NULL;
        
        g_autofree gchar *value  = g_strdup_printf("FW: %d.%d | HW: %d.%d | Current %d.%d  | Fallback: %d.%d", rce_device->fw.major, rce_device->fw.minor,rce_device->hw.major,
        rce_device->hw.minor, rce_device->latest_fw.major, rce_device->latest_fw.minor, rce_device->fallback_fw.major, rce_device->fallback_fw.minor);
        g_autofree gchar *key = g_strdup_printf("%s:%d", rce_device->name , rce_device->id);
        
        tmp = g_strdup_printf(" | %s:%d.%d",rce_device->name, rce_device->fw.major, rce_device->fw.minor);
        if (hash_value != NULL)
        { 
            tmp = g_strdup_printf(" %s | %s:%d.%d",hash_value,rce_device->name, rce_device->fw.major, rce_device->fw.minor);  
            g_free(hash_value);
        } 
        else{ 
            tmp = g_strdup_printf(" | %s:%d.%d",rce_device->name, rce_device->fw.major, rce_device->fw.minor);
        }

        hash_value = g_strdup_printf("%s", tmp);
        g_free(tmp);
        g_hash_table_insert(hash, g_steal_pointer(&key), g_steal_pointer(&value));
        
        rce_devices_list = next;
    } 
    g_hash_table_insert(hash,g_steal_pointer(&devices),hash_value);


    return true;

}





/**
 * @brief Parse firwmare chunks from hawkbit and call download thread
 *
 * @param[in] json_chunks json chunks
 * @param[in] feedback_url_tmp url for feedback
 * @param[in] forced parameter which determines if we want to check version or not
 * @return  True if Success, False otherwise
 */
gboolean parse_fw(JsonArray *json_chunks, gchar *feedback_url_tmp, gboolean forced)
{ 
    GError **error = NULL;
    JsonNode *chunk, *device, *metadata = NULL;
    JsonArray *devices = NULL;
    JsonArray *metadata_array = NULL;
    GList *Artifact_list, *rce_devices_list, *one_device =  NULL;
    gchar *hw = NULL;
    gchar *config_ptr = NULL;
    gchar *can_install = NULL;
    RCE_DEVICE *rce_device = NULL;
    rce_devices_list = get_current_devices();
    guint8 len = json_array_get_length(json_chunks);

    for (int i = 0; i < len; i++)
    { 
        Artifact *artifact = malloc(sizeof(Artifact));

        chunk = json_array_get_element(json_chunks, i);
        devices = json_get_array(chunk, "$.artifacts", error);

        metadata_array = json_get_array(chunk,"$.metadata",error);
        
        int metadata_length = json_array_get_length(metadata_array);
        for (int x = 0; x < metadata_length; x++)
        { 
            metadata = json_array_get_element(metadata_array,x);
            if (!g_strcmp0(json_get_string(metadata,"$.key",error),"HW"))
            { 
              hw = json_get_string(metadata,"$.value",error);
            }
            else if (!g_strcmp0(json_get_string(metadata,"$.key",error),"install"))
            {
            
                can_install = json_get_string(metadata,"$.value",error);
            }
        }

        device  = json_array_get_element(devices, 0);
        

        //Check if we have fw for device which is in our config

        config_ptr = strstr(json_get_string(chunk, "$.name", error),"config");
        
        if(!config_ptr)
        {

            one_device = g_list_find_custom(rce_devices_list,json_get_string(chunk, "$.name", error),find_name);
            if(one_device)
            { 
                rce_device = one_device->data;

                if(compare_version(parse_version(hw),rce_device->hw))
                {
                    //if update is forced we dont care what version it is
                    if (!forced)
                    {
                    if (!compare_version(rce_device->fw,parse_version(json_get_string(chunk, "$.version", error))))
                        { 
                            g_debug("Latest version already installed, skipping this device");
                            g_free(artifact);
                            continue;
                        }
                    }
                }
                else 
                { 
                        g_debug("SW is for different HW VERSIOn");
                        g_free(artifact);
                        continue;
                }
            
            }
            else
            {
                continue;
            }
        } 

        
      //  print_version(parse_version(json_get_string(chunk, "$.version", error)));
        artifact->version = json_get_string(chunk, "$.version", error);
        artifact->name = json_get_string(chunk, "$.name", error);   
        artifact->size = json_get_int(device, "$.size", error);
        artifact->install_can = !g_strcmp0(can_install,"yes");
        artifact->sha1 = json_get_string(device, "$.hashes.sha1", error);
        artifact->feedback_url = malloc(strlen(feedback_url_tmp));
        artifact->config_install = config_ptr;
        strcpy(artifact->feedback_url,feedback_url_tmp);
        
        // favour https download
        artifact->download_url = json_get_string(device, "$._links.download.href", NULL);
        if (!artifact->download_url)
                artifact->download_url = json_get_string(device, "$._links.download-http.href", error);
        if (!artifact->download_url) {
                g_prefix_error(error, "\"$._links.download{-http,}.href\": ");
                g_debug("error during parsing");
                return 0;
        }  

        g_message("FW: New software ready for download (Name: %s, Version: %s, Size: %" G_GINT64_FORMAT " bytes, URL: %s)",
                  artifact->name, artifact->version, artifact->size, artifact->download_url);
        Artifact_list = g_list_append(Artifact_list, (gpointer) artifact);

    } 


    g_list_foreach(Artifact_list, (GFunc) print_Artifact,NULL);

    if (thread_download)
                g_thread_join(thread_download);


        // start download thread
    thread_download = g_thread_new("downloader", download_and_install, (gpointer) g_steal_pointer(&Artifact_list));

    g_list_foreach(Artifact_list, (GFunc) artifact_free,NULL); 
    g_list_foreach(rce_devices_list, (GFunc) free_image,NULL);

    g_debug("fw_interface donre");

    return 1;
}