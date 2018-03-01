/******************************************************************************/
/*                                                                            */
/*   A M Q E R R   T O   Q U E U E                                            */
/*                                                                            */
/*  ------------------------------------------------------------------------  */
/*                                                                            */
/*  file: rotate.c                                                            */
/*                                                                            */
/*  functions:                                                                */
/*    - initMQ                                            */
/*    - houseKeepingMQ                        */
/*    - getSendState                        */
/*    - putInitStateMsg                            */
/*    - disQmgr                        */
/*                    */
/*  macros: (amqerr.h)            */
/*    - getDataPath                                                           */
/*                                                                            */
/*  history:                                    */
/*  24.02.2018 am initial version                  */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*   I N C L U D E S                                                          */
/******************************************************************************/

// ---------------------------------------------------------
// system
// ---------------------------------------------------------
#include <limits.h>
#include <libgen.h>

// ---------------------------------------------------------
// MQ
// ---------------------------------------------------------
#include <cmqc.h>

// ---------------------------------------------------------
// own 
// ---------------------------------------------------------
#include <ctl.h>
#include <msgcat/lgmqm.h>

#include <mqbase.h>
#include <mqtype.h>

// ---------------------------------------------------------
// local
// ---------------------------------------------------------
#include "amqerr.h"
#include "lgloc.h"

/******************************************************************************/
/*   G L O B A L S                                                            */
/******************************************************************************/
MQHCONN ghCon;                // connection handle   
MQHOBJ  ghStoreQ;               // queue handle   
MQHOBJ  ghStateQ;               // queue handle   

/******************************************************************************/
/*   D E F I N E S                                                            */
/******************************************************************************/
#define AMQ_STATE_Q "ADMIN.AMQERR.STATE.QUEUE"
#define AMQ_STORE_Q "ADMIN.AMQERR.STORE.QUEUE"

/******************************************************************************/
/*   M A C R O S                                                              */
/******************************************************************************/

/******************************************************************************/
/*   P R O T O T Y P E S                                                      */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*   F U N C T I O N S                                                        */
/*                                                                            */
/******************************************************************************/
MQLONG initMQ( const char* _qmgrName )
{
  logFuncCall( );

  MQOD dStoreQ = {MQOD_DEFAULT}; // queue descriptor
  MQOD dStateQ = {MQOD_DEFAULT}; // queue descriptor

  int sysRc = MQRC_NONE ;

  // -------------------------------------------------------
  // connect to queue manager
  // -------------------------------------------------------
  sysRc = mqConn( (char*) _qmgrName, // queue manager          
                  &ghCon );         // connection handle            
  switch( sysRc )                    //
  {                                  //
    case MQRC_NONE: break;           // OK
    case MQRC_Q_MGR_NAME_ERROR:      // queue manager does not exists
    {                                //
      logger( LMQM_UNKNOWN_QMGR, _qmgrName );
      goto _door;                 //
    }                                //
    default: goto _door;          // error logged in mqConn
  }                                  //

  // -------------------------------------------------------
  // open queues
  // -------------------------------------------------------
  memcpy( dStoreQ.ObjectName, AMQ_STORE_Q, sizeof(AMQ_STORE_Q) );

  sysRc=mqOpenObject( ghCon                 , // connection handle
                      &dStoreQ              , // queue descriptor
                      MQOO_OUTPUT           | // put message
                      MQOO_INPUT_AS_Q_DEF   | 
                      MQOO_FAIL_IF_QUIESCING, // fail if queue manager stopping
                      &ghStoreQ              ); // queue handle

  switch( sysRc )
  {
    case MQRC_NONE: break;
    default: goto _door;
  }

  memcpy( dStateQ.ObjectName, AMQ_STATE_Q, sizeof(AMQ_STATE_Q) );

  sysRc=mqOpenObject( ghCon                 , // connection handle
                      &dStateQ              , // queue descriptor
                      MQOO_OUTPUT           | // put message
                      MQOO_INPUT_SHARED     | // get message
                      MQOO_BROWSE           | // open for browse
                      MQOO_FAIL_IF_QUIESCING, // fail if queue manager stopping
                      &ghStateQ              ); // queue handle

  switch( sysRc )
  {
    case MQRC_NONE: break;
    default: goto _door;
  }

  _door :

  logFuncExit( );

  return sysRc ;
}

/******************************************************************************/
/*  HOUSE KEEPING MQ                                                          */
/*                                                                            */
/*  description:                                                              */
/*    close all opened objects and disconnect from queue manager              */
/*                                                                            */
/*  attributes:                                                               */
/*    void                                                                    */
/*                                                                            */
/*  return code:                                                              */
/*    MQRC_NONE -> OK                                                         */
/*    other     -> ERR                                                        */
/*                                                                            */
/******************************************************************************/
MQLONG houseKeepingMQ()
{
  logFuncCall( );

  MQLONG sysRc ;

  sysRc = mqCloseObject( ghCon, &ghStoreQ );
  sysRc = mqCloseObject( ghCon, &ghStateQ );

  sysRc = mqDisc( &ghCon ); // connection handle            
  switch( sysRc )
  {
    case MQRC_NONE: break;
    default: break;
  }

  logFuncExit( );

  return sysRc ;
}

/******************************************************************************/
/*  GET SEND STATE                                                            */
/*                                                                            */
/*  description:                                                              */
/*    get the data about already send data from the ADMIN.AMQERR.SEND.QUEUE   */
/*    if no data available or missing put the initialization messages on the  */
/*      queue                                                                 */
/*                                                                            */
/*  attributes:                                                               */
/*    array of file structure                                                 */
/*                                                                            */
/*  return code:                                                              */
/*    MQRC_NONE -> OK                                                         */
/*    other     -> ERR                                                        */
/*    MQRC_NO_MESSAGE_AVAILABLE will is not an error. In this case            */
/*      initialization messages will be put to queue and return code will be  */
/*      will be set to MQRC_NONE                                              */
/*                                                                            */
/******************************************************************************/
MQLONG getSendState( tAmqerr* _file )
{
  logFuncCall( );

  MQLONG sysRc ;

  MQMD  bmd = { MQMD_DEFAULT };     // browse message descriptor
  MQMD  gmd = { MQMD_DEFAULT };     // get message descriptor (destructive)

  MQGMO bmo = { MQGMO_DEFAULT };    // browse message options
  MQGMO gmo = { MQGMO_DEFAULT };    // get message options (destructive)

  bmo.Version       = MQGMO_VERSION_2 ;
  bmo.Options      |= MQGMO_BROWSE_FIRST;
  bmo.MatchOptions  = MQMO_NONE;

  gmo.Version      = MQGMO_VERSION_2 ;
  gmo.Options     |=
  gmo.MatchOptions = MQMO_MATCH_MSG_ID ;

  tAmqerrState msg;
  tAmqerrState dummy;  // dummy message; garbage
  MQLONG msgLng = sizeof(tAmqerrState);

  unsigned short id ;
  

  // -------------------------------------------------------
  // browse all messages
  // -------------------------------------------------------
  for( id=1; sysRc != MQRC_NO_MSG_AVAILABLE; id++ )
  {                                       //
    sysRc = mqGet( ghCon          ,       // browse message
                   ghStateQ       ,       //
                   &msg           ,       //
                   &msgLng        ,       //
                   &bmd           ,       //
                   bmo            ,       //
                   MQGMO_NO_WAIT );       //
                                          //
    if( (bmo.Options & MQGMO_BROWSE_FIRST) == MQGMO_BROWSE_FIRST) 
    {                                     //
      bmo.Options -= MQGMO_BROWSE_FIRST;  // change browse first to 
      bmo.Options |= MQGMO_BROWSE_NEXT ;  // browse next after 
    }                                     //  the first message
                                          //
    switch( sysRc )                       //
    {                                     //
      case MQRC_NONE:                     //
      {                                   //
        if( memcmp( _file[msg.fileId].msgId, MQMI_NONE, sizeof(MQBYTE24) ) != 0)
	{                                 //
	  memcpy( gmd.MsgId, _file[msg.fileId].msgId, sizeof(MQBYTE24) );
	  sysRc = mqGet( ghCon         ,  //
                         ghStateQ      ,  //
                         &dummy        ,  //
                         &msgLng       ,  //
                         &gmd          ,  //
                         gmo           ,  //
                         MQGMO_NO_WAIT ); //
          switch( sysRc )
	  {
            case MQRC_NONE:             break;
	    case MQRC_NO_MSG_AVAILABLE: break;
	    default: goto _door;
	  }
	}
	_file[msg.fileId].length = msg.length;
	_file[msg.fileId].mtime  = msg.time  ;
        memcpy(_file[msg.fileId].name,msg.file,sizeof(msg.file));
	memcpy(_file[msg.fileId].msgId, bmd.MsgId, sizeof(MQBYTE24) );
        break;
      }
      case MQRC_NO_MSG_AVAILABLE:
      {
        break;
      }
      default: goto _door;
    }
  }

  for( id=1; id<AMQ_MAX_BASE_ID+1; id++ )
  {
    
    if( memcmp( _file[id].msgId, MQMI_NONE, sizeof(MQBYTE24) ) == 0)
    {
      sysRc = putInitStateMsg(id);
      if( sysRc != MQRC_NONE ) goto _door;
    }
  }

  _door:

  logFuncExit( );
  if( sysRc == MQRC_NO_MSG_AVAILABLE ) sysRc = MQRC_NONE ;
  return sysRc ;
}

/******************************************************************************/
/*  PUT INITIALIZATION STATE MESSAGES ON THE STATE QUEUE                      */
/*                                                                            */
/*  description:                                                              */
/*    if some the status messages are missing, create default ones and put    */
/*    them on the queue                                                       */
/*                                                                            */
/*  attributes:                                                               */
/*    void                                                                    */
/*                                                                            */
/*  return code:                                                              */
/*    MQRC_NONE -> OK                                                         */
/*    -1        -> ERR file id to high (>3)            */
/*    other     -> ERR                                                        */
/*                                                                            */
/******************************************************************************/
MQLONG putInitStateMsg( unsigned short _id )
{
  logFuncCall( );
  MQLONG sysRc = MQRC_NONE;

  tAmqerrState msg = {AMQERR_LOG_DEFAULT} ;
  char fileName[sizeof(msg.file)+1];

  MQMD md = {MQMD_DEFAULT};
  MQPMO pmo = {MQPMO_DEFAULT};

  pmo.Options = MQPMO_FAIL_IF_QUIESCING + 
                MQPMO_NO_CONTEXT ;

  if( _id > AMQ_MAX_BASE_ID ) 
  {
    logger( LAER_LOG_ID_HIGH, _id );
    sysRc = -1 ;
    goto _door;
  }

  msg.fileId = _id;  
  sprintf(fileName,AMQERR"0%d.LOG",_id);
  memcpy(msg.file,fileName,sizeof(msg.file));

  sysRc = mqPut( ghCon,
		ghStateQ,
		&md,
		&pmo,
		(PMQVOID)&msg,
		sizeof(msg) );

  switch( sysRc )
  {
    case MQRC_NONE: break;
    default: goto _door;
  }

  _door:

  logFuncExit( );
  return sysRc ;
}

/******************************************************************************/
/*  DISPLAY QUEUE MANAGER                                                     */
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
MQLONG disQmgr( MQLONG _selector, char* _strAttr )
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
  MQCHAR  selStrVal[ITEM_LENGTH];

  MQLONG selStrLng;

  char   sBuffer[ITEM_LENGTH + 1];

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
  //   1. setup the list of arguments MQIACF_ALL = selector
  //        selectors with macro: 
  //          - MQCA_SSL_KEY_REPOSITORY
  //   2. send a command MQCMD_INQUIRE_Q_MGR = DISPLAY QMGR
  // -------------------------------------------------------
  mqrc = mqSetInqAttr( cmdBag, _selector);// set attribute 
                                          // for the PCF command
  switch( mqrc )                          // DISPLAY QMGR SSLKEYR
  {                                       //
    case MQRC_NONE: break;                //
    default: goto _door;                  //
  }                                       //
                                          //
  mqrc = mqExecPcf( ghCon,                // send a command to 
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
	      if( childSelector == _selector )
              {
               strncpy( _strAttr, dirname( dirname( sBuffer )), PATH_MAX );
              }
#if(0)
              switch( childSelector )       // 
              {                             //
                case MQCA_SSL_KEY_REPOSITORY://
                {                           //
                 strncpy( _strAttr, dirname( dirname( sBuffer )), PATH_MAX );
                  break;                    //
                }                           //
                default:                    //
                {                           //
                  break;                    //
                }                           // - internal loop over child items
              }                             // - analyze selector of type string
#endif
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