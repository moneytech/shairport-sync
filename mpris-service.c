#include <stdio.h>
#include <string.h>

#include "config.h"

#include "common.h"
#include "player.h"
#include "rtsp.h"

#include "rtp.h"

#include "dacp.h"

#include "metadata_hub.h"
#include "mpris-service.h"

void mpris_metadata_watcher(struct metadata_bundle *argc, void *userdata) {
  // debug(1, "MPRIS metadata watcher called");
  char response[100];

  switch (argc->repeat_status) {
  case RS_NONE:
    strcpy(response, "None");
    break;
  case RS_SINGLE:
    strcpy(response, "Track");
    break;
  case RS_ALL:
    strcpy(response, "Playlist");
    break;
  }

  // debug(1,"Set loop status to \"%s\"",response);
  media_player2_player_set_loop_status(mprisPlayerPlayerSkeleton, response);
 
  switch (argc->player_state) {
  case PS_STOPPED:
    strcpy(response, "Stopped");
    break;
  case PS_PAUSED:
    strcpy(response, "Paused");
    break;
  case PS_PLAYING:
    strcpy(response, "Playing");
    break;
  }

  // debug(1,"From player_state, set playback status to \"%s\"",response);
  media_player2_player_set_playback_status(mprisPlayerPlayerSkeleton, response); 

  GVariantBuilder *dict_builder, *aa;
 
   /* Build the metadata array */
  // debug(1,"Build metadata");
  dict_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

  // Make up the artwork URI if we have one
  if (argc->cover_art_pathname) {
    char artURIstring[1024];
    sprintf(artURIstring, "file://%s", argc->cover_art_pathname);
    // sprintf(artURIstring,"");
    // debug(1,"artURI String: \"%s\".",artURIstring);
    GVariant *artUrl = g_variant_new("s", artURIstring);
    g_variant_builder_add(dict_builder, "{sv}", "mpris:artUrl", artUrl);
  }
  
  // Add the TrackID if we have one
  // Build the Track ID from the 16-byte item_composite_id in hex prefixed by
  // /org/gnome/ShairportSync
  char st[33];
  char *pt = st;
  int it;
  int non_zero = 0;
  for (it = 0; it < 16; it++) {
    if (argc->item_composite_id[it])
      non_zero = 1;
    sprintf(pt, "%02X", argc->item_composite_id[it]);
    pt += 2;
  }
  *pt = 0;
  if (non_zero) {
    //debug(1, "Set ID using composite ID: \"0x%s\".", st);
    char trackidstring[1024];
    sprintf(trackidstring, "/org/gnome/ShairportSync/%s", st);
    GVariant* trackid = g_variant_new("o", trackidstring);
    g_variant_builder_add(dict_builder, "{sv}", "mpris:trackid", trackid);
  } else if (argc->item_id) {
    char trackidstring[128];
    //debug(1, "Set ID using mper ID: \"%u\".",argc->item_id);
   sprintf(trackidstring, "/org/gnome/ShairportSync/mper_%u", argc->item_id);
    GVariant* trackid = g_variant_new("o", trackidstring);
    g_variant_builder_add(dict_builder, "{sv}", "mpris:trackid", trackid);  
  }
  
  // Add the track length if it's non-zero
  if (argc->songtime_in_milliseconds) {
   uint64_t track_length_in_microseconds = argc->songtime_in_milliseconds;
   track_length_in_microseconds *= 1000; // to microseconds in 64-bit precision
    // Make up the track name and album name
   //debug(1, "Set tracklength to %lu.", track_length_in_microseconds);
   GVariant *tracklength = g_variant_new("x", track_length_in_microseconds);
   g_variant_builder_add(dict_builder, "{sv}", "mpris:length", tracklength);
  }

  // Add the track name if there is one
  if (argc->track_name) {
    // debug(1, "Track name set to \"%s\".", argc->track_name);
    GVariant *trackname = g_variant_new("s", argc->track_name);
    g_variant_builder_add(dict_builder, "{sv}", "xesam:title", trackname);
  }
  
  // Add the album name if there is one
   if (argc->album_name) {
    // debug(1, "Album name set to \"%s\".", argc->album_name);
    GVariant *albumname = g_variant_new("s", argc->album_name);
    g_variant_builder_add(dict_builder, "{sv}", "xesam:album", albumname);
  }
  
  // Add the artists if there are any (actually there will be at most one, but put it in an array)
  if (argc->artist_name) {
    /* Build the artists array */
    // debug(1,"Build artist array");
    aa = g_variant_builder_new(G_VARIANT_TYPE("as"));
    g_variant_builder_add(aa, "s", argc->artist_name);
    GVariant *artists = g_variant_builder_end(aa);
    g_variant_builder_unref(aa);
    g_variant_builder_add(dict_builder, "{sv}", "xesam:artist", artists);
  }
  
  // Add the genres if there are any (actually there will be at most one, but put it in an array)
  if (argc->genre) {
    // debug(1,"Build genre");
    aa = g_variant_builder_new(G_VARIANT_TYPE("as"));
    g_variant_builder_add(aa, "s", argc->genre);
    GVariant *genres = g_variant_builder_end(aa);
    g_variant_builder_unref(aa);
    g_variant_builder_add(dict_builder, "{sv}", "xesam:genre", genres);
  }
 
  GVariant *dict = g_variant_builder_end(dict_builder);
  g_variant_builder_unref(dict_builder);

  // debug(1,"Set metadata");
  media_player2_player_set_metadata(mprisPlayerPlayerSkeleton, dict);
  
  media_player2_player_set_volume(mprisPlayerPlayerSkeleton, metadata_store.speaker_volume);

}

static gboolean on_handle_next(MediaPlayer2Player *skeleton, GDBusMethodInvocation *invocation,
                               gpointer user_data) {
  send_simple_dacp_command("nextitem");
  media_player2_player_complete_next(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_previous(MediaPlayer2Player *skeleton, GDBusMethodInvocation *invocation,
                                   gpointer user_data) {
  send_simple_dacp_command("previtem");
  media_player2_player_complete_previous(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_stop(MediaPlayer2Player *skeleton, GDBusMethodInvocation *invocation,
                               gpointer user_data) {
  send_simple_dacp_command("stop");
  media_player2_player_complete_stop(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_pause(MediaPlayer2Player *skeleton, GDBusMethodInvocation *invocation,
                                gpointer user_data) {
  send_simple_dacp_command("pause");
  media_player2_player_complete_pause(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_play_pause(MediaPlayer2Player *skeleton,
                                     GDBusMethodInvocation *invocation, gpointer user_data) {
  send_simple_dacp_command("playpause");
  media_player2_player_complete_play_pause(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_play(MediaPlayer2Player *skeleton, GDBusMethodInvocation *invocation,
                               gpointer user_data) {
  send_simple_dacp_command("play");
  media_player2_player_complete_play(skeleton, invocation);
  return TRUE;
}

static void on_mpris_name_acquired(GDBusConnection *connection, const gchar *name,
                                   gpointer user_data) {

  const char *empty_string_array[] = {NULL};

  // debug(1, "MPRIS well-known interface name \"%s\" acquired on the %s bus.", name, (config.mpris_service_bus_type == DBT_session) ? "session" : "system");
  mprisPlayerSkeleton = media_player2_skeleton_new();
  mprisPlayerPlayerSkeleton = media_player2_player_skeleton_new();

  g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(mprisPlayerSkeleton), connection,
                                   "/org/mpris/MediaPlayer2", NULL);
  g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(mprisPlayerPlayerSkeleton), connection,
                                   "/org/mpris/MediaPlayer2", NULL);

  media_player2_set_desktop_entry(mprisPlayerSkeleton, "shairport-sync");
  media_player2_set_identity(mprisPlayerSkeleton, "Shairport Sync");
  media_player2_set_can_quit(mprisPlayerSkeleton, FALSE);
  media_player2_set_can_raise(mprisPlayerSkeleton, FALSE);
  media_player2_set_has_track_list(mprisPlayerSkeleton, FALSE);
  media_player2_set_supported_uri_schemes(mprisPlayerSkeleton, empty_string_array);
  media_player2_set_supported_mime_types(mprisPlayerSkeleton, empty_string_array);

  media_player2_player_set_playback_status(mprisPlayerPlayerSkeleton, "Stopped");
  media_player2_player_set_loop_status(mprisPlayerPlayerSkeleton, "None");
  media_player2_player_set_volume(mprisPlayerPlayerSkeleton, 0.5);
  media_player2_player_set_minimum_rate(mprisPlayerPlayerSkeleton, 1.0);
  media_player2_player_set_maximum_rate(mprisPlayerPlayerSkeleton, 1.0);
  media_player2_player_set_can_go_next(mprisPlayerPlayerSkeleton, TRUE);
  media_player2_player_set_can_go_previous(mprisPlayerPlayerSkeleton, TRUE);
  media_player2_player_set_can_play(mprisPlayerPlayerSkeleton, TRUE);
  media_player2_player_set_can_pause(mprisPlayerPlayerSkeleton, TRUE);
  media_player2_player_set_can_seek(mprisPlayerPlayerSkeleton, FALSE);
  media_player2_player_set_can_control(mprisPlayerPlayerSkeleton, TRUE);

  g_signal_connect(mprisPlayerPlayerSkeleton, "handle-play", G_CALLBACK(on_handle_play), NULL);
  g_signal_connect(mprisPlayerPlayerSkeleton, "handle-pause", G_CALLBACK(on_handle_pause), NULL);
  g_signal_connect(mprisPlayerPlayerSkeleton, "handle-play-pause", G_CALLBACK(on_handle_play_pause),
                   NULL);
  g_signal_connect(mprisPlayerPlayerSkeleton, "handle-stop", G_CALLBACK(on_handle_stop), NULL);
  g_signal_connect(mprisPlayerPlayerSkeleton, "handle-next", G_CALLBACK(on_handle_next), NULL);
  g_signal_connect(mprisPlayerPlayerSkeleton, "handle-previous", G_CALLBACK(on_handle_previous),
                   NULL);

  add_metadata_watcher(mpris_metadata_watcher, NULL);

  debug(1, "MPRIS service started at \"%s\" on the %s bus.", name, (config.mpris_service_bus_type == DBT_session) ? "session" : "system");
}

static void on_mpris_name_lost_again(GDBusConnection *connection, const gchar *name,
                                     gpointer user_data) {
  warn("Could not acquire an MPRIS interface named \"%s\" on the %s bus.",name,(config.mpris_service_bus_type == DBT_session) ? "session" : "system");
}

static void on_mpris_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data) {
  //debug(1, "Could not acquire MPRIS interface \"%s\" on the %s bus -- will try adding the process "
  //         "number to the end of it.",
  //      name,(mpris_bus_type==G_BUS_TYPE_SESSION) ? "session" : "system");
  pid_t pid = getpid();
  char interface_name[256] = "";
  sprintf(interface_name, "org.mpris.MediaPlayer2.ShairportSync.i%d", pid);
  GBusType mpris_bus_type = G_BUS_TYPE_SYSTEM;
  if (config.mpris_service_bus_type == DBT_session)
    mpris_bus_type = G_BUS_TYPE_SESSION;
  // debug(1, "Looking for an MPRIS interface \"%s\" on the %s bus.",interface_name, (mpris_bus_type==G_BUS_TYPE_SESSION) ? "session" : "system");
  g_bus_own_name(mpris_bus_type, interface_name, G_BUS_NAME_OWNER_FLAGS_NONE, NULL,
                 on_mpris_name_acquired, on_mpris_name_lost_again, NULL, NULL);
}

int start_mpris_service() {
  mprisPlayerSkeleton = NULL;
  mprisPlayerPlayerSkeleton = NULL;
  GBusType mpris_bus_type = G_BUS_TYPE_SYSTEM;
  if (config.mpris_service_bus_type == DBT_session)
    mpris_bus_type = G_BUS_TYPE_SESSION;
  // debug(1, "Looking for an MPRIS interface \"org.mpris.MediaPlayer2.ShairportSync\" on the %s bus.",(mpris_bus_type==G_BUS_TYPE_SESSION) ? "session" : "system");
  g_bus_own_name(mpris_bus_type, "org.mpris.MediaPlayer2.ShairportSync",
                 G_BUS_NAME_OWNER_FLAGS_NONE, NULL, on_mpris_name_acquired, on_mpris_name_lost,
                 NULL, NULL);
  return 0; // this is just to quieten a compiler warning
}
