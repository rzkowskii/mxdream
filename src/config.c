/**
 * $Id$
 *
 * User configuration support
 *
 * Copyright (c) 2005 Nathan Keynes.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "dreamcast.h"
#include "config.h"
#include "lxpaths.h"
#include "maple/maple.h"

#define MAX_ROOT_GROUPS 16

extern struct lxdream_config_group hotkeys_group;
extern struct lxdream_config_group serial_group;

gboolean lxdream_load_config_file( const gchar *filename );
gboolean lxdream_save_config_file( const gchar *filename );
gboolean lxdream_load_config_stream( FILE *f );
gboolean lxdream_save_config_stream( FILE *f );

static struct lxdream_config_group global_group =
    { "global", dreamcast_config_changed, NULL, NULL,
       {{ "bios", N_("Bios ROM"), CONFIG_TYPE_FILE, NULL },
        { "flash", N_("Flash ROM"), CONFIG_TYPE_FILE, NULL },
        { "default path", N_("Default disc path"), CONFIG_TYPE_PATH, "." },
        { "save path", N_("Save-state path"), CONFIG_TYPE_PATH, NULL },
        { "vmu path", N_("VMU path"), CONFIG_TYPE_PATH, NULL },
        { "bootstrap", N_("Bootstrap IP.BIN"), CONFIG_TYPE_FILE, NULL },
        { "gdrom", NULL, CONFIG_TYPE_FILE, NULL },
        { "recent", NULL, CONFIG_TYPE_FILELIST, NULL },
        { "vmu", NULL, CONFIG_TYPE_FILELIST, NULL },
        { "quick state", NULL, CONFIG_TYPE_INTEGER, "0" },
        { NULL, CONFIG_TYPE_NONE }} };

/**
 * Dummy group for controllers (handled specially)
 */
static struct lxdream_config_group controllers_group =
    { "controllers", NULL, NULL, NULL, {{NULL, CONFIG_TYPE_NONE}} };

static struct lxdream_config_group video_group =
    { "video", NULL, NULL, NULL,
      { { "msaa_samples", N_("MSAA samples (0,2,4,8)"), CONFIG_TYPE_INTEGER, "0" },
        { "integer_scale", N_("Integer scaling (0/1)"), CONFIG_TYPE_BOOLEAN, "0" },
        { "internal_scale_percent", N_("Internal render scale (%)"), CONFIG_TYPE_INTEGER, "100" },
        { NULL, CONFIG_TYPE_NONE } } };

struct lxdream_config_group *lxdream_config_root[MAX_ROOT_GROUPS+1] = 
       { &global_group, &controllers_group, &hotkeys_group, &serial_group, &video_group, NULL };

static gchar *lxdream_config_load_filename = NULL;
static gchar *lxdream_config_save_filename = NULL;

void lxdream_register_config_group( const gchar *key, lxdream_config_group_t group )
{
    int i;
    for( i=0; i<MAX_ROOT_GROUPS; i++ ) {
        if( lxdream_config_root[i] == NULL ) {
            lxdream_config_root[i] = group;
            lxdream_config_root[i+1] = NULL;
            return;
        }
    }
    ERROR( "Unable to register config group '%s': Too many configuration groups", key );
}

gboolean lxdream_find_config()
{
    gboolean result = TRUE;
    char *home = getenv("HOME");
    if( lxdream_config_save_filename == NULL ) {
        /* For compatibility, look for the old ~/.lxdreamrc first. Use it if 
         * found, otherwise use the new ~/.mxdream/lxdreamrc.
         */
        lxdream_config_save_filename = g_strdup_printf("%s/.%s", home, DEFAULT_CONFIG_FILENAME);
        if( access(lxdream_config_save_filename, R_OK) != 0 ) {
            g_free(lxdream_config_save_filename);
            const char *user_path = get_user_data_path();
            lxdream_config_save_filename = g_strdup_printf("%s/%s", user_path, DEFAULT_CONFIG_FILENAME);
        }
    }
    if( lxdream_config_load_filename == NULL ) {
        char *sysconfig = g_strdup_printf("%s/%s", get_sysconf_path(), DEFAULT_CONFIG_FILENAME);
        if( access(lxdream_config_save_filename, R_OK) == 0 ) {
            lxdream_config_load_filename = g_strdup(lxdream_config_save_filename);
            g_free(sysconfig);
        } else if( access( sysconfig, R_OK ) == 0 ) {
            lxdream_config_load_filename = sysconfig;
        } else {
            lxdream_config_load_filename = g_strdup(lxdream_config_save_filename);
            g_free(sysconfig);
            result = FALSE;
        }	
    }
    return result;
}

void lxdream_set_config_filename( const gchar *filename )
{
    if( lxdream_config_load_filename != NULL ) {
        g_free(lxdream_config_load_filename);
    }
    lxdream_config_load_filename = g_strdup(filename);
    if( lxdream_config_save_filename != NULL ) {
        g_free(lxdream_config_save_filename);
    }
    lxdream_config_save_filename = g_strdup(filename);
}

void lxdream_set_default_config( )
{
    /* Construct platform dependent defaults */
    const gchar *user_path = get_user_data_path();
    global_group.params[CONFIG_BIOS_PATH].default_value = g_strdup_printf( "%s/dcboot.rom", user_path ); 
    global_group.params[CONFIG_FLASH_PATH].default_value = g_strdup_printf( "%s/dcflash.rom", user_path ); 
    global_group.params[CONFIG_SAVE_PATH].default_value = g_strdup_printf( "%s/save", user_path ); 
    global_group.params[CONFIG_VMU_PATH].default_value = g_strdup_printf( "%s/vmu", user_path ); 
    global_group.params[CONFIG_BOOTSTRAP].default_value = g_strdup_printf( "%s/IP.BIN", user_path ); 
    
    /* Copy defaults into main values */
    for( int i=0; lxdream_config_root[i] != NULL; i++ ) {
        struct lxdream_config_entry *param = lxdream_config_root[i]->params;
        if( param != NULL ) {
            while( param->key != NULL ) {
                if( param->value != param->default_value ) {
                    if( param->value != NULL )
                        free( param->value );
                    param->value = (gchar *)param->default_value;
                }
                param++;
            }
        }
    }
    maple_detach_all();
}

const gchar *lxdream_get_config_value( lxdream_config_group_t group, int key )
{
    return group->params[key].value;
}

gboolean lxdream_get_config_boolean_value( lxdream_config_group_t group, int key )
{
    const gchar *value = lxdream_get_config_value(group, key);
    if( strcasecmp(value, "on") == 0 || strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "yes") == 0 || strcasecmp(value, "1") == 0 ) {
        return TRUE;
    } else {
        return FALSE;
    }
}

gboolean lxdream_set_config_boolean_value( lxdream_config_group_t group, int key, gboolean value )
{
    return lxdream_set_config_value(group, key, value ? "on" : "off");
}

gboolean lxdream_set_config_value( lxdream_config_group_t group, int key, const gchar *value )
{
    lxdream_config_entry_t param = &group->params[key];
    if( param->value != value &&
        (param->value == NULL || value == NULL || strcmp(param->value,value) != 0)  ) {

        gchar *new_value = g_strdup(value);

        /* If the group defines an on_change handler, it can block the change
         * (ie due to an invalid setting).
         */
        if( group->on_change == NULL ||
            group->on_change(group->data, group,key, param->value, new_value) ) {

            /* Don't free the default value, but otherwise need to release the
             * old value.
             */
            if( param->value != param->default_value && param->value != NULL ) {
                free( param->value );
            }
            param->value = new_value;
        } else { /* on_change handler said no. */
            g_free(new_value);
            return FALSE;
        }
    }
    return TRUE;
}

const gchar *lxdream_get_global_config_value( int key )
{
    return global_group.params[key].value;
}

void lxdream_set_global_config_value( int key, const gchar *value )
{
    lxdream_set_config_value(&global_group, key, value);
}

GList *lxdream_get_global_config_list_value( int key )
{
    GList *result = NULL;
    const gchar *str = lxdream_get_global_config_value( key );
    if( str != NULL ) {
        gchar **strv = g_strsplit(str, ":",0);
        int i;
        for( i=0; strv[i] != NULL; i++ ) {
            result = g_list_append( result, g_strdup(strv[i]) );
        }
        g_strfreev(strv);
    }
    return result;
}

void lxdream_set_global_config_list_value( int key, const GList *list )
{
    if( list == NULL ) {
        lxdream_set_global_config_value( key, NULL );
    } else {
        const GList *ptr;
        int size = 0;
        
        for( ptr = list; ptr != NULL; ptr = g_list_next(ptr) ) {
            size += strlen( (gchar *)ptr->data ) + 1;
        }
        char buf[size];
        strcpy( buf, (gchar *)list->data );
        for( ptr = g_list_next(list); ptr != NULL; ptr = g_list_next(ptr) ) {
            strcat( buf, ":" );
            strcat( buf, (gchar *)ptr->data );
        }
        lxdream_set_global_config_value( key, buf );
    }
}

gchar *lxdream_get_global_config_path_value( int key )
{
    const gchar *str = lxdream_get_global_config_value(key);
    if( str == NULL ) {
        return NULL;
    } else {
        return get_expanded_path(str);
    }
}

const gchar *lxdream_set_global_config_path_value( int key, const gchar *value )
{
    gchar *temp = get_escaped_path(value);
    lxdream_set_global_config_value(key,temp);
    g_free(temp);
    return lxdream_get_global_config_value(key);
}

struct lxdream_config_group * lxdream_get_config_group( int group )
{
    return lxdream_config_root[group];
}

void lxdream_copy_config_group( lxdream_config_group_t dest, lxdream_config_group_t src )
{
    int i;
    for( i=0; src->params[i].key != NULL; i++ ) {
        lxdream_set_config_value( dest, i, src->params[i].value );
    }
}

void lxdream_clone_config_group( lxdream_config_group_t dest, lxdream_config_group_t src )
{
    int i;

    dest->key = src->key;
    dest->on_change = NULL;
    dest->key_binding = NULL;
    dest->data = NULL;
    for( i=0; src->params[i].key != NULL; i++ ) {
        dest->params[i].key = src->params[i].key;
        dest->params[i].label = src->params[i].label;
        dest->params[i].type = src->params[i].type;
        dest->params[i].tag = src->params[i].tag;
        dest->params[i].default_value = src->params[i].default_value;
        dest->params[i].value = NULL;
        lxdream_set_config_value( dest, i, src->params[i].value );
    }
    dest->params[i].key = NULL;
    dest->params[i].label = NULL;
}

gboolean lxdream_load_config( )
{
    if( lxdream_config_load_filename == NULL ) {
        lxdream_find_config();
    }
    return lxdream_load_config_file(lxdream_config_load_filename);
}

/* Async/coalesced config save support */
static pthread_mutex_t cfg_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cfg_cond = PTHREAD_COND_INITIALIZER;
static gboolean cfg_worker_started = FALSE;
static gboolean cfg_dirty = FALSE;
static gboolean cfg_shutdown = FALSE;

static void *config_save_worker(void *arg)
{
    (void)arg;
    struct timespec ts;
    while( 1 ) {
        pthread_mutex_lock(&cfg_mutex);
        while( !cfg_dirty && !cfg_shutdown ) {
            pthread_cond_wait(&cfg_cond, &cfg_mutex);
        }
        if( cfg_shutdown ) {
            pthread_mutex_unlock(&cfg_mutex);
            break;
        }
        /* Coalesce multiple save requests within 500ms */
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 500 * 1000 * 1000;
        if( ts.tv_nsec >= 1000000000L ) { ts.tv_sec += 1; ts.tv_nsec -= 1000000000L; }
        while( cfg_dirty && !cfg_shutdown ) {
            int rc = pthread_cond_timedwait(&cfg_cond, &cfg_mutex, &ts);
            if( rc == ETIMEDOUT ) break; /* perform save */
        }
        cfg_dirty = FALSE;
        pthread_mutex_unlock(&cfg_mutex);

        /* Perform the actual save out of lock */
        if( lxdream_config_save_filename == NULL ) {
            lxdream_find_config();
        }
        int attempts = 0;
        const int max_attempts = 3;
        gboolean ok = FALSE;
        struct timespec backoff = { .tv_sec = 0, .tv_nsec = 200 * 1000 * 1000 };
        while( attempts < max_attempts ) {
            ok = lxdream_save_config_file(lxdream_config_save_filename);
            if( ok ) break;
            attempts++;
            nanosleep(&backoff, NULL);
            /* Exponential backoff (200ms, 400ms, 800ms) */
            long nn = backoff.tv_nsec * 2;
            if( nn >= 1000000000L ) { backoff.tv_sec += 1; nn -= 1000000000L; }
            backoff.tv_nsec = nn;
        }
        if( !ok ) {
            ERROR("Configuration save failed after %d attempts", max_attempts);
        }
    }
    return NULL;
}

static void config_start_worker_if_needed()
{
    if( !cfg_worker_started ) {
        pthread_t th;
        if( pthread_create(&th, NULL, config_save_worker, NULL) == 0 ) {
            pthread_detach(th);
            cfg_worker_started = TRUE;
        }
    }
}

gboolean lxdream_save_config( )
{
    config_start_worker_if_needed();
    pthread_mutex_lock(&cfg_mutex);
    cfg_dirty = TRUE;
    pthread_cond_signal(&cfg_cond);
    pthread_mutex_unlock(&cfg_mutex);
    return TRUE; /* report success; actual write is deferred */
}

/* Ensure any pending config writes are flushed before process exit */
void lxdream_flush_config_saves( void )
{
    pthread_mutex_lock(&cfg_mutex);
    if( cfg_worker_started ) {
        cfg_shutdown = TRUE;
        pthread_cond_signal(&cfg_cond);
        pthread_mutex_unlock(&cfg_mutex);
        /* Sleep briefly to allow worker to finish last save */
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 200 * 1000 * 1000 };
        nanosleep(&ts, NULL);
    } else {
        pthread_mutex_unlock(&cfg_mutex);
    }
}

gboolean lxdream_load_config_file( const gchar *filename )
{
    FILE *f;
    gboolean result;

    if( access(filename, F_OK) != 0 ) {
        INFO( "Configuration file '%s' does not exist, creating from defaults", filename );
        lxdream_set_default_config();
        lxdream_save_config();
    }

    f = fopen(filename, "r");
    if( f == NULL ) {
        ERROR( "Unable to open configuration file '%s': %s", filename, strerror(errno) );
        lxdream_set_default_config();
        return FALSE;
    }

    result = lxdream_load_config_stream( f );
    fclose(f);
    return result;
}

gboolean lxdream_load_config_stream( FILE *f )
{

    char buf[512];
    int maple_device = -1, maple_subdevice = -1, i;
    struct lxdream_config_group *group = NULL;
    struct lxdream_config_group *top_group = NULL;
    maple_device_t device = NULL;
    lxdream_set_default_config();

    while( fgets( buf, sizeof(buf), f ) != NULL ) {
        g_strstrip(buf);
        if( buf[0] == '#' )
            continue;
        if( *buf == '[' ) {
            char *p = strchr(buf, ']');
            if( p != NULL ) {
                maple_device = maple_subdevice = -1;
                *p = '\0';
                g_strstrip(buf+1);
                for( i=0; lxdream_config_root[i] != NULL; i++ ) {
                    if( strcasecmp(lxdream_config_root[i]->key, buf+1) == 0 ) {
                        top_group = group = lxdream_config_root[i];
                        break;
                    }
                }
            }
        } else if( group != NULL ) {
            char *value = strchr( buf, '=' );
            if( value != NULL ) {
                struct lxdream_config_entry *param = group->params;
                *value = '\0';
                value++;
                g_strstrip(buf);
                g_strstrip(value);
                if( top_group == &controllers_group ) {
                    if( g_strncasecmp( buf, "device ", 7 ) == 0 ) {
                        maple_device = strtoul( buf+7, NULL, 0 );
                        if( maple_device < 0 || maple_device > 3 ) {
                            ERROR( "Device number must be between 0..3 (not '%s')", buf+7);
                            continue;
                        }
                        maple_subdevice = 0;
                        device = maple_new_device( value );
                        if( device == NULL ) {
                            ERROR( "Unrecognized device '%s'", value );
                        } else {
                            group = maple_get_device_config(device);
                            maple_attach_device( device, maple_device, maple_subdevice );
                        }
                        continue;
                    } else if( g_strncasecmp( buf, "subdevice ", 10 ) == 0 ) {
                        maple_subdevice = strtoul( buf+10, NULL, 0 );
                        if( maple_device == -1 ) {
                            ERROR( "Subdevice not allowed without primary device" );
                        } else if( maple_subdevice < 1 || maple_subdevice > 5 ) {
                            ERROR( "Subdevice must be between 1..5 (not '%s')", buf+10 );
                        } else if( (device = maple_new_device(value)) == NULL ) {
                            ERROR( "Unrecognized subdevice '%s'", value );
                        } else {
                            group = maple_get_device_config(device);
                            maple_attach_device( device, maple_device, maple_subdevice );
                        }
                        continue;
                    }
                }
                while( param->key != NULL ) {
                    if( strcasecmp( param->key, buf ) == 0 ) {
                        param->value = g_strdup(value);
                        break;
                    }
                    param++;
                }
            }
        }
    }
    return TRUE;
}

gboolean lxdream_save_config_file( const gchar *filename )
{
    FILE *f = fopen(filename, "w");
    gboolean result;
    if( f == NULL ) {
        ERROR( "Unable to open '%s': %s", filename, strerror(errno) );
        return FALSE;
    }
    result = lxdream_save_config_stream(f);
    fclose(f);
    return TRUE;
}    

gboolean lxdream_save_config_stream( FILE *f )
{
    int i;
    for( i=0; lxdream_config_root[i] != NULL; i++ ) {
        fprintf( f, "[%s]\n", lxdream_config_root[i]->key );

        if( lxdream_config_root[i] == &controllers_group ) {
            int i,j;
            for( i=0; i<4; i++ ) {
                for( j=0; j<6; j++ ) {
                    maple_device_t dev = maple_get_device( i, j );
                    if( dev != NULL ) {
                        if( j == 0 )
                            fprintf( f, "Device %d = %s\n", i, dev->device_class->name );
                        else 
                            fprintf( f, "Subdevice %d = %s\n", j, dev->device_class->name );
                        lxdream_config_group_t group = maple_get_device_config(dev);
                        if( group != NULL ) {
                            lxdream_config_entry_t entry = group->params;
                            while( entry->key != NULL ) {
                                if( entry->value != NULL ) {
                                    fprintf( f, "%*c%s = %s\n", j==0?4:8, ' ',entry->key, entry->value );
                                }
                                entry++;
                            }
                        }
                    }
                }
            }
        } else {
            struct lxdream_config_entry *entry = lxdream_config_root[i]->params;
            while( entry->key != NULL ) {
                if( entry->value != NULL ) {
                    fprintf( f, "%s = %s\n", entry->key, entry->value );
                }
                entry++;
            }
        }
        fprintf( f, "\n" );
    }
    return TRUE;
}

void lxdream_make_config_dir( )
{
    const char *user_path = get_user_data_path();
    struct stat st;

    if( access( user_path, R_OK|X_OK ) == 0 && lstat( user_path, &st ) == 0 &&
            (st.st_mode & S_IFDIR) != 0 ) {
        /* All good */
        return;
    }
    
    if( mkdir( user_path, 0777 ) != 0 ) {
        ERROR( "Unable to create user configuration directory %s: %s", user_path, strerror(errno) );
        return;
    }

    char *vmupath = g_strdup_printf( "%s/vmu", user_path );
    mkdir( vmupath, 0777 );
    g_free( vmupath );
    
    char *savepath = g_strdup_printf( "%s/save", user_path );
    mkdir( savepath, 0777 );
    g_free( vmupath );
}
