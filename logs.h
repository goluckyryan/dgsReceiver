//--------------------------------------------------------------------------------
// Company:		Argonne National Laboratory
// Division:	Physics
// Project:		DGS Receiver
// Author:		Michael Oberling
// File:		logs.cpp
// Description: Log file functions.
//--------------------------------------------------------------------------------

#ifndef _LOGS_H
#define _LOGS_H

//==============================
//---     Include Files      ---
//==============================
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

/* open standard rate and message log files */
int		open_log_files	(void);
/* close all log files */
void	close_log_files	(void);
/* log statistics */
void	log_statitics	(void);
/* log message */
void	log_message		(char* const &msg);
/* log error message*/
void	log_error		(char* const &func, char* const &msg);

#endif // _LOGS_H