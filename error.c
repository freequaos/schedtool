/*
 Schedtool
 (c) by Freek <email_undisclosed>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 Please see the file LICENSE for details.

 */

/*
 Content:

 This includes a nice error-function
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "error.h"

/* print ERROR: + given message + errormsg received via errno */
void decode_error(char *fmt, ...)
{
	va_list args;

	char *msg=NULL;
	int tmp_errno=errno;

	printf("ERROR: ");
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);

	if(errno) {
		/* do our own errors */
		if (errno == EINVAL) {
			msg="value out of range / policy not implemented";
			goto bail;
		}

		/* hu, can this be unsave ? */
		msg=(char *)strerror(errno);

		/* the strerror()-call went wrong */
		if(errno != tmp_errno) {
			msg="unknown error code, sorry";
		}

	bail:
		printf(" - %s",msg);
	}
        printf("\n");
}
