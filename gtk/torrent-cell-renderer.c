/*
 * This file Copyright (C) 2007-2010 Mnemosyne LLC
 *
 * This file is licensed by the GPL version 2. Works owned by the
 * Transmission project are granted a special exemption to clause 2(b)
 * so that the bulk of its code can remain under the MIT license.
 * This exemption does not extend to derived works not owned by
 * the Transmission project.
 *
 * $Id$
 */

#include <string.h> /* strcmp() */
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libtransmission/transmission.h>
#include <libtransmission/utils.h> /* tr_truncd() */
#include "hig.h"
#include "icons.h"
#include "pieces-cell-renderer.h"
#include "torrent-cell-renderer.h"
#include "tr-torrent.h"
#include "util.h"

/* #define TEST_RTL */

enum
{
    P_TORRENT = 1,
    P_UPLOAD_SPEED,
    P_DOWNLOAD_SPEED,
    P_BAR_HEIGHT,
    P_COMPACT,
    P_SHOW_PIECES
};

#define DEFAULT_BAR_HEIGHT 12
#define SMALL_SCALE 0.9
#define COMPACT_ICON_SIZE GTK_ICON_SIZE_MENU
#define FULL_ICON_SIZE GTK_ICON_SIZE_DND

/***
****
***/

static char*
getProgressString( const tr_torrent * tor,
                   const tr_info    * info,
                   const tr_stat    * torStat )
{
    const int      isDone = torStat->leftUntilDone == 0;
    const uint64_t haveTotal = torStat->haveUnchecked + torStat->haveValid;
    const int      isSeed = torStat->haveValid >= info->totalSize;
    char           buf1[32], buf2[32], buf3[32], buf4[32], buf5[32], buf6[32];
    char *         str;
    double         seedRatio;
    const gboolean hasSeedRatio = tr_torrentGetSeedRatio( tor, &seedRatio );

    if( !isDone ) /* downloading */
    {
        str = g_strdup_printf(
            /* %1$s is how much we've got,
               %2$s is how much we'll have when done,
               %3$s%% is a percentage of the two */
            _( "%1$s of %2$s (%3$s%%)" ),
            tr_strlsize( buf1, haveTotal, sizeof( buf1 ) ),
            tr_strlsize( buf2, torStat->sizeWhenDone, sizeof( buf2 ) ),
            tr_strlpercent( buf3, torStat->percentDone * 100.0, sizeof( buf3 ) ) );
    }
    else if( !isSeed ) /* partial seeds */
    {
        if( hasSeedRatio )
        {
            str = g_strdup_printf(
                /* %1$s is how much we've got,
                   %2$s is the torrent's total size,
                   %3$s%% is a percentage of the two,
                   %4$s is how much we've uploaded,
                   %5$s is our upload-to-download ratio,
                   %6$s is the ratio we want to reach before we stop uploading */
                _( "%1$s of %2$s (%3$s%%), uploaded %4$s (Ratio: %5$s Goal: %6$s)" ),
                tr_strlsize( buf1, haveTotal, sizeof( buf1 ) ),
                tr_strlsize( buf2, info->totalSize, sizeof( buf2 ) ),
                tr_strlpercent( buf3, torStat->percentComplete * 100.0, sizeof( buf3 ) ),
                tr_strlsize( buf4, torStat->uploadedEver, sizeof( buf4 ) ),
                tr_strlratio( buf5, torStat->ratio, sizeof( buf5 ) ),
                tr_strlratio( buf6, seedRatio, sizeof( buf6 ) ) );
        }
        else
        {
            str = g_strdup_printf(
                /* %1$s is how much we've got,
                   %2$s is the torrent's total size,
                   %3$s%% is a percentage of the two,
                   %4$s is how much we've uploaded,
                   %5$s is our upload-to-download ratio */
                _( "%1$s of %2$s (%3$s%%), uploaded %4$s (Ratio: %5$s)" ),
                tr_strlsize( buf1, haveTotal, sizeof( buf1 ) ),
                tr_strlsize( buf2, info->totalSize, sizeof( buf2 ) ),
                tr_strlpercent( buf3, torStat->percentComplete * 100.0, sizeof( buf3 ) ),
                tr_strlsize( buf4, torStat->uploadedEver, sizeof( buf4 ) ),
                tr_strlratio( buf5, torStat->ratio, sizeof( buf5 ) ) );
        }
    }
    else /* seeding */
    {
        if( hasSeedRatio )
        {
            str = g_strdup_printf(
                /* %1$s is the torrent's total size,
                   %2$s is how much we've uploaded,
                   %3$s is our upload-to-download ratio,
                   %4$s is the ratio we want to reach before we stop uploading */
                _( "%1$s, uploaded %2$s (Ratio: %3$s Goal: %4$s)" ),
                tr_strlsize( buf1, info->totalSize, sizeof( buf1 ) ),
                tr_strlsize( buf2, torStat->uploadedEver, sizeof( buf2 ) ),
                tr_strlratio( buf3, torStat->ratio, sizeof( buf3 ) ),
                tr_strlratio( buf4, seedRatio, sizeof( buf4 ) ) );
        }
        else /* seeding w/o a ratio */
        {
            str = g_strdup_printf(
                /* %1$s is the torrent's total size,
                   %2$s is how much we've uploaded,
                   %3$s is our upload-to-download ratio */
                _( "%1$s, uploaded %2$s (Ratio: %3$s)" ),
                tr_strlsize( buf1, info->totalSize, sizeof( buf1 ) ),
                tr_strlsize( buf2, torStat->uploadedEver, sizeof( buf2 ) ),
                tr_strlratio( buf3, torStat->ratio, sizeof( buf3 ) ) );
        }
    }

    /* add time when downloading */
    if( ( torStat->activity == TR_STATUS_DOWNLOAD )
        || ( hasSeedRatio && ( torStat->activity == TR_STATUS_SEED ) ) )
    {
        const int eta = torStat->eta;
        GString * gstr = g_string_new( str );
        g_string_append( gstr, " - " );
        if( eta < 0 )
            g_string_append( gstr, _( "Remaining time unknown" ) );
        else
        {
            char timestr[128];
            tr_strltime( timestr, eta, sizeof( timestr ) );
            /* time remaining */
            g_string_append_printf( gstr, _( "%s remaining" ), timestr );
        }
        g_free( str );
        str = g_string_free( gstr, FALSE );
    }

    return str;
}

static char*
getShortTransferString( const tr_torrent  * tor,
                        const tr_stat     * torStat,
                        double              uploadSpeed_KBps,
                        double              downloadSpeed_KBps,
                        char              * buf,
                        size_t              buflen )
{
    char downStr[32], upStr[32];
    const int haveMeta = tr_torrentHasMetadata( tor );
    const int haveDown = haveMeta && torStat->peersSendingToUs > 0;
    const int haveUp = haveMeta && torStat->peersGettingFromUs > 0;

    if( haveDown )
        tr_formatter_speed_KBps( downStr, downloadSpeed_KBps, sizeof( downStr ) );
    if( haveUp )
        tr_formatter_speed_KBps( upStr, uploadSpeed_KBps, sizeof( upStr ) );

    if( haveDown && haveUp )
        /* 1==down arrow, 2==down speed, 3==up arrow, 4==down speed */
        g_snprintf( buf, buflen, _( "%1$s %2$s, %3$s %4$s" ),
                    gtr_get_unicode_string( GTR_UNICODE_DOWN ), downStr,
                    gtr_get_unicode_string( GTR_UNICODE_UP ), upStr );
    else if( haveDown )
        /* bandwidth speed + unicode arrow */
        g_snprintf( buf, buflen, _( "%1$s %2$s" ),
                    gtr_get_unicode_string( GTR_UNICODE_DOWN ), downStr );
    else if( haveUp )
        /* bandwidth speed + unicode arrow */
        g_snprintf( buf, buflen, _( "%1$s %2$s" ),
                    gtr_get_unicode_string( GTR_UNICODE_UP ), upStr );
    else if( haveMeta )
        /* the torrent isn't uploading or downloading */
        g_strlcpy( buf, _( "Idle" ), buflen );
    else
        *buf = '\0';

    return buf;
}

static char*
getShortStatusString( const tr_torrent  * tor,
                      const tr_stat     * torStat,
                      double              uploadSpeed_KBps,
                      double              downloadSpeed_KBps )
{
    GString * gstr = g_string_new( NULL );

    switch( torStat->activity )
    {
        case TR_STATUS_STOPPED:
            if( torStat->finished )
                g_string_assign( gstr, _( "Finished" ) );
            else
                g_string_assign( gstr, _( "Paused" ) );
            break;

        case TR_STATUS_CHECK_WAIT:
            g_string_assign( gstr, _( "Waiting to verify local data" ) );
            break;

        case TR_STATUS_CHECK:
            g_string_append_printf( gstr,
                                    _( "Verifying local data (%.1f%% tested)" ),
                                    tr_truncd( torStat->recheckProgress * 100.0, 1 ) );
            break;

        case TR_STATUS_DOWNLOAD:
        case TR_STATUS_SEED:
        {
            const tr_info * info = tr_torrentInfo( tor );
            char buf[512];
            if( info->trackerCount <= 0 )
            {
                /* 1==seeder symbol, 2==seeders connected,
                   3==leecher symbol, 4==leechers connected */
                g_string_append_printf( gstr, _( "%1$s %2$d/?  %3$s %4$d/?" ),
                    gtr_get_unicode_string( GTR_UNICODE_SEEDER ),
                    torStat->seedersConnected + torStat->webseedsSendingToUs,
                    gtr_get_unicode_string( GTR_UNICODE_LEECHER ),
                    torStat->leechersConnected );
            }
            else
            {
                /* 1==seeder symbol, 2==seeders connected, 3==seeder count,
                   4==leecher symbol, 5==leechers connected, 6==leecher count */
                g_string_append_printf( gstr, _( "%1$s %2$d/%3$d  %4$s %5$d/%6$d" ),
                    gtr_get_unicode_string( GTR_UNICODE_SEEDER ),
                    torStat->seedersConnected + torStat->webseedsSendingToUs,
                    torStat->swarmSeeders + torStat->webseedsSendingToUs,
                    gtr_get_unicode_string( GTR_UNICODE_LEECHER ),
                    torStat->leechersConnected,
                    torStat->swarmLeechers );
            }
            g_string_append( gstr, " - " );

            if( torStat->activity != TR_STATUS_DOWNLOAD )
            {
                tr_strlratio( buf, torStat->ratio, sizeof( buf ) );
                g_string_append_printf( gstr, _( "Ratio %s" ), buf );
                g_string_append( gstr, ", " );
            }
            getShortTransferString( tor, torStat, uploadSpeed_KBps, downloadSpeed_KBps, buf, sizeof( buf ) );
            g_string_append( gstr, buf );
            break;
        }

        default:
            break;
    }

    return g_string_free( gstr, FALSE );
}

static char*
getStatusString( const tr_torrent  * tor,
                 const tr_stat     * torStat,
                 const double        uploadSpeed_KBps,
                 const double        downloadSpeed_KBps )
{
    const int isActive = torStat->activity != TR_STATUS_STOPPED;
    const int isChecking = torStat->activity == TR_STATUS_CHECK
                        || torStat->activity == TR_STATUS_CHECK_WAIT;

    GString * gstr = g_string_new( NULL );

    if( torStat->error )
    {
        const char * fmt[] = { NULL, N_( "Tracker gave a warning: \"%s\"" ),
                                     N_( "Tracker gave an error: \"%s\"" ),
                                     N_( "Error: %s" ) };
        g_string_append_printf( gstr, _( fmt[torStat->error] ), torStat->errorString );
    }
    else switch( torStat->activity )
    {
        case TR_STATUS_STOPPED:
        case TR_STATUS_CHECK_WAIT:
        case TR_STATUS_CHECK:
        {
            char * pch = getShortStatusString( tor, torStat, uploadSpeed_KBps, downloadSpeed_KBps );
            g_string_assign( gstr, pch );
            g_free( pch );
            break;
        }

        case TR_STATUS_DOWNLOAD:
        {
            if( tr_torrentHasMetadata( tor ) )
            {
                const tr_info * info = tr_torrentInfo( tor );
                g_string_append_printf( gstr,
                    gtr_ngettext( "Downloading from %1$'d of %2$'d connected peer",
                                  "Downloading from %1$'d of %2$'d connected peers",
                                  torStat->peersConnected ),
                    torStat->peersSendingToUs +
                    torStat->webseedsSendingToUs,
                    torStat->peersConnected +
                    torStat->webseedsSendingToUs );
                g_string_append( gstr, " - " );
                if( info->trackerCount <= 0 )
                {
                    g_string_append_printf( gstr, "Swarm size unknown" );
                }
                else
                {
                    const char * fmt = info->trackerCount == 1
                        ? _( "Swarm has %d seeders and %d leechers" )
                        : _( "Swarm has at least %d seeders and %d leechers" );
                    g_string_append_printf( gstr, fmt,
                        torStat->swarmSeeders +
                        torStat->webseedsSendingToUs,
                        torStat->swarmLeechers );
                }
            }
            else
            {
                g_string_append_printf( gstr,
                    gtr_ngettext( "Downloading metadata from %1$'d peer (%2$d%% done)",
                                  "Downloading metadata from %1$'d peers (%2$d%% done)",
                                  torStat->peersConnected ),
                    torStat->peersConnected + torStat->webseedsSendingToUs,
                    (int)(100.0*torStat->metadataPercentComplete) );
            }
            break;
        }

        case TR_STATUS_SEED:
            g_string_append_printf( gstr,
                gtr_ngettext( "Seeding to %1$'d of %2$'d connected peer",
                              "Seeding to %1$'d of %2$'d connected peers",
                              torStat->peersConnected ),
                torStat->peersGettingFromUs,
                torStat->peersConnected );
                break;
    }

    if( isActive && !isChecking )
    {
        char buf[256];
        getShortTransferString( tor, torStat, uploadSpeed_KBps, downloadSpeed_KBps, buf, sizeof( buf ) );
        if( *buf )
            g_string_append_printf( gstr, " - %s", buf );
    }

    return g_string_free( gstr, FALSE );
}

/***
****
***/

static GtkCellRendererClass * parent_class = NULL;

struct TorrentCellRendererPrivate
{
    tr_torrent       * tor;
    GtkCellRenderer  * text_renderer;
    GtkCellRenderer  * text_renderer_err;
    GtkCellRenderer  * pbar_renderer;
    GtkCellRenderer  * prog_renderer;
    GtkCellRenderer  * icon_renderer;
    int bar_height;

    /* Use this instead of tr_stat.pieceUploadSpeed so that the model can
       control when the speed displays get updated. This is done to keep
       the individual torrents' speeds and the status bar's overall speed
       in sync even if they refresh at slightly different times */
    double upload_speed_KBps;

    /* @see upload_speed_Bps */
    double download_speed_KBps;

    gboolean compact;
    gboolean show_pieces;
};

/***
****
***/

static GdkPixbuf*
get_icon( const tr_torrent * tor, GtkIconSize icon_size, GtkWidget * for_widget )
{
    const char * mime_type;
    const tr_info * info = tr_torrentInfo( tor );

    if( info->fileCount == 0  )
        mime_type = UNKNOWN_MIME_TYPE;
    else if( info->fileCount > 1 )
        mime_type = DIRECTORY_MIME_TYPE;
    else if( strchr( info->files[0].name, '/' ) != NULL )
        mime_type = DIRECTORY_MIME_TYPE;
    else
        mime_type = gtr_get_mime_type_from_filename( info->files[0].name );

    return gtr_get_mime_type_icon( mime_type, icon_size, for_widget );
}

static GtkCellRenderer*
get_text_renderer( const tr_stat * st, TorrentCellRenderer * r )
{
    return st->error ? r->priv->text_renderer_err : r->priv->text_renderer;
}

static inline int
bar_width( TorrentCellRenderer * cell )
{
    return cell->priv->show_pieces ? 100 : 50;
}

/***
****
***/

static void
get_size_compact( TorrentCellRenderer * cell,
                  GtkWidget           * widget,
                  gint                * width,
                  gint                * height )
{
    int w, h;
    int xpad, ypad;
    GdkRectangle icon_area;
    GdkRectangle name_area;
    GdkRectangle stat_area;
    const char * name;
    char * status;
    GdkPixbuf * icon;
    GtkCellRenderer * text_renderer;

    struct TorrentCellRendererPrivate * p = cell->priv;
    const tr_torrent * tor = p->tor;
    const tr_stat * st = tr_torrentStatCached( (tr_torrent*)tor );

    icon = get_icon( tor, COMPACT_ICON_SIZE, widget );
    name = tr_torrentInfo( tor )->name;
    status = getShortStatusString( tor, st, p->upload_speed_KBps, p->download_speed_KBps );
    gtr_cell_renderer_get_padding( GTK_CELL_RENDERER( cell ), &xpad, &ypad );

    /* get the idealized cell dimensions */
    g_object_set( p->icon_renderer, "pixbuf", icon, NULL );
    gtk_cell_renderer_get_size( p->icon_renderer, widget, NULL, NULL, NULL, &w, &h );
    icon_area.width = w;
    icon_area.height = h;
    text_renderer = get_text_renderer( st, cell );
    g_object_set( text_renderer, "text", name, "ellipsize", PANGO_ELLIPSIZE_NONE,  "scale", 1.0, NULL );
    gtk_cell_renderer_get_size( text_renderer, widget, NULL, NULL, NULL, &w, &h );
    name_area.width = w;
    name_area.height = h;
    g_object_set( text_renderer, "text", status, "scale", SMALL_SCALE, NULL );
    gtk_cell_renderer_get_size( text_renderer, widget, NULL, NULL, NULL, &w, &h );
    stat_area.width = w;
    stat_area.height = h;

    /**
    *** LAYOUT
    **/

    if( width != NULL )
        *width = ( xpad * 2 + icon_area.width + GUI_PAD + name_area.width
                   + GUI_PAD + bar_width( cell ) + GUI_PAD + stat_area.width );
    if( height != NULL )
        *height = ypad * 2 + MAX( name_area.height, p->bar_height );

    /* cleanup */
    g_free( status );
    g_object_unref( icon );
}

#define MAX3(a,b,c) MAX(a,MAX(b,c))

static void
get_size_full( TorrentCellRenderer * cell,
               GtkWidget           * widget,
               gint                * width,
               gint                * height )
{
    int w, h;
    int xpad, ypad;
    GdkRectangle icon_area;
    GdkRectangle name_area;
    GdkRectangle stat_area;
    GdkRectangle prog_area;
    const char * name;
    char * status;
    char * progress;
    GdkPixbuf * icon;
    GtkCellRenderer * text_renderer;

    struct TorrentCellRendererPrivate * p = cell->priv;
    const tr_torrent * tor = p->tor;
    const tr_stat * st = tr_torrentStatCached( (tr_torrent*)tor );
    const tr_info * inf = tr_torrentInfo( tor );

    icon = get_icon( tor, FULL_ICON_SIZE, widget );
    name = inf->name;
    status = getStatusString( tor, st, p->upload_speed_KBps, p->download_speed_KBps );
    progress = getProgressString( tor, inf, st );
    gtr_cell_renderer_get_padding( GTK_CELL_RENDERER( cell ), &xpad, &ypad );

    /* get the idealized cell dimensions */
    g_object_set( p->icon_renderer, "pixbuf", icon, NULL );
    gtk_cell_renderer_get_size( p->icon_renderer, widget, NULL, NULL, NULL, &w, &h );
    icon_area.width = w;
    icon_area.height = h;
    text_renderer = get_text_renderer( st, cell );
    g_object_set( text_renderer, "text", name, "weight", PANGO_WEIGHT_BOLD, "scale", 1.0, "ellipsize", PANGO_ELLIPSIZE_NONE, NULL );
    gtk_cell_renderer_get_size( text_renderer, widget, NULL, NULL, NULL, &w, &h );
    name_area.width = w;
    name_area.height = h;
    g_object_set( text_renderer, "text", progress, "weight", PANGO_WEIGHT_NORMAL, "scale", SMALL_SCALE, NULL );
    gtk_cell_renderer_get_size( text_renderer, widget, NULL, NULL, NULL, &w, &h );
    prog_area.width = w;
    prog_area.height = h;
    g_object_set( text_renderer, "text", status, NULL );
    gtk_cell_renderer_get_size( text_renderer, widget, NULL, NULL, NULL, &w, &h );
    stat_area.width = w;
    stat_area.height = h;

    /**
    *** LAYOUT
    **/

    if( width != NULL )
        *width = xpad * 2 + icon_area.width + GUI_PAD + MAX3( name_area.width, prog_area.width, stat_area.width );
    if( height != NULL )
        *height = ypad * 2 + name_area.height + prog_area.height + GUI_PAD_SMALL + p->bar_height + GUI_PAD_SMALL + stat_area.height;

    /* cleanup */
    g_free( status );
    g_free( progress );
    g_object_unref( icon );
}


static void
torrent_cell_renderer_get_size( GtkCellRenderer  * cell,
                                GtkWidget        * widget,
                                GdkRectangle     * cell_area,
                                gint             * x_offset,
                                gint             * y_offset,
                                gint             * width,
                                gint             * height )
{
    TorrentCellRenderer * self = TORRENT_CELL_RENDERER( cell );

    if( self && self->priv->tor )
    {
        int w, h;
        struct TorrentCellRendererPrivate * p = self->priv;

        if( p->compact )
            get_size_compact( TORRENT_CELL_RENDERER( cell ), widget, &w, &h );
        else
            get_size_full( TORRENT_CELL_RENDERER( cell ), widget, &w, &h );

        if( width )
            *width = w;

        if( height )
            *height = h;

        if( x_offset )
            *x_offset = cell_area ? cell_area->x : 0;

        if( y_offset ) {
            int xpad, ypad;
            gtr_cell_renderer_get_padding( cell, &xpad, &ypad );
            *y_offset = cell_area ? (int)((cell_area->height - (ypad*2 +h)) / 2.0) : 0;
        }
    }
}

static void
render_compact( TorrentCellRenderer   * cell,
                GdkDrawable           * window,
                GtkWidget             * widget,
                GdkRectangle          * background_area,
                GdkRectangle          * cell_area UNUSED,
                GdkRectangle          * expose_area UNUSED,
                GtkCellRendererState    flags )
{
    int xpad, ypad;
    GdkRectangle icon_area;
    GdkRectangle name_area;
    GdkRectangle stat_area;
    GdkRectangle pbar_area;
    GdkRectangle fill_area;
    const char * name;
    char * status;
    GdkPixbuf * icon;
    GtkCellRenderer * text_renderer;

    struct TorrentCellRendererPrivate * p = cell->priv;
    const tr_torrent * tor = p->tor;
    const tr_stat * st = tr_torrentStatCached( (tr_torrent*)tor );
    const gboolean active = st->activity != TR_STATUS_STOPPED;
    const gboolean sensitive = active || st->error;

    icon = get_icon( tor, COMPACT_ICON_SIZE, widget );
    name = tr_torrentInfo( tor )->name;
    status = getShortStatusString( tor, st, p->upload_speed_KBps, p->download_speed_KBps );
    gtr_cell_renderer_get_padding( GTK_CELL_RENDERER( cell ), &xpad, &ypad );

    fill_area = *background_area;
    fill_area.x += xpad;
    fill_area.y += ypad;
    fill_area.width -= xpad * 2;
    fill_area.height -= ypad * 2;
    icon_area = name_area = stat_area = pbar_area = fill_area;

    g_object_set( p->icon_renderer, "pixbuf", icon, NULL );
    gtk_cell_renderer_get_size( p->icon_renderer, widget, NULL, NULL, NULL, &icon_area.width, NULL );
    text_renderer = get_text_renderer( st, cell );
    g_object_set( text_renderer, "text", name, "ellipsize", PANGO_ELLIPSIZE_NONE, "scale", 1.0, NULL );
    gtk_cell_renderer_get_size( text_renderer, widget, NULL, NULL, NULL, &name_area.width, NULL );
    g_object_set( text_renderer, "text", status, "scale", SMALL_SCALE, NULL );
    gtk_cell_renderer_get_size( text_renderer, widget, NULL, NULL, NULL, &stat_area.width, NULL );

    icon_area.x = fill_area.x;
    pbar_area.x = fill_area.x + fill_area.width - bar_width( cell );
    pbar_area.width = bar_width( cell );
    stat_area.x = pbar_area.x - GUI_PAD - stat_area.width;
    name_area.x = icon_area.x + icon_area.width + GUI_PAD;
    name_area.y = fill_area.y;
    name_area.width = stat_area.x - GUI_PAD - name_area.x;

    /**
    *** RENDER
    **/

    g_object_set( p->icon_renderer, "pixbuf", icon, "sensitive", sensitive, NULL );
    gtk_cell_renderer_render( p->icon_renderer, window, widget, &icon_area, &icon_area, &icon_area, flags );
    if( p->show_pieces )
    {
        g_object_set( p->pbar_renderer, "sensitive", sensitive, NULL );
        gtk_cell_renderer_render( p->pbar_renderer, window, widget, &pbar_area, &pbar_area, &pbar_area, flags );
    }
    else
    {
        const double percentDone = MAX( 0.0, st->percentDone );
        g_object_set( p->prog_renderer, "value", (int)(percentDone*100.0), "text", NULL, "sensitive", sensitive, NULL );
        gtk_cell_renderer_render( p->prog_renderer, window, widget, &pbar_area, &pbar_area, &pbar_area, flags );
    }
    g_object_set( text_renderer, "text", status, "scale", SMALL_SCALE, "sensitive", sensitive, "ellipsize", PANGO_ELLIPSIZE_END, NULL );
    gtk_cell_renderer_render( text_renderer, window, widget, &stat_area, &stat_area, &stat_area, flags );
    g_object_set( text_renderer, "text", name, "scale", 1.0, NULL );
    gtk_cell_renderer_render( text_renderer, window, widget, &name_area, &name_area, &name_area, flags );

    /* cleanup */
    g_free( status );
    g_object_unref( icon );
}

static void
render_full( TorrentCellRenderer   * cell,
             GdkDrawable           * window,
             GtkWidget             * widget,
             GdkRectangle          * background_area,
             GdkRectangle          * cell_area UNUSED,
             GdkRectangle          * expose_area UNUSED,
             GtkCellRendererState    flags )
{
    int w, h;
    int xpad, ypad;
    GdkRectangle fill_area;
    GdkRectangle icon_area;
    GdkRectangle name_area;
    GdkRectangle stat_area;
    GdkRectangle prog_area;
    GdkRectangle pbar_area;
    const char * name;
    char * status;
    char * progress;
    GdkPixbuf * icon;
    GtkCellRenderer * text_renderer;

    struct TorrentCellRendererPrivate * p = cell->priv;
    const tr_torrent * tor = p->tor;
    const tr_stat * st = tr_torrentStatCached( (tr_torrent*)tor );
    const tr_info * inf = tr_torrentInfo( tor );
    const gboolean active = st->activity != TR_STATUS_STOPPED;
    const gboolean sensitive = active || st->error;

    icon = get_icon( tor, FULL_ICON_SIZE, widget );
    name = inf->name;
    status = getStatusString( tor, st, p->upload_speed_KBps, p->download_speed_KBps );
    progress = getProgressString( tor, inf, st );
    gtr_cell_renderer_get_padding( GTK_CELL_RENDERER( cell ), &xpad, &ypad );

    /* get the idealized cell dimensions */
    g_object_set( p->icon_renderer, "pixbuf", icon, NULL );
    gtk_cell_renderer_get_size( p->icon_renderer, widget, NULL, NULL, NULL, &w, &h );
    icon_area.width = w;
    icon_area.height = h;
    text_renderer = get_text_renderer( st, cell );
    g_object_set( text_renderer, "text", name, "weight", PANGO_WEIGHT_BOLD, "ellipsize", PANGO_ELLIPSIZE_NONE, "scale", 1.0, NULL );
    gtk_cell_renderer_get_size( text_renderer, widget, NULL, NULL, NULL, &w, &h );
    name_area.width = w;
    name_area.height = h;
    g_object_set( text_renderer, "text", progress, "weight", PANGO_WEIGHT_NORMAL, "scale", SMALL_SCALE, NULL );
    gtk_cell_renderer_get_size( text_renderer, widget, NULL, NULL, NULL, &w, &h );
    prog_area.width = w;
    prog_area.height = h;
    g_object_set( text_renderer, "text", status, NULL );
    gtk_cell_renderer_get_size( text_renderer, widget, NULL, NULL, NULL, &w, &h );
    stat_area.width = w;
    stat_area.height = h;

    /**
    *** LAYOUT
    **/

    fill_area = *background_area;
    fill_area.x += xpad;
    fill_area.y += ypad;
    fill_area.width -= xpad * 2;
    fill_area.height -= ypad * 2;

    /* icon */
    icon_area.x = fill_area.x;
    icon_area.y = fill_area.y + ( fill_area.height - icon_area.height ) / 2;

    /* name */
    name_area.x = icon_area.x + icon_area.width + GUI_PAD;
    name_area.y = fill_area.y;
    name_area.width = fill_area.width - GUI_PAD - icon_area.width - GUI_PAD_SMALL;

    /* progress string */
    prog_area.x = name_area.x;
    prog_area.y = name_area.y + name_area.height;
    prog_area.width = name_area.width;

    /* progress bar */
    pbar_area.x = prog_area.x;
    pbar_area.y = prog_area.y + prog_area.height + GUI_PAD_SMALL;
    pbar_area.width = prog_area.width;
    pbar_area.height = p->bar_height;

    /* status */
    stat_area.x = pbar_area.x;
    stat_area.y = pbar_area.y + pbar_area.height + GUI_PAD_SMALL;
    stat_area.width = pbar_area.width;

    /**
    *** RENDER
    **/

    g_object_set( p->icon_renderer, "pixbuf", icon, "sensitive", sensitive, NULL );
    gtk_cell_renderer_render( p->icon_renderer, window, widget, &icon_area, &icon_area, &icon_area, flags );
    g_object_set( text_renderer, "text", name, "scale", 1.0, "sensitive", sensitive, "ellipsize", PANGO_ELLIPSIZE_END, "weight", PANGO_WEIGHT_BOLD, NULL );
    gtk_cell_renderer_render( text_renderer, window, widget, &name_area, &name_area, &name_area, flags );
    g_object_set( text_renderer, "text", progress, "scale", SMALL_SCALE, "weight", PANGO_WEIGHT_NORMAL, NULL );
    gtk_cell_renderer_render( text_renderer, window, widget, &prog_area, &prog_area, &prog_area, flags );
    if( p->show_pieces )
    {
        g_object_set( p->pbar_renderer, "sensitive", sensitive, NULL );
        gtk_cell_renderer_render( p->pbar_renderer, window, widget, &pbar_area, &pbar_area, &pbar_area, flags );
    }
    else
    {
        const double percentDone = MAX( 0.0, st->percentDone );
        g_object_set( p->prog_renderer, "value", (int)(percentDone*100.0), "text", "", "sensitive", sensitive, NULL );
        gtk_cell_renderer_render( p->prog_renderer, window, widget, &pbar_area, &pbar_area, &pbar_area, flags );
    }
    g_object_set( text_renderer, "text", status, NULL );
    gtk_cell_renderer_render( text_renderer, window, widget, &stat_area, &stat_area, &stat_area, flags );

    /* cleanup */
    g_free( status );
    g_free( progress );
    g_object_unref( icon );
}

static void
torrent_cell_renderer_render( GtkCellRenderer       * cell,
                              GdkDrawable           * window,
                              GtkWidget             * widget,
                              GdkRectangle          * background_area,
                              GdkRectangle          * cell_area,
                              GdkRectangle          * expose_area,
                              GtkCellRendererState    flags )
{
    TorrentCellRenderer * self = TORRENT_CELL_RENDERER( cell );

#ifdef TEST_RTL
    GtkTextDirection      real_dir = gtk_widget_get_direction( widget );
    gtk_widget_set_direction( widget, GTK_TEXT_DIR_RTL );
#endif

    if( self && self->priv->tor )
    {
        struct TorrentCellRendererPrivate * p = self->priv;
        if( p->compact )
            render_compact( self, window, widget, background_area, cell_area, expose_area, flags );
        else
            render_full( self, window, widget, background_area, cell_area, expose_area, flags );
    }

#ifdef TEST_RTL
    gtk_widget_set_direction( widget, real_dir );
#endif
}

static void
torrent_cell_renderer_set_property( GObject      * object,
                                    guint          property_id,
                                    const GValue * v,
                                    GParamSpec   * pspec )
{
    TorrentCellRenderer * self = TORRENT_CELL_RENDERER( object );
    struct TorrentCellRendererPrivate * p = self->priv;

    switch( property_id )
    {
        case P_TORRENT:
            p->tor = g_value_get_pointer( v );
            g_object_set( p->pbar_renderer, "torrent", p->tor, NULL );
            break;
        case P_UPLOAD_SPEED:   p->upload_speed_KBps   = g_value_get_double( v ); break;
        case P_DOWNLOAD_SPEED: p->download_speed_KBps = g_value_get_double( v ); break;
        case P_BAR_HEIGHT:     p->bar_height          = g_value_get_int( v ); break;
        case P_COMPACT:        p->compact             = g_value_get_boolean( v ); break;
        case P_SHOW_PIECES:    p->show_pieces         = g_value_get_boolean( v ); break;
        default: G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, pspec ); break;
    }
}

static void
torrent_cell_renderer_get_property( GObject     * object,
                                    guint         property_id,
                                    GValue      * v,
                                    GParamSpec  * pspec )
{
    const TorrentCellRenderer * self = TORRENT_CELL_RENDERER( object );
    struct TorrentCellRendererPrivate * p = self->priv;

    switch( property_id )
    {
        case P_TORRENT:        g_value_set_pointer( v, p->tor ); break;
        case P_UPLOAD_SPEED:   g_value_set_double( v, p->upload_speed_KBps ); break;
        case P_DOWNLOAD_SPEED: g_value_set_double( v, p->download_speed_KBps ); break;
        case P_BAR_HEIGHT:     g_value_set_int( v, p->bar_height ); break;
        case P_COMPACT:        g_value_set_boolean( v, p->compact ); break;
        case P_SHOW_PIECES:    g_value_set_boolean( v, p->show_pieces ); break;
        default: G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, pspec ); break;
    }
}

static void
torrent_cell_renderer_dispose( GObject * o )
{
    TorrentCellRenderer * r = TORRENT_CELL_RENDERER( o );
    GObjectClass *        parent;

    if( r && r->priv )
    {
        g_object_unref( G_OBJECT( r->priv->text_renderer ) );
        g_object_unref( G_OBJECT( r->priv->text_renderer_err ) );
        g_object_unref( G_OBJECT( r->priv->pbar_renderer ) );
        g_object_unref( G_OBJECT( r->priv->prog_renderer ) );
        g_object_unref( G_OBJECT( r->priv->icon_renderer ) );
        r->priv = NULL;
    }

    parent = g_type_class_peek( g_type_parent( TORRENT_CELL_RENDERER_TYPE ) );
    parent->dispose( o );
}

static void
torrent_cell_renderer_class_init( TorrentCellRendererClass * klass )
{
    GObjectClass *         gobject_class = G_OBJECT_CLASS( klass );
    GtkCellRendererClass * cell_class = GTK_CELL_RENDERER_CLASS( klass );

    g_type_class_add_private( klass,
                             sizeof( struct TorrentCellRendererPrivate ) );

    parent_class = (GtkCellRendererClass*) g_type_class_peek_parent( klass );

    cell_class->render = torrent_cell_renderer_render;
    cell_class->get_size = torrent_cell_renderer_get_size;
    gobject_class->set_property = torrent_cell_renderer_set_property;
    gobject_class->get_property = torrent_cell_renderer_get_property;
    gobject_class->dispose = torrent_cell_renderer_dispose;

    g_object_class_install_property( gobject_class, P_TORRENT,
                                    g_param_spec_pointer( "torrent", NULL,
                                                          "tr_torrent*",
                                                          G_PARAM_READWRITE ) );

    g_object_class_install_property( gobject_class, P_UPLOAD_SPEED,
                                    g_param_spec_double( "piece-upload-speed", NULL,
                                                         "tr_stat.pieceUploadSpeed_KBps",
                                                         0, INT_MAX, 0,
                                                         G_PARAM_READWRITE ) );

    g_object_class_install_property( gobject_class, P_DOWNLOAD_SPEED,
                                    g_param_spec_double( "piece-download-speed", NULL,
                                                         "tr_stat.pieceDownloadSpeed_KBps",
                                                         0, INT_MAX, 0,
                                                         G_PARAM_READWRITE ) );

    g_object_class_install_property( gobject_class, P_BAR_HEIGHT,
                                    g_param_spec_int( "bar-height", NULL,
                                                      "Bar Height",
                                                      1, INT_MAX,
                                                      DEFAULT_BAR_HEIGHT,
                                                      G_PARAM_READWRITE ) );

    g_object_class_install_property( gobject_class, P_COMPACT,
                                    g_param_spec_boolean( "compact", NULL,
                                                          "Compact Mode",
                                                          FALSE,
                                                          G_PARAM_READWRITE ) );

    g_object_class_install_property( gobject_class, P_SHOW_PIECES,
                                    g_param_spec_boolean( "show-pieces", NULL,
                                                          "Show Pieces",
                                                          FALSE,
                                                          G_PARAM_READWRITE ) );
}

static void
torrent_cell_renderer_init( GTypeInstance *  instance,
                            gpointer g_class UNUSED )
{
    TorrentCellRenderer *               self = TORRENT_CELL_RENDERER(
        instance );
    struct TorrentCellRendererPrivate * p;

    p = self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
            self,
            TORRENT_CELL_RENDERER_TYPE,
            struct
            TorrentCellRendererPrivate );

    p->tor = NULL;
    p->text_renderer = gtk_cell_renderer_text_new( );
    g_object_set( p->text_renderer, "xpad", 0, "ypad", 0, NULL );
    p->text_renderer_err = gtk_cell_renderer_text_new(  );
    g_object_set( p->text_renderer_err, "xpad", 0, "ypad", 0, NULL );
    p->pbar_renderer = pieces_cell_renderer_new( );
    p->prog_renderer = gtk_cell_renderer_progress_new( );
    p->icon_renderer = gtk_cell_renderer_pixbuf_new(  );
    g_object_set( p->text_renderer_err, "foreground", "red", NULL );
    gtr_object_ref_sink( p->text_renderer );
    gtr_object_ref_sink( p->text_renderer_err );
    gtr_object_ref_sink( p->pbar_renderer );
    gtr_object_ref_sink( p->prog_renderer );
    gtr_object_ref_sink( p->icon_renderer );

    p->bar_height = DEFAULT_BAR_HEIGHT;
}

GType
torrent_cell_renderer_get_type( void )
{
    static GType type = 0;

    if( !type )
    {
        static const GTypeInfo info =
        {
            sizeof( TorrentCellRendererClass ),
            NULL,                                            /* base_init */
            NULL,                                            /* base_finalize */
            (GClassInitFunc)torrent_cell_renderer_class_init,
            NULL,                                            /* class_finalize
                                                               */
            NULL,                                            /* class_data */
            sizeof( TorrentCellRenderer ),
            0,                                               /* n_preallocs */
            (GInstanceInitFunc)torrent_cell_renderer_init,
            NULL
        };

        type = g_type_register_static( GTK_TYPE_CELL_RENDERER,
                                       "TorrentCellRenderer",
                                       &info, (GTypeFlags)0 );
    }

    return type;
}

GtkCellRenderer *
torrent_cell_renderer_new( void )
{
    return (GtkCellRenderer *) g_object_new( TORRENT_CELL_RENDERER_TYPE,
                                             NULL );
}

