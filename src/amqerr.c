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
/*    - getDataPath                                                           */
/*    - dir2queue                                                             */
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
#include <mqbase.h>
#include <mqtype.h>

// ---------------------------------------------------------
// local
// ---------------------------------------------------------
#include <amqerr.h>

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
MQLONG amqerrsend( MQHCONN *_hConn );
MQLONG getDataPath( MQHCONN *_hConn, char* _path );
MQLONG dir2queue( MQHCONN  *_hConn, char* _path );

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
  int locRc = MQRC_NONE;

  char qmgrName[MQ_Q_MGR_NAME_LENGTH+1];

  MQHCONN hCon;                // connection handle   

  // -------------------------------------------------------
  // initialize
  // -------------------------------------------------------
  memset( qmgrName, ' ', MQ_Q_MGR_NAME_LENGTH );
  qmgrName[MQ_Q_MGR_NAME_LENGTH] = '\0' ;

  if( getStrAttr( "qmgr" ) )
  {
    memcpy( qmgrName,  getStrAttr( "qmgr"  ), strlen( getStrAttr( "qmgr"  ) ));
  }

  // -------------------------------------------------------
  // connect to queue manager
  // -------------------------------------------------------
  sysRc = mqConn( (char*) qmgrName,  // queue manager          
                  &hCon );           // connection handle            
  switch( sysRc )                    //
  {                                  //
    case MQRC_NONE: break;           // OK
    case MQRC_Q_MGR_NAME_ERROR:      // queue manager does not exists
    {                                //
      logger( LMQM_UNKNOWN_QMGR, qmgrName );
      goto _disconn;                    //
    }                                //
    default: goto _disconn;             // error logged in mqConn
  }                                  //

  // -------------------------------------------------------
  // put amqerr tag to the queue
  // -------------------------------------------------------
  if( !getFlagAttr( "send" ) )
  {
    sysRc = amqerrsend( &hCon );
    goto _door;
  }

  _door:

  // -------------------------------------------------------
  // disconnect queue manager
  // -------------------------------------------------------
  locRc = mqDisc( &hCon ); // connection handle            
  switch( locRc )
  {
    case MQRC_NONE: break;
    default: logger( LSTD_GEN_SYS, progname );
  }

  sysRc = sysRc == MQRC_NONE ? locRc : sysRc ;

  _disconn:

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
MQLONG amqerrsend( MQHCONN *_hConn )
{
 logFuncCall( );
  MQLONG sysRc = MQRC_NONE ;

  char path[PATH_MAX+1];

  // -------------------------------------------------------
  // get data path of the queue manager
  // -------------------------------------------------------
  sysRc = getDataPath( _hConn, path );
  if( sysRc != MQRC_NONE )
  {
    goto _door;
  }
  strcat( path, "/errors" );

  // -------------------------------------------------------
  // rotate files, transfer data from AMQERR files to store queue
  // -------------------------------------------------------
  dir2queue( _hConn, path );

  _door:


  logFuncExit( );

  return sysRc;
}


/******************************************************************************/
/*  GET DATA PATH                                                             */
/*                                                                            */
/*  description:                                                              */
/*    display qmstatus all                                                    */
/*                                                                            */
/*                                                                            */
/*  return code:                                                              */
/*    (char*) path -> OK                                                      */
/*    (char*) NULL -> ERR                                                     */
/*                                                                            */
/******************************************************************************/
MQLONG getDataPath( MQHCONN *_hConn, char* _path )
{
  logFuncCall( );

  MQLONG mqrc = MQRC_NONE;

  MQHBAG cmdBag = MQHB_UNUSABLE_HBAG;
  MQHBAG respBag = MQHB_UNUSABLE_HBAG;
  MQHBAG attrBag;

  MQLONG parentItemCount;
  MQLONG parentItemType;
  MQLONG parentSelector;

  MQLONG childItemCount;
  MQLONG childItemType;
  MQLONG childSelector;

  MQINT32 selInt32Val;
  MQCHAR selStrVal[ITEM_LENGTH];

  MQLONG selStrLng;

  char sBuffer[ITEM_LENGTH + 1];

  int i;
  int j;


#define _LOGTERM_ 

  // -------------------------------------------------------
  // open bags for MQ Execute
  // -------------------------------------------------------
  mqrc = mqOpenAdminBag( &cmdBag );
  switch( mqrc )
  {
    case MQRC_NONE: break;
    default: goto _door;
  }

  mqrc = mqOpenAdminBag( &respBag );
  switch( mqrc )
  {
    case MQRC_NONE: break;
    default: goto _door;
  }

  // -------------------------------------------------------
  // DISPLAY QMGR ALL 
  //   process command in two steps
  //   1. setup the list of arguments MQIACF_ALL = MQCA_SSL_KEY_REPOSITORY
  //   2. send a command MQCMD_INQUIRE_Q_MGR = DISPLAY QMGR
  // -------------------------------------------------------
  mqrc = mqSetInqAttr( cmdBag,            // set attribute 
                       MQCA_SSL_KEY_REPOSITORY) ;
                                          // for the PCF command
  switch( mqrc )                          // DISPLAY QMGR SSLKEYR
  {                                       //
    case MQRC_NONE: break;                //
    default: goto _door;                  //
  }                                       //
                                          //
  mqrc = mqExecPcf( *_hConn,              // send a command to 
                    MQCMD_INQUIRE_Q_MGR,  //  the command queue
                    cmdBag,               //
                    respBag );            //
                                          //
  switch( mqrc )                          //
  {                                       //
    case MQRC_NONE: break;                //
    default:                              // mqExecPcf includes  
    {                                     // evaluating mqErrBag,
      mqrc = (MQLONG) selInt32Val;        // additional evaluating of 
      goto _door;                         // MQIASY_REASON is therefor
    }                                     // not necessary
  }                                       //
                                          //
  // ---------------------------------------------------------
  // count the items in response bag
  // -------------------------------------------------------
  mqrc=mqBagCountItem(respBag,             // get the amount of items
                      MQSEL_ALL_SELECTORS);//  for all selectors
                                           //
  if( mqrc > 0 )                           // 
  {                                        //
    goto _door;                            //
  }                                        //
  else                                     // if reason code is less 
  {                                        //  then 0 then it is not 
    parentItemCount = -mqrc;               //  a real reason code it's  
    mqrc = MQRC_NONE;                      //  the an item counter
  }                                        //
                                           //
  // ---------------------------------------------------------
  // go through all items
  //  there are two loops, first one over parent items
  //  and the second one for child items
  //  child items are included in a internal (child) bag in one of parents item
  // ---------------------------------------------------------
  for( i = 0; i < parentItemCount; i++ )   // analyze all items
  {                                        //
    mqrc = mqItemInfoInq( respBag,         // find out the item type
                          MQSEL_ANY_SELECTOR, 
                          i,               //
                          &parentSelector, //
                          &parentItemType ); 
                                           //
    switch( mqrc )                         //
    {                                      //
      case MQRC_NONE: break;               //
      default: goto _door;                 //
    }                                      //
                                           //
#ifdef  _LOGTERM_                          //
    char* pBuffer;                         //
    printf( "%2d selector: %04d %-30.30s type %10.10s",
            i,                             //
            parentSelector,                //
            mqSelector2str( parentSelector ), 
            mqItemType2str( parentItemType ) );
#endif                                     //
                                           //
    // -------------------------------------------------------
    // for each item:
    //    - get the item type
    //    - analyze selector depending on the type
    // -------------------------------------------------------
    switch( parentItemType )               //
    {                                      //
      // -----------------------------------------------------
      // Parent Bag:
      // TYPE: 32 bit integer -> in this function only system 
      //       items will be needed, f.e. compilation and reason code 
      // -----------------------------------------------------
      case MQITEM_INTEGER:                 // in this program only 
      {                                    //  system selectors will 
        mqrc=mqIntInq(respBag,             //  be expected
                      MQSEL_ANY_SELECTOR,  // out of system selectors 
                      i,                   //  only compelition and 
                      &selInt32Val );      //  reason code are important
        switch( mqrc )                     //  all other will be ignored  
        {                                  //
          case MQRC_NONE: break;           // analyze InquireInteger
          default: goto _door;             //  reason code
        }                                  //
                                           //
#ifdef _LOGTERM_                           //
        pBuffer = (char*) itemValue2str( parentSelector,
                                         (MQLONG) selInt32Val );
        if( pBuffer )                      //
        {                                  //
          printf( " value %s\n", pBuffer );//
        }                                  //
        else                               //
        {                                  //
          printf( " value %d\n", (int) selInt32Val ); 
        }                                  //
#endif                                     //
        // ---------------------------------------------------
        // Parent Bag: 
        // TYPE: 32 bit integer; analyze selector
        // ---------------------------------------------------
        switch( parentSelector )           // all 32 bit integer 
        {                                  //  selectors are system
          case MQIASY_COMP_CODE:           //  selectors
          {                                //
            break;                         // only mqExec completion 
          }                                //  code and reason code are 
          case MQIASY_REASON:              //  interesting for later use
          {                                // 
            mqrc = (MQLONG) selInt32Val;   //
	    switch( mqrc )                 //
	    {                              //
	      case MQRC_NONE: break;       //
	      default: goto _door;         //
            }                              //
            break;                         //
          }                                //
          default:                         //
          {                                // all other selectors can
            break;                         //  be ignored
          }                                //
        }                                  //
        break;                             //
      }                                    //
                                           //
      // -----------------------------------------------------
      // Parent Type:
      // TYPE: Bag -> Bag in Bag 
      //       cascaded bag contains real data 
      //       like Installation and Log Path 
      // -----------------------------------------------------
      case MQITEM_BAG:                     //
      {                                    //
#ifdef  _LOGTERM_                          //
        printf( "\n======================================================\n" );
#endif                                     //
        mqrc=mqBagInq(respBag,0,&attrBag); // usable data are located 
        switch( mqrc )                     //  in cascaded (child) bag
        {                                  // use only:
          case MQRC_NONE: break;           //  - installation path
          default: goto _door;             //  - log path
        }                                  //
                                           //
        // ---------------------------------------------------
        // count the items in the child bag
        // ---------------------------------------------------
        mqrc=mqBagCountItem(attrBag,       // get the amount of items
                            MQSEL_ALL_SELECTORS);//  for all selectors in 
        if( mqrc > 0 )                     //  child bag
        {                                  //
          goto _door;                      //
        }                                  //
        else                               //
        {                                  //
          childItemCount = -mqrc;          //
          mqrc = MQRC_NONE;                //
        }                                  //
                                           //
        // ---------------------------------------------------
        // go through all child items
        //  this is the internal loop
        // ---------------------------------------------------
        for( j=0; j<childItemCount; j++ )  //
        {                                  //
          mqrc=mqItemInfoInq(attrBag,      //
                             MQSEL_ANY_SELECTOR, 
                             j,            //
                             &childSelector,
                             &childItemType );
          switch( mqrc )                   //
          {                                //
            case MQRC_NONE: break;         //
            default: goto _door;           //
          }                                //
                                           //
#ifdef    _LOGTERM_                        //
          printf( "   %2d selector: %04d %-30.30s type %10.10s",
                  j,                       //
                  childSelector,           //
                  mqSelector2str( childSelector ), 
                  mqItemType2str( childItemType ) ); 
#endif                                     //
                                           //
          // -------------------------------------------------
          // CHILD ITEM
          //   analyze each child item / selector
          // -------------------------------------------------
          switch( childItemType )          // main switch in 
          {                                //  internal loop
            // -----------------------------------------------
            // CHILD ITEM
            // TYPE: 32 bit integer
            // -----------------------------------------------
            case MQITEM_INTEGER:           // not a single integer 
            {                              //  can be used in this 
              mqrc = mqIntInq( attrBag,    //  program.
                               MQSEL_ANY_SELECTOR, // so the the selInt32Val
                               j,          //  does not have to be 
                               &selInt32Val ); //  evaluated
              switch( mqrc )               //
              {                            //
                case MQRC_NONE: break;     //
                default: goto _door;       //
              }                            //
                                           //
#ifdef        _LOGTERM_                    //
              pBuffer = (char*) itemValue2str( childSelector,
                                               (MQLONG) selInt32Val );
              if( pBuffer )                //
              {                            //
                printf( " value %s\n", pBuffer );
              }                            //
              else                         //
              {                            //
                printf( " value %d\n", (int) selInt32Val );
              }                            //
#endif                                     //               //
              break;                       // --- internal loop over child items
            }                              // --- Item Type Integer
                                           // 
            // -----------------------------------------------
            // CHILD ITEM
            // TYPE: string
            // -----------------------------------------------
            case MQITEM_STRING:            // installation path
            {                              //  and log path have
              mqrc = mqStrInq( attrBag,    //  type STRING
                               MQSEL_ANY_SELECTOR, 
                               j,           //  
                               ITEM_LENGTH, //
                               selStrVal,   //
                               &selStrLng );//
              switch( mqrc )                //
              {                             //
                case MQRC_NONE: break;      //
                default: goto _door;        //
              }                             //
                                            //
              mqrc = mqTrimStr( ITEM_LENGTH,// trim string 
                                selStrVal,  //
                                sBuffer );  //
              switch( mqrc )                //
              {                             //
                case MQRC_NONE: break;      //
                default: goto _door;        //
              }                             //
                                            //
              // ---------------------------------------------
              // CHILD ITEM
              // TYPE: string
              // analyze selector
              // ---------------------------------------------
              switch( childSelector )       // 
              {                             //
                case MQCA_SSL_KEY_REPOSITORY://
                {                           //
                 strncpy( _path, dirname( dirname( sBuffer )), PATH_MAX );
                  break;                    //
                }                           //
                default:                    //
                {                           //
                  break;                    //
                }                           // - internal loop over child items
              }                             // - analyze selector of type string
#ifdef        _LOGTERM_                     // 
              printf( " value %s\n", sBuffer );
#endif                                      //      
              break;                        // - internal loop over child items
            }                               // - Item Type String
                                            //
            // -----------------------------------------------
            // any other item type for child bag is an error
            // -----------------------------------------------
            default:                        //
            {                               //
              goto _door;                   //
            }                               // - switch child item type- default 
          }                                 // - switch child item type         
        }                                   // for each child item type
        break;                              // - switch parent item type
      }                                     // - case MQ item bag     
                                            //
      // -----------------------------------------------------
      // all other parent item types are not expected
      // -----------------------------------------------------
      default:                              //
      {                                     //
        goto _door;                         //
      }                                     // - switch parent item type-default
    }                                       // - switch parent item type     
  }                                         // - for each parent item       


_door:

  mqCloseBag( &cmdBag );
  mqCloseBag( &respBag );

#ifdef _LOGTERM_
  printf( "\n" );
#endif 

  logFuncExit( );
  return mqrc;
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
/*                                                                            */
/*  return code:                                                              */
/*    (int)    0  -> OK                                                       */
/*    (int) != 0 -> MQ Reason Code                                            */
/*                                                                            */
/******************************************************************************/
MQLONG dir2queue( MQHCONN  *_hConn, char* _path )
{
  logFuncCall( );

  MQLONG sysRc = MQRC_NONE;

  tAmqerr allFile[AMQ_MAX_ID+1];  // all  AMQERR??.LOG files [ 01 - 99]
  tAmqerr baseFile[4];            // base AMQERR??.LOG files [ 01 - 03]
                                  // allFile[0] is not in use
  MQOD dStoreQ = {MQOD_DEFAULT}; // queue descriptor
  MQOD dStateQ = {MQOD_DEFAULT}; // queue descriptor

  MQHOBJ hStoreQ;               // queue handle   
  MQHOBJ hStateQ;               // queue handle   


  struct stat aFileAttr ;          // file attributes for AMQERR file
  struct stat cFileAttr ;          // file attributes for CMPERR file

  int i;

  // -------------------------------------------------------
  // open queues
  // -------------------------------------------------------
  memcpy( dStoreQ.ObjectName, AMQ_STORE_Q, MQ_Q_NAME_LENGTH );

  sysRc=mqOpenObject( *_hConn               , // connection handle
                      &dStoreQ              , // queue descriptor
                      MQOO_OUTPUT           | // put message
                      MQOO_INPUT_AS_Q_DEF   | 
                      MQOO_FAIL_IF_QUIESCING, // fail if queue manager stopping
                      &hStoreQ              ); // queue handle

  switch( sysRc )
  {
    case MQRC_NONE: break;
    default: goto _door;
  }

  memcpy( dStoreQ.ObjectName, AMQ_STORE_Q, MQ_Q_NAME_LENGTH );

  sysRc=mqOpenObject( *_hConn               , // connection handle
                      &dStateQ              , // queue descriptor
                      MQOO_OUTPUT           | // put message
                      MQOO_FAIL_IF_QUIESCING, // fail if queue manager stopping
                      &hStateQ              ); // queue handle

  switch( sysRc )
  {
    case MQRC_NONE: break;
    default: goto _door;
  }

  // -------------------------------------------------------
  // initialize file list (assuming no file exists)
  // -------------------------------------------------------
  for( i=1; i<AMQ_MAX_ID+1; i++ )
  {
    sprintf( allFile[i].name, "%s/"AMQERR"%02d.LOG",_path,i);
    allFile[i].mtime = 0;
    allFile[i].length = 0;
  }

  for( i=1; i<4; i++ )
  {
    baseFile[i].mtime = 0;
    baseFile[i].length = 0;
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
    if( baseFile[1].mtime == 0 )
    {
    }

    sleep(LOOP);
  }

  _door:

  mqCloseObject( *_hConn, &hStoreQ );
  mqCloseObject( *_hConn, &hStateQ );

  logFuncExit( );
  return sysRc;
}