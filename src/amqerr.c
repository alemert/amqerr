/******************************************************************************/
/*                                                                            */
/*   A M Q E R R   T O   Q U E U E                                            */
/*                                                                            */
/*  ------------------------------------------------------------------------  */
/*                                                                            */
/*  file: amqerr.c                                                            */
/*                                                                            */
/*  functions:                                                                */
/*    - amqerr                                                                */
/*    - amqerrsend                                                            */
/*    - dir2queue                                                             */
/*    - file2queue            */
/*                                                                            */
/*  history                                                                   */
/*  19.02.2018 am initial version                                    */
/*  24.02.2018 am functions lsAmqerr, rotateAmqerr and copy moved to rotate.c */
/*                                                                            */
/******************************************************************************/
#define C_MODULE_AMQERR

/******************************************************************************/
/*   I N C L U D E S                                                          */
/******************************************************************************/

// ---------------------------------------------------------
// system
// ---------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

// ---------------------------------------------------------
// MQ
// ---------------------------------------------------------
#include <cmqc.h>

// ---------------------------------------------------------
// own 
// ---------------------------------------------------------
#include <ctl.h>
#include <msgcat/lgstd.h>
#include <msgcat/lgmqm.h>

#include <libgen.h>

#include <cmdln.h>

// ---------------------------------------------------------
// local
// ---------------------------------------------------------
#include <amqerr.h>

#include "lgloc.h"

/******************************************************************************/
/*   G L O B A L S                                                            */
/******************************************************************************/

/******************************************************************************/
/*   D E F I N E S                                                            */
/******************************************************************************/

#define LOOP          5    // time to sleep in each main loop

/******************************************************************************/
/*   M A C R O S                                                              */
/******************************************************************************/

/******************************************************************************/
/*   S T R U C T                                                              */
/******************************************************************************/

/******************************************************************************/
/*   T Y P E S                                                                */
/******************************************************************************/

/******************************************************************************/
/*   P R O T O T Y P E S                                                      */
/******************************************************************************/
MQLONG amqerrsend( );
MQLONG dir2queue( char* _path, int _cmdVer );
MQLONG file2queue(int _cmdVer,const char* _path,const char* file,off_t offset);

/******************************************************************************/
/*                                                                            */
/*   F U N C T I O N S                                                        */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*  AMQ ERRROR                                                                */
/*                                                                            */
/*  description: central function; entry point for whole functionality except */
/*               command line and error-log handling                          */
/*                                                                            */
/*  attributes: void                                                          */
/*                                                                            */
/*  return code:                                                              */
/*    (int) 0  -> OK                                                          */
/*    (int) >0 -> MQ Reason Code                                              */
/*    (int) <0 -> non-MQ Error                                                */
/*                                                                            */
/******************************************************************************/
int amqerr()
{
  logFuncCall( );

  int sysRc = MQRC_NONE;

  char qmgrName[MQ_Q_MGR_NAME_LENGTH+1];

  // -------------------------------------------------------
  // initialize
  // -------------------------------------------------------
  memset( qmgrName, ' ', MQ_Q_MGR_NAME_LENGTH );
  qmgrName[MQ_Q_MGR_NAME_LENGTH] = '\0' ;

  if( getStrAttr( "qmgr" ) )
  {
    memcpy( qmgrName,  getStrAttr( "qmgr"  ), strlen( getStrAttr( "qmgr"  ) ));
  }

  sysRc = initMQ( qmgrName );

  if( sysRc != MQRC_NONE ) goto _door ;

  // -------------------------------------------------------
  // put amqerr tag to the queue
  // -------------------------------------------------------
  if( !getFlagAttr( "send" ) )
  {
    sysRc = amqerrsend( );
    goto _door;
  }

  _door:

  logFuncExit( );

  return sysRc;
}

/******************************************************************************/
/*  AMQ ERRROR SEND                                                           */
/*                                                                            */
/*  description:                                                              */
/*    open both queues (store and state)            */
/*    rotate AMQERR files                       */
/*    transfer AMQERR files information to queues          */
/*                    */
/*  attributes: void                                                          */
/*                                                                            */
/*  return code:                                                              */
/*    (int) 0  -> OK                                                          */
/*    (int) >0 -> MQ Reason Code                                              */
/*    (int) <0 -> non-MQ Error                                                */
/*                                                                            */
/******************************************************************************/
MQLONG amqerrsend( )
{
 logFuncCall( );
  MQLONG sysRc = MQRC_NONE ;

  char path[PATH_MAX+1];
  int  cmVer;


  // -------------------------------------------------------
  // get data path of the queue manager
  // -------------------------------------------------------
  getDataPath( sysRc, path );
  if( sysRc != MQRC_NONE )
  {
    goto _door;
  }
  strcat( path, "/errors" );

  getCmdLevel( sysRc, &cmVer );
  if( sysRc != MQRC_NONE )
  {
    goto _door;
  }

  // -------------------------------------------------------
  // rotate files, transfer data from AMQERR files to store queue
  // -------------------------------------------------------
  dir2queue( path, cmVer );

  _door:


  logFuncExit( );

  return sysRc;
}

/******************************************************************************/
/*  directory to queue                                                        */
/*                                                                            */
/*  description:                                                              */
/*   - list all AMQERR files                                                  */ 
/*   - rotate AMQERR04 to AMQERR98 files if AMQERR02 was moved to AMQERR03    */
/*   - copy new error items to a queue from AMQERR02 if AMQERR01 was moved    */
/*      to AMQERR02                                                           */
/*   - copy new error items to a queue from AMQERR01 if its time stamp has    */
/*      been modified                                                         */
/*                                                                            */
/*  attributes:                                                               */
/*     - MQ connection handle                                                 */
/*     - path to error files                                                  */
/*     - command level of the queue manager                                   */
/*                                                                            */
/*  return code:                                                              */
/*    (int)    0  -> OK                                                       */
/*    (int) != 0 -> MQ Reason Code                                            */
/*                                                                            */
/******************************************************************************/
MQLONG dir2queue( char* _path, int _cmdVer )
{
  logFuncCall( );

  MQLONG sysRc = MQRC_NONE;

  tAmqerr allFile[AMQ_MAX_ID+1];  // all  AMQERR??.LOG files [ 01 - 99]
  tAmqerr baseFile[4];            // base AMQERR??.LOG files [ 01 - 03]

  struct stat aFileAttr ;          // file attributes for AMQERR file
  struct stat cFileAttr ;          // file attributes for CMPERR file

  int i;


  // -------------------------------------------------------
  // initialize file list (assuming no file exists)
  // -------------------------------------------------------
  for( i=1; i<AMQ_MAX_ID+1; i++ )
  {
    sprintf( allFile[i].name, "%s/"AMQERR"%02d.LOG",_path,i);
    allFile[i].mtime = 0;
    allFile[i].length = 0;
    memcpy( allFile[i].msgId, MQMI_NONE, sizeof(MQBYTE24) );
  }

  for( i=1; i<AMQ_MAX_BASE_ID+1; i++ )
  {
    baseFile[i].mtime = 0;
    baseFile[i].length = 0;
    memcpy(baseFile[i].msgId, MQMI_NONE, sizeof(MQBYTE24) );
  }
  
  sysRc = lsAmqerr( _path, baseFile, AMQ_MAX_ID );

  // -------------------------------------------------------
  // check if compare file exists, if not create it
  // -------------------------------------------------------
  sprintf( allFile[0].name, "%s/"CMPERR,_path);

  while( 1 )
  {
    sysRc = lsAmqerr( _path, allFile, AMQ_MAX_ID );

    // -----------------------------------------------------
    // rotate AMQERR files
    // -----------------------------------------------------
    stat( allFile[3].name, &aFileAttr ); // get change time of AMQERR03.LOG
    stat( allFile[0].name, &cFileAttr ); // get change time of CMPERR03.LOG
    if( errno == ENOENT )                //
    {                                    // CMPERR03.LOG doesn't exist
      copy( allFile[3].name ,            // create it by copying  AMQERR03
            allFile[0].name );           // to CMPERR03
    }                                    //
    else if( aFileAttr.st_ctim.tv_sec >  // CMPERR03 exists and
             cFileAttr.st_ctim.tv_sec )  // AMQERR03 is newer than CMPERR03 
    {                                    //
      rotateAmqerr( allFile );           // rotate log files
    }                                    //

    // -----------------------------------------------------
    // get the base file information from the 
    // -----------------------------------------------------
    sysRc = getSendState( baseFile );
    if( sysRc != MQRC_NONE ) goto _door;

    for( i=AMQ_MAX_BASE_ID; i>0; i-- )
    {
      if( baseFile[i].mtime == 0 ) 
      {
	sysRc = file2queue( _cmdVer, 
                            _path, 
                            baseFile[i].name, 
                            baseFile[i].length );
        if( sysRc > 0 )
	{
	  goto _door;
	}
      }
    }

    sleep(LOOP);
  }

  _door :

  houseKeepingMQ() ;

  logFuncExit( );
  return sysRc;
}

/******************************************************************************/
/*  file to queue                                                             */
/*                                                                            */
/*  description:                                                              */
/*   - move to the position offset                                            */
/*   - loop until EOF                                                         */
/*     - read and analyze log                                                 */
/*     - put log information to the store queue                               */
/*   - end of loop                                                            */
/*   - put new position and mtime to the state queue                          */
/*                                                                            */
/*  attributes:                                                               */
/*     - command level of the queue manager                                   */
/*     - path to the AMQERR files                                             */
/*     - name of the AMQERR file                                              */
/*     - file offset                                                          */
/*                                                                            */
/*  return code:                                                              */
/*    (int)    0  -> OK                                                       */
/*    (int) != 0 -> MQ Reason Code                                            */
/*                                                                            */
/******************************************************************************/
MQLONG file2queue( int _cmdLevel    , 
                   const char* _path, 
                   const char* _file, 
                   off_t _offset    )
{
  logFuncCall( );

  MQLONG sysRc = MQRC_NONE;
  off_t newOffset ;

  FILE* fp;
  tAmqerrMessage msg;

  char fileName[PATH_MAX];
   
  snprintf(fileName,PATH_MAX, "%s/%-*.*s", _path,
	   (int)sizeof(AMQ_FILE_NAME)-1,(int)sizeof(AMQ_FILE_NAME)-1,_file);
 
  if( !(fp = fopen(fileName,"r")) )
  {
    sysRc = errno ;
    logger( LSTD_OPEN_FILE_FAILED, _file );
    logger( LSTD_ERRNO_ERR, sysRc, strerror( sysRc ) );
    goto _door ;
  }

  if( fseek(fp, _offset,SEEK_SET) != 0 )
  {
    sysRc = errno ;
    logger( LSTD_ERRNO_ERR, sysRc, strerror( sysRc ) );
  }

  switch( _cmdLevel )
  {
    case MQCMDL_LEVEL_1   :
    case MQCMDL_LEVEL_101 :
    case MQCMDL_LEVEL_110 :
    case MQCMDL_LEVEL_114 :
    case MQCMDL_LEVEL_120 :             
    case MQCMDL_LEVEL_200 :            
    case MQCMDL_LEVEL_201 :           
    case MQCMDL_LEVEL_210 :          
    case MQCMDL_LEVEL_211 :         
    case MQCMDL_LEVEL_220 :        
    case MQCMDL_LEVEL_221 :       
    case MQCMDL_LEVEL_230 :      
    case MQCMDL_LEVEL_320 :     
    case MQCMDL_LEVEL_420 :    
    case MQCMDL_LEVEL_500 :   
    case MQCMDL_LEVEL_510 :  
    case MQCMDL_LEVEL_520 : 
    case MQCMDL_LEVEL_530 :
    case MQCMDL_LEVEL_531 :             
    case MQCMDL_LEVEL_600 :            
    case MQCMDL_LEVEL_700 :           
    case MQCMDL_LEVEL_701 :          
    case MQCMDL_LEVEL_710 :         
    case MQCMDL_LEVEL_711 :        
    {
      logger( LAER_MQ_CMD_VER_ERR, _cmdLevel );
      sysRc = 1;
      goto _door;
    }
    case MQCMDL_LEVEL_750 :       
    case MQCMDL_LEVEL_800 :      
    {
      sysRc = parseFile800( fp, &msg );
      newOffset = (sysRc > 0) ? sysRc : 0;
      break;
    }
    case MQCMDL_LEVEL_801 :     
    case MQCMDL_LEVEL_802 :    
    default:
    {
      logger( LAER_MQ_CMD_VER_ERR, _cmdLevel );
      sysRc = 1;
      goto _door;
    }
  }

  if( sysRc > 0 )
  {
    putAmqMessage( &msg );
    putStateMessage( _file, newOffset );
  }

  _door:

  if( fp != NULL)
  {
    fclose(fp);
  }
  logFuncExit( );
  return sysRc;
}
