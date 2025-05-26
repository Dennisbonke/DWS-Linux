#include <curl/curl.h>
#include <curl/easy.h>
#include <gtk/gtk.h>
#include <libayatana-appindicator/app-indicator.h>
#include <time.h>

struct MemoryBuffer {
    char *data;
    size_t size;
};

int current_level = -1;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryBuffer *mem = (struct MemoryBuffer *)userp;

    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (ptr == NULL) {
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
		CURLcode setopt_ret = curl_easy_setopt(easy_handle, CURLOPT_URL, "https://defconwarningsystem.com/code.dat");
		if(setopt_ret) {
			// Exit
		}
		setopt_ret = curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION, write_callback);
		if(setopt_ret) {
			// Exit
		}
        setopt_ret = curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, (void *)&chunk);
		if(setopt_ret) {
			// Exit
		}
		CURLcode res = curl_easy_perform(easy_handle);
		if(res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			app_indicator_set_icon_full(indicator, "defcon-no-connection", "DWS No Connection");
			app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
		} else {
			printf("Fetched data: '%s'\n", chunk.data);
			if(chunk.size == 1) {
				if(chunk.data[0] == '1') {
					if(current_level != 1) {
						app_indicator_set_icon_full(indicator, "defcon1", "DWS DEFCON 1");
						app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
						current_level = 1;
					}
				} else if(chunk.data[0] == '2') {
					if(current_level != 2) {
						app_indicator_set_icon_full(indicator, "defcon2", "DWS DEFCON 2");
						app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
						current_level = 2;
					}
				} else if(chunk.data[0] == '3') {
					if(current_level != 3) {
						app_indicator_set_icon_full(indicator, "defcon3", "DWS DEFCON 3");
						app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
						current_level = 3;
					}
				} else if(chunk.data[0] == '4') {
					if(current_level != 4) {
						app_indicator_set_icon_full(indicator, "defcon4", "DWS DEFCON 4");
						app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
						current_level = 4;
					}
				} else if(chunk.data[0] == '5') {
					if(current_level != 5) {
						app_indicator_set_icon_full(indicator, "defcon5", "DWS DEFCON 5");
						app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
						current_level = 5;
					}
				} else {
					// Invalid data received
					printf("DWS: Invalid data received, size ok but got: '%s'\n", chunk.data);
					app_indicator_set_icon_full(indicator, "defcon-no-connection", "DWS No Connection");
					app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
					current_level = -1;
				}
			} else {
				// Invalid data received
				printf("DWS: Invalid data received, size too big, got: '%s'\n", chunk.data);
				app_indicator_set_icon_full(indicator, "defcon-no-connection", "DWS No Connection");
				app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
				current_level = -1;
			}
		}
		curl_easy_cleanup(easy_handle);
    	free(chunk.data);
	} else {
		printf("DWS: Failed to initialize curl easy session, resetting icon to no connection.\n");
		app_indicator_set_icon_full(indicator, "defcon-no-connection", "DWS No Connection");
		app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
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
    gtk_init(&argc, &argv);

    AppIndicator *indicator = app_indicator_new(
        "DWS Tray",
        "defcon-no-connection",  // Icon from system theme
        APP_INDICATOR_CATEGORY_APPLICATION_STATUS
    );

    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);

    GtkWidget *menu = gtk_menu_new();

    GtkWidget *item_quit = gtk_menu_item_new_with_label("Quit");
    g_signal_connect(item_quit, "activate", G_CALLBACK(gtk_main_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_quit);

    gtk_widget_show_all(menu);
    app_indicator_set_menu(indicator, GTK_MENU(menu));

	// Run it once manually
	fetch_dws_status(indicator);

	// Hardcode the interval to 10 seconds for now.
	// TODO: Make this configurable
	g_timeout_add(10000, fetch_dws_status, indicator);

    gtk_main();

	curl_global_cleanup();
    return 0;
}
