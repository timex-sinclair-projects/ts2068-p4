/* szx_resave: read a snapshot with libspectrum and write it back as SZX,
   for tools/host/compare_szx.py.

     szx_resave IN.szx OUT.szx [--compress]

   Uncompressed (libspectrum's NO_COMPRESSION flag) unless --compress, when
   it writes as Fuse does, with zlib-compressed pages. No creator chunk.
   Built against the reference libspectrum in ~/esp/ref/prefix
   (test/machine/Makefile). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libspectrum.h>

int
main( int argc, char **argv )
{
  FILE *f;
  long n;
  libspectrum_byte *in, *out = NULL;
  size_t length = 0;
  int out_flags, compress = argc > 3 && !strcmp( argv[3], "--compress" );
  libspectrum_snap *snap;

  if( argc < 3 ) {
    fprintf( stderr, "usage: szx_resave IN.szx OUT.szx [--compress]\n" );
    return 2;
  }
  f = fopen( argv[1], "rb" );
  if( !f ) { perror( argv[1] ); return 1; }
  fseek( f, 0, SEEK_END ); n = ftell( f ); fseek( f, 0, SEEK_SET );
  in = malloc( n );
  if( fread( in, 1, n, f ) != (size_t)n ) { perror( argv[1] ); return 1; }
  fclose( f );

  if( libspectrum_init() ) return 1;
  snap = libspectrum_snap_alloc();
  if( libspectrum_snap_read( snap, in, n, LIBSPECTRUM_ID_SNAPSHOT_SZX, argv[1] ) ) {
    fprintf( stderr, "szx_resave: %s: libspectrum can't read it\n", argv[1] );
    return 1;
  }
  if( libspectrum_snap_write( &out, &length, &out_flags, snap,
                              LIBSPECTRUM_ID_SNAPSHOT_SZX, NULL,
                              compress ? 0 : LIBSPECTRUM_FLAG_SNAPSHOT_NO_COMPRESSION ) ) {
    fprintf( stderr, "szx_resave: can't write it\n" );
    return 1;
  }
  if( out_flags & LIBSPECTRUM_FLAG_SNAPSHOT_MAJOR_INFO_LOSS )
    fprintf( stderr, "szx_resave: libspectrum reports major information loss\n" );

  f = fopen( argv[2], "wb" );
  if( !f || fwrite( out, 1, length, f ) != length ) { perror( argv[2] ); return 1; }
  fclose( f );
  libspectrum_free( out );
  libspectrum_snap_free( snap );
  return 0;
}
