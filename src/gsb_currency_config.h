#ifndef _GSB_CURRENCY_CONFIG_H
#define _GSB_CURRENCY_CONFIG_H (1)

#include <gtk/gtk.h>

/* START_INCLUDE_H */
#include "gsb_real.h"
/* END_INCLUDE_H */

/** This structure holds information needed for exchange rates
  cache. */
struct CachedExchangeRate {
    gint currency1_number;	/** First currency */
    gint currency2_number;	/** Second currency */
    GsbReal rate;			/** Exchange rate between currency1 and currency 2 */
    GsbReal fees;			/** Fees associated with exchange rate */
};

/** ISO 4217 currency.  Not specific to Grisbi. */
struct Iso4217Currency
{										/* + correspondance struct Iso4217Currency */
    const gchar * continent;			/*                          = Europe       */
	const gchar * currency_name;		/* -> currency_name         = Euro         */
	const gchar * country_name;			/*                          = France       */
    const gchar * currency_code_iso;	/* -> currency_code_iso4217 = EUR          */
    const gchar * currency_nickname;	/* -> currency_code         = €            */
    gboolean active;
    const gchar *flag_filename;
    gint floating_point;  				/* ->floating_point (Number of digits after the point => 2 : 0.00 / 1 : 0.0 )*/
    gboolean main_currency;				/** Skip this when autodetecting base currency, in case there are
										 * several countries using the same currency but a "main" one,
	 									 * like USA and Panama sharing USD as official currency..  */
};

/** Columns numbers for currencies list  */
enum CurrencyConfigListColumns {
    CURRENCY_FLAG_COLUMN,
    CURRENCY_HAS_FLAG,
    COUNTRY_NAME_COLUMN,
    CURRENCY_NAME_COLUMN,
    CURRENCY_CODE_ISO_COLUMN,
    CURRENCY_NICKNAME_COLUMN,
    CURRENCY_FLOATING_COLUMN,
    CURRENCY_NUMBER_COLUMN,
    CURRENCY_MAIN_CURRENCY_COLUMN,
	CURRENCY_BACKGROUND_COLOR,
    NUM_CURRENCIES_COLUMNS
};


/* START_DECLARATION */
gboolean 	gsb_currency_config_add_currency 						(GtkWidget *button,
																	 GtkWidget *tree_view);
gboolean 	gsb_currency_config_add_currency_set_combobox 			(GtkWidget *button,
																	 GtkWidget *combobox);
GtkWidget *	gsb_currency_config_create_box_popup 					(GCallback select_callback);
gint 		gsb_currency_config_create_currency 					(const gchar *currency_name,
																	 const gchar *currency_code,
																	 const gchar *currency_isocode,
																	 gint floating_point);
gint 		gsb_currency_config_create_currency_from_iso4217list	(const gchar *currency_name);
GtkWidget *	gsb_currency_config_create_page 						(void);
GtkWidget *	gsb_currency_config_create_totals_page 					(void);
GtkWidget * gsb_currency_config_new_combobox						(gint *value,
																	 GCallback hook);
gboolean 	gsb_currency_config_select_default 						(GtkTreeModel *tree_model,
																	 GtkTreePath *path,
																	 GtkTreeIter *iter,
																	 GtkTreeView *tree_view);
/* END_DECLARATION */
#endif
