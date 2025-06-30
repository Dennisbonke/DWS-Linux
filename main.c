#include <canberra-gtk.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <gtk/gtk.h>
#include <libayatana-appindicator/app-indicator.h>
#include <libnotify/notify.h>
#include <string.h>
#include <time.h>

#include <stdbool.h>

#define FETCH_URL "https://defconwarningsystem.com/code.dat"

AppIndicator *indicator = NULL;

gboolean fetch_dws_status(gpointer user_data);

struct MemoryBuffer {
	char *data;
	size_t size;
};

int current_level = -1;

static guint timeout_seconds = 3600 * 1000; // Default value of 3600 seconds == 1 hour interval
static guint timeout_source_id = 0;

ca_context *ctx = NULL;

void play_notification_sound(bool alert) {
	if(!ctx) {
		ca_context_create(&ctx);
	}

	if(!alert) {
		ca_context_play(ctx, 0,
			CA_PROP_EVENT_ID, "bell-window-system",
			CA_PROP_APPLICATION_NAME, "DWS Tray",
			NULL);
	} else {
		ca_context_play(ctx, 0,
			CA_PROP_MEDIA_FILENAME, "/usr/share/dws-tray/sounds/alert.wav",
			CA_PROP_APPLICATION_NAME, "DWS Tray",
			NULL);
	}
}

void load_config() {
	GKeyFile *keyfile = g_key_file_new();
	gchar *config_path = g_build_filename(g_get_user_config_dir(), "dws", "config.ini", NULL);

	if(g_key_file_load_from_file(keyfile, config_path, G_KEY_FILE_NONE, NULL)) {
		timeout_seconds = g_key_file_get_integer(keyfile, "Settings", "interval", NULL);
	}

	g_key_file_free(keyfile);
	g_free(config_path);
}

void save_config() {
	GKeyFile *keyfile = g_key_file_new();
	gchar *config_path = g_build_filename(g_get_user_config_dir(), "dws", "config.ini", NULL);

	g_key_file_set_integer(keyfile, "Settings", "interval", timeout_seconds);

	gchar *data = g_key_file_to_data(keyfile, NULL, NULL);
	g_file_set_contents(config_path, data, -1, NULL);

	g_free(data);
	g_key_file_free(keyfile);
	g_free(config_path);
}

void update_timeout(GSourceFunc func, gpointer data) {
	if(timeout_source_id > 0) {
		g_source_remove(timeout_source_id);
	}
	timeout_source_id = g_timeout_add(timeout_seconds, func, data);
}

void show_notification(const char *title, const char *body, const char *icon, bool alert) {
	NotifyNotification *n;

	if(!notify_is_initted()) {
		notify_init("DWS_Tray");
	}

	// TODO: Sound support?
	n = notify_notification_new(title, body, icon);
	notify_notification_set_timeout(n, 5000);
	notify_notification_show(n, NULL);
	play_notification_sound(alert);

	g_object_unref(G_OBJECT(n));
}

void on_interval_selected(GtkCheckMenuItem *item, gpointer user_data) {
	if(!gtk_check_menu_item_get_active(item)) {
		return;
	}

	timeout_seconds = GPOINTER_TO_UINT(user_data);

	save_config();
	update_timeout(fetch_dws_status, NULL);

	// Uncheck siblings
	GtkWidget *parent = gtk_widget_get_parent(GTK_WIDGET(item));
	GList *children = gtk_container_get_children(GTK_CONTAINER(parent));
	for(GList *l = children; l != NULL; l = l->next) {
		if(l->data != item)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(l->data), FALSE);
	}
	g_list_free(children);
}

GtkWidget *build_menu() {
	GtkWidget *menu = gtk_menu_new();

	// Create "Update frequency" submenu
	GtkWidget *update_item = gtk_menu_item_new_with_label("Update frequency");
	GtkWidget *submenu = gtk_menu_new();

	struct {
		const char *label;
		guint value;
	} intervals[] = {
		{ "Every minute", 60 * 1000 },
		{ "Every 5 minutes", 60 * 5 * 1000 },
		{ "Every 10 minutes", 60 * 10 * 1000 },
		{ "Every 15 minutes", 60 * 15 * 1000 },
		{ "Every 30 minutes", 60 * 30 * 1000 },
		{ "Every hour (the default)", 60 * 60 * 1000 },
	};

	GSList *group = NULL;

	for (int i = 0; i < 6; i++) {
		GtkWidget *item = gtk_radio_menu_item_new_with_label(group, intervals[i].label);
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
		// GtkWidget *check = gtk_check_menu_item_new_with_label(intervals[i].label);
		
		// Check current value
		if (timeout_seconds == intervals[i].value) {
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
		}

		g_signal_connect(item, "toggled", G_CALLBACK(on_interval_selected), GINT_TO_POINTER(intervals[i].value));

		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
	}

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(update_item), submenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), update_item);

	GtkWidget *item_quit = gtk_menu_item_new_with_label("Quit");
	g_signal_connect(item_quit, "activate", G_CALLBACK(gtk_main_quit), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_quit);

	gtk_widget_show_all(menu);
	return menu;
}

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	struct MemoryBuffer *mem = (struct MemoryBuffer *)userp;

	char *ptr = realloc(mem->data, mem->size + realsize + 1);
	if(ptr == NULL) {
		// out of memory
		fprintf(stderr, "Not enough memory\n");
		return 0;
	}

	mem->data = ptr;
	memcpy(&(mem->data[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->data[mem->size] = 0;  // null-terminate

	return realsize;
}

gboolean fetch_dws_status(gpointer user_data) {
	AppIndicator *indicator = user_data;
	CURL *easy_handle = curl_easy_init();
	if(easy_handle) {
		struct MemoryBuffer chunk = {0};
		CURLcode setopt_ret = curl_easy_setopt(easy_handle, CURLOPT_URL, FETCH_URL);
		if(setopt_ret) {
			// Failed to set options, fall back to no connection
			app_indicator_set_icon_full(indicator, "defcon-no-connection", "DWS No Connection");
			app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
			show_notification("DEFCON STATUS UPDATE", "Failed to connect to DWS servers!", "defcon-no-connection", false);
			current_level = -1;
			curl_easy_cleanup(easy_handle);
			return TRUE;
		}
		setopt_ret = curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION, write_callback);
		if(setopt_ret) {
			// Failed to set options, fall back to no connection
			app_indicator_set_icon_full(indicator, "defcon-no-connection", "DWS No Connection");
			app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
			show_notification("DEFCON STATUS UPDATE", "Failed to connect to DWS servers!", "defcon-no-connection", false);
			current_level = -1;
			curl_easy_cleanup(easy_handle);
			return TRUE;
		}
		setopt_ret = curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, (void *)&chunk);
		if(setopt_ret) {
			// Failed to set options, fall back to no connection
			app_indicator_set_icon_full(indicator, "defcon-no-connection", "DWS No Connection");
			app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
			show_notification("DEFCON STATUS UPDATE", "Failed to connect to DWS servers!", "defcon-no-connection", false);
			current_level = -1;
			curl_easy_cleanup(easy_handle);
			return TRUE;
		}
		CURLcode res = curl_easy_perform(easy_handle);
		if(res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			app_indicator_set_icon_full(indicator, "defcon-no-connection", "DWS No Connection");
			app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
			show_notification("DEFCON STATUS UPDATE", "Failed to connect to DWS servers!", "defcon-no-connection", false);
			current_level = -1;
		} else {
			printf("Fetched data: '%s'\n", chunk.data);
			if(chunk.size == 1) {
				if(chunk.data[0] == '1') {
					if(current_level != 1) {
						app_indicator_set_icon_full(indicator, "defcon1", "DWS DEFCON 1");
						app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
						show_notification("DEFCON STATUS UPDATE", "DEFCON 1, Condition code RED", "defcon1", true);
						current_level = 1;
					}
				} else if(chunk.data[0] == '2') {
					if(current_level != 2) {
						app_indicator_set_icon_full(indicator, "defcon2", "DWS DEFCON 2");
						app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
						show_notification("DEFCON STATUS UPDATE", "DEFCON 2, Condition code ORANGE", "defcon2", true);
						current_level = 2;
					}
				} else if(chunk.data[0] == '3') {
					if(current_level != 3) {
						app_indicator_set_icon_full(indicator, "defcon3", "DWS DEFCON 3");
						app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
						show_notification("DEFCON STATUS UPDATE", "DEFCON 3, Condition code YELLOW", "defcon3", false);
						current_level = 3;
					}
				} else if(chunk.data[0] == '4') {
					if(current_level != 4) {
						app_indicator_set_icon_full(indicator, "defcon4", "DWS DEFCON 4");
						app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
						show_notification("DEFCON STATUS UPDATE", "DEFCON 4, Condition code BLUE", "defcon4", false);
						current_level = 4;
					}
				} else if(chunk.data[0] == '5') {
					if(current_level != 5) {
						app_indicator_set_icon_full(indicator, "defcon5", "DWS DEFCON 5");
						app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
						show_notification("DEFCON STATUS UPDATE", "DEFCON 5, Condition code GREEN", "defcon5", false);
						current_level = 5;
					}
				} else {
					// Invalid data received
					printf("DWS: Invalid data received, size ok but got: '%s'\n", chunk.data);
					app_indicator_set_icon_full(indicator, "defcon-no-connection", "DWS No Connection");
					app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
					show_notification("DEFCON STATUS UPDATE", "Failed to connect to DWS servers!", "defcon-no-connection", false);
					current_level = -1;
				}
			} else {
				// Invalid data received
				printf("DWS: Invalid data received, size too big, got: '%s'\n", chunk.data);
				app_indicator_set_icon_full(indicator, "defcon-no-connection", "DWS No Connection");
				app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
				show_notification("DEFCON STATUS UPDATE", "Failed to connect to DWS servers!", "defcon-no-connection", false);
				current_level = -1;
			}
		}
		curl_easy_cleanup(easy_handle);
		free(chunk.data);
	} else {
		printf("DWS: Failed to initialize curl easy session, resetting icon to no connection.\n");
		app_indicator_set_icon_full(indicator, "defcon-no-connection", "DWS No Connection");
		app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
		show_notification("DEFCON STATUS UPDATE", "Failed to connect to DWS servers!", "defcon-no-connection", false);
		current_level = -1;
	}

	return TRUE;
}

int main(int argc, char **argv) {
	CURLcode init_ret = curl_global_init(CURL_GLOBAL_ALL);
	if(init_ret) {
		// CURL Init went wrong!!!
		printf("DWS: Curl global initialization failed, exiting...");
		exit(EXIT_FAILURE);
	}
	gchar *config_path = g_build_filename(g_get_user_config_dir(), "dws", "config.ini", NULL);
	g_mkdir_with_parents(g_path_get_dirname(config_path), 0700);
	g_free(config_path);
	load_config();
	gtk_init(&argc, &argv);

	indicator = app_indicator_new(
		"DWS Tray",
		"defcon-no-connection",  // Icon from system theme
		APP_INDICATOR_CATEGORY_APPLICATION_STATUS
	);

	app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);

	GtkWidget *menu = build_menu();
	app_indicator_set_menu(indicator, GTK_MENU(menu));

	// Run it once manually
	fetch_dws_status(indicator);

	update_timeout(fetch_dws_status, indicator);

	gtk_main();

	if(ctx) {
		ca_context_destroy(ctx);
		ctx = NULL;
	}

	curl_global_cleanup();
	notify_uninit();
	return 0;
}
