#include <stdio.h>

#include "mongoose.h"

static const char log_file[] = "/tmp/rescue_httpd";

static char str_buf[1024];

static void http_begin (struct mg_connection *conn, char *title, char *extra)
{
    mg_printf(conn, "HTTP/1.0 200 OK\r\n"
                    "Content-Type: text/html\r\n\r\n"
                    "<html><head>\r\n");
    if (title)
        mg_printf(conn, "<title>%s</title>\r\n", title);

    mg_printf(conn, "<title>Raptor rescue</title>\r\n"
                    "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\r\n"
                    "<meta http-equiv=\"Cache-Control\" content=\"no-cache\"/>\r\n"
                    "<meta http-equiv=\"pragma\" content=\"no-cache\"/>\r\n");

    if (extra) mg_printf(conn, "%s\r\n", extra);

    mg_printf(conn, "</head>\r\n");
}

static void http_end (struct mg_connection *conn)
{
    mg_printf(conn, "</body></html>\r\n");
}

static const char refresh_html[] = "<meta http-equiv=\"refresh\" content=\"1; url=index.html\"/>";

void http_refresh_page (struct mg_connection *conn, char *msg)
{
    http_begin(conn,NULL,refresh_html);

    if (msg)
        mg_printf(conn, "<body style=\"font-family:arial;font-size:30px;\"> %s <br></body></html>\r\n", msg);
    else
        mg_printf(conn, "<body>\r\n");

    http_end(conn);
}

static const char reboot_html[] =
    "<script type=\"text/javascript\">\r\n"
        "var cnt = 0;\r\n"
        "var timeout = 60;\r\n"
        "function Refresh() {\r\n"
            "var d = new Date();\r\n"
            "document.location = \"http://\" + document.location.hostname + \"/?\" + d.getTime();\r\n"
        "}\r\n"
        "window.onload = function() {\r\n"
            "document.getElementById('counter').innerHTML = timeout;\r\n"
            "hReloadInterval = setInterval (function() {\r\n"
            "cnt++;\r\n"
            "if ( cnt >= timeout ) {\r\n"
                "document.getElementById('counter').innerHTML = 0;\r\n"
                "Refresh();\r\n"
            "} else {\r\n"
                "document.getElementById('counter').innerHTML = timeout - cnt;\r\n"
            "}\r\n"
            "}, 1000);\r\n"
        "}\r\n"
    "</script>\r\n"
    "Page will be reloaded automatically in <div id=counter style=\"display: inline\"></div> second(-s)...\r\n"
    "<br>\r\n"
    "<form id=\"openid_form\" action=\"/\">\r\n"
    "<br>\r\n"
    "Press <input type=\"button\" value=\"this\" onclick=\"javascript: Refresh();\" />\r\n"
    "button to reload the page manually.\r\n";

void http_reboot_page (struct mg_connection *conn)
{
    http_begin(conn,NULL,NULL);
    mg_printf(conn, reboot_html);
    http_end(conn);
}

void clear_log (void)
{
    FILE *f = fopen ("/tmp/httpd.log","w");
    if (f) fclose(f);
}

static void log_2_html (struct mg_connection *conn)
{
    FILE *f;

    mg_printf(conn, "<body style=\"font-family:arial;font-size:15px;\" onload=\"setTimeout(function(){window.scrollTo(0,document.body.scrollHeight)}, 100);\" >\r\n");

    f = fopen ("/tmp/httpd.log","r");
    if (f) {
        while (fgets(str_buf,sizeof(str_buf)-1,f) != NULL)
            mg_printf(conn,"%s<br>\r\n",str_buf);
        mg_printf(conn,"<br>\r\n");
        fclose(f);
    }
}

void http_progress_page (struct mg_connection *conn, int enable_abort)
{
    if (!enable_abort)
        http_begin(conn,"Raptor rescue: processing request...","<meta http-equiv=\"refresh\" content=\"2\"/>\r\n");
    else
        http_begin(conn,"Raptor rescue: processing request...","<meta http-equiv=\"refresh\" content=\"5\"/>\r\n");

    log_2_html(conn);

    if (enable_abort)
        mg_printf(conn, "<form action=\"/abort\" > <input type=submit value=\"Abort\" /> </form>\r\n");

    http_end(conn);
}

void http_log_page (struct mg_connection *conn)
{
    log_2_html(conn);

    mg_printf(conn, "<form action=\"/back\" > <input type=submit value=\"Back\" /> </form>\r\n");

    http_end(conn);
}

static const char reboot_action_html[] =
    "<form action=\"/reboot\" >\r\n"
        "<input type=submit value=\"Reboot\" />\r\n"
    "</form> <br>\r\n";

static const char log_action_html[] =
    "<form action=\"/log\" >\r\n"
        "<input type=submit value=\"Log\" />\r\n"
    "</form> <br>\r\n";

static const char upload_file_html[] =
    "<form method=POST action=\"/%s\" enctype=\"multipart/form-data\">\r\n"
        "%s: <input type=submit value=\"Upload\" /> <input type=file name=\"file\" size=50 />\r\n"
    "</form>\r\n";

static const char date_time_html[] =
    "<form method=POST action=\"/date_time\" enctype=\"multipart/form-data\" >\r\n"
        "<b>Date:</b> <input type=text name=\"DD\" size=2 /> <input type=text name=\"MM\" size=2 /> <input type=text name=\"YYYY\" size=4 /> (DD/MM/YYYY) <br>\r\n"
        "<b>Time:</b><input type=text name=\"hh\" size=2 /> <input type=text name=\"mm\" size=2 /> <input type=text name=\"ss\" size=2 /> (HH/MM/SS) <br>\r\n"
        "<input type=submit value=\"Apply\" />\r\n"
    "</form> <br>\r\n";

void http_home_page (struct mg_connection *conn, int history)
{
    FILE *f;

    http_begin(conn,"Embedded webserver",NULL);

    mg_printf(conn, "<body>\r\n");

    /* Try to read /etc/issue and insert its content to html */
    f = fopen ("/etc/issue","r");
    if (f) {
        while (fgets(str_buf,sizeof(str_buf)-1,f) != NULL)
            mg_printf(conn,"<b>%s</b><br>\r\n",str_buf);
        mg_printf(conn,"<br>\r\n");
        fclose(f);
    }

    /* Try to read /tmp/mac and insert its content to html as device serial number */
    f = fopen ("/etc/versioninfo","r");
    if (f) {
        while (fgets(str_buf,sizeof(str_buf)-1,f) != NULL)
            mg_printf(conn,"<b>Version: %s</b><br>\r\n",str_buf);
        mg_printf(conn,"<br>\r\n");
        fclose(f);
    }

    //mg_printf(conn, reboot_action_html);

    mg_printf(conn, upload_file_html, "upload_fw","Firmware image");

    mg_printf(conn,"<br><br>\r\n");

    mg_printf(conn, upload_file_html, "upload_fpga", "FPGA image");

    mg_printf(conn,"<br><br>\r\n");

    mg_printf(conn, date_time_html);

    if (history) {
        mg_printf(conn,"<br><br>\r\n");
        mg_printf(conn, log_action_html);
    }

    http_end(conn);
}
