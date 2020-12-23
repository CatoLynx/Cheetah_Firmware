idf_component_register(SRCS           browser_ota.c
                       INCLUDE_DIRS   include
                       REQUIRES       esp_http_server
                       PRIV_REQUIRES  app_update
                       EMBED_TXTFILES WebsiteFiles/index.html WebsiteFiles/favicon.ico WebsiteFiles/jquery-3.4.1.min.js WebsiteFiles/spinner.gif)