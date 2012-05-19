/* ************************************************************************** */
/*                                                                            */
/*     Copyright (C)    2001-2008 Cédric Auger (cedric@grisbi.org)            */
/*          2003-2008 Benjamin Drieu (bdrieu@april.org)                       */
/*          2009-2012 Pierre Biava (grisbi@pierre.biava.name)                 */
/*          http://www.grisbi.org                                             */
/*                                                                            */
/*  This program is free software; you can redistribute it and/or modify      */
/*  it under the terms of the GNU General Public License as published by      */
/*  the Free Software Foundation; either version 2 of the License, or         */
/*  (at your option) any later version.                                       */
/*                                                                            */
/*  This program is distributed in the hope that it will be useful,           */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/*  GNU General Public License for more details.                              */
/*                                                                            */
/*  You should have received a copy of the GNU General Public License         */
/*  along with this program; if not, write to the Free Software               */
/*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */
/*                                                                            */
/* ************************************************************************** */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

/*START_INCLUDE*/
#include "grisbi_window.h"
#include "accueil.h"
#include "fenetre_principale.h"
#include "grisbi_app.h"
#include "grisbi_ui.h"
#include "gsb_dirs.h"
#include "gsb_navigation.h"
#include "menu.h"
#include "structures.h"
#include "traitement_variables.h"
#include "utils_gtkbuilder.h"
#include "utils_str.h"
/*END_INCLUDE*/


#define GSB_NBRE_CHAR 15
#define GSB_NAMEFILE_TOO_LONG 45

#define GRISBI_WINDOW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), GRISBI_TYPE_WINDOW, GrisbiWindowPrivate))

static GtkBuilder *grisbi_window_builder = NULL;

/* mutex for GrisbiAppConf */
static GMutex *grisbi_window_etat_mutex = NULL;

struct _GrisbiWindowPrivate
{
    /* Vbox générale */
    GtkWidget           *main_box;

    /* page d'accueil affichée si pas de fichier chargé automatiquement */
    GtkWidget           *accueil_page;

    /* widget général si un fichier est chargé */
    GtkWidget           *vbox_general;
    GtkWidget           *hpaned_general;

    /* nom du fichier associé à la fenêtre */
    gchar               *filename;

    /* titre du fichier */
    gchar               *file_title;

    /* titre de la fenêtre */
    gchar               *window_title;

    /* Menus et barres d'outils */
    GtkWidget           *menu_bar;
    GtkUIManager        *ui_manager;
    GtkActionGroup      *always_sensitive_action_group;
    GtkActionGroup      *division_sensitive_action_group;
    GtkActionGroup      *file_save_action_group;
    GtkActionGroup      *file_recent_files_action_group;
    GtkActionGroup      *file_debug_toggle_action_group;
    GtkActionGroup      *edit_sensitive_action_group;
    GtkActionGroup      *edit_transactions_action_group;
    GtkActionGroup      *view_sensitive_action_group;
    guint                recent_files_merge_id;             /* utile pour la mise à jour du menu recent file */
    guint                move_to_account_merge_id;          /* utile pour la mise à jour du menu move_to_account */

    /* statusbar */
    GtkWidget           *statusbar;
    guint               context_id;
    guint               message_id;

    /* headings_bar */
    GtkWidget           *headings_eb;

    /* variables de configuration de la fenêtre */
    GrisbiWindowEtat    *etat;

    /* structure run */
    GrisbiWindowRun     *run;

    /* account list */
    GtkWidget           *navigation_tree_view;
    GSList              *list_accounts;
    gpointer            account_buffer;

    /* Widget that hold the scheduler calendar. */
    GtkWidget *scheduler_calendar;

};


G_DEFINE_TYPE(GrisbiWindow, grisbi_window, GTK_TYPE_WINDOW)


/**
 *
 *
 * \param
 *
 * \return
 **/
static void grisbi_window_realized ( GtkWidget *window,
                        gpointer  *data )
{

    /* return */
}


/**
 *
 *
 * \param
 *
 * \return
 **/
static void grisbi_window_finalize ( GObject *object )
{
    GrisbiWindow *window;
    GrisbiWindowEtat *etat;
    GrisbiWindowRun *run;

    devel_debug (NULL);

    window = GRISBI_WINDOW ( object );
    etat = window->priv->etat;
    run = window->priv->run;

    /* libère la mémoire utilisée par les données de priv */
    grisbi_window_free_priv_file ( window );

    /* libération mémoire de la structure run */
    g_free ( run->reconcile_final_balance );
    if ( run->reconcile_new_date )
        g_date_free ( run->reconcile_new_date );
    g_free ( run );

    G_OBJECT_CLASS ( grisbi_window_parent_class )->finalize ( object );
}


/**
 *
 *
 * \param
 *
 * \return
 **/
static gboolean grisbi_window_key_press_event ( GtkWidget *widget,
                        GdkEventKey *event,
                        gpointer data )
{
    GrisbiAppConf *conf;

    switch ( event -> keyval )
    {
        case GDK_KEY_F11 :
            conf = grisbi_app_get_conf ();
            if ( conf->full_screen )
                gtk_window_unfullscreen ( GTK_WINDOW ( widget ) );
            else
                gtk_window_fullscreen ( GTK_WINDOW ( widget ) );
        break;
    }

    return FALSE;
}


/**
 *
 *
 * \param
 *
 * \return
 **/
static gboolean grisbi_window_state_event ( GtkWidget *widget,
                        GdkEventWindowState *event )
{
    GrisbiWindow *window = GRISBI_WINDOW ( widget );
    GrisbiAppConf *conf;
    gboolean show;

    if ( event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED )
    {
        show = !( event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED );

        gtk_statusbar_set_has_resize_grip ( GTK_STATUSBAR ( window->priv->statusbar ), show );
        conf = grisbi_app_get_conf ();
        conf->maximize_screen = !show;
    }
    else if ( event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN )
    {
        show = !( event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN );

        gtk_statusbar_set_has_resize_grip ( GTK_STATUSBAR ( window->priv->statusbar ), show );
        conf = grisbi_app_get_conf ();
        conf->full_screen = !show;
    }

    return FALSE;
}


/**
 *
 *
 * \param
 *
 * \return
 **/
static void grisbi_window_class_init ( GrisbiWindowClass *klass )
{
    GObjectClass *object_class = G_OBJECT_CLASS ( klass );
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS ( klass );

    object_class->finalize = grisbi_window_finalize;
    widget_class->window_state_event = grisbi_window_state_event;

    g_type_class_add_private ( object_class, sizeof ( GrisbiWindowPrivate ) );
}


/* MENUS */
/**
 * Add menu items to the action_group "FileRecentFilesGroupAction"
 *
 * \param
 *
 * \return
 **/
static GtkActionGroup *grisbi_window_add_recents_action_group ( GtkUIManager *ui_manager,
                        GrisbiAppConf *conf )
{
    GtkActionGroup *actions;
    gint i;

    actions = gtk_action_group_new ( "FileRecentFilesGroupAction" );
    for ( i = 0 ; i < conf->nb_derniers_fichiers_ouverts ; i++ )
    {
        gchar *tmp_name;
        GtkAction *action;

        tmp_name = g_strdup_printf ( "LastFile%d", i );

        action = gtk_action_new ( tmp_name,
                        conf->tab_noms_derniers_fichiers_ouverts[i],
                        "",
                        "gtk-open" );
        g_free ( tmp_name );
        g_signal_connect ( action,
                        "activate",
                        G_CALLBACK ( gsb_file_open_direct_menu ),
                        GINT_TO_POINTER ( i ) );
        gtk_action_group_add_action ( actions, action );
    }

    gtk_ui_manager_insert_action_group ( ui_manager, actions, 1 );
    g_object_unref ( actions );

    return actions;
}


/**
 * Add menu items to the action_group "FileRecentFilesGroupAction".
 *
 * \param
 *
 * \return
 **/
static void grisbi_window_add_recents_sub_menu ( GrisbiWindow *window,
                        gint nb_derniers_fichiers_ouverts )
{
    GtkUIManager *ui_manager;
    gint i;

    ui_manager = window->priv->ui_manager;

    window->priv->recent_files_merge_id = gtk_ui_manager_new_merge_id ( ui_manager );

    for ( i=0 ; i < nb_derniers_fichiers_ouverts ; i++ )
    {
        gchar *tmp_name;
        gchar *tmp_label;

        tmp_name = g_strdup_printf ( "LastFile%d", i );
        tmp_label = g_strdup_printf ( "_%d LastFile%d", i, i );

        gtk_ui_manager_add_ui ( ui_manager,
                    window->priv->recent_files_merge_id,
                    "/menubar/FileMenu/RecentFiles/FileRecentsPlaceholder",
                    tmp_label,
                    tmp_name,
                    GTK_UI_MANAGER_MENUITEM,
                    FALSE );

        g_free ( tmp_name );
        g_free ( tmp_label );
    }
}


/**
 *
 *
 * \param
 *
 * \return
 **/
static void grisbi_window_init_menus ( GrisbiWindow *window )
{
    GrisbiAppConf *conf;
    GtkUIManager *ui_manager;
    GtkActionGroup *actions;
    GError *error = NULL;
    gchar *ui_file;

    devel_debug (NULL);

    ui_manager = gtk_ui_manager_new ();
    window->priv->ui_manager = ui_manager;
    window->priv->recent_files_merge_id = -1;

    conf = grisbi_app_get_conf ();

    /* actions toujours accessibles (sensitives) */
    actions = gtk_action_group_new ( "AlwaysSensitiveActions" );
    gtk_action_group_set_translation_domain ( actions, NULL );
    gtk_action_group_add_actions (actions,
                        always_sensitive_entries,
                        G_N_ELEMENTS ( always_sensitive_entries ),
                        window );

    /* ShowFullScreenAction sensitive */
    gtk_action_group_add_toggle_actions ( actions,
                        show_full_screen_entrie,
                        G_N_ELEMENTS ( show_full_screen_entrie ),
                        NULL );

    gtk_ui_manager_insert_action_group ( ui_manager, actions, 0 );
    g_object_unref ( actions );
    window->priv->always_sensitive_action_group = actions;

    /* Actions pour la gestion des fichiers sensitives */
    actions = gtk_action_group_new ( "DivisionSensitiveActions" );
    gtk_action_group_set_translation_domain ( actions, NULL );
    gtk_action_group_add_actions (actions,
                        division_sensitive_entries,
                        G_N_ELEMENTS ( division_sensitive_entries ),
                        window );

    gtk_ui_manager_insert_action_group ( ui_manager, actions, 0 );
    g_object_unref ( actions );
    window->priv->division_sensitive_action_group = actions;
    gtk_action_group_set_sensitive ( actions, FALSE );

    /* Actions pour la gestion des fichiers sensitives */
    actions = gtk_action_group_new ( "FileSaveAction" );
    gtk_action_group_set_translation_domain ( actions, NULL );
    gtk_action_group_add_actions (actions,
                        file_save_entries,
                        G_N_ELEMENTS ( file_save_entries ),
                        window );

    gtk_ui_manager_insert_action_group ( ui_manager, actions, 0 );
    g_object_unref ( actions );
    window->priv->file_save_action_group = actions;
    gtk_action_group_set_sensitive ( actions, FALSE );

    /* Actions pour la gestion des fichiers récents */
    actions = gtk_action_group_new ( "FileRecentFilesAction" );
    gtk_action_group_set_translation_domain ( actions, NULL );
    gtk_action_group_add_actions (actions,
                        file_recent_files_entrie,
                        G_N_ELEMENTS ( file_recent_files_entrie ),
                        window );

    gtk_ui_manager_insert_action_group ( ui_manager, actions, 0 );
    g_object_unref ( actions );
    gtk_action_group_set_sensitive ( actions, TRUE );

    /* DebugToggle_Action sensitive */
    actions = gtk_action_group_new ( "FileDebugToggleAction" );
    gtk_action_group_set_translation_domain ( actions, NULL );
    gtk_action_group_add_toggle_actions ( actions,
                        file_debug_toggle_entrie,
                        G_N_ELEMENTS ( file_debug_toggle_entrie ),
                        NULL );

    gtk_ui_manager_insert_action_group ( ui_manager, actions, 0 );
    g_object_unref ( actions );
    window->priv->file_debug_toggle_action_group = actions;
    gtk_action_group_set_sensitive ( actions, FALSE );

    /* actions du menu Edit sensitives */
    actions = gtk_action_group_new ( "EditSensitiveActions" );
    gtk_action_group_set_translation_domain ( actions, NULL );
    gtk_action_group_add_actions (actions,
                        edit_sensitive_entries,
                        G_N_ELEMENTS ( edit_sensitive_entries ),
                        window );

    gtk_ui_manager_insert_action_group ( ui_manager, actions, 0 );
    g_object_unref ( actions );
    window->priv->edit_sensitive_action_group = actions;
    gtk_action_group_set_sensitive ( actions, FALSE );

    /* actions propres aux transactions */
    actions = gtk_action_group_new ( "EditTransactionsActions" );
    gtk_action_group_set_translation_domain ( actions, NULL );
    gtk_action_group_add_actions (actions,
                        edit_transactions_entries,
                        G_N_ELEMENTS ( edit_transactions_entries ),
                        window );

    gtk_ui_manager_insert_action_group ( ui_manager, actions, 0 );
    g_object_unref ( actions );
    window->priv->edit_transactions_action_group = actions;
    gtk_action_group_set_sensitive ( actions, FALSE );

    /* actions du menu View sensitives */
    actions = gtk_action_group_new ( "ViewSensitiveActions" );
    gtk_action_group_set_translation_domain ( actions, NULL );
    gtk_action_group_add_actions (actions,
                        view_init_width_col_entrie,
                        G_N_ELEMENTS ( view_init_width_col_entrie ),
                        window );

    gtk_action_group_add_radio_actions ( actions,
                        view_radio_entries,
                        G_N_ELEMENTS ( view_radio_entries ),
                        -1,
                        G_CALLBACK ( gsb_gui_toggle_line_view_mode ),
                        NULL );

    gtk_action_group_add_toggle_actions ( actions,
                        view_toggle_entries,
                        G_N_ELEMENTS ( view_toggle_entries ),
                        NULL );

    gtk_ui_manager_insert_action_group ( ui_manager, actions, 0 );
    g_object_unref ( actions );
    window->priv->view_sensitive_action_group = actions;
    gtk_action_group_set_sensitive ( actions, FALSE );

    /* now load the UI definition */
    ui_file = g_build_filename ( gsb_dirs_get_ui_dir (), "grisbi_ui.xml", NULL );
    gtk_ui_manager_add_ui_from_file ( ui_manager, ui_file, &error );
    if ( error != NULL )
    {
        g_warning ( "Could not merge %s: %s", ui_file, error->message );
        g_error_free ( error );
    }
    g_free ( ui_file );

    /* on ajoute les derniers fichiers utilisés */
    if ( conf->nb_derniers_fichiers_ouverts && conf->nb_max_derniers_fichiers_ouverts )
    {
        actions = grisbi_window_add_recents_action_group ( ui_manager, conf );
        grisbi_window_add_recents_sub_menu ( window, conf->nb_derniers_fichiers_ouverts );
        window->priv->file_recent_files_action_group = actions;
    }

#ifndef GTKOSXAPPLICATION
    gtk_window_add_accel_group ( GTK_WINDOW ( window ),
                        gtk_ui_manager_get_accel_group ( ui_manager ) );
#endif /* !GTKOSXAPPLICATION */

    window->priv->menu_bar = gtk_ui_manager_get_widget ( ui_manager, "/menubar" );
    gtk_box_pack_start ( GTK_BOX ( window->priv->main_box ),  window->priv->menu_bar, FALSE, TRUE, 0 );

    /* return */
}


/* GTK_BUILDER */
/**
 * Crée un builder et récupère les widgets du fichier grisbi.ui
 *
 * \param
 *
 * \rerurn
 * */
static gboolean grisbi_window_initialise_builder ( GrisbiWindow *window )
{
    /* Creation d'un nouveau GtkBuilder */
    grisbi_window_builder = gtk_builder_new ();
    if ( grisbi_window_builder == NULL )
        return FALSE;

    /* récupère les widgets */
    if ( !utils_gtkbuilder_merge_ui_data_in_builder ( grisbi_window_builder, "grisbi.ui" ) )
        return FALSE;

    window->priv->main_box = GTK_WIDGET ( gtk_builder_get_object ( grisbi_window_builder, "main_vbox" ) );
    window->priv->accueil_page = GTK_WIDGET ( gtk_builder_get_object ( grisbi_window_builder, "accueil_page" ) );
    window->priv->vbox_general = GTK_WIDGET ( gtk_builder_get_object ( grisbi_window_builder, "vbox_general" ) );

    return TRUE;
}


/* BLANK_PAGE */
/**
 * page d'accueil si on ne charge pas un fichier automatiquement
 *
 * \param
 *
 * \return
 **/
static void grisbi_window_new_accueil_page ( GrisbiWindow *window )
{
    GtkWidget *bouton;
    GtkWidget *table;
    GtkAction *action;
    gint i;
    gint col = 0;
    gint row = 1;
    GrisbiAppConf *conf;

    conf = grisbi_app_get_conf ();

    table = GTK_WIDGET ( gtk_builder_get_object ( grisbi_window_builder, "table_accueil" ) );

    /* add buttons "Nouveau" */
    action = gtk_action_group_get_action ( window->priv->always_sensitive_action_group, "NewAction" );
    bouton = GTK_WIDGET ( gtk_builder_get_object ( grisbi_window_builder, "bouton_nouveau" ) );
    gtk_activatable_set_related_action  ( GTK_ACTIVATABLE ( bouton ), GTK_ACTION ( action ) );

    /* add buttons "Ouvrir" */
    action = gtk_action_group_get_action ( window->priv->always_sensitive_action_group, "OpenAction" );
    bouton =  GTK_WIDGET ( gtk_builder_get_object ( grisbi_window_builder, "bouton_ouvrir" ) );
    gtk_activatable_set_related_action  ( GTK_ACTIVATABLE ( bouton ), GTK_ACTION ( action ) );

    /* add buttons "Importer" */
    action = gtk_action_group_get_action ( window->priv->always_sensitive_action_group, "ImportFileAction" );
    bouton = GTK_WIDGET ( gtk_builder_get_object ( grisbi_window_builder, "bouton_importer" ) );
    gtk_activatable_set_related_action  ( GTK_ACTIVATABLE ( bouton ), GTK_ACTION ( action ) );

    for ( i = 0; i < conf->nb_derniers_fichiers_ouverts; i++ )
    {
        GtkWidget *image;
        gchar *tmp_str;

        image = gtk_image_new_from_stock ( "gtk-open", GTK_ICON_SIZE_BUTTON );
        gtk_widget_set_size_request ( image, 50, 50 );

        if ( g_utf8_strlen ( conf->tab_noms_derniers_fichiers_ouverts[i], -1 ) > GSB_NAMEFILE_TOO_LONG )
        {
            gchar *basename;

            basename = g_path_get_basename ( conf->tab_noms_derniers_fichiers_ouverts[i] );
            tmp_str = utils_str_break_file_name ( basename, GSB_NBRE_CHAR );
            g_free ( basename );
        }
        else
            tmp_str = utils_str_break_file_name ( conf->tab_noms_derniers_fichiers_ouverts[i], GSB_NBRE_CHAR );

        bouton = gtk_button_new_with_label ( tmp_str );
        gtk_button_set_image ( GTK_BUTTON ( bouton ), image );
        gtk_button_set_image_position ( GTK_BUTTON ( bouton ), GTK_POS_TOP );
        gtk_widget_set_size_request ( bouton, 150, 150 );
        gtk_widget_show ( bouton );

        g_signal_connect ( bouton,
                        "clicked",
                        G_CALLBACK ( gsb_file_open_direct_menu ),
                        GINT_TO_POINTER ( i ) );

        gtk_table_attach ( GTK_TABLE ( table ), bouton, col, col+1, row, row+1, GTK_SHRINK | GTK_FILL, 0, 0, 0 );
        col++;
        if ( ( col % 3 ) == 0 )
        {
            col = 0;
            row++;
        }

        g_free ( tmp_str );
    }
}


/* STATUS_BAR */
/**
 * Create and return a new GtkStatusBar to hold various status
 * information.
 *
 * \param
 *
 * \return  A newly allocated GtkStatusBar.
 */
static GtkWidget *grisbi_window_new_statusbar ( GrisbiWindow *window )
{
    GtkWidget *statusbar;

    statusbar = GTK_WIDGET ( gtk_builder_get_object ( grisbi_window_builder, "main_statusbar" ) );
    window->priv->statusbar = statusbar;
    window->priv->context_id = gtk_statusbar_get_context_id ( GTK_STATUSBAR ( statusbar ), "Grisbi" );
    window->priv->message_id = -1;

    return statusbar;
}


/* HEADINGS_EB */
/**
 * Trigger a callback functions only if button event that triggered it
 * was a simple click.
 *
 * \param button
 * \param event
 * \param callback function
 *
 * \return  TRUE.
 */
static gboolean grisbi_window_headings_simpleclick_event_run ( GtkWidget *button,
                        GdkEvent *button_event,
                        GCallback callback )
{
    if ( button_event -> type == GDK_BUTTON_PRESS )
    {
        callback ();
    }

    return TRUE;
}


/**
 * Create and return a new headings_bar
 * information.
 *
 * \param
 *
 * \return  A newly allocated headings_eb.
 */
static GtkWidget *grisbi_window_new_headings_eb ( GrisbiWindow *window )
{
    GtkWidget *headings_eb;
    GtkWidget *arrow_eb;
    GtkStyle *style;

    headings_eb = GTK_WIDGET ( gtk_builder_get_object ( grisbi_window_builder, "headings_eb" ) );
    window->priv->headings_eb = headings_eb;
    style = gtk_widget_get_style ( headings_eb );

    arrow_eb = GTK_WIDGET ( gtk_builder_get_object ( grisbi_window_builder, "arrow_eb_left" ) );
    gtk_widget_modify_bg ( arrow_eb, 0, &(style -> bg[GTK_STATE_ACTIVE]) );
    g_signal_connect ( G_OBJECT ( arrow_eb ),
                        "button-press-event",
                        G_CALLBACK ( grisbi_window_headings_simpleclick_event_run ),
                        gsb_gui_navigation_select_prev );

    arrow_eb = GTK_WIDGET ( gtk_builder_get_object ( grisbi_window_builder, "arrow_eb_right" ) );
    gtk_widget_modify_bg ( arrow_eb, 0, &(style -> bg[GTK_STATE_ACTIVE]) );
    g_signal_connect ( G_OBJECT ( arrow_eb ), "button-press-event",
                        G_CALLBACK ( grisbi_window_headings_simpleclick_event_run ),
                        gsb_gui_navigation_select_next );

    gtk_widget_modify_bg ( headings_eb, 0, &(style -> bg[GTK_STATE_ACTIVE]) );


    return headings_eb;
}


/* HPANED */
/**
 * met à jour la taille du panneau de gauche
 *
 * \param hpaned
 * \param allocation
 * \param null
 *
 * \return
 **/
static gboolean grisbi_window_hpaned_size_allocate ( GtkWidget *hpaned,
                        GtkAllocation *allocation,
                        gpointer null )
{
    GrisbiAppConf *conf;

    conf = grisbi_app_get_conf ();
    conf->panel_width = gtk_paned_get_position ( GTK_PANED ( hpaned ) );
    
    return FALSE;

}


/**
 * Initialise le panneau du widget principal
 *
 * \param window
 *
 * \return hpaned
 **/
static GtkWidget *grisbi_window_new_hpaned ( GrisbiWindow *window )
{
    GtkWidget *hpaned_general;
    GrisbiAppConf *conf;

    conf = grisbi_app_get_conf ();

    /* initialisation du hpaned */
    hpaned_general = GTK_WIDGET ( gtk_builder_get_object ( grisbi_window_builder, "hpaned_general" ) );

    g_signal_connect ( G_OBJECT ( hpaned_general ),
                        "size_allocate",
                        G_CALLBACK ( grisbi_window_hpaned_size_allocate ),
                        NULL );
    gtk_container_set_border_width ( GTK_CONTAINER ( hpaned_general ), 6 );

    if ( conf->panel_width == -1 )
    {
        gint width, height;

        gtk_window_get_size ( GTK_WINDOW ( grisbi_app_get_active_window ( NULL ) ), &width, &height );
        gtk_paned_set_position ( GTK_PANED ( hpaned_general ), (gint) width / 4 );
    }
    else
    {
        if ( conf->panel_width )
            gtk_paned_set_position ( GTK_PANED ( hpaned_general ), conf->panel_width );
        else
            gtk_paned_set_position ( GTK_PANED ( hpaned_general ), 1 );
    }

    return hpaned_general;
}


/* STRUCTURE ETAT */
/**
 * Init etat mutex
 *
 * \param
 *
 * \return
 **/
static void grisbi_window_init_etat_mutex ( void )
{
  g_assert ( grisbi_window_etat_mutex == NULL );
  grisbi_window_etat_mutex = g_mutex_new ();
}


/**
 * définit la structure etat
 *
 * \param window
 * \param etat
 *
 * \return TRUE if OK or FALSE
 **/
static gboolean grisbi_window_set_struct_etat ( GrisbiWindow *window,
                        GrisbiWindowEtat *etat )
{
    if ( !GRISBI_IS_WINDOW ( window ) )
        return FALSE;

    window->priv->etat = etat;

    return TRUE;
}


/**
 * libère la mémoire utilisée par etat
 *
 * \param object
 *
 * \return
 **/
static void grisbi_window_free_struct_etat ( GrisbiWindowEtat *etat )
{

    devel_debug (NULL);

    g_free ( etat->name_logo );
    etat->name_logo = NULL;

    g_free ( etat->navigation_list_order );
    etat->navigation_list_order = NULL;

    g_free ( etat->csv_separator );
    etat->csv_separator = NULL;

    g_free ( etat->transaction_column_width );
    etat->transaction_column_width = NULL;

    g_free ( etat->scheduler_column_width );
    etat->scheduler_column_width = NULL;

    g_free ( etat );
}


/* CREATE OBJECT */
/**
 * Initialise GrisbiWindow
 *
 * \param window
 *
 * \return
 */
static void grisbi_window_init ( GrisbiWindow *window )
{
    GtkWidget *statusbar;
    GtkWidget *headings_eb;
    GrisbiAppConf *conf;
    GrisbiWindowPrivate *priv;

    devel_debug (NULL);

    window->priv = priv = GRISBI_WINDOW_GET_PRIVATE ( window );

    if ( !grisbi_window_initialise_builder ( window ) )
        exit ( 1 );

    /* initialisation du mutex */
    grisbi_window_init_etat_mutex ();

    /* creation de la structure run */
    priv->run = g_malloc0 ( sizeof ( GrisbiWindowRun ) );

    /* initialisation de la structure etat */
    grisbi_window_init_struct_etat ( window );

    /* initialisation de la liste des comptes */
    priv->list_accounts = NULL;
    priv->account_buffer = NULL;

    /* Création de la fenêtre principale de Grisbi */
    gtk_container_add ( GTK_CONTAINER ( window ), priv->main_box );
    gtk_widget_show ( priv->main_box );

    /* create the menus */
    grisbi_window_init_menus ( window );

    /* create the headings eb */
    headings_eb = grisbi_window_new_headings_eb ( window );
    gtk_box_pack_start ( GTK_BOX ( priv->main_box ), headings_eb, FALSE, FALSE, 0 );

    /* create the statusbar */
    statusbar = grisbi_window_new_statusbar ( window );
    gtk_box_pack_end ( GTK_BOX ( priv->main_box ), statusbar, FALSE, FALSE, 0 );

    /* on initialise une page d'accueil si on ne charge pas de fichier */
    conf = grisbi_app_get_conf ();

    grisbi_window_new_accueil_page ( window );
    gtk_box_pack_start ( GTK_BOX ( priv->main_box ), priv->accueil_page, FALSE, FALSE, 0 );

    if ( conf->load_last_file && conf->nb_derniers_fichiers_ouverts > 0 )
        gtk_widget_hide ( priv->accueil_page );

    /* set the signals */
    g_signal_connect ( G_OBJECT ( window ),
                        "realize",
                        G_CALLBACK ( grisbi_window_realized ),
                        NULL );

    g_signal_connect ( G_OBJECT ( window ),
                        "key-press-event",
                        G_CALLBACK ( grisbi_window_key_press_event ),
                        NULL );
}

/* FONCTIONS PUBLIQUES */
/* PAGE_ACCUEIL */
/**
 * retourne la page d'accueil de grisbi
 *
 * \param active window
 *
 * \return accueil_page
 **/
GtkWidget *grisbi_window_get_accueil_page ( GrisbiWindow *window )
{
    return window->priv->accueil_page;
}


/* GENERAL_WIDGET */
/**
 * Create the main widget that holds all the user interface save the
 * menus.
 *
 * \return A newly-allocated vbox holding all elements.
 */
GtkWidget *grisbi_window_new_general_widget ( void )
{
    GrisbiWindow *window;

    window = grisbi_app_get_active_window ( grisbi_app_get_default ( FALSE ) );

    /* on cache la page d'accueil si elle est visible */
    if ( gtk_widget_get_visible ( window->priv->accueil_page ) )
        gtk_widget_hide ( window->priv->accueil_page );

    /* Then create and fill the main hpaned if necessary */
    if ( window->priv->hpaned_general == NULL )
        window->priv->hpaned_general = grisbi_window_new_hpaned ( window );
    gsb_gui_navigation_create_navigation_pane ();
    gsb_gui_create_general_notebook ( window );
    gtk_container_set_border_width ( GTK_CONTAINER ( window->priv->hpaned_general ), 6 );

/*         gsb_gui_navigation_update_home_page ();  */
    gtk_widget_show ( window->priv->hpaned_general );

    gtk_widget_show ( window->priv->vbox_general );

    /* return */
    return window->priv->vbox_general;
}


/* MENUS */
/**
 * return ui_manager
 *
 * \param GrisbiWindow
 *
 * \return ui_manager
**/
GtkUIManager *grisbi_window_get_ui_manager ( GrisbiWindow *window )
{
    return window->priv->ui_manager;
}


/**
 *
 *
 * \param
 *
 * \return
 **/
GtkActionGroup *grisbi_window_get_action_group ( GrisbiWindow *window,
                        const gchar *action_group_name )
{
    if ( strcmp ( action_group_name, "AlwaysSensitiveActions" ) == 0 )
        return window->priv->always_sensitive_action_group;
    else if ( strcmp ( action_group_name, "DivisionSensitiveActions" ) == 0 )
        return window->priv->division_sensitive_action_group;
    else if ( strcmp ( action_group_name, "FileRecentFilesAction" ) == 0 )
        return window->priv->file_recent_files_action_group;
    else if ( strcmp ( action_group_name, "FileDebugToggleAction" ) == 0 )
        return window->priv->file_debug_toggle_action_group;
    else if ( strcmp ( action_group_name, "EditSensitiveActions" ) == 0 )
        return window->priv->edit_sensitive_action_group;
    else if ( strcmp ( action_group_name, "EditTransactionsActions" ) == 0 )
        return window->priv->edit_transactions_action_group;
    else if ( strcmp ( action_group_name, "ViewSensitiveActions" ) == 0 )
        return window->priv->view_sensitive_action_group;
    else
        return NULL;
}

/**
 * retourne merge_id utile pour efface_derniers_fichiers_ouverts ()
 *
 * \param window
 * \param nom du sous menu concerné
 *
 * \return recent_files_merge_id
 **/
guint grisbi_window_get_sub_menu_merge_id ( GrisbiWindow *window,
                        const gchar *sub_menu )
{
    if ( strcmp ( sub_menu, "recent_file" ) == 0 )
        return window->priv->recent_files_merge_id;
    else if ( strcmp ( sub_menu, "move_to_account" ) == 0 )
        return window->priv->move_to_account_merge_id;
    else
        return 0;
}


/**
 * set the merge_id for the submenu given in parameter
 *
 * \param window
 * \param merge_id
 * \param name of the submenu
 *
 * \return recent_files_merge_id
 **/
void grisbi_window_set_sub_menu_merge_id ( GrisbiWindow *window,
                        guint merge_id,
                        const gchar *sub_menu )
{
    if ( strcmp ( sub_menu, "recent_file" ) == 0 )
        window->priv->recent_files_merge_id = merge_id;
    else if ( strcmp ( sub_menu, "move_to_account" ) == 0 )
        window->priv->move_to_account_merge_id = merge_id;
    else
        window->priv->recent_files_merge_id = 0;
}


/* FILENAME */
/**
 * retourne le nom du fichier associé à la fenêtre
 *
 * \param GrisbiWindow
 *
 * \return filename
 **/
const gchar *grisbi_window_get_filename ( GrisbiWindow *window )
{
    return window->priv->filename;
}

/**
 * définit le nom du fichier associé à la fenêtre
 *
 * \param GrisbiWindow
 * \param filename
 *
 * \return TRUE
 **/
gboolean grisbi_window_set_filename ( GrisbiWindow *window,
                        const gchar *filename )
{
    devel_debug ( filename );

    g_free ( window->priv->filename );

    window->priv->filename = g_strdup ( filename );

    return TRUE;
}


/* TITLES */
/**
 * retourne le titre du fichier associé à la fenêtre
 *
 * \param GrisbiWindow
 *
 * \return title
 **/
const gchar *grisbi_window_get_file_title ( GrisbiWindow *window )
{
    return window->priv->file_title;
}

/**
 * définit le titre du fichier associé à la fenêtre
 *
 * \param GrisbiWindow
 * \param title
 *
 * \return TRUE
 **/
gboolean grisbi_window_set_file_title ( GrisbiWindow *window,
                        const gchar *file_title )
{
    devel_debug ( file_title );

    g_free ( window->priv->file_title );

    window->priv->file_title = g_strdup ( file_title );

    return TRUE;
}


/**
 * set window title
 *
 * \param GrisbiWindow
 * \param title
 *
 * \return
 * */
void grisbi_window_set_window_title ( GrisbiWindow *window,
                        const gchar *title )
{
    g_free ( window->priv->window_title );

    window->priv->window_title = g_strdup ( title );

    gtk_window_set_title ( GTK_WINDOW ( window ), title );
}


/**
 * définit le titre de la fenêtre active
 *
 * \param title
 *
 * \return TRUE
 **/
gboolean grisbi_window_set_active_title ( gint account_number )
{
    GrisbiApp *app;
    GrisbiWindow *window;
    GrisbiAppConf *conf;
    gchar *nom_fichier_comptes;
    const gchar *titre_fichier;
    gchar *titre_grisbi = NULL;
    gchar *titre = NULL;
    gint tmp_number;
    gboolean return_value;

    devel_debug_int ( account_number );

    app = grisbi_app_get_default ( FALSE );
    window = grisbi_app_get_active_window ( app );
    conf = grisbi_app_get_conf ();

    titre_fichier = grisbi_app_get_active_file_title ();
    nom_fichier_comptes = g_strdup ( grisbi_app_get_active_filename () );
    if ( nom_fichier_comptes == NULL )
    {
        titre_grisbi = g_strdup ( _("Grisbi") );
        return_value = TRUE;
    }
    else
    {
        switch ( conf->display_grisbi_title )
        {
            case GSB_ACCOUNTS_TITLE:
            {
                if ( titre_fichier && strlen ( titre_fichier ) )
                    titre = g_strdup ( titre_fichier );
            }
            break;
            case GSB_ACCOUNT_HOLDER:
            {
                if ( account_number == -1 )
                    tmp_number = gsb_data_account_first_number ();
                else
                    tmp_number = account_number;

                if ( tmp_number == -1 )
                {
                    if ( titre_fichier && strlen ( titre_fichier ) )
                        titre = g_strdup ( titre_fichier );
                }
                else
                {
                    titre = g_strdup ( gsb_data_account_get_holder_name ( tmp_number ) );

                    if ( titre == NULL )
                        titre = g_strdup ( gsb_data_account_get_name ( tmp_number ) );
                }
            break;
            }
            case GSB_ACCOUNTS_FILE:
                if ( nom_fichier_comptes && strlen ( nom_fichier_comptes ) )
                    titre = g_path_get_basename ( nom_fichier_comptes );
            break;
        }

        g_free ( nom_fichier_comptes );

        if ( titre && strlen ( titre ) > 0 )
        {
            titre_grisbi = g_strconcat ( titre, " - ", _("Grisbi"), NULL );
            g_free ( titre );

            return_value = TRUE;
        }
        else
        {
            titre_grisbi = g_strconcat ( "<", _("unnamed"), ">", NULL );
            return_value = FALSE;
        }
    }
    grisbi_window_set_window_title ( window, titre_grisbi );

    gsb_main_page_update_homepage_title ( titre_grisbi );

    if ( titre_grisbi && strlen ( titre_grisbi ) > 0 )
        g_free ( titre_grisbi );

    /* return */
    return return_value;
}


/* STATUS_BAR */
/**
 * supprime le message de la liste
 *
 * \param GrisbiWindow
 *
 * \return
 **/
void grisbi_window_statusbar_remove ( GrisbiWindow *window )
{
    if ( window->priv->message_id >= 0 )
    {
        gtk_statusbar_remove ( GTK_STATUSBAR ( window->priv->statusbar ),
                        window->priv->context_id,
                        window->priv->message_id );
        window->priv->message_id = -1;
    }

}


/**
 * met un message dans la barre d'état de la fenêtre
 *
 * \param GrisbiWindow
 * \param msg
 *
 * \return
 **/
void grisbi_window_statusbar_push ( GrisbiWindow *window,
                        const gchar *msg )
{
    window->priv->message_id = gtk_statusbar_push ( GTK_STATUSBAR ( window->priv->statusbar ),
                        window->priv->context_id, msg );
}


/* HEADINGS_EB */
/**
 * retourne headings_eb
 *
 * \param
 *
 * \return
 **/
GtkWidget *grisbi_window_get_headings_eb ( GrisbiWindow *window )
{
    return window->priv->headings_eb;
}


/**
 * Update one of the heading bar label with a new text.
 *
 * \param label Label to update.
 * \param text  String to display in headings bar.
 * \param text  escape string to display in headings bar.
 *
 */
void grisbi_window_headings_update_label_markup ( gchar *label_name,
                        const gchar *text,
                        gboolean escape_text )
{
    GtkWidget *label;
    gchar* tmpstr;

    if ( escape_text )
        tmpstr = g_markup_printf_escaped ("<b>%s</b>", text );
    else
        tmpstr = g_strconcat ( "<b>", text, "</b>", NULL );

    label = GTK_WIDGET ( gtk_builder_get_object ( grisbi_window_builder, label_name ) );

    if ( GTK_IS_LABEL ( label ) )
        gtk_label_set_markup ( GTK_LABEL ( label ), tmpstr );

    g_free ( tmpstr );
}


/* STRUCTURE ETAT */
/**
 * retourne la structure etat
 *
 * \param
 *
 * \return etat
 **/
GrisbiWindowEtat *grisbi_window_get_struct_etat ( void )
{
    GrisbiApp *app;
    GrisbiWindow *window;

    app = grisbi_app_get_default ( TRUE );
    
    window = grisbi_app_get_active_window ( app );

    return window->priv->etat;
}


/**
 * Init la structure etat
 *
 * \param
 *
 * \return
 **/
void grisbi_window_init_struct_etat ( GrisbiWindow *window )
{
    GrisbiWindowEtat *etat;

    devel_debug (NULL);

    /* creation de la structure etat */
    etat = g_malloc0 ( sizeof ( GrisbiWindowEtat ) );
    grisbi_window_set_struct_etat ( window, etat );

    /* blocage du mutex */
    g_mutex_lock ( grisbi_window_etat_mutex );

    /* init logo */
    etat->is_pixmaps_dir = TRUE;
    if ( etat->name_logo && strlen ( etat->name_logo ) )
        g_free ( etat->name_logo );
    etat->name_logo = NULL;
    etat->utilise_logo = 1;

    etat->valeur_echelle_recherche_date_import = 2;
    etat->get_fyear_by_value_date = FALSE;

    /* init default combofix values */
    etat->combofix_mixed_sort = FALSE;
    etat->combofix_max_item = 0;
    etat->combofix_case_sensitive = FALSE;
    etat->combofix_enter_select_completion = FALSE;
    etat->combofix_force_payee = FALSE;
    etat->combofix_force_category = FALSE;

    /* defaut value for width and align of columns */
    if ( etat->transaction_column_width && strlen ( etat->transaction_column_width ) )
    {
        g_free ( etat->transaction_column_width );
        etat->transaction_column_width = NULL;
    }
    if ( etat->scheduler_column_width && strlen ( etat->scheduler_column_width ) )
    {
        g_free ( etat->scheduler_column_width );
        etat->scheduler_column_width = NULL;
    }

    /* divers */
    etat->add_archive_in_total_balance = TRUE;  /* add the archived transactions by default */
    etat->get_fyear_by_value_date = 0;          /* By default use transaction-date */
    etat->retient_affichage_par_compte = 0;     /* Par défaut affichage identique pour tous les comptes */
    memset ( etat->csv_skipped_lines, '\0', sizeof(gboolean) *CSV_MAX_TOP_LINES );

    /* initializes the variables for the estimate balance module */
    etat->bet_deb_period = 1;

    /* initialisation des autres variables */
    init_variables ( window->priv->etat, window->priv->run );

    /* libération du mutex */
    g_mutex_unlock ( grisbi_window_etat_mutex );

}


/* STRUCTURE RUN */
/**
 * retourne la structure run
 *
 * \param GrisbiWindow
 *
 * \return struct GrisbiWindowRun
 **/
GrisbiWindowRun *grisbi_window_get_struct_run ( GrisbiWindow *window )
{
    if ( window == NULL )
    {
        GrisbiApp *app;

        app = grisbi_app_get_default ( TRUE );
        if ( app )
            window = grisbi_app_get_active_window ( app );
    }
    if ( window )
        return window->priv->run;
    else
        return NULL;
}


/* NAVIGATION PANEL */
/**
 * retourne navigation_tree_view
 *
 * \param
 *
 * \return navigation_tree_view
 **/
GtkWidget *grisbi_window_get_navigation_tree_view ( void )
{
    GrisbiWindow *window;

    window = grisbi_app_get_active_window ( grisbi_app_get_default ( TRUE ) );

    if ( window )
        return window->priv->navigation_tree_view;
    else
        return NULL;
}


/**
 * retourne navigation_tree_view
 *
 * \param
 *
 * \return navigation_tree_view
 **/
gboolean grisbi_window_set_navigation_tree_view ( GtkWidget *navigation_tree_view )
{
    GrisbiWindow *window;

    window = grisbi_app_get_active_window ( grisbi_app_get_default ( TRUE ) );

    if ( window )
    {
        window->priv->navigation_tree_view = navigation_tree_view;
        return TRUE;
    }
    else
    {
        window->priv->navigation_tree_view = NULL;
        return FALSE;
    }
}


/**
 * retourne la liste des comptes
 *
 * \param window
 *
 * \return
 **/
GSList *grisbi_window_get_list_accounts ( GrisbiWindow *window )
{
    return window->priv->list_accounts;
}


/**
 * libère t initialise la liste des comptes et le buffer
 *
 * \param window
 *
 * \return
 **/
void grisbi_window_free_list_accounts ( GrisbiWindow *window )
{
    if ( window == NULL )
        return;

    g_slist_free ( window->priv->list_accounts );

    window->priv->list_accounts = NULL;
    window->priv->account_buffer = NULL;
}


/**
 * définit la liste des comptes
 *
 * \param window
 * \param nouvelle liste des comptes.
 *
 * \return
 **/
gboolean grisbi_window_set_list_accounts ( GrisbiWindow *window,
                        GSList *list_accounts )
{
    window->priv->list_accounts = list_accounts;

    return TRUE;
}


/**
 * retourne scheduler_calendar
 *
 * \param               window
 * \param               GtkWidget *scheduler_calendar
 *
 * \return              TRUE
 **/
GtkWidget *grisbi_window_get_scheduler_calendar ( GrisbiWindow *window )
{
    return window->priv->scheduler_calendar;
}


/**
 * initialise scheduler_calendar
 *
 * \param               window
 * \param               GtkWidget *scheduler_calendar
 *
 * \return              TRUE
 **/
gboolean grisbi_window_set_scheduler_calendar ( GrisbiWindow *window,
                        GtkWidget *scheduler_calendar )
{
    window->priv->scheduler_calendar = scheduler_calendar;

    return TRUE;
}


/* FONCTIONS UTILITAIRES */
/**
 * retourne le widget nommé
 *
 * \param nom du widget recherché
 *
 * \return
 **/
GtkWidget *grisbi_window_get_widget_by_name ( const gchar *name )
{
    GtkWidget *widget;

    widget = GTK_WIDGET ( gtk_builder_get_object ( grisbi_window_builder, name ) );

    return widget;
}


/**
 * lock etat_mutex
 *
 * \param
 *
 * \return
 **/
void grisbi_window_etat_mutex_lock ( void )
{
    g_mutex_lock ( grisbi_window_etat_mutex );
}


/**
 * unlock etat_mutex
 *
 * \param
 *
 * \return
 **/
void grisbi_window_etat_mutex_unlock ( void )
{
    g_mutex_unlock ( grisbi_window_etat_mutex );
}


/**
 * libère la mémoire utilisée par la partie de priv propre a un fichier
 *
 * \param active window
 * \param struct etat
 *
 * \return
 **/
void grisbi_window_free_priv_file ( GrisbiWindow *window )
{

    devel_debug (NULL);

    /* libération de la mémoire des titres et fichier */
    g_free ( window->priv->filename );
    window->priv->filename = NULL;

    g_free ( window->priv->file_title );
    window->priv->file_title = NULL;

    g_free ( window->priv->window_title );
    window->priv->window_title = NULL;

    /* free others variables */
    free_variables ();

    /* libération de la mémoiré utilisée par etat */
    grisbi_window_free_struct_etat ( window->priv->etat );
    window->priv->etat = NULL;

    /* libération de la mémoire utilisée par la liste des comptes */
    grisbi_window_free_list_accounts ( window );


}


/**
 *
 *
 * \param
 *
 * \return
 **/
/* Local Variables: */
/* c-basic-offset: 4 */
/* End: */
