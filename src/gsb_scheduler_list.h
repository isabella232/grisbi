#ifndef _ECHEANCIER_LISTE_H
#define _ECHEANCIER_LISTE_H (1)

#define COL_NB_DATE 0
#define COL_NB_ACCOUNT 1
#define COL_NB_PARTY 2
#define COL_NB_FREQUENCY 3
#define COL_NB_MODE 4
#define COL_NB_NOTES 5
#define COL_NB_AMOUNT 6		/* doit Ãªtre le dernier de la liste
				   Ã  cause de plusieurs boucles for */
#define NB_COLS_SCHEDULER 7

/* define the columns in the store
 * as the data are filled above, the number here
 * begin at NB_COLS_SCHEDULER */

#define SCHEDULER_COL_NB_BACKGROUND 8		/*< color of the background */
#define SCHEDULER_COL_NB_SAVE_BACKGROUND 9	/*< when selection, save of the normal color of background */
#define SCHEDULER_COL_NB_AMOUNT_COLOR 10 	/*< color of the amount */
#define SCHEDULER_COL_NB_TRANSACTION_NUMBER 11
#define SCHEDULER_COL_NB_FONT 12		/*< PangoFontDescription if used */
#define SCHEDULER_COL_NB_VIRTUAL_TRANSACTION 13 /*< to 1 if it's a calculated scheduled transaction (for longer view), so, cannot edit */

#define SCHEDULER_COL_NB_TOTAL 14


enum scheduler_periodicity {
    SCHEDULER_PERIODICITY_ONCE_VIEW,
    SCHEDULER_PERIODICITY_WEEK_VIEW,
    SCHEDULER_PERIODICITY_MONTH_VIEW,
    SCHEDULER_PERIODICITY_TWO_MONTHS_VIEW,
    SCHEDULER_PERIODICITY_TRIMESTER_VIEW,
    SCHEDULER_PERIODICITY_YEAR_VIEW,
    SCHEDULER_PERIODICITY_CUSTOM_VIEW,
    SCHEDULER_PERIODICITY_NB_CHOICES,
};


/* START_INCLUDE_H */
#include "gsb_scheduler_list.h"
/* END_INCLUDE_H */


/* START_DECLARATION */
gboolean affichage_traits_liste_echeances ( void );
void gsb_scheduler_check_scheduled_transactions_time_limit ( void );
gboolean gsb_scheduler_delete_scheduled_transaction ( gint scheduled_number );
gboolean gsb_scheduler_list_change_scheduler_view ( enum scheduler_periodicity periodicity );
GtkWidget *gsb_scheduler_list_create_list ( void );
gboolean gsb_scheduler_list_edit_transaction ( gint scheduled_number );
gboolean gsb_scheduler_list_execute_transaction ( gint scheduled_number );
gboolean gsb_scheduler_list_fill_list ( GtkWidget *tree_view );
gint gsb_scheduler_list_get_current_scheduled_number ( GtkWidget *tree_view );
gboolean gsb_scheduler_list_key_press ( GtkWidget *tree_view,
					GdkEventKey *ev );
gboolean gsb_scheduler_list_show_notes ( void );
void supprime_echeance ( gint scheduled_number );
/* END_DECLARATION */
#endif
