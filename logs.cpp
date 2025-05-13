//--------------------------------------------------------------------------------
// Company:		Argonne National Laboratory
// Division:	Physics
// Project:		DGS Receiver
// Author:		Michael Oberling
// File:		logs.cpp
// Description: Log file functions.
//--------------------------------------------------------------------------------

//==============================
//---     Include Files      ---
//==============================
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "dgsReceiver.h"
#include "logs.h"

//==============================
//---        Defines         ---
//==============================
//==============================
//---        Typedefs        ---
//==============================
//==============================
//---         Enums          ---
//==============================
//==============================
//---        Sturcts         ---
//==============================
//==============================
//---         Unions         ---
//==============================
//==============================
//---        Externs         ---
//==============================
//==============================
//---        Globals         ---
//==============================
//==============================
//---       Prototypes       ---
//==============================
/* open error message log file */
void open_error_log (void);

/* open standard rate and message log files */
int open_normal_logs (void) {
	sprintf(g_file_strings.sts_log_file_name, "%s%s", g_file_strings.user_file_name, STS_FILE_NAME_SUFFIX);
	sprintf(g_file_strings.msg_log_file_name, "%s%s", g_file_strings.user_file_name, MSG_FILE_NAME_SUFFIX);
	sprintf(g_file_strings.err_log_file_name, "%s%s", g_file_strings.user_file_name, ERR_FILE_NAME_SUFFIX);
	if (g_file.stat_log == NULL)
		g_file.stat_log = fopen(g_file_strings.sts_log_file_name, "w");
	if (g_file.msg_log == NULL)
		g_file.msg_log = fopen(g_file_strings.msg_log_file_name, "w");

	if ((g_file.stat_log == NULL) || (g_file.msg_log == NULL))
	{
		puts("FATAL ERROR: Cannot open log files for writing.");
		printf("FATAL ERROR: Statistics Log File: %s", g_file_strings.sts_log_file_name);
		printf("FATAL ERROR: Message Log File: %s", g_file_strings.msg_log_file_name);
		return 1;
	}

	return 0;
}

/* open error message log file */
void open_error_log	(void) {
	if (g_file.error_log == NULL)
		g_file.error_log = fopen(g_file_strings.err_log_file_name, "w");

	return;
}

/* close all log files */
void close_log_files (void) {
	if (g_file.stat_log != NULL)
		fclose(g_file.stat_log);
	if (g_file.msg_log != NULL)
		fclose(g_file.msg_log);
	if (g_file.error_log != NULL)
		fclose(g_file.error_log);

	return;
}

/* log rates */
void log_statitics (void) {
	return;
}

/* log message */
void log_message (char* const &msg) {
	time_t current_time = time(NULL);
	char base_string[] = "YYYY-MM-DD HH:MM:SS CST: ";
	strftime (base_string, strlen(base_string)+1, "%Y-%m-%d %H:%M:%S %Z: ", localtime(&current_time));
	printf ("%s%s\n", base_string, msg);

	if (g_file.msg_log != NULL)
		fprintf (g_file.msg_log, "%s%s\n", base_string, msg);

	return;
}
/* log error message*/
void log_error (char* const &func, char* const &msg) {
	time_t current_time = time(NULL);
	char base_string[] = "YYYY-MM-DD HH:MM:SS CST: ERROR: ";
	strftime (base_string, strlen(base_string)+1, "%Y-%m-%d %H:%M:%S %Z: ERROR: ", localtime(&current_time));
	printf ("%s%s: %s\n", base_string, func, msg);

	if (g_file.error_log == NULL)
		open_error_log();
	if (g_file.error_log != NULL)
	{
		fprintf(g_file.error_log, "%s%s: %s\n", base_string, func, msg);
		fflush(g_file.error_log);
	}

	return;
}
