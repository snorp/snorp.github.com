
/* 
 * Copyright (C) 2001, 2002 Free Software Foundation
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Authors : Eskil Heyn Olsen <eskil@eskil.dk>
 * 	Bastien Nocera <hadess@hadess.net> for the Gnome2 port
 */

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <dirent.h>

#include <gnome.h>
#include <panel-applet.h>
#include <panel-applet-gconf.h>
#include <glade/glade.h>

#include <egg-screen-help.h>
#include <iwlib.h>
#include "ephy-cell-renderer-progress.h"

#define CFG_DEVICE "eth0"
#define CFG_UPDATE_INTERVAL 2

typedef enum {
	PIX_BROKEN,
	PIX_NO_LINK,
	PIX_SIGNAL_1,
	PIX_SIGNAL_2,
	PIX_SIGNAL_3,
	PIX_SIGNAL_4,
	PIX_CONNECT_0,
	PIX_CONNECT_1,
	PIX_CONNECT_2,
	PIX_CONNECT_3,
	PIX_NUMBER,
} PixmapState;

static char * pixmap_names[] = {
	"broken-0.png",
	"no-link-0.png",
	"signal-1-40.png",
	"signal-41-60.png",
	"signal-61-80.png",
	"signal-81-100.png",
	"connect-0.png",
	"connect-1.png",
	"connect-2.png",
	"connect-3.png",
};

typedef struct {
	PanelApplet base;
	gchar *device;
	gboolean show_percent;

	GList *devices;

	/* contains pointers into the images GList.
	 * 0-100 are for link */
	GdkPixbuf*pixmaps[PIX_NUMBER];
	/* pointer to the current used file name */
	GdkPixbuf *current_pixbuf;

	GtkWidget *pct_label;
	GtkWidget *pixmap;
	GtkWidget *button;
	GtkWidget *box;
	GtkWidget *about_dialog;
	guint timeout_handler_id;
	GtkTooltips *tips;
	GtkWidget *prefs;
	GtkWidget *ap_popup;

	GtkListStore *ap_store;
	GtkWidget *ap_view;
	
	int skfd;
	wireless_config cfg;
	GList *scan_list;

	int helper_pid;
} WirelessApplet;

static GladeXML *xml = NULL;
static gchar* glade_file=NULL;

static void show_error_dialog (gchar*,...);
static void show_warning_dialog (gchar*,...);
static int wireless_applet_timeout_handler (WirelessApplet *applet);
static void wireless_applet_properties_dialog (BonoboUIComponent *uic,
		WirelessApplet *applet);
static void wireless_applet_help_cb (BonoboUIComponent *uic,
		WirelessApplet *applet);
static void wireless_applet_about_cb (BonoboUIComponent *uic,
		WirelessApplet *applet);
static void prefs_response_cb (GtkDialog *dialog, gint response, gpointer data);

static const BonoboUIVerb wireless_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("WirelessProperties",
			wireless_applet_properties_dialog),
	BONOBO_UI_UNSAFE_VERB ("WirelessHelp",
			wireless_applet_help_cb),
	BONOBO_UI_UNSAFE_VERB ("WirelessAbout",
			wireless_applet_about_cb),
	BONOBO_UI_VERB_END
};

static GType
wireless_applet_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (PanelAppletClass),
			NULL, NULL, NULL, NULL, NULL,
			sizeof (WirelessApplet),
			0, NULL, NULL
		};

		type = g_type_register_static (
				PANEL_TYPE_APPLET, "WirelessApplet", &info, 0);
	}

	return type;
}

static GType
wireless_ap_get_type (void)
{
	static GType boxed_type = 0;

	if (!boxed_type) {
		boxed_type = g_boxed_type_register_static ("wireless_ap",
							   (GBoxedCopyFunc)iw_wireless_ap_copy,
							   (GBoxedFreeFunc)iw_wireless_ap_free);
	}

	return boxed_type;
}


static int
wireless_applet_get_percent (double qual, int level)
{
	int percent;

	percent = (int)rint ((log (qual) / log (94)) * 100.0);
	percent = CLAMP (percent, 0, 100);

	return percent;
}


/* FIXME: The icon used by this applet when there's no signal is impossible
 * to localise as it says N/A in the icon itself. Need to swap the icon
 * with something more l10n friendly. Also, localising the label makes it
 * necessary to ditch g_strcasecmp() in favor of something else.
 */

static void 
wireless_applet_draw (WirelessApplet *applet, int percent)
{
	const char *label_text;
	char *tmp;
	PixmapState state;

	/* Update the percentage */
	if (percent > 0) {
		tmp = g_strdup_printf ("%2.0d%%", percent);
	} else {
		tmp = g_strdup_printf (_("N/A"));
	}

	label_text = gtk_label_get_text (GTK_LABEL (applet->pct_label));
	if (g_strcasecmp (tmp, label_text) != 0)
	{
		gtk_tooltips_set_tip (applet->tips,
				GTK_WIDGET (applet),
				tmp, NULL);

		if (applet->helper_pid <= 0)
			gtk_label_set_text (GTK_LABEL (applet->pct_label),
					    tmp);
		else
			gtk_label_set_text (GTK_LABEL (applet->pct_label),
					    "");
	}
	g_free (tmp);

	/* Update the image */
	percent = CLAMP (percent, -1, 100);

	if (percent < 0)
		state = PIX_BROKEN;
	else if (percent == 0)
		state = PIX_NO_LINK;
	else if (percent <= 40)
		state = PIX_SIGNAL_1;
	else if (percent <= 60)
		state = PIX_SIGNAL_2;
	else if (percent <= 80)
		state = PIX_SIGNAL_3;
	else
		state = PIX_SIGNAL_4;

	if (applet->pixmaps[state] != applet->current_pixbuf)
	{
		applet->current_pixbuf = (GdkPixbuf *)applet->pixmaps[state];
		gtk_image_set_from_pixbuf (GTK_IMAGE (applet->pixmap),
				applet->current_pixbuf);
	}
}

static void
wireless_applet_update_state (WirelessApplet *applet,
			     char *device,
			     double link,
			     long int level,
			     long int noise)
{
	int percent;

	/* Calculate the percentage based on the link quality */
	if (level < 0) {
		percent = -1;
	} else {
		if (link < 1) {
			percent = 0;
		} else {
			percent = wireless_applet_get_percent (link, level);
		}
	}

	wireless_applet_draw (applet, percent);
}

static void
wireless_applet_start_timeout (WirelessApplet *applet)
{
	applet->timeout_handler_id = g_timeout_add
		(CFG_UPDATE_INTERVAL * 1000,
		 (GtkFunction)wireless_applet_timeout_handler,
		 applet);
}

static void
wireless_applet_cancel_timeout (WirelessApplet *applet)
{
	g_source_remove (applet->timeout_handler_id);
	applet->timeout_handler_id = -1;
	wireless_applet_update_state (applet, applet->device, -1, -1, -1);
}



static void
wireless_applet_load_theme (WirelessApplet *applet) {
	char *pixmapdir;
	char *pixmapname;
	int i;

	pixmapdir = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP,
			"wireless-applet/", FALSE, NULL);


	for (i = 0; i < PIX_NUMBER; i++)
	{
		pixmapname = g_build_filename (G_DIR_SEPARATOR_S,
				pixmapdir, pixmap_names[i], NULL);
		applet->pixmaps[i] = gdk_pixbuf_new_from_file (pixmapname, NULL);
		g_free (pixmapname);
	}

	g_free (pixmapdir);
}

static void
wireless_applet_set_device (WirelessApplet *applet, gchar *device) {
	g_return_if_fail (device != NULL);

	g_free (applet->device);
	applet->device = g_strdup (device);
	iw_get_basic_config (applet->skfd, applet->device, &applet->cfg);
}

static void
wireless_applet_set_show_percent (WirelessApplet *applet, gboolean show) {
	applet->show_percent = show;
	if (applet->show_percent) {
		/* reeducate label */
		gtk_widget_show_all (applet->pct_label);
	} else {
		gtk_widget_hide_all (applet->pct_label);
	}
}

static int
wireless_applet_device_handler (int skfd,
				char *ifname,
				char *args[],
				int count)
{
	WirelessApplet *ap = (WirelessApplet *)args[0];

	ap->devices = g_list_append (ap->devices, g_strdup (ifname));
}

/* check stats, modify the state attribute */
static void
wireless_applet_read_device_state (WirelessApplet *applet)
{
	iwrange range;
	iwstats stats;
	gboolean has_range;
	
	/* ewwwww */
	char *enum_args[] = { (char *)applet };

	/* clear the device list */
	g_list_foreach (applet->devices, (GFunc)g_free, NULL);
	g_list_free (applet->devices);
	applet->devices = NULL;

	/* get the config */
	iw_get_basic_config (applet->skfd, applet->device, &applet->cfg);

	iw_enum_devices (applet->skfd,
			 wireless_applet_device_handler,
			 enum_args, 1);

	has_range = iw_get_range_info (applet->skfd, applet->device, &range) < 0 ? FALSE : TRUE;

	if (!iw_get_stats (applet->skfd, applet->device, &stats,
			  &range, has_range)) {
		wireless_applet_update_state (applet,
					      applet->device,
					      stats.qual.qual,
					      stats.qual.level,
					      stats.qual.noise);
	} else {
		wireless_applet_update_state (applet,
					      applet->device,
					      -1, -1, -1);
	}
}

static gboolean
wireless_applet_find_mac (WirelessApplet *applet, char *mac, GtkTreeIter *ret)
{
	GtkTreeIter iter;

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (applet->ap_store),
					    &iter))
		return FALSE;
	
	do {
		wireless_ap *ap;

		gtk_tree_model_get (GTK_TREE_MODEL (applet->ap_store),
				    &iter, 0, &ap, -1);
		if (strcmp (ap->address, mac) == 0) {
			*ret = iter;
			return TRUE;
		}
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (applet->ap_store),
					   &iter));

	return FALSE;
}

static void
wireless_applet_remove_old_ap (WirelessApplet *applet,
			       time_t stamp)
{
	GtkTreeIter iter;

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (applet->ap_store),
					    &iter))
		return;
	
	do {
		wireless_ap *ap;

		gtk_tree_model_get (GTK_TREE_MODEL (applet->ap_store),
				    &iter, 0, &ap, -1);

		if (ap->stamp != stamp) {
			gtk_list_store_remove
				(GTK_LIST_STORE (applet->ap_store), &iter);
		}
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (applet->ap_store),
					   &iter));
}

static wireless_ap *
find_ap (GList *list, const char *essid)
{
	GList *l;
	for (l = list; l; l = l->next) {
		wireless_ap *ap = l->data;

		if (strcmp (ap->essid, essid) == 0)
			return ap;
	}
	
	return NULL;
}

static void
scan_handler (wireless_ap *ap, void *user_data)
{
	WirelessApplet *applet = user_data;

	applet->scan_list = g_list_append (applet->scan_list, iw_wireless_ap_copy (ap));
}

static void
wireless_applet_scan (WirelessApplet *applet)
{
	GList *filtered_list = NULL;
	GList *l = NULL;
	GList *ap_list = NULL;
	time_t stamp = 0;
	
	iw_scan (applet->skfd, applet->device, scan_handler, applet);

	ap_list = applet->scan_list;
	applet->scan_list = NULL;
	
	for (l = ap_list; l; l = l->next) {
		wireless_ap *ap = l->data;
		wireless_ap *found_ap = find_ap (filtered_list, ap->essid);

		if (found_ap != NULL) {
			if (found_ap->quality < ap->quality) {
				/* we want to display the max quality of
				 * a given essid	
				 */
				found_ap->quality = ap->quality;
				iw_wireless_ap_free (ap);
			}
		} else {
			filtered_list = g_list_append (filtered_list, ap);
		}
	}

	g_list_free (ap_list);

	for (l = filtered_list; l; l = l->next) {
		wireless_ap *ap = l->data;
		GtkTreeIter iter;

		if (!wireless_applet_find_mac (applet, ap->address, &iter)) {
			
			gtk_list_store_append (applet->ap_store,
					       &iter);
		}
		
		gtk_list_store_set (applet->ap_store,
				    &iter, 0, ap, -1);
		stamp = ap->stamp;
	}

	if (stamp != 0) {
		wireless_applet_remove_old_ap (applet, stamp);
	}

	//g_list_foreach (filtered_list, (GFunc)iw_wireless_ap_free, NULL);
	g_list_free (filtered_list);
}

static int
wireless_applet_timeout_handler (WirelessApplet *applet)
{
	if (!GTK_WIDGET_IS_SENSITIVE (applet->ap_view))
		gtk_widget_set_sensitive (GTK_WIDGET (applet->ap_view), TRUE);
	
	wireless_applet_read_device_state (applet);
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (applet->button)))
			wireless_applet_scan (applet);

	return TRUE;
}


static void 
show_error_dialog (gchar *mesg,...) 
{
	GtkWidget *dialog;
	char *tmp;
	va_list ap;

	va_start (ap,mesg);
	tmp = g_strdup_vprintf (mesg,ap);
	dialog = gtk_message_dialog_new (NULL,
			0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
			mesg, NULL);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	g_free (tmp);
	va_end (ap);
}

static void 
show_warning_dialog (gchar *mesg,...) 
{
	GtkWidget *dialog;
	char *tmp;
	va_list ap;

	va_start (ap,mesg);
	tmp = g_strdup_vprintf (mesg,ap);
	dialog = gtk_message_dialog_new (NULL,
			0, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
			mesg, NULL);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	g_free (tmp);
	va_end (ap);
}

static void
check_wireless (WirelessApplet *applet)
{
	struct stat sbuf;
	
	if (stat ("/proc/net/wireless", &sbuf) != 0) {
		gtk_tooltips_set_tip (applet->tips,
				GTK_WIDGET (applet),
				_("No Wireless Devices"),
				NULL);
		show_error_dialog (_("There doesn't seem to be any wireless devices configured on your system.\nPlease verify your configuration if you think this is incorrect."));
	}
}

static void
wireless_applet_load_properties (WirelessApplet *applet)
{
	wireless_applet_set_device (applet,
			  panel_applet_gconf_get_string (PANEL_APPLET (applet),
							 "device", NULL));

	/* Oooh, new applet, let's put in the defaults */
	if (applet->device == NULL)
	{
		applet->device = g_strdup (CFG_DEVICE);
		applet->show_percent = TRUE;
		return;
	}

	applet->show_percent = panel_applet_gconf_get_bool
		(PANEL_APPLET (applet), "percent", NULL);
}

static void
wireless_applet_save_properties (WirelessApplet *applet)
{
	if (applet->device)
		panel_applet_gconf_set_string (PANEL_APPLET (applet),
			"device", applet->device, NULL);
	panel_applet_gconf_set_bool (PANEL_APPLET (applet),
			"percent", applet->show_percent, NULL);
}

static void
wireless_applet_option_change (GtkWidget *widget, gpointer user_data)
{
	GtkWidget *entry;
	GtkWidget *menu = NULL;
	char *str;
	WirelessApplet *applet = (WirelessApplet *)user_data;

	/* Get all the properties and update the applet */
	entry = g_object_get_data (G_OBJECT (applet->prefs),
			"show-percent-button");
	wireless_applet_set_show_percent (applet,
			gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON (entry)));

	entry = g_object_get_data (G_OBJECT (applet->prefs), "device-menu");

	menu = gtk_menu_get_active
		(GTK_MENU (gtk_option_menu_get_menu (GTK_OPTION_MENU (entry))));
	if (menu) {
		str = g_object_get_data (G_OBJECT (menu), "device-selected");
		wireless_applet_set_device (applet, str);
	}

	/* Save the properties */
	wireless_applet_save_properties (applet);
}

static void
wireless_applet_properties_dialog (BonoboUIComponent *uic,
				  WirelessApplet *applet)
{
	static GtkWidget *global_property_box = NULL,
		*glade_property_box = NULL;
	GtkWidget *pct, *dialog, *device;

	g_return_if_fail (PANEL_IS_APPLET (PANEL_APPLET (applet)));

	if (applet->prefs != NULL)
	{
		gtk_widget_show (applet->prefs);
		gtk_window_present (GTK_WINDOW (applet->prefs));
		return;
	}

	if (global_property_box == NULL) {
		xml = glade_xml_new (glade_file, NULL, NULL);
		glade_property_box = glade_xml_get_widget (xml,"dialog1");
	}

	applet->prefs = glade_property_box;
	gtk_window_set_resizable (GTK_WINDOW (applet->prefs), FALSE);

	pct = glade_xml_get_widget (xml, "pct_check_button");
	dialog = glade_xml_get_widget (xml, "dialog_check_button");
	device = glade_xml_get_widget (xml, "device_menu");

	/* Set the show-percent thingy */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pct),
			applet->show_percent);
	g_signal_connect (GTK_OBJECT (pct),
			"toggled",
			GTK_SIGNAL_FUNC (wireless_applet_option_change),
			applet);
	gtk_object_set_data (GTK_OBJECT (applet->prefs),
			"show-percent-button", pct);

        /* Set the device menu */
	gtk_option_menu_remove_menu (GTK_OPTION_MENU (device));
	{
		GtkWidget *menu;
		GtkWidget *item;
		GList *d;
		int idx = 0, choice = 0;

		menu = gtk_menu_new ();

		for (d = applet->devices; d != NULL; d = g_list_next (d)) {
			item = gtk_menu_item_new_with_label ((char*)d->data);
			gtk_menu_shell_append  (GTK_MENU_SHELL (menu),item);
			gtk_object_set_data_full (GTK_OBJECT (item), 
					"device-selected",
					g_strdup (d->data),
					g_free);
			g_signal_connect (GTK_OBJECT (item),
					"activate",
					GTK_SIGNAL_FUNC (wireless_applet_option_change),
					applet);

			if ((applet->device != NULL)
					&& (d->data != NULL)
					&& strcmp (applet->device, d->data)==0)
			{
				choice = idx;
			}
			idx++;
		}
		if (applet->devices == NULL) {
			char *markup;
			GtkWidget *label;
			
			label = gtk_label_new (NULL);
			markup = g_strdup_printf ("<i>%s</i>",
					_("No Wireless Devices"));
			gtk_label_set_markup (GTK_LABEL (label), markup);
			g_free (markup);

			item = gtk_menu_item_new ();
			gtk_container_add (GTK_CONTAINER (item), label);
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		}
		gtk_option_menu_set_menu (GTK_OPTION_MENU (device), menu);
		gtk_option_menu_set_history (GTK_OPTION_MENU (device), choice);
	}
	gtk_object_set_data (GTK_OBJECT (applet->prefs), "device-menu", device);

	g_signal_connect (GTK_OBJECT (applet->prefs),
			"response", 
			G_CALLBACK (prefs_response_cb),
			NULL);
	g_signal_connect (GTK_OBJECT (applet->prefs),
			"destroy",
			GTK_SIGNAL_FUNC (gtk_widget_destroy),
			NULL);

	g_object_add_weak_pointer (G_OBJECT (applet->prefs),
			(void**)&(applet->prefs));
	gtk_window_set_screen (GTK_WINDOW (applet->prefs),
			       gtk_widget_get_screen (GTK_WIDGET (applet)));
	gtk_widget_show_all (applet->prefs);
}

static void
wireless_applet_help_cb (BonoboUIComponent *uic, WirelessApplet *applet)
{
	GError *error = NULL;
	egg_help_display_on_screen ("wireless", NULL,
				   gtk_widget_get_screen (GTK_WIDGET (
						applet)), &error);
	if (error) {
		GtkWidget *dialog =
		gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
					GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
					_("There was an error displaying help: %s"),
					error->message);
		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_window_set_screen (GTK_WINDOW (dialog),
				       gtk_widget_get_screen (GTK_WIDGET (
							      applet)));
		gtk_widget_show (dialog);
		g_error_free (error);
	}
}

static void
wireless_applet_about_cb (BonoboUIComponent *uic, WirelessApplet *applet)
{
	GdkPixbuf *pixbuf;
	char *file;

	const gchar *authors[] = {
		"Eskil Heyn Olsen <eskil@eskil.org>",
		"Bastien Nocera <hadess@hadess.net> (Gnome2 port)",
		NULL
	};

	const gchar *documenters[] = { 
                 "Sun GNOME Documentation Team <gdocteam@sun.com>",
 	        NULL 
         };	

	const gchar *translator_credits = _("translator_credits");

	if (applet->about_dialog != NULL) {
		gtk_window_set_screen (GTK_WINDOW (applet->about_dialog),
				       gtk_widget_get_screen (GTK_WIDGET (&applet->base)));
		
		gtk_window_present (GTK_WINDOW (applet->about_dialog));
		return;
	}

	file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP,
			"wireless-applet/wireless-applet.png", FALSE, NULL);
	pixbuf = gdk_pixbuf_new_from_file (file, NULL);
	g_free (file);

	applet->about_dialog = gnome_about_new (
			_("Wireless Link Monitor"),
			VERSION,
			_("(C) 2001, 2002 Free Software Foundation "),
			_("This utility shows the status of a wireless link."),
			authors,
			documenters,
			strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
			pixbuf);

	g_object_unref (pixbuf);

	g_signal_connect (applet->about_dialog, "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &applet->about_dialog);
 
	gtk_widget_show (applet->about_dialog);

	return;
}

static void
prefs_response_cb (GtkDialog *dialog, gint response, gpointer data)
{
	GError *error = NULL;
	if (response == GTK_RESPONSE_HELP) {
		egg_help_display_on_screen ("wireless", "wireless-prefs",
					    gtk_widget_get_screen (GTK_WIDGET (
					    dialog)), &error);
		if (error) {
			GtkWidget *dlg =
			gtk_message_dialog_new (GTK_WINDOW (dialog),
						GTK_DIALOG_DESTROY_WITH_PARENT
						|| GTK_DIALOG_MODAL,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						_("There was an error displaying help: %s"),
						error->message);
			g_signal_connect (G_OBJECT (dlg), "response",
					  G_CALLBACK (gtk_widget_destroy),
					  NULL);
			gtk_window_set_resizable (GTK_WINDOW (dlg), FALSE);
			gtk_window_set_screen (GTK_WINDOW (dlg),
					       gtk_widget_get_screen (
							GTK_WIDGET (dialog)));
			gtk_widget_show (dlg);
			g_error_free (error);
		}
	}
	else
		gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
wireless_applet_destroy (WirelessApplet *applet, gpointer horse)
{
	int i;

	g_free (applet->device);

	g_list_foreach (applet->devices, (GFunc)g_free, NULL);
	g_list_free (applet->devices);

	if (applet->timeout_handler_id > 0) {
		gtk_timeout_remove (applet->timeout_handler_id);
		applet->timeout_handler_id = 0;
	}

	for (i = 0; i < PIX_NUMBER; i++)
		g_object_unref (applet->pixmaps[i]);

	if (applet->about_dialog) {
		gtk_widget_destroy (applet->about_dialog);
		applet->about_dialog = NULL;
	}

	if (applet->prefs) {
		gtk_widget_destroy (applet->prefs);
		applet->prefs = NULL;
	}

	if (applet->tips)
		g_object_unref (applet->tips);
}

static void
present_ap_popup (WirelessApplet *applet)
{
	GtkRequisition  req;
	GdkScreen      *screen;
	GdkRectangle    monitor;
	int             button_w, button_h;
	int             x, y;
	int             w, h;
	int             i, n;
	gboolean        found_monitor = FALSE;
	GtkWidget      *button = applet->button;
	GtkWidget      *window = applet->ap_popup;
	
	/* Get root origin of the toggle button, and position above that. */
	gdk_window_get_origin (button->window, &x, &y);

	gtk_window_get_size (GTK_WINDOW (window), &w, &h);
	gtk_widget_size_request (window, &req);
	w = req.width;
	h = req.height;

	button_w = button->allocation.width;
	button_h = button->allocation.height;

	screen = gtk_window_get_screen (GTK_WINDOW (window));

	n = gdk_screen_get_n_monitors (screen);
	for (i = 0; i < n; i++) {
		gdk_screen_get_monitor_geometry (screen, i, &monitor);
		if (x >= monitor.x && x <= monitor.x + monitor.width &&
		    y >= monitor.y && y <= monitor.y + monitor.height) {
			found_monitor = TRUE;
			break;
		}
	}

	if ( ! found_monitor) {
		/* eek, we should be on one of those xinerama
		   monitors */
		monitor.x = 0;
		monitor.y = 0;
		monitor.width = gdk_screen_get_width (screen);
		monitor.height = gdk_screen_get_height (screen);
	}
		
	/* Based on panel orientation, position the popup.
	 * Ignore window gravity since the window is undecorated.
	 * The orientations are all named backward from what
	 * I expected.
	 */
	switch (panel_applet_get_orient (PANEL_APPLET (applet))) {
	case PANEL_APPLET_ORIENT_RIGHT:
		x += button_w;
		if ((y + h) > monitor.y + monitor.height)
			y -= (y + h) - (monitor.y + monitor.height);
		break;
	case PANEL_APPLET_ORIENT_LEFT:
		x -= w;
		if ((y + h) > monitor.y + monitor.height)
			y -= (y + h) - (monitor.y + monitor.height);
		break;
	case PANEL_APPLET_ORIENT_DOWN:
		y += button_h;
		if ((x + w) > monitor.x + monitor.width)
			x -= (x + w) - (monitor.x + monitor.width);
		break;
	case PANEL_APPLET_ORIENT_UP:
		y -= h;
		if ((x + w) > monitor.x + monitor.width)
			x -= (x + w) - (monitor.x + monitor.width);
		break;
	}

	gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (applet->ap_view)));
		
	gtk_window_move (GTK_WINDOW (window), x, y);
	gtk_window_present (GTK_WINDOW (window));
	//gtk_widget_show (window);
}

static void
applet_button_toggled (GtkWidget *button, WirelessApplet *applet)
{
	wireless_ap *ap_list, *ap;
	

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
		wireless_applet_scan (applet);
		present_ap_popup (applet);
	} else {
		gtk_widget_hide (applet->ap_popup);
	}
}

static void
ap_essid_cell_data_cb (GtkTreeViewColumn *column,
		       GtkCellRenderer *renderer,
		       GtkTreeModel *model,
		       GtkTreeIter *iter,
		       gpointer data)
{
	WirelessApplet *applet = data;
	wireless_ap *ap;
	char *markup;

	g_return_if_fail (renderer != NULL);
	g_return_if_fail (iter != NULL);
	g_return_if_fail (model != NULL);
	
	gtk_tree_model_get (model, iter, 0, &ap, -1);

	if (applet->cfg.has_freq &&
	    strcmp (ap->essid, applet->cfg.essid) == 0)
		markup = g_strdup_printf ("<b>%s</b>", ap->essid);
	else
		markup = g_strdup (ap->essid);
	
	g_object_set (G_OBJECT (renderer), "markup", markup, NULL);
	g_free (markup);
}



static void
ap_quality_cell_data_cb (GtkTreeViewColumn *column,
			 GtkCellRenderer *renderer,
			 GtkTreeModel *model,
			 GtkTreeIter *iter,
			 gpointer data)
{
	WirelessApplet *applet = data;
	wireless_ap *ap;
	int percent;
	
	gtk_tree_model_get (model, iter, 0, &ap, -1);
	
	percent = wireless_applet_get_percent (ap->quality, 92);
	
	g_object_set (G_OBJECT (renderer), "value", percent, NULL);
}

static void
ap_encryption_cell_data_cb (GtkTreeViewColumn *column,
			    GtkCellRenderer *renderer,
			    GtkTreeModel *model,
			    GtkTreeIter *iter,
			    gpointer data)
{
	WirelessApplet *applet = data;
	wireless_ap *ap;
	
	gtk_tree_model_get (model, iter, 0, &ap, -1);

	/*
	if (ap->key != NULL)
		g_object_set (G_OBJECT (renderer), "stock_id",
			      GTK_STOCK_CLOSE, NULL);
	*/
}

static gboolean
check_pid_status (WirelessApplet *applet)
{
	int status;
	int ret;
	static PixmapState state = PIX_CONNECT_0;

	ret = waitpid (applet->helper_pid, &status, WNOHANG);
	if (ret > 0) {
		applet->helper_pid = -1;
		wireless_applet_start_timeout (applet);
		state = PIX_CONNECT_0;
		return FALSE;
	} else if (ret < 0) {
		g_message ("waitpid: %s", strerror(errno));
		state = PIX_CONNECT_0;
		wireless_applet_start_timeout (applet);
		return FALSE;
	} else {
		applet->current_pixbuf = (GdkPixbuf *)applet->pixmaps[state];
		gtk_image_set_from_pixbuf (GTK_IMAGE (applet->pixmap),
				applet->current_pixbuf);

		if (state == PIX_CONNECT_3)
			state = PIX_CONNECT_0;
		else
			state++;
	}

	return TRUE;
}

static void
wireless_applet_set_essid (WirelessApplet *applet,
			   char *essid)
{
	char *args[4];
	int child;
	
	args[0] = PREFIX "/bin/wireless-applet-helper";
	args[1] = applet->device;
	args[2] = essid;
	args[3] = NULL;
	
	g_spawn_async("/tmp", args, NULL,
		      G_SPAWN_SEARCH_PATH|G_SPAWN_DO_NOT_REAP_CHILD,
		      NULL, NULL, &applet->helper_pid,
		      NULL);
	g_timeout_add (250, (GSourceFunc)check_pid_status, applet);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (applet->button),
				      FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (applet->ap_view), FALSE);
	wireless_applet_cancel_timeout (applet);
}


static void
wireless_applet_row_activate_cb (GtkTreeView *tree_view,
				 GtkTreePath *path,
				 GtkTreeViewColumn *column,
				 WirelessApplet *applet)
{
	GtkTreeIter iter;
	wireless_ap *ap;

	gtk_tree_model_get_iter (GTK_TREE_MODEL (applet->ap_store),
				 &iter, path);

	gtk_tree_model_get (GTK_TREE_MODEL (applet->ap_store),
			    &iter, 0, &ap, -1);


	/* set the essid */
	wireless_applet_set_essid (applet, ap->essid);
	
}

static gint
ap_compare_func (GtkTreeModel *model,
		 GtkTreeIter *a,
		 GtkTreeIter *b,
		 gpointer user_data)
{
	wireless_ap *ap_a;
	wireless_ap *ap_b;

	gtk_tree_model_get (model, a, 0, &ap_a, -1);
	gtk_tree_model_get (model, b, 0, &ap_b, -1);

	if (ap_a->quality > ap_b->quality)
		return 1;
	if (ap_a->quality == ap_b->quality)
		return 0;
	else
		return -1;
}


static void
setup_widgets (WirelessApplet *applet)
{
	GtkRequisition req;
	gint total_size = 0;
	gboolean horizontal = FALSE;
	gint panel_size;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *viewport;
	
	panel_size = panel_applet_get_size (PANEL_APPLET (applet));
	
	switch (panel_applet_get_orient(PANEL_APPLET (applet))) {
	case PANEL_APPLET_ORIENT_LEFT:
	case PANEL_APPLET_ORIENT_RIGHT:
		horizontal = FALSE;
		break;
	case PANEL_APPLET_ORIENT_UP:
	case PANEL_APPLET_ORIENT_DOWN:
		horizontal = TRUE;
		break;
	}

	/* construct pixmap widget */
	applet->pixmap = gtk_image_new ();
	gtk_image_set_from_pixbuf (GTK_IMAGE (applet->pixmap), applet->pixmaps[PIX_BROKEN]);
	gtk_widget_size_request (applet->pixmap, &req);
	gtk_widget_show (applet->pixmap);

	if (horizontal)
		total_size += req.height;
	else
		total_size += req.width;

	/* construct pct widget */
	applet->pct_label = gtk_label_new ("N/A");

	if (applet->show_percent == TRUE) {
		gtk_widget_show (applet->pct_label);
		gtk_widget_size_request (applet->pct_label, &req);
		if (horizontal)
			total_size += req.height;
		else
			total_size += req.width;
	}

	/* construct ap popup window */
	applet->ap_popup = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_type_hint (GTK_WINDOW (applet->ap_popup),
				  GDK_WINDOW_TYPE_HINT_DOCK);
	gtk_window_set_resizable (GTK_WINDOW (applet->ap_popup),
				  FALSE);
	gtk_window_stick (GTK_WINDOW (applet->ap_popup));
	gtk_window_set_default_size (GTK_WINDOW (applet->ap_popup),
				     0, 0);
	gtk_window_set_decorated (GTK_WINDOW (applet->ap_popup), FALSE);

	applet->ap_store = gtk_list_store_new (1, wireless_ap_get_type());
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (applet->ap_store),
					 0,
					 ap_compare_func,
					 applet,
					 NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (applet->ap_store),
					      0, GTK_SORT_DESCENDING);
	
	applet->ap_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (applet->ap_store));
	g_signal_connect (applet->ap_view, "row_activated",
			  G_CALLBACK (wireless_applet_row_activate_cb),
			  applet);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (applet->ap_view),
				      FALSE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (applet->ap_view),
					   FALSE);
	
	/* the quality column */
	renderer = GTK_CELL_RENDERER (ephy_cell_renderer_progress_new ());
	column = gtk_tree_view_column_new_with_attributes ("Quality",
							   renderer,
							   NULL);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 ap_quality_cell_data_cb,
						 applet, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (applet->ap_view),
				     column);

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport),
				      GTK_SHADOW_ETCHED_IN);
	
	gtk_container_add (GTK_CONTAINER (viewport), applet->ap_view);
	gtk_container_add (GTK_CONTAINER(applet->ap_popup), viewport);

	gtk_widget_show_all (viewport);
	
	/* the essid column */
	renderer = GTK_CELL_RENDERER (gtk_cell_renderer_text_new ());
	column = gtk_tree_view_column_new_with_attributes ("ESSID",
							   renderer,
							   NULL);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 ap_essid_cell_data_cb,
						 applet, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (applet->ap_view),
				     column);

	/* encryption column */
	renderer = GTK_CELL_RENDERER (gtk_cell_renderer_pixbuf_new ());
	column = gtk_tree_view_column_new_with_attributes ("Encryption",
							   renderer,
							   NULL);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 ap_encryption_cell_data_cb,
						 applet, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (applet->ap_view),
				     column);
	
	/* pack */
	if (applet->button)
		gtk_widget_destroy (applet->button);

	if (horizontal && (total_size <= panel_size))
		applet->box = gtk_vbox_new (FALSE, 0);
	else if (horizontal && (total_size > panel_size))
		applet->box = gtk_hbox_new (FALSE, 0);
	else if (!horizontal && (total_size <= panel_size))
		applet->box = gtk_hbox_new (FALSE, 0);
	else 
		applet->box = gtk_vbox_new (FALSE, 0);

	applet->button = gtk_toggle_button_new ();
	g_signal_connect(applet->button, "toggled",
			 G_CALLBACK(applet_button_toggled), applet);
	gtk_button_set_relief (GTK_BUTTON (applet->button), GTK_RELIEF_NONE);
	gtk_container_add (GTK_CONTAINER(applet->button), applet->box);
	
	gtk_box_pack_start (GTK_BOX (applet->box), applet->pixmap, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (applet->box), applet->pct_label, TRUE, TRUE, 0);
	/* note, I don't use show_all, because this way the percent label is
	 * only realised if it's enabled */
	gtk_widget_show (applet->button);
	gtk_widget_show (applet->box);
	gtk_container_add (GTK_CONTAINER (applet), applet->button);

	applet->current_pixbuf = NULL;
	applet->about_dialog = NULL;
}

static void change_size_cb(PanelApplet *pa, gint s, WirelessApplet *applet)
{
	setup_widgets (applet);
	wireless_applet_timeout_handler (applet);
}

static void change_orient_cb(PanelApplet *pa, gint s, WirelessApplet *applet)
{
	setup_widgets (applet);
	wireless_applet_timeout_handler (applet);
}

static gboolean
do_not_eat_button_press (GtkWidget      *widget,
                         GdkEventButton *event)
{
        if (event->button != 1)
                g_signal_stop_emission_by_name (widget, "button_press_event");
                                                                                
        return FALSE;
}

static GtkWidget *
wireless_applet_new (WirelessApplet *applet)
{
	panel_applet_set_flags (PANEL_APPLET (applet), PANEL_APPLET_EXPAND_MINOR);

	applet->skfd = iw_sockets_open ();
	
	/* this ensures that properties are loaded */
	wireless_applet_load_properties (applet);
	wireless_applet_load_theme (applet);

	setup_widgets (applet);

	applet->tips = gtk_tooltips_new ();
	g_object_ref (applet->tips);
	gtk_object_sink (GTK_OBJECT (applet->tips));
	applet->prefs = NULL;

	g_signal_connect (applet,"destroy",
			  G_CALLBACK (wireless_applet_destroy),NULL);

	g_signal_connect (applet->button, "button_press_event",
			  G_CALLBACK (do_not_eat_button_press), NULL);
	
	/* Setup the menus */
	panel_applet_setup_menu_from_file (PANEL_APPLET (applet),
			NULL,
			"GNOME_WirelessApplet.xml",
			NULL,
			wireless_menu_verbs,
			applet);

	if (panel_applet_get_locked_down (PANEL_APPLET (applet))) {
		BonoboUIComponent *popup_component;

		popup_component = panel_applet_get_popup_component (PANEL_APPLET (applet));

		bonobo_ui_component_set_prop (popup_component,
					      "/commands/WirelessProperties",
					      "hidden", "1",
					      NULL);
	}

	check_wireless (applet);
	wireless_applet_timeout_handler (applet);

	
	wireless_applet_start_timeout (applet);
		 
	g_signal_connect (G_OBJECT (applet), "change_size",
			  G_CALLBACK (change_size_cb), applet);
	g_signal_connect (G_OBJECT (applet), "change_orient",
			  G_CALLBACK (change_orient_cb), applet);
  
	return GTK_WIDGET (applet);
}

static gboolean
wireless_applet_fill (WirelessApplet *applet)
{
	gnome_window_icon_set_default_from_file
		(ICONDIR"/wireless-applet/wireless-applet.png");

	glade_gnome_init ();
	glade_file = gnome_program_locate_file
		(NULL, GNOME_FILE_DOMAIN_DATADIR,
		 "wireless-applet/wireless-applet.glade", FALSE, NULL);

	wireless_applet_new (applet);
	gtk_widget_show (GTK_WIDGET (applet));

	return TRUE;
}

static gboolean
wireless_applet_factory (WirelessApplet *applet,
		const gchar          *iid,
		gpointer              data)
{
	gboolean retval = FALSE;

	if (!strcmp (iid, "OAFIID:GNOME_Panel_WirelessApplet"))
		retval = wireless_applet_fill (applet);

	return retval;
}

PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_Panel_WirelessApplet_Factory",
		wireless_applet_get_type (),
		"wireless",
		"0",
		(PanelAppletFactoryCallback) wireless_applet_factory,
		NULL)

