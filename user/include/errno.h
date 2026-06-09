/* errno.h - Error number definitions */

#ifndef ERRNO_H
#define ERRNO_H

/* Global errno variable (per-process in future, global for now) */
extern int errno;

/* Error codes */
#define EPERM     1   /* Operation not permitted */
#define ENOENT    2   /* No such file or directory */
#define ESRCH     3   /* No such process */
#define EINTR     4   /* Interrupted system call */
#define EIO       5   /* I/O error */
#define ENXIO     6   /* No such device or address */
#define ENOMEM   12   /* Out of memory */
#define EACCES   13   /* Permission denied */
#define EFAULT   14   /* Bad address */
#define EBUSY    16   /* Device or resource busy */
#define EEXIST   17   /* File exists */
#define ENODEV   19   /* No such device */
#define ENOTDIR  20   /* Not a directory */
#define EISDIR   21   /* Is a directory */
#define EINVAL   22   /* Invalid argument */
#define ENFILE   23   /* Too many open files in system */
#define EMFILE   24   /* Too many open files */
#define ENOSPC   28   /* No space left on device */
#define ENOSYS   38   /* Function not implemented */
#define ENOTEMPTY 39  /* Directory not empty */
#define EDOM     33   /* Math argument out of domain */
#define ERANGE   34   /* Result too large */
#define EAGAIN   11   /* Try again / Resource temporarily unavailable */
#define ETIMEDOUT 110 /* Connection timed out */
#define ECONNREFUSED 111 /* Connection refused */
#define ECONNRESET  104  /* Connection reset by peer */

#endif /* ERRNO_H */
