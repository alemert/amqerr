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
/*    - getDataPath                                                          */
/*    - lsAmqerr                                */
/*    - rotateAmqerr                        */
/*    - dir2queue                                                      */
/*                                                                            */
/*  history                                                                   */
/*  19.02.2018 am initial version                              */
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
#include <limits.h>
#include <libgen.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

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
#define ITEM_LENGTH   PATH_MAX 
#define AMQERR        "AMQERR"
#define AMQ_FILE_NAME AMQERR"??.LOG"
#define AMQ_MAX_ID    99
#define CMPERR        "CMPERR03.LOG"

/******************************************************************************/
/*   M A C R O S                                                              */
/******************************************************************************/

/******************************************************************************/
/*   S T R U C T                                                              */
/******************************************************************************/
struct sAmqerr
{
  char name[PATH_MAX+1]; // length of AMQ_FILE_NAME + 1
  time_t mtime ;         // modification time of the amqerr file
  off_t length;          // lenght of the amqerr file
};

/******************************************************************************/
/*   T Y P E S                                                                */
/******************************************************************************/
typedef struct sAmqerr tAmqerr ;

/******************************************************************************/
/*   P R O T O T Y P E S                                                      */
/******************************************************************************/
MQLONG amqerrsend( MQHCONN *_hConn );
MQLONG getDataPath( MQHCONN *_hConn, char* _path );
int    lsAmqerr( const char* _path, tAmqerr* _arr, int _lng);
int    rotateAmqerr( tAmqerr *_arr );
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
/*  description: read amqerr log tag and put it to the queue                  */
/*                                                                            */
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
  dir2queue( _hConn, path );

  _door:

  logFuncExit( );

  return sysRc;
}

/******************************************************************************/
/*  COPY FILE                                                                 */
/*                                                                            */
/*  description:                                                              */
/*    copy file source to file destination                                    */
/*                                                                            */
/*    attributes:                                                             */
/*                                                                            */
/*  return code:                                                              */
/*    0  -> OK                                                                */
/*    >0 -> ERR                                                               */
/*                                                                            */
/******************************************************************************/
int copy( const char* _src, const char* _dst )
{
  logFuncCall( );

  int sysRc = 0 ;

  int srcFD ;
  int dstFD ;

  ssize_t size ;

  #define CHUNK_SIZE 4096
  char chunk[CHUNK_SIZE];

  errno = 0;
  // -------------------------------------------------------
  // open both files
  // -------------------------------------------------------
  srcFD=open(_src,O_RDONLY) ;
  if( errno )
  {
    sysRc = errno ;
    logger( LSTD_OPEN_FILE_FAILED, _src );
    logger( LSTD_ERRNO_ERR, sysRc, strerror( sysRc ) );
    goto _door ;
  }

  if( !(dstFD=open( _dst,
                    O_CREAT|O_TRUNC|O_WRONLY,
		    S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)) )
  {
    sysRc = errno ;
    logger( LSTD_OPEN_FILE_FAILED, _src );
    logger( LSTD_ERRNO_ERR, sysRc, strerror( sysRc ) );
    goto _door ;
  }
  
  // -------------------------------------------------------
  // copy data
  // -------------------------------------------------------
  while( 1 )
  {
    size = read( srcFD, chunk, CHUNK_SIZE );
    if( errno )
    {
      sysRc = errno ;
      logger( LSTD_ERR_READING_FILE, _src );
      logger( LSTD_ERRNO_ERR, sysRc, strerror( sysRc ) );
      goto _door ;
    }
    if( size == 0 ) break;
    if( (write( dstFD, chunk, size )) != size )
    {
      sysRc = errno ;
      logger( LSTD_FILE_COPY_ERR, _src, _dst );
      logger( LSTD_ERRNO_ERR, sysRc, strerror( sysRc ) );
    }
    if( size < CHUNK_SIZE ) break;
  }

  _door:

  if( srcFD ) close(srcFD) ;
  if( dstFD ) close(dstFD) ;

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
/*  list AMQERR files                                                         */
/*                                                                            */
/*  description:                                                              */
/*    list all AMQERR??.LOG files, get name, modification time and length     */
/*                                                                            */
/*  attributes:                                                               */
/*    - path to queue manager AMQERR??.LOG files                              */
/*    - array for list                                                        */
/*    - the length of the array                                               */
/*                                                                            */
/*  return code:                                                              */
/*    0  -> OK                                                                */
/*    >0 -> errno                                                             */
/*    <0 -> size of the array error                                           */
/*                                                                            */
/******************************************************************************/
int lsAmqerr( const char* _path, tAmqerr* _arr, int _lng )
{
  logFuncCall( );
  int sysRc ;

  struct dirent *pDirent ;        // data path directory
  DIR *pDir ;                     // data path directory

  struct stat fileAttr ;          // file attributes for AMQERR??.LOG file
  unsigned short id = 0;          // the ?? part of AMQERR??.LOG file name

  char amqerr[PATH_MAX+1];        // absolute file name of AMQERR??.LOG file

  // -------------------------------------------------------
  // open data path directory for list
  // -------------------------------------------------------
  pDir = opendir( _path );
  if( !pDir )
  {
    sysRc = errno ;
    logger( LSTD_OPEN_DIR_FAILED, _path );
    logger( LSTD_ERRNO_ERR, sysRc, strerror( sysRc ) );
    goto _door ;
  }

  // -------------------------------------------------------
  // list all files
  // -------------------------------------------------------
  while( (pDirent=readdir(pDir)) )  // go through all files
  {                                 //
    if( memcmp( pDirent->d_name, AMQERR, strlen(AMQERR) ) != 0  &&
        memcmp( pDirent->d_name, CMPERR, strlen(CMPERR) ) != 0  )
    {                               // ignore all files but AMQERR
      continue;                     //
    }                               //

    // -----------------------------------------------------
    // get the id of the AMQERR file - ?? in AMQERR??.LOG
    //   convert the digit from letter to digit by subtract 
    //   ASCII(letter) by ASCII('0')
    // -----------------------------------------------------
    if( memcmp( pDirent->d_name, CMPERR, strlen(CMPERR) ) == 0 )
    {
      id = 0 ;
    }
    else
    {
      id = ((int)pDirent->d_name[6]-48)*10 + // 1st digit - ASCII(0) * 10
           ((int)pDirent->d_name[7]-48) ;    // 2nd digit - ASCII(0) 
    }

    if( id > _lng ) continue ;  // file id is to high for the array

    // -----------------------------------------------------
    // get the file information e.g length and modification time 
    // -----------------------------------------------------
   snprintf(amqerr, PATH_MAX, "%s/%s", (char*) _path, pDirent->d_name );
   stat( amqerr, &fileAttr );

   #if(1)
     printf( "%s ", amqerr );
     printf( "%d ", (int)id );
     printf( "size: %d ", (int)fileAttr.st_size );
     printf( "mtime: %d\n", (int)fileAttr.st_mtime );
   #endif

   // ---------------------------------------------------
   // fill file lists
   // ---------------------------------------------------
    memcpy( _arr[id].name,amqerr,strlen(amqerr));
    _arr[id].mtime = fileAttr.st_mtime ;
    _arr[id].length = fileAttr.st_size  ;
  }

  _door:

  if( pDir ) closedir(pDir);

  logFuncExit( );
  return sysRc;
}

/******************************************************************************/
/*  rotate AMQERR files                                                       */
/*                                                                            */
/*  description:                                                              */
/*    move AMQERR files >3 to higher name                                     */
/*    file AMQERR98.LOG to AMQERR99.LOG                                       */
/*    file AMQERR{n}.LOG to AMQERR{n+1}.LOG                                   */
/*    file AMQERR04.LOG to AMQERR05.LOG                                       */
/*    file CMPERR03.LOG to AMQERR04.LOG                                       */
/*                                                                            */
/*  attributes:                                                               */
/*    - list of files with time and length                                    */
/*                                                                            */
/*  return code:                                                              */
/*    0 -> OK                                                                 */
/*    1 -> ERR                                                                */
/*                                                                            */
/******************************************************************************/
int rotateAmqerr( tAmqerr *_arr )
{
  logFuncCall( );
  int sysRc = 0 ;

  int i;

  FILE* file;

  if( (file=fopen(_arr[0].name,"r") ) )
  {
    fclose(file);
  }
  else
  {
    copy( _arr[3].name, _arr[0].name );
  }

  for( i=AMQ_MAX_ID; i>3; i-- )
  {
    if( _arr[i].mtime > 0 ) break;
  }

  if( i== 3 ) goto _door;

  for( ; i>4; i-- )
  {
    unlink( _arr[i].name );
    link( _arr[i-1].name, _arr[i].name);
  }

    unlink( _arr[4].name );
    link( _arr[0].name, _arr[4].name);
    unlink( _arr[4].name );

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

  int i;

  // -------------------------------------------------------
  // initialize file list (assuming no file exists)
  // -------------------------------------------------------
  for( i=0; i<AMQ_MAX_ID; i++ )
  {
    allFile[i].mtime = 0;
    allFile[i].length = 0;
  }

  for( i=1; i<4; i++ )
  {
    baseFile[i].mtime = 0;
    baseFile[i].length = 0;
  }

  // -------------------------------------------------------
  // check if compare file exists, if not create it
  // -------------------------------------------------------
  sprintf( allFile[0].name, "%s/"CMPERR,_path);

  while( 1 )
  {
    sysRc = lsAmqerr( _path, allFile, AMQ_MAX_ID );
    if( allFile[0].mtime < allFile[3].mtime ) rotateAmqerr( allFile );
    
    sleep(5);
  }

  _door:


  logFuncExit( );
  return sysRc;
}