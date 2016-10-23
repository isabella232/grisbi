/* *******************************************************************************/
/*                                 GRISBI                                        */
/*              Programme de gestion financière personnelle                      */
/*                              license : GPLv2                                  */
/*                                                                               */
/*     Copyright (C)    2000-2008 Cédric Auger (cedric@grisbi.org)               */
/*                      2003-2008 Benjamin Drieu (bdrieu@april.org)              */
/*          2008-2016 Pierre Biava (grisbi@pierre.biava.name)                    */
/*          http://www.grisbi.org                                                */
/*                                                                               */
/*     This program is free software; you can redistribute it and/or modify      */
/*     it under the terms of the GNU General Public License as published by      */
/*     the Free Software Foundation; either version 2 of the License, or         */
/*     (at your option) any later version.                                       */
/*                                                                               */
/*     This program is distributed in the hope that it will be useful,           */
/*     but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/*     GNU General Public License for more details.                              */
/*                                                                               */
/*     You should have received a copy of the GNU General Public License         */
/*     along with this program; if not, write to the Free Software               */
/*     Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */
/*                                                                               */
/* *******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "include.h"
#include <stdlib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <errno.h>

#ifdef HAVE_GOFFICE
#include <goffice/goffice.h>
#endif /* HAVE_GOFFICE */

/*START_INCLUDE*/
#include "grisbi_app.h"
#include "grisbi_settings.h"
#include "gsb_assistant_first.h"
#include "gsb_color.h"
#include "gsb_dirs.h"
#include "gsb_file.h"
#include "gsb_locale.h"
#include "gsb_rgba.h"
#include "help.h"
#include "import.h"
#include "menu.h"
#include "structures.h"
#include "tip.h"
#include "traitement_variables.h"
#include "utils_str.h"
#include "erreur.h"
/*END_INCLUDE*/

/*START_STATIC*/
 static GrisbiWin *grisbi_app_create_window (GrisbiApp *app,
                                             GdkScreen *screen);
/*END_STATIC*/

/*START_EXTERN Variables externes PROVISOIRE*/
/*END_EXTERN*/

typedef struct  /* GrisbiAppPrivate */
{
    /* command line parsing */
    gboolean 			new_window;
    gint 				debug_level;
    gchar 				*geometry;
    GSList 				*file_list;

	/* Menuapp et menubar */
    gboolean            has_app_menu;
	GMenuModel 			*appmenu;
	GMenuModel 			*menubar;

    GMenu               *item_recent_files;
    GMenu               *item_edit;

    GAction             *prefs_action;
}GrisbiAppPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GrisbiApp, grisbi_app, GTK_TYPE_APPLICATION);

/* global variable, see structures.h */
struct GrisbiAppConf    conf;                   /* conf structure Provisoire */
GtkCssProvider *        css_provider = NULL;    /* css provider */

/******************************************************************************/
/* Private functions                                                            */
/******************************************************************************/
/* STRUCTURE CONF */
/**
 * initialisation de la variable conf
 *
 * \param
 *
 * \return
 **/
static void grisbi_app_struct_conf_init ( void )
{
    devel_debug (NULL);

    conf.font_string = NULL;
    conf.browser_command = NULL;
    conf.last_open_file = NULL;
}

/**
 * libération de la mémoire de la variable conf
 *
 * \param
 *
 * \return
 **/
static void grisbi_app_struct_conf_free ( void )
{
    devel_debug (NULL);

	if ( conf.font_string )
    {
		g_free ( conf.font_string );
		conf.font_string = NULL;
    }
	if ( conf.browser_command )
	{
        g_free ( conf.browser_command );
		conf.browser_command = NULL;
	}

    if ( conf.last_open_file )
    {
        g_free ( conf.last_open_file );
        conf.last_open_file = NULL;
    }

	gsb_file_free_last_path ();
	gsb_file_free_backup_path ();

}

/* ACCELERATORS*/
/**
 * Charge les raccourcis claviers (non modifiables)
 *
 * \param GrisbiApp     *app
 *
 * \return
 **/
static void grisbi_app_setup_accelerators (GrisbiApp *app)
{
    const gchar *accels[] = {NULL, NULL, NULL, NULL};

    accels[0] = "F11";
    gtk_application_set_accels_for_action ( GTK_APPLICATION ( app ), "win.fullscreen", accels );

}

/* MENU APP*/
/**
 *
 *
 * \param GSimpleAction     action
 * \param GVariant          state
 * \param gpointer          user_data
 *
 * \return
 **/
static void grisbi_app_change_fullscreen_state ( GSimpleAction *action,
                        GVariant *state,
                        gpointer user_data )
{
    GtkWindow *win;

    win = GTK_WINDOW ( grisbi_app_get_active_window ( user_data ) );

    if ( g_variant_get_boolean ( state ) )
    {
        conf.full_screen = TRUE;
        gtk_window_fullscreen ( win );
    }
    else
    {
        conf.full_screen = FALSE;
        gtk_window_unfullscreen ( win );
    }

    g_simple_action_set_state ( action, state );
}

/**
 *
 *
 * \param GSimpleAction     action
 * \param GVariant          state
 * \param gpointer          user_data
 *
 * \return
 **/
static void grisbi_app_change_radio_state ( GSimpleAction *action,
                        GVariant *state,
                        gpointer user_data )
{
    g_simple_action_set_state ( action, state );
}

static void grisbi_app_new_window (GSimpleAction *action,
                                   GVariant      *parameter,
                                   gpointer       user_data)
{
    GrisbiApp *app;

    app = GRISBI_APP (user_data);
    grisbi_app_create_window (GRISBI_APP (app), NULL);
}

static void grisbi_app_quit (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
    GrisbiApp *app;
    GList *l;
    gboolean first_win = TRUE;

    app = GRISBI_APP (user_data);

    /* Remove all windows registered in the application */
    while ((l = gtk_application_get_windows (GTK_APPLICATION (app))))
    {
        if (first_win)
        {
            if ( l->data && conf.full_screen == 0 && conf.maximize_screen == 0 )
            {
                /* sauvegarde la position de la fenetre principale */
                gtk_window_get_position (GTK_WINDOW (l->data), &conf.x_position, &conf.y_position);

                /* sauvegarde de la taille de la fenêtre si nécessaire */
                gtk_window_get_size (GTK_WINDOW (l->data), &conf.main_width, &conf.main_height);
            }
            first_win = FALSE;
        }
        gsb_file_close ();
        grisbi_win_free_private_struct (GRISBI_WIN (l->data));
        gtk_application_remove_window (GTK_APPLICATION (app), GTK_WINDOW (l->data));
    }
}

/* Disable: warning: missing field 'padding' initializer
 *
 * 'padding' is a private field in the GActionEntry stucture so it is
 * not a good idea to explicitly initialize it. */
/* https://gcc.gnu.org/onlinedocs/gcc/Diagnostic-Pragmas.html */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#else
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

static GActionEntry app_entries[] =
{
	{ "new-window", grisbi_app_new_window, NULL, NULL, NULL },
	{ "about", grisbi_cmd_about, NULL, NULL, NULL },
	{ "prefs", grisbi_cmd_prefs, NULL, NULL, NULL },
	{ "quit", grisbi_app_quit, NULL, NULL, NULL },
};

static const GActionEntry win_always_enabled_entries[] =
{
	{ "new-acc-file", grisbi_cmd_file_new, NULL, NULL, NULL },
	{ "open-file", grisbi_cmd_file_open_menu, NULL, NULL, NULL },
    { "direct-open-file", grisbi_cmd_file_open_direct_menu, "s", NULL, NULL },
	{ "import-file", grisbi_cmd_importer_fichier, NULL, NULL, NULL },
	{ "obf-qif-file", grisbi_cmd_obf_qif_file, NULL, NULL, NULL },
	{ "manual", grisbi_cmd_manual, NULL, NULL, NULL },
	{ "quick-start", grisbi_cmd_quick_start, NULL, NULL, NULL },
	{ "web-site", grisbi_cmd_web_site, NULL, NULL, NULL },
	{ "report-bug", grisbi_cmd_report_bug, NULL, NULL, NULL },
	{ "day_tip", grisbi_cmd_day_tip, NULL, NULL, NULL },
    { "fullscreen", NULL, NULL, "false", grisbi_app_change_fullscreen_state }
};

static const GActionEntry win_context_enabled_entries[] =
{
	{ "save", grisbi_cmd_file_save, NULL, NULL, NULL },
	{ "save-as", grisbi_cmd_file_save_as, NULL, NULL, NULL },
	{ "export-accounts", grisbi_cmd_export_accounts, NULL, NULL, NULL },
	{ "create-archive", grisbi_cmd_create_archive, NULL, NULL, NULL },
	{ "export-archive", grisbi_cmd_export_archive, NULL, NULL, NULL },
	{ "debug-acc-file", grisbi_cmd_debug_acc_file, NULL, NULL, NULL },
	{ "obf-acc-file", grisbi_cmd_obf_acc_file, NULL, NULL, NULL },
	{ "debug-mode", grisbi_cmd_debug_mode_toggle, NULL, "false", NULL },
	{ "file-close", grisbi_cmd_file_close, NULL, NULL, NULL },
	{ "edit-ope", grisbi_cmd_edit_ope, NULL, NULL, NULL },
	{ "new-ope", grisbi_cmd_new_ope, NULL, NULL, NULL },
	{ "remove-ope", grisbi_cmd_remove_ope, NULL, NULL, NULL },
	{ "template-ope", grisbi_cmd_template_ope, NULL, NULL, NULL },
	{ "clone-ope", grisbi_cmd_clone_ope, NULL, NULL, NULL },
	{ "convert-ope", grisbi_cmd_convert_ope, NULL, NULL, NULL },
    { "move-to-acc", grisbi_cmd_move_to_account_menu, NULL, NULL, NULL },
	{ "new-acc", grisbi_cmd_new_acc, NULL, NULL, NULL },
	{ "remove-acc", grisbi_cmd_remove_acc, NULL, NULL, NULL },
	{ "show-form", grisbi_cmd_show_form_toggle, NULL, "false", NULL },
	{ "show-reconciled", grisbi_cmd_show_reconciled_toggle, NULL, "false", NULL },
	{ "show-archived", grisbi_cmd_show_archived_toggle, NULL, "false", NULL },
	{ "show-closed-acc", grisbi_cmd_show_closed_acc_toggle, NULL, "false", NULL },
	{ "show-ope", grisbi_cmd_show_ope_radio, "s", "'1'", grisbi_app_change_radio_state },
	{ "reset-width-col", grisbi_cmd_reset_width_col, NULL, NULL, NULL }
};

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#else
#pragma clang diagnostic pop
#endif

/**
 * crée et initialise le menu de grisbi.
 *
 * \param GApplication *app
 *
 * \return
 * */
static void grisbi_app_set_main_menu (GrisbiApp *app,
                                      gboolean has_app_menu)
{
    GrisbiAppPrivate *priv;
	GtkBuilder *builder;
	GError *error = NULL;

    priv = grisbi_app_get_instance_private ( GRISBI_APP ( app ) );

	/* chargement des actions */
	/* adding menus */
	builder = gtk_builder_new ();
	if ( !gtk_builder_add_from_resource ( builder,
						"/org/gtk/grisbi/ui/grisbi_menu.ui",
						&error ) )
	{
		g_warning ("loading menu builder file: %s", error->message);
		g_error_free (error);

		exit (1);
	}

    /* Menu Application */
    priv->has_app_menu = has_app_menu;

    if (has_app_menu)
    {
        GAction *action;

        g_action_map_add_action_entries ( G_ACTION_MAP ( app ),
                        app_entries,
						G_N_ELEMENTS ( app_entries ),
                        app );

        priv->appmenu = G_MENU_MODEL ( gtk_builder_get_object ( builder, "appmenu" ) );
        gtk_application_set_app_menu ( GTK_APPLICATION ( app ), priv->appmenu );
        priv->menubar = G_MENU_MODEL ( gtk_builder_get_object ( builder, "menubar" ) );
        gtk_application_set_menubar ( GTK_APPLICATION ( app ), priv->menubar );

        priv->prefs_action = g_action_map_lookup_action (G_ACTION_MAP ( app ), "prefs" );
        priv->item_recent_files = G_MENU ( gtk_builder_get_object ( builder, "recent-file" ) );
        priv->item_edit = G_MENU ( gtk_builder_get_object ( builder, "edit" ) );
        action = g_action_map_lookup_action (G_ACTION_MAP ( app ), "new-window" );
        g_simple_action_set_enabled ( G_SIMPLE_ACTION ( action ), FALSE );

    }
    else
    {
        /* Menu "traditionnel" */
        priv->menubar = G_MENU_MODEL ( gtk_builder_get_object ( builder, "classic" ) );
        gtk_application_set_menubar ( GTK_APPLICATION ( app ), priv->menubar );
        priv->item_recent_files = G_MENU ( gtk_builder_get_object ( builder, "classic-recent-file" ) );
        priv->item_edit = G_MENU ( gtk_builder_get_object ( builder, "classic-edit" ) );
    }

    grisbi_app_set_recent_files_menu ( app, FALSE );

    g_object_unref ( builder );
}

/* WINDOWS */
/**
 *
 *
 * \param
 *
 * \return
 **/
static gboolean grisbi_app_window_delete_event ( GrisbiWin *win,
                        GdkEvent *event,
                        GrisbiApp *app )
{
	devel_debug (NULL);

    if ( conf.full_screen == 0 && conf.maximize_screen == 0 )
    {
        /* sauvegarde la position de la fenetre principale */
        gtk_window_get_position ( GTK_WINDOW ( win ), &conf.x_position, &conf.y_position );

        /* sauvegarde de la taille de la fenêtre si nécessaire */
        gtk_window_get_size ( GTK_WINDOW ( win ), &conf.main_width, &conf.main_height );
    }

    gsb_file_close ();
    grisbi_win_free_private_struct (GRISBI_WIN (win));
    gtk_application_remove_window (GTK_APPLICATION (app), GTK_WINDOW (win));

	return FALSE;
}

/**
 * Création d'une fenêtre GrisbiWin.
 *
 * \param app
 * \param screen
 *
 * \return une fenêtre pour Grisbi
 */
static GrisbiWin *grisbi_app_create_window ( GrisbiApp *app,
                        GdkScreen *screen )
{
    GrisbiWin *win;
    GrisbiAppPrivate *priv;
    gchar *string;

    priv = grisbi_app_get_instance_private ( GRISBI_APP ( app ) );

	win = g_object_new ( GRISBI_TYPE_WIN, "application", app, NULL );

    g_signal_connect ( win,
                        "delete_event",
                        G_CALLBACK ( grisbi_app_window_delete_event ),
                        app );

	gtk_application_add_window ( GTK_APPLICATION ( app ), GTK_WINDOW ( win ) );

    /* create the icon of grisbi (set in the panel of gnome or other) */
    string = g_build_filename ( gsb_dirs_get_pixmaps_dir (), "grisbi-logo.png", NULL );
    if ( g_file_test ( string, G_FILE_TEST_EXISTS ) )
        gtk_window_set_default_icon_from_file ( string, NULL );

    g_free (string);

	if ( priv->geometry )
		gtk_window_parse_geometry ( GTK_WINDOW ( win ), priv->geometry );
    else
        grisbi_win_set_size_and_position ( GTK_WINDOW ( win ) );

    /* set menubar */
	/* Win Menu : actions toujours actives */
	g_action_map_add_action_entries ( G_ACTION_MAP ( win ),
						win_always_enabled_entries,
						G_N_ELEMENTS ( win_always_enabled_entries ),
						app );

	/* Win Menu : actions actives selon le contexte */
	g_action_map_add_action_entries ( G_ACTION_MAP ( win ),
						win_context_enabled_entries,
						G_N_ELEMENTS ( win_context_enabled_entries ),
						app );

    /* Actions du menu Application à intégrer dans le menu classique */
    if (!priv->has_app_menu)
    {
        g_action_map_add_action_entries ( G_ACTION_MAP ( win ),
                        app_entries,
						G_N_ELEMENTS ( app_entries ),
                        app );

        priv->prefs_action = g_action_map_lookup_action ( G_ACTION_MAP ( win ), "prefs" );
    }

    grisbi_win_init_menubar ( win, app );

	/* affiche la fenêtre principale */
	gtk_window_present (GTK_WINDOW (win));

	if ( screen != NULL )
        gtk_window_set_screen ( GTK_WINDOW ( win ), screen );

    return win;
}

static const GOptionEntry options[] =
{
	/* Version */
	{
		"version", 'V', 0, G_OPTION_ARG_NONE, NULL,
		N_("Show the application's version"), NULL
	},

	/* Open a new window */
	{
		"new-window", '\0', 0, G_OPTION_ARG_NONE, NULL,
		N_("Create a new top-level window in an existing instance of grisbi"),
		NULL
	},

    /* debug level */
    {
        "debug", 'd', 0, G_OPTION_ARG_STRING, NULL,
        N_("Debug mode: level 0-5"),
		N_("DEBUG")
    },

	/* Window geometry */
	{
		"geometry", 'g', 0, G_OPTION_ARG_STRING, NULL,
		N_("Set the size and position of the window (WIDTHxHEIGHT+X+Y)"),
		N_("GEOMETRY")
	},

	/* New instance */
/*	{
		"standalone", 's', 0, G_OPTION_ARG_NONE, NULL,
		N_("Run grisbi in standalone mode"),
		NULL
	},
*/
	/* collects file arguments */
	{
		G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, NULL, NULL,
		N_("[FILE...]")
	},

	{ NULL, 0, 0, 0, NULL, NULL, NULL}
};

/**
 * affiche les information d'environnement
 *
 * \param
 *
 * \return
 * */
static gboolean grisbi_app_print_environment_var ( void )
{
    gchar *tmp_str;

    g_printf ("Variables d'environnement :\n\n" );

    tmp_str = gsb_locale_get_print_locale_var ();
    g_printf ("%s", tmp_str);

    g_free ( tmp_str );

    g_printf ( "gint64\n\tG_GINT64_MODIFIER = \"%s\"\n"
                        "\t%"G_GINT64_MODIFIER"d\n\n",
                        G_GINT64_MODIFIER,
                        G_MAXINT64 );

    tmp_str = gsb_dirs_get_print_dir_var ();
    g_printf ("%s", tmp_str);

    g_free ( tmp_str );

    return FALSE;
}

/**
 * gestionnaire des options passées au programme
 *
 * \param GApplication  *app
 * \param GApplicationCommandLine *command_line
 *
 * \return always TRUE
 **/
static gboolean grisbi_app_cmdline ( GApplication *application,
                        GApplicationCommandLine *cmdline )
{
	GrisbiAppPrivate *priv;
	GVariantDict *options;
	gchar *tmp_str = NULL;
	const gchar **remaining_args;

	priv = grisbi_app_get_instance_private (GRISBI_APP (application));

	/* initialisation de debug_level à -1 */
	priv->debug_level = -1;

	/* traitement des autres options */
	options = g_application_command_line_get_options_dict ( cmdline );

	g_variant_dict_lookup ( options, "new-window", "b", &priv->new_window );
	g_variant_dict_lookup ( options, "debug", "s", &tmp_str );
	g_variant_dict_lookup ( options, "d", "s", &tmp_str );
	g_variant_dict_lookup ( options, "geometry", "s", &priv->geometry );
	g_variant_dict_lookup ( options, "g", "s", &priv->geometry );

    /* Parse filenames */
	if ( g_variant_dict_lookup ( options, G_OPTION_REMAINING, "^a&ay", &remaining_args ) )
	{
		gint i;

		for (i = 0; remaining_args[i]; i++)
		{
			if ( g_file_test ( remaining_args[i], G_FILE_TEST_EXISTS ) )
			{
				priv->file_list = g_slist_prepend ( priv->file_list, g_strdup ( remaining_args[i] ) );
			}
		}

		if ( g_slist_length ( priv->file_list ) > 1 )
			priv->file_list = g_slist_reverse (priv->file_list);

		g_free ( remaining_args );
	}

	/* modification du niveau de débug si besoin */
	if ( tmp_str && strlen ( tmp_str ) > 0 )
	{
		gchar *endptr;
		gint64 number;

		errno = 0;
		number = g_ascii_strtoll ( tmp_str, &endptr, 10 );
		if ( endptr == NULL )
			priv->debug_level = number;
		else if ( errno == 0 && number == 0 )
			priv->debug_level = number;
	}

	if ( IS_DEVELOPMENT_VERSION == 1 )
    {
		if ( priv->debug_level >= 0 && priv->debug_level < 5 )
			debug_set_cmd_line_debug_level ( priv->debug_level );
    }
	else if ( priv->debug_level > 0 )
	{
		debug_initialize_debugging ( priv->debug_level );
        grisbi_app_print_environment_var ();
	}

	if (priv->new_window)
		grisbi_app_create_window (GRISBI_APP (application), NULL);

	g_application_activate (application);

	return FALSE;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
static gint grisbi_app_handle_local_options ( GApplication *app,
                        GVariantDict *options)
{
    if ( g_variant_dict_contains ( options, "version" ) )
    {
        g_print ( "%s - Version %s\n", g_get_application_name (), VERSION );
        g_print( "\n\n" );
        g_print( "%s", extra_support () );
        return 0;
    }

    return -1;
}

/**
 * On detourne les signaux SIGINT, SIGTERM, SIGSEGV
 *
 * \param
 *
 * \return
 * */
static void grisbi_app_trappe_signaux ( void )
{
#ifndef G_OS_WIN32
    struct sigaction sig_sev;

    memset ( &sig_sev, 0, sizeof ( struct sigaction ) );
    sig_sev.sa_handler = debug_traitement_sigsegv;
    sig_sev.sa_flags = 0;
    sigemptyset ( &( sig_sev.sa_mask ) );

    if ( sigaction ( SIGINT, &sig_sev, NULL ) )
        g_print ( _("Error on sigaction: SIGINT won't be trapped\n") );

    if ( sigaction ( SIGTERM, &sig_sev, NULL ) )
        g_print ( _("Error on sigaction: SIGTERM won't be trapped\n") );

    if ( sigaction ( SIGSEGV, &sig_sev, NULL ) )
        g_print ( _("Error on sigaction: SIGSEGV won't be trapped\n") );
#endif /* G_OS_WIN32 */
}

/**
 * Load file if necessary
 *
 * \param GApplication *app
 *
 * \return
 **/
static gboolean grisbi_app_load_file_if_necessary ( GrisbiApp *app )
{
    GrisbiAppPrivate *priv;

    priv = grisbi_app_get_instance_private ( GRISBI_APP ( app ) );

    /* check the command line, if there is something to open */
    if ( priv->file_list )
    {
		gchar *tmp_str = NULL;
		GSList *tmp_list;

		tmp_list = 	priv->file_list;

        /* on n'ouvre que le premier fichier de la liste */
        tmp_str = tmp_list -> data;

        if ( gsb_file_open_file ( tmp_str ) )
        {
            gchar *tmp_uri;

            tmp_uri = g_filename_to_uri ( tmp_str, NULL, NULL );
            gsb_menu_recent_manager_remove_item ( NULL, tmp_str );
            gtk_recent_manager_add_item ( gtk_recent_manager_get_default (), tmp_uri );
            g_free ( tmp_uri );
            grisbi_app_set_recent_files_menu ( NULL, TRUE );

            return TRUE;
        }
        else
            return FALSE;
	}
    else
    {
        /* open the last file if needed, nom_fichier_comptes was filled while loading the configuration */
        if ( conf.dernier_fichier_auto && conf.last_open_file )
        {
            if ( !gsb_file_open_file ( conf.last_open_file ) )
            {
                g_free ( conf.last_open_file );
                conf.last_open_file = NULL;

                return FALSE;
            }
            else
                return TRUE;
        }
    }

    return FALSE;
}

/**
 * lancement de l'application
 *
 * \param GApplication *app
 *
 * \return
 **/
static void grisbi_app_startup ( GApplication *application )
{
	GrisbiApp *app = GRISBI_APP (application);
    GFile *file = NULL;
    gchar *tmp_dir;
    GtkSettings* settings;
	gboolean has_app_menu = FALSE;

    /* Chain up parent's startup */
    G_APPLICATION_CLASS (grisbi_app_parent_class)->startup (application);

	settings = gtk_settings_get_default ();
    if (settings)
    {
        g_object_get (G_OBJECT (settings),
                      "gtk-shell-shows-app-menu", &has_app_menu,
                      NULL);
    }

    /* on commence par détourner le signal SIGSEGV */
    grisbi_app_trappe_signaux ();

    /* initialisation des variables de configuration globales */
    grisbi_settings_get ();

    /* load the CSS properties */
    css_provider = gtk_css_provider_get_default ();
    tmp_dir = g_strconcat ( gsb_dirs_get_ui_dir (), "/grisbi.css", NULL );
    file = g_file_new_for_path ( tmp_dir );
    if ( !gtk_css_provider_load_from_file ( css_provider, file, NULL ) )
        warning_debug (tmp_dir);
    g_free ( tmp_dir );

    /* initialise les couleurs */
    gsb_rgba_initialise_couleurs_par_defaut ();
    /* initialise les variables d'état */
    init_variables ();
    /* enregistre les formats d'importation */
    register_import_formats ();

	/* app menu */
    if (conf.force_classic_menu)
        has_app_menu = FALSE;
    grisbi_app_set_main_menu (app, has_app_menu);

    /* charge les raccourcis claviers */
    grisbi_app_setup_accelerators ( app );
}

/**
 * grisbi_app_activate
 *
 * \param GApplication *app
 *
 * \return
 **/
static void grisbi_app_activate (GApplication *application)
{
    gboolean load_file = FALSE;

	devel_debug ( NULL );

	/* création de la fenêtre pincipale */
    grisbi_app_create_window (GRISBI_APP (application), NULL);

    /* set the CSS properties */
    if ( css_provider )
        gtk_style_context_add_provider_for_screen ( gdk_display_get_default_screen (
                                                    gdk_display_get_default () ),
                                                    GTK_STYLE_PROVIDER ( css_provider ),
                                                    GTK_STYLE_PROVIDER_PRIORITY_USER );

    /* lance un assistant si première utilisation */
    if (conf.first_use)
    {
        gsb_assistant_first_run ();
        conf.first_use = FALSE;
    }
    else
    {
        /* ouvre un fichier si demandé */
        load_file = grisbi_app_load_file_if_necessary (GRISBI_APP (application));
        if (load_file)
            display_tip (FALSE);
    }
}

/**
 * grisbi_app_open
 *
 * \param GApplication *app
 *
 * \return
 **/
static void grisbi_app_open ( GApplication *application,
                        GFile **files,
                        gint n_files,
                        const gchar *hint )
{
    GList *windows;
    GrisbiWin *win;
    int i;

    windows = gtk_application_get_windows (GTK_APPLICATION (application));
    if ( windows )
        win = GRISBI_WIN ( windows->data );
    else
        win = grisbi_app_create_window (GRISBI_APP (application), NULL);

    for ( i = 0; i < n_files; i++ )
	{

/*        grisbi_win_open ( win, files[i] );
*/	}
    gtk_window_present ( GTK_WINDOW ( win ) );
}

/******************************************************************************/
/* Fonctions propres à l'initialisation de l'application                      */
/******************************************************************************/
/**
 * grisbi_app_dispose
 *
 * \param GApplication *app
 *
 * \return
 **/
static void grisbi_app_dispose ( GObject *object )
{
    GrisbiAppPrivate *priv;

	devel_debug (NULL);

	priv = grisbi_app_get_instance_private ( GRISBI_APP ( object ) );

    /* liberation de la mémoire utilisée par les objets de priv*/
    g_clear_object ( &priv->appmenu );
    g_clear_object ( &priv->menubar );

    G_OBJECT_CLASS (grisbi_app_parent_class)->dispose ( object );
}

/**
 * grisbi_app_shutdown
 *
 * \param GApplication *app
 *
 * \return
 **/
static void grisbi_app_shutdown (GApplication *application)
{
	devel_debug (NULL);

    /* clean finish of the debug file */
    if ( etat.debug_mode )
        debug_finish_log ();

    /* on libère la mémoire utilisée par etat */
    free_variables ();

	/* libération de mémoire utilisée par locale*/
    gsb_locale_shutdown ();

	/* libération de mémoire utilisée par gsb_dirs*/
    gsb_dirs_shutdown ();

    /* Sauvegarde de la configuration générale */
    grisbi_settings_save_app_config ();

	/* on libère la mémoire utilisée par conf */
    grisbi_app_struct_conf_free ();

    G_APPLICATION_CLASS (grisbi_app_parent_class)->shutdown (application);
}

/**
 * grisbi_app_init
 *
 * \param GApplication *app
 *
 * \return
 **/
static void grisbi_app_init ( GrisbiApp *app )
{
    /* initialize debugging */
    if (IS_DEVELOPMENT_VERSION == 1)
    {
        debug_initialize_debugging (5);
        grisbi_app_print_environment_var ();
    }

    g_set_application_name ("Grisbi");

	/* add options for app */
    g_application_add_main_option_entries ( G_APPLICATION ( app ), options );

    /* initialisation de la variable conf */
    grisbi_app_struct_conf_init ();
}

/**
 * grisbi_app_class_init
 *
 * \param GrisbiAppClass    *class
 *
 * \return
 **/
static void grisbi_app_class_init ( GrisbiAppClass *klass )
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

    app_class->startup = grisbi_app_startup;
    app_class->activate = grisbi_app_activate;
    app_class->open = grisbi_app_open;
    app_class->handle_local_options = grisbi_app_handle_local_options;
    app_class->command_line = grisbi_app_cmdline;

    app_class->shutdown = grisbi_app_shutdown;
    object_class->dispose = grisbi_app_dispose;
}

/******************************************************************************/
/* Public functions                                                           */
/******************************************************************************/
/**
 * get active window.
 *
 * \param app
 *
 * \return active_window
 */
GrisbiWin *grisbi_app_get_active_window ( GrisbiApp *app )
{
	GrisbiWin *win;

    if ( app == NULL )
        app = GRISBI_APP ( g_application_get_default () );

    win = GRISBI_WIN ( gtk_application_get_active_window ( GTK_APPLICATION ( app ) ) );

    return win;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
gboolean grisbi_app_get_has_app_menu (GrisbiApp *app)
{
	GrisbiAppPrivate *priv;

	priv = grisbi_app_get_instance_private (GRISBI_APP (app));

	return priv->has_app_menu;


}

/**
 * grisbi_app_get_menu_edit
 *
 * \param
 *
 * \return GMenu
 * */
GMenu *grisbi_app_get_menu_edit ( void )
{
	GApplication *app;
	GrisbiAppPrivate *priv;

    app = g_application_get_default ();
	priv = grisbi_app_get_instance_private ( GRISBI_APP ( app ) );

	return priv->item_edit;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
GAction *grisbi_app_get_prefs_action ( void )
{
    GrisbiAppPrivate *priv;

    priv = grisbi_app_get_instance_private ( GRISBI_APP ( g_application_get_default () ) );

    return priv->prefs_action;
}

/**
 *
 *
 * \param gchar **  recent_array
 *
 * \return
 **/
void grisbi_app_init_recent_manager ( gchar **recent_array )
{
	GtkRecentManager *recent_manager;
	gchar *uri = NULL;
	gint i;
    gint nb_effectif = 0;
	gboolean result = 0;

    uri = g_build_filename ( g_get_user_data_dir (), "recently-used.xbel", NULL);
    if ( !g_file_test ( uri,  G_FILE_TEST_EXISTS ) )
    {
        g_free ( uri );
        return;
    }
    g_free ( uri );

	recent_manager = gtk_recent_manager_get_default ();

    if ( conf.nb_derniers_fichiers_ouverts > conf.nb_max_derniers_fichiers_ouverts )
    {
        conf.nb_derniers_fichiers_ouverts = conf.nb_max_derniers_fichiers_ouverts;
    }

	for ( i=0 ; i < conf.nb_derniers_fichiers_ouverts ; i++ )
    {
		uri = g_filename_to_uri ( recent_array[i], NULL, NULL );
        if ( g_file_test ( recent_array[i], G_FILE_TEST_EXISTS ) )
        {
            if ( !gtk_recent_manager_has_item ( recent_manager, uri ) )
                result = gtk_recent_manager_add_item (  recent_manager, uri );
            if ( result )
            {
                nb_effectif++;
            }
        }
        g_free ( uri );
	}
    conf.nb_derniers_fichiers_ouverts = nb_effectif;
}

/**
 * cherche si le fichier est déjà utilisé
 *
 * \param gchar		filename
 *
 * \return TRUE is duplicate file FALSE otherwise
 **/
gboolean grisbi_app_is_duplicated_file ( const gchar *filename )
{
	GrisbiApp *app;
	GList *windows;
	GList *tmp_list;

	app = GRISBI_APP ( g_application_get_default () );

	windows = gtk_application_get_windows ( GTK_APPLICATION ( app ) );
	tmp_list = windows;

	while ( tmp_list )
	{
		GrisbiWin *win;
		const gchar *tmp_filename;
		gchar *key1;
		gchar *key2;

		win = tmp_list->data;
		tmp_filename = grisbi_win_get_filename ( win );
		key1 = g_utf8_collate_key_for_filename ( filename, -1 );
		key2 = g_utf8_collate_key_for_filename ( tmp_filename, -1 );

		if ( strcmp ( key1, key2 ) == 0 )
		{
			g_free ( key1 );
			g_free ( key2 );
			return TRUE;
		}

		tmp_list = tmp_list->next;
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
gchar **grisbi_app_get_recent_files_array ( void )
{
	GtkRecentManager *recent_manager;
    GList *tmp_list;
    GList *l;
    gchar **recent_array = NULL;
	gchar *uri = NULL;
    gint index = 0;

	/* initialisation du tableau des fichiers récents */
    recent_array = g_new ( gchar *, conf.nb_max_derniers_fichiers_ouverts + 1);

    recent_manager = gtk_recent_manager_get_default ();

    tmp_list = gtk_recent_manager_get_items ( recent_manager );
    for ( l = tmp_list; l != NULL; l = l->next )
    {
        GtkRecentInfo *info;

        info = l->data;
        uri = gtk_recent_info_get_uri_display ( info );
        if ( g_str_has_suffix ( uri, ".gsb" ) )
		{
            if ( !g_file_test ( uri, G_FILE_TEST_EXISTS ) )
            {
                tmp_list = tmp_list->next;
                g_free ( uri );
                continue;
            }
			if ( index < conf.nb_max_derniers_fichiers_ouverts )
			{
				recent_array[index++] = g_strdup ( uri );
			}
		}
        g_free ( uri );
    }
    recent_array[index] = NULL;
    conf.nb_derniers_fichiers_ouverts = index;
    g_list_free_full ( tmp_list, ( GDestroyNotify ) gtk_recent_info_unref );

    return recent_array;
}

/**
 * crée et initialise le sous menu des fichiers récents.
 *
 * \param GApplication  *app
 * \param gboolean      reset 0 = création 1 = update
 *
 * \return
 * */
void grisbi_app_set_recent_files_menu (GrisbiApp *app,
                        gboolean reset )
{
    GrisbiAppPrivate *priv;
    GList *tmp_list;
    GList *l;
    gchar *detailled_action;
    gchar *uri;
    const gchar *filename;
    gint index = 0;

    devel_debug_int (reset);

    if ( app == NULL )
        app = GRISBI_APP (g_application_get_default ());

    priv = grisbi_app_get_instance_private ( GRISBI_APP ( app ) );

    if ( reset )
    {
        GMenuItem *menu_item;

        g_menu_remove_all ( priv->item_recent_files );
        filename = grisbi_win_get_filename ( NULL );
        detailled_action = g_strdup_printf ("win.direct-open-file::%d", index+1 );
        menu_item = g_menu_item_new ( filename, detailled_action );
        g_menu_append_item ( priv->item_recent_files, menu_item );
        index++;
        g_free ( detailled_action );
        g_object_unref ( menu_item );
    }

    tmp_list = gtk_recent_manager_get_items ( gtk_recent_manager_get_default () );
    for ( l = tmp_list; l != NULL; l = l->next )
    {
        GtkRecentInfo *info;

        info = l->data;
        uri = gtk_recent_info_get_uri_display ( info );
        if ( g_str_has_suffix ( uri, ".gsb" ) )
		{
            if ( !g_file_test ( uri, G_FILE_TEST_EXISTS ) )
            {
                g_free ( uri );
                continue;
            }
            if ( reset )
            {
                if ( strcmp ( uri, filename ) == 0 )
                {
                    g_free ( uri );
                    continue;
                }
            }
			if ( index < conf.nb_max_derniers_fichiers_ouverts )
			{
                GMenuItem *menu_item;

                detailled_action = g_strdup_printf ("win.direct-open-file::%d", index+1 );
                menu_item = g_menu_item_new ( uri, detailled_action );
                g_menu_append_item ( priv->item_recent_files, menu_item );
                if ( index == 0 )
                {
                    conf.last_open_file = my_strdup ( uri );
                }
				index++;
                g_free ( detailled_action );
                g_object_unref ( menu_item );
			}
		}
        g_free ( uri );
    }
    g_list_free_full ( tmp_list, ( GDestroyNotify ) gtk_recent_info_unref );
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
