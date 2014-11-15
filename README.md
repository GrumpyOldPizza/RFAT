-



OVERVIEW

RFAT got it's name from the need for a Robust FAT implementation. The original
problem at hand was to implement low latency logging on an embedded system
with a limited amount of RAM. While there are many available alternative out
there none seemed to cater especially to SDCARDs, or would deal with the fact
that the SPI/SDIO bus connection can be error prone. While there are solutions
to part of the problem out there, none was to be found that would put
everything together.

What does RFAT differently ?

    CRC protected SPI interface with error detection and command retry
    logic at almost no extra performance cost.

    Optimized backend interface to the SDCARD taking advantage read
    prefetching and write coalescing allowing read/write throughput of
    around 2MB/sec on a 20MHz SPI bus using small 512 byte chunks. 

    Support for various cache configurations ranging from 512 bytes to
    2048 bytes, or even a per file 512 byte sector/block cache. 

    Special handling for meta data updates, so that in case of a
    read/write failure any further updates to the corruped file system
    are omitted. 

    Contiguous pre-allocation for files at f_open() time, so that
    writing to such a file can commence without further extra meta
    data accesses. This results in the lowest possible write latency
    and the highest possible throughput. 

    All FAT and Directory updates have been structured in a way such
    that a power failure will most likely only result in lost cluster
    chains (space wasted on the SDCARD), but no file system corruption. 

    There is a TRANSACTION SAFE mode available that is 100% power fail
    safe. First the intent to execute a file system operation is
    recorded, and then the operation is commited. If there is a power
    failure during the commit, the commit is restarted. The transition from
    record to commit is done via a single atomic SDCARD update.

    There are various feature options like VFAT support, or allowing
    file names to be specified in full UTF8 range for VFAT. There is
    FSINFO support, options to maintain the VOLUME_DIRTY and/or
    MEDIA_FAILURE bits in the boot sector.   
-



API

The API is based upon the ANSI-C Library interface. This API picked
mirrors various commercial and freely available options. There is no
good reason to reinvent the wheel. The API is divided in volume
related, directory related, file related and stream related routines:


  VOLUME

    int     f_initvolume(void);
    int     f_delvolume(void);
    int     f_format(int fattype);
    int     f_hardformat(int fattype);
    int     f_getfreespace(F_SPACE *pspace);
    int     f_getserial(unsigned long *p_serial);
    int     f_setlabel(const char *volname);
    int     f_getlabel(char *volname, int length);


  DIRECTORY

    int     f_mkdir(const char *dirname);
    int     f_rmdir(const char *dirname);
    int     f_chdir(const char *dirname);
    int     f_getcwd(char *dirname, int length);


  FILE
   
    int     f_rename(const char *filename, const char *newname);
    int     f_delete(const char *filename);
    long    f_filelength(const char *filename);
    int     f_findfirst(const char *filename, F_FIND *find);
    int     f_findnext(F_FIND *find);
    int     f_settimedate(const char *filename, unsigned short ctime, unsigned short cdate);
    int     f_gettimedate(const char *filename, unsigned short *p_ctime, unsigned short *p_cdate);
    int     f_setattr(const char *filename, unsigned char attr);
    int     f_getattr(const char *filename, unsigned char *p_attr);


  STREAM

    F_FILE *f_open(const char *filename, const char *type);
    int     f_close(F_FILE *file);
    int     f_flush(F_FILE *file);
    long    f_write(const void *buffer, long size, long count, F_FILE *file);
    long    f_read(void *buffer, long size, long count, F_FILE *file);
    int     f_seek(F_FILE *file, long offset, int whence);
    long    f_tell(F_FILE *file);
    int     f_eof(F_FILE *file);
    int     f_error(F_FILE *file);
    int     f_rewind(F_FILE *file);
    int     f_putc(int c, F_FILE *file);
    int     f_getc(F_FILE *file);
    int     f_seteof(F_FILE *file);


It is noteworthy that RFAT implements full file/stream buffering. That implies
that errors might be arriving late, when buffered data ultimatively ends up on
the SDCARD. If an error occurs, it's sticky. As with the ANSI-C standard
library an error condition can be cleaned by using f_rewind(). The rationale
behind this is that if an error occurs, one cannot have a defined file
position. Hence it needs to be ensured that along with clearing the error
status, the file position gets reset to a value that is always legal.

-



PORTING

The porting interface is directly at the SPI port level to avoid overly
complicated and tricky protocol handling. A reasonable implementation needs to
only deal with the following expected routines:

    uint32_t RFAT_PORT_DISK_SPI_MODE(int mode);
    int      RFAT_PORT_DISK_SPI_PRESENT(void);
    int      RFAT_PORT_DISK_SPI_WRITE_PROTECTED(void);
    void     RFAT_PORT_DISK_SPI_SELECT(void);
    void     RFAT_PORT_DISK_SPI_DESELECT(void);
    void     RFAT_PORT_DISK_SPI_SEND(uint8_t data);
    uint8_t  RFAT_PORT_DISK_SPI_RECEIVE(void);
    void     RFAT_PORT_DISK_SPI_SEND_BLOCK(const uint8_t *data);
    uint16_t RFAT_PORT_DISK_SPI_RECEIVE_BLOCK(uint8_t *data);
-



SOURCE

The source is organized into a few file, so that the compiler can do the best
possible job of while file optimizations. It's assumed that gcc is in use (say
https://launchpad.net/gcc-arm-embedded), with "-fdata-sections" and
"-ffunction-sections", so that unneeded code/data can be
stripped by the linker.

    rfat.h

       Application level main include file.


    rfat_config.h
    
        Configuration options/defines. See CONFIGURATION.txt for details.


    rfat_core.[ch]

        Core FAT file system implementation.


    rfat_disk.[ch]

        Backend SDCARD interface for the file system; protocol
	handling, error handling and such.


    rfat_port.h

        Include file for rfat_core.c / rfat_disk.c to declare the
	porting interface macros/defines.


    tm4c123_disk.[ch]

        Sample target implementation for the TM4C123.


    API.txt

       A detailed descripiton of the supported API.


    CONFIGUIRATION.txt

       Implementation notes, design notes, and detailed description of
       the configuration options in rfat_config.h.


    PORTING.txt

       Detailed description of the porting interface.


    TM4C123.txt

       Implementation notes for the "TM4C123GXL Lauchpad" along with the
       "Color LCD Booster Pack" that is the traget for the reference
       implementation. 


    BSP/*
    CMSIS/*
    Makefile
    main.c

        A simple sample application demonstrating the contiguous write
	performance using 512 byte chunks to write 32MB into a SDCARD.



