/******************************************************************************/
/*                                                                            */
/*   A M Q E R R   T O   Q U E U E                                            */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/*   file: main.c                                                             */
/*                                                                            */
/*   description: amqerr2q will read the text from AMQERR??.LOG files and     */
/*                copy the information to a queue.                            */
/*                Each entry will be copied in a new message. Messages are    */
/*                non-persistent. The recovery of missing messages is done    */
/*                with a message on admin.amqerr.send.queue                   */
/*                                                                            */
/*   functions:                                                               */
/*     - main                                                                 */
/*                                                                            */
/*   history:                                                                 */
/*   19.02.2018 am initial version                                            */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*   I N C L U D E S                                                          */
/******************************************************************************/

// ---------------------------------------------------------
// system
// ---------------------------------------------------------

// ---------------------------------------------------------
// mq
// ---------------------------------------------------------

// ---------------------------------------------------------
// own 
// ---------------------------------------------------------
#include "main.h"
#include <ctl.h>
#include <msgcat/lgstd.h>
#include <amqerr.h>

// ---------------------------------------------------------
// local
// ---------------------------------------------------------

/******************************************************************************/
/*   D E F I N E S                                                            */
/******************************************************************************/
#define LOG_DIRECTORY   "/var/mqm/errors/appl"

/******************************************************************************/
/*   M A C R O S                                                              */
/******************************************************************************/

/******************************************************************************/
/*   P R O T O T Y P E S                                                      */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*                                  M A I N                                   */
/*                                                                            */
/******************************************************************************/
#ifndef __TDD__

int main(int argc, const char* argv[] )
{
  int sysRc ;

  sysRc = handleCmdLn( argc, argv ) ;
  if( sysRc != 0 ) goto _door ;

  char logDir[PATH_MAX];
  char logName[PATH_MAX+NAME_MAX];
  int logLevel = LNA ;   // log level not available

  // -------------------------------------------------------
  // setup the logging
  // -------------------------------------------------------
  if( getStrAttr( "log") )
  {
    snprintf( logDir, PATH_MAX, "%s", getStrAttr( "log") ) ;
  }
  else
  {
    snprintf( logDir, PATH_MAX, "%s", LOG_DIRECTORY );
  }

  if( getStrAttr( "loglev" ) )
  {
    logLevel = logStr2lev( getStrAttr( "loglev" ) );
  }

  if( logLevel == LNA ) logLevel = LOG; 

  snprintf( logName, PATH_MAX+NAME_MAX, "%s/%s.log", logDir, progname );

  sysRc = initLogging( (const char*) logName, logLevel );

  if( sysRc != 0 ) goto _door ;


  sysRc = amqerr();

_door :
  return sysRc ;
}

#endif

/******************************************************************************/
/*                                                                            */
/*   F U N C T I O N S                                                        */
/*                                                                            */
/******************************************************************************/

