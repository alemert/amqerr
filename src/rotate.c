/******************************************************************************/
/*                                                                            */
/*   A M Q E R R   T O   Q U E U E                                            */
/*                                                                            */
/*  ------------------------------------------------------------------------  */
/*                                                                            */
/*  file: rotate.c                                                            */
/*                                                                            */
/*  functions:                                                                */
/*    - lsAmqerr                                                              */
/*    - rotateAmqerr                                              */
/*    - copy                                    */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*   I N C L U D E S                                                          */
/******************************************************************************/

// ---------------------------------------------------------
// system
// ---------------------------------------------------------
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

// ---------------------------------------------------------
// own 
// ---------------------------------------------------------
#include <ctl.h>
#include <msgcat/lgstd.h>

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

  for( i=AMQ_MAX_ID; i>3; i-- )
  {
    if( _arr[i].mtime > 0 ) break;
  }

  i++;
  if( i== 3 ) goto _door;

  for( ; i>4; i-- )
  {
    link( _arr[i-1].name, _arr[i].name);
  }

  if( _arr[4].mtime > 0 ) unlink( _arr[4].name );

  link( _arr[0].name, _arr[4].name);
  unlink( _arr[0].name );

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